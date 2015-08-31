/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Handling of method debug info in a .dex file.
 */

#include "DexDebugInfo.h"
#include "DexProto.h"
#include "Leb128.h"

#include <stdlib.h>
#include <string.h>

/*
 * Decode the arguments in a method signature, which looks something
 * like "(ID[Ljava/lang/String;)V".
 *
 * Returns the type signature letter for the next argument, or ')' if
 * there are no more args.  Advances "pSig" to point to the character
 * after the one returned.
 */
static char decodeSignature(const char** pSig)
{
    const char* sig = *pSig;

    if (*sig == '(')
        sig++;

    if (*sig == 'L') {
        /* object ref */
        while (*++sig != ';')
            ;
        *pSig = sig+1;
        return 'L';
    }
    if (*sig == '[') {
        /* array; advance past array type */
        while (*++sig == '[')
            ;
        if (*sig == 'L') {
            while (*++sig != ';')
                ;
        }
        *pSig = sig+1;
        return '[';
    }
    if (*sig == '\0')
        return *sig;        /* don't advance further */

    *pSig = sig+1;
    return *sig;
}

/*
 * returns the length of a type string, given the start of the
 * type string. Used for the case where the debug info format
 * references types that are inside a method type signature.
 */
static int typeLength(const char *type) {
    // Assumes any leading '(' has already been gobbled
    const char *end = type;
    decodeSignature(&end);
    return end - type;
}

/*
 * Reads a string index as encoded for the debug info format,
 * returning a string pointer or NULL as appropriate.
 */
static const char* readStringIdx(const DexFile* pDexFile,
        const u1** pStream) {
    u4 stringIdx = readUnsignedLeb128(pStream);

    // Remember, encoded string indicies have 1 added to them.
    if (stringIdx == 0) {
        return NULL;
    } else {
        return dexStringById(pDexFile, stringIdx - 1);
    }
}

/*
 * Reads a type index as encoded for the debug info format, returning
 * a string pointer for its descriptor or NULL as appropriate.
 */
static const char* readTypeIdx(const DexFile* pDexFile,
        const u1** pStream) {
    u4 typeIdx = readUnsignedLeb128(pStream);

    // Remember, encoded type indicies have 1 added to them.
    if (typeIdx == 0) {
        return NULL;
    } else {
        return dexStringByTypeIdx(pDexFile, typeIdx - 1);
    }
}

struct LocalInfo {
    const char *name;
    const char *descriptor;
    const char *signature;
    u2 startAddress;
    bool live;
};

static void emitLocalCbIfLive(void *cnxt, int reg, u4 endAddress,
        LocalInfo *localInReg, DexDebugNewLocalCb localCb)
{
    if (localCb != NULL && localInReg[reg].live) {
        localCb(cnxt, reg, localInReg[reg].startAddress, endAddress,
                localInReg[reg].name,
                localInReg[reg].descriptor,
                localInReg[reg].signature == NULL
                ? "" : localInReg[reg].signature );
    }
}

static void invalidStream(const char* classDescriptor, const DexProto* proto) {
    IF_ALOGE() {
        char* methodDescriptor = dexProtoCopyMethodDescriptor(proto);
        ALOGE("Invalid debug info stream. class %s; proto %s",
                classDescriptor, methodDescriptor);
        free(methodDescriptor);
    }
}

static void dexDecodeDebugInfo0(
            const DexFile* pDexFile,
            const DexCode* pCode,
            const char* classDescriptor,
            u4 protoIdx,
            u4 accessFlags,
            DexDebugNewPositionCb posCb, DexDebugNewLocalCb localCb,
            void* cnxt,
            const u1* stream,
            LocalInfo* localInReg)
{
    DexProto proto = { pDexFile, protoIdx };
    u4 insnsSize = pCode->insnsSize;
    u4 line = readUnsignedLeb128(&stream);
    u4 parametersSize = readUnsignedLeb128(&stream);
    u2 argReg = pCode->registersSize - pCode->insSize;
    u4 address = 0;

    if ((accessFlags & ACC_STATIC) == 0) {
        /*
         * The code is an instance method, which means that there is
         * an initial this parameter. Also, the proto list should
         * contain exactly one fewer argument word than the insSize
         * indicates.
         */
        assert(pCode->insSize == (dexProtoComputeArgsSize(&proto) + 1));
        localInReg[argReg].name = "this";
        localInReg[argReg].descriptor = classDescriptor;
        localInReg[argReg].startAddress = 0;
        localInReg[argReg].live = true;
        argReg++;
    } else {
        assert(pCode->insSize == dexProtoComputeArgsSize(&proto));
    }

    DexParameterIterator iterator;
    dexParameterIteratorInit(&iterator, &proto);

    while (parametersSize-- != 0) {
        const char* descriptor = dexParameterIteratorNextDescriptor(&iterator);
        const char *name;
        int reg;

        if ((argReg >= pCode->registersSize) || (descriptor == NULL)) {
            invalidStream(classDescriptor, &proto);
            return;
        }

        name = readStringIdx(pDexFile, &stream);
        reg = argReg;

        switch (descriptor[0]) {
            case 'D':
            case 'J':
                argReg += 2;
                break;
            default:
                argReg += 1;
                break;
        }

        if (name != NULL) {
            localInReg[reg].name = name;
            localInReg[reg].descriptor = descriptor;
            localInReg[reg].signature = NULL;
            localInReg[reg].startAddress = address;
            localInReg[reg].live = true;
        }
    }

    for (;;)  {
        u1 opcode = *stream++;
        u2 reg;

        switch (opcode) {
            case DBG_END_SEQUENCE:
                return;

            case DBG_ADVANCE_PC:
                address += readUnsignedLeb128(&stream);
                break;

            case DBG_ADVANCE_LINE:
                line += readSignedLeb128(&stream);
                break;

            case DBG_START_LOCAL:
            case DBG_START_LOCAL_EXTENDED:
                reg = readUnsignedLeb128(&stream);
                if (reg > pCode->registersSize) {
                    invalidStream(classDescriptor, &proto);
                    return;
                }

                // Emit what was previously there, if anything
                emitLocalCbIfLive(cnxt, reg, address,
                    localInReg, localCb);

                localInReg[reg].name = readStringIdx(pDexFile, &stream);
                localInReg[reg].descriptor = readTypeIdx(pDexFile, &stream);
                if (opcode == DBG_START_LOCAL_EXTENDED) {
                    localInReg[reg].signature
                        = readStringIdx(pDexFile, &stream);
                } else {
                    localInReg[reg].signature = NULL;
                }
                localInReg[reg].startAddress = address;
                localInReg[reg].live = true;
                break;

            case DBG_END_LOCAL:
                reg = readUnsignedLeb128(&stream);
                if (reg > pCode->registersSize) {
                    invalidStream(classDescriptor, &proto);
                    return;
                }

                emitLocalCbIfLive (cnxt, reg, address, localInReg, localCb);
                localInReg[reg].live = false;
                break;

            case DBG_RESTART_LOCAL:
                reg = readUnsignedLeb128(&stream);
                if (reg > pCode->registersSize) {
                    invalidStream(classDescriptor, &proto);
                    return;
                }

                if (localInReg[reg].name == NULL
                        || localInReg[reg].descriptor == NULL) {
                    invalidStream(classDescriptor, &proto);
                    return;
                }

                /*
                 * If the register is live, the "restart" is superfluous,
                 * and we don't want to mess with the existing start address.
                 */
                if (!localInReg[reg].live) {
                    localInReg[reg].startAddress = address;
                    localInReg[reg].live = true;
                }
                break;

            case DBG_SET_PROLOGUE_END:
            case DBG_SET_EPILOGUE_BEGIN:
            case DBG_SET_FILE:
                break;

            default: {
                int adjopcode = opcode - DBG_FIRST_SPECIAL;

                address += adjopcode / DBG_LINE_RANGE;
                line += DBG_LINE_BASE + (adjopcode % DBG_LINE_RANGE);

                if (posCb != NULL) {
                    int done;
                    done = posCb(cnxt, address, line);

                    if (done) {
                        // early exit
                        return;
                    }
                }
                break;
            }
        }
    }
}

// TODO optimize localCb == NULL case
void dexDecodeDebugInfo(
            const DexFile* pDexFile,
            const DexCode* pCode,
            const char* classDescriptor,
            u4 protoIdx,
            u4 accessFlags,
            DexDebugNewPositionCb posCb, DexDebugNewLocalCb localCb,
            void* cnxt)
{
    const u1* stream = dexGetDebugInfoStream(pDexFile, pCode);
    LocalInfo localInReg[pCode->registersSize];

    memset(localInReg, 0, sizeof(LocalInfo) * pCode->registersSize);

    if (stream != NULL) {
        dexDecodeDebugInfo0(pDexFile, pCode, classDescriptor, protoIdx, accessFlags,
            posCb, localCb, cnxt, stream, localInReg);
    }

    for (int reg = 0; reg < pCode->registersSize; reg++) {
        emitLocalCbIfLive(cnxt, reg, pCode->insnsSize, localInReg, localCb);
    }
}
