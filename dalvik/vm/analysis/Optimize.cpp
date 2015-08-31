/*
 * Copyright (C) 2008 The Android Open Source Project
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
 * Perform some simple bytecode optimizations, chiefly "quickening" of
 * opcodes.
 */
#include "Dalvik.h"
#include "libdex/InstrUtils.h"
#include "Optimize.h"

#include <zlib.h>

#include <stdlib.h>

/*
 * Virtual/direct calls to "method" are replaced with an execute-inline
 * instruction with index "idx".
 */
struct InlineSub {
    Method* method;
    int     inlineIdx;
};


/* fwd */
static void optimizeMethod(Method* method, bool essentialOnly);
static void rewriteInstField(Method* method, u2* insns, Opcode quickOpc,
    Opcode volatileOpc);
static void rewriteStaticField(Method* method, u2* insns, Opcode volatileOpc);
static void rewriteVirtualInvoke(Method* method, u2* insns, Opcode newOpc);
static bool rewriteInvokeObjectInit(Method* method, u2* insns);
static bool rewriteExecuteInline(Method* method, u2* insns,
    MethodType methodType);
static bool rewriteExecuteInlineRange(Method* method, u2* insns,
    MethodType methodType);
static void rewriteReturnVoid(Method* method, u2* insns);
static bool needsReturnBarrier(Method* method);


/*
 * Create a table of inline substitutions.  Sets gDvm.inlineSubs.
 *
 * TODO: this is currently just a linear array.  We will want to put this
 * into a hash table as the list size increases.
 */
bool dvmCreateInlineSubsTable()
{
    const InlineOperation* ops = dvmGetInlineOpsTable();
    const int count = dvmGetInlineOpsTableLength();
    InlineSub* table;
    int i, tableIndex;

    assert(gDvm.inlineSubs == NULL);

    /*
     * One slot per entry, plus an end-of-list marker.
     */
    table = (InlineSub*) calloc(count + 1, sizeof(InlineSub));

    tableIndex = 0;
    for (i = 0; i < count; i++) {
        Method* method = dvmFindInlinableMethod(ops[i].classDescriptor,
            ops[i].methodName, ops[i].methodSignature);
        if (method == NULL) {
            /*
             * Not expected.  We only use this for key methods in core
             * classes, so we should always be able to find them.
             */
            ALOGE("Unable to find method for inlining: %s.%s:%s",
                ops[i].classDescriptor, ops[i].methodName,
                ops[i].methodSignature);
            free(table);
            return false;
        }

        table[tableIndex].method = method;
        table[tableIndex].inlineIdx = i;
        tableIndex++;
    }

    /* mark end of table */
    table[tableIndex].method = NULL;

    gDvm.inlineSubs = table;
    return true;
}

/*
 * Release inline sub data structure.
 */
void dvmFreeInlineSubsTable()
{
    free(gDvm.inlineSubs);
    gDvm.inlineSubs = NULL;
}


/*
 * Optimize the specified class.
 *
 * If "essentialOnly" is true, we only do essential optimizations.  For
 * example, accesses to volatile 64-bit fields must be replaced with
 * "-wide-volatile" instructions or the program could behave incorrectly.
 * (Skipping non-essential optimizations makes us a little bit faster, and
 * more importantly avoids dirtying DEX pages.)
 */
void dvmOptimizeClass(ClassObject* clazz, bool essentialOnly)
{
    int i;

    for (i = 0; i < clazz->directMethodCount; i++) {
        optimizeMethod(&clazz->directMethods[i], essentialOnly);
    }
    for (i = 0; i < clazz->virtualMethodCount; i++) {
        optimizeMethod(&clazz->virtualMethods[i], essentialOnly);
    }
}

/*
 * Optimize instructions in a method.
 *
 * This does a single pass through the code, examining each instruction.
 *
 * This is not expected to fail if the class was successfully verified.
 * The only significant failure modes on unverified code occur when an
 * "essential" update fails, but we can't generally identify those: if we
 * can't look up a field, we can't know if the field access was supposed
 * to be handled as volatile.
 *
 * Instead, we give it our best effort, and hope for the best.  For 100%
 * reliability, only optimize a class after verification succeeds.
 */
static void optimizeMethod(Method* method, bool essentialOnly)
{
    bool needRetBar, forSmp;
    u4 insnsSize;
    u2* insns;

    if (dvmIsNativeMethod(method) || dvmIsAbstractMethod(method))
        return;

    forSmp = gDvm.dexOptForSmp;
    needRetBar = needsReturnBarrier(method);

    insns = (u2*) method->insns;
    assert(insns != NULL);
    insnsSize = dvmGetMethodInsnsSize(method);

    while (insnsSize > 0) {
        Opcode opc, quickOpc, volatileOpc;
        size_t width;
        bool matched = true;

        opc = dexOpcodeFromCodeUnit(*insns);
        width = dexGetWidthFromInstruction(insns);
        volatileOpc = OP_NOP;

        /*
         * Each instruction may have:
         * - "volatile" replacement
         *   - may be essential or essential-on-SMP
         * - correctness replacement
         *   - may be essential or essential-on-SMP
         * - performance replacement
         *   - always non-essential
         *
         * Replacements are considered in the order shown, and the first
         * match is applied.  For example, iget-wide will convert to
         * iget-wide-volatile rather than iget-wide-quick if the target
         * field is volatile.
         */

        /*
         * essential substitutions:
         *  {iget,iput,sget,sput}-wide --> {op}-wide-volatile
         *  invoke-direct[/range] --> invoke-object-init/range
         *
         * essential-on-SMP substitutions:
         *  {iget,iput,sget,sput}-* --> {op}-volatile
         *  return-void --> return-void-barrier
         *
         * non-essential substitutions:
         *  {iget,iput}-* --> {op}-quick
         *
         * TODO: might be time to merge this with the other two switches
         */
        switch (opc) {
        case OP_IGET:
        case OP_IGET_BOOLEAN:
        case OP_IGET_BYTE:
        case OP_IGET_CHAR:
        case OP_IGET_SHORT:
            quickOpc = OP_IGET_QUICK;
            if (forSmp)
                volatileOpc = OP_IGET_VOLATILE;
            goto rewrite_inst_field;
        case OP_IGET_WIDE:
            quickOpc = OP_IGET_WIDE_QUICK;
            volatileOpc = OP_IGET_WIDE_VOLATILE;
            goto rewrite_inst_field;
        case OP_IGET_OBJECT:
            quickOpc = OP_IGET_OBJECT_QUICK;
            if (forSmp)
                volatileOpc = OP_IGET_OBJECT_VOLATILE;
            goto rewrite_inst_field;
        case OP_IPUT:
        case OP_IPUT_BOOLEAN:
        case OP_IPUT_BYTE:
        case OP_IPUT_CHAR:
        case OP_IPUT_SHORT:
            quickOpc = OP_IPUT_QUICK;
            if (forSmp)
                volatileOpc = OP_IPUT_VOLATILE;
            goto rewrite_inst_field;
        case OP_IPUT_WIDE:
            quickOpc = OP_IPUT_WIDE_QUICK;
            volatileOpc = OP_IPUT_WIDE_VOLATILE;
            goto rewrite_inst_field;
        case OP_IPUT_OBJECT:
            quickOpc = OP_IPUT_OBJECT_QUICK;
            if (forSmp)
                volatileOpc = OP_IPUT_OBJECT_VOLATILE;
            /* fall through */
rewrite_inst_field:
            if (essentialOnly)
                quickOpc = OP_NOP;      /* if essential-only, no "-quick" sub */
            if (quickOpc != OP_NOP || volatileOpc != OP_NOP)
                rewriteInstField(method, insns, quickOpc, volatileOpc);
            break;

        case OP_SGET:
        case OP_SGET_BOOLEAN:
        case OP_SGET_BYTE:
        case OP_SGET_CHAR:
        case OP_SGET_SHORT:
            if (forSmp)
                volatileOpc = OP_SGET_VOLATILE;
            goto rewrite_static_field;
        case OP_SGET_WIDE:
            volatileOpc = OP_SGET_WIDE_VOLATILE;
            goto rewrite_static_field;
        case OP_SGET_OBJECT:
            if (forSmp)
                volatileOpc = OP_SGET_OBJECT_VOLATILE;
            goto rewrite_static_field;
        case OP_SPUT:
        case OP_SPUT_BOOLEAN:
        case OP_SPUT_BYTE:
        case OP_SPUT_CHAR:
        case OP_SPUT_SHORT:
            if (forSmp)
                volatileOpc = OP_SPUT_VOLATILE;
            goto rewrite_static_field;
        case OP_SPUT_WIDE:
            volatileOpc = OP_SPUT_WIDE_VOLATILE;
            goto rewrite_static_field;
        case OP_SPUT_OBJECT:
            if (forSmp)
                volatileOpc = OP_SPUT_OBJECT_VOLATILE;
            /* fall through */
rewrite_static_field:
            if (volatileOpc != OP_NOP)
                rewriteStaticField(method, insns, volatileOpc);
            break;

        case OP_INVOKE_DIRECT:
        case OP_INVOKE_DIRECT_RANGE:
            if (!rewriteInvokeObjectInit(method, insns)) {
                /* may want to try execute-inline, below */
                matched = false;
            }
            break;
        case OP_RETURN_VOID:
            if (needRetBar)
                rewriteReturnVoid(method, insns);
            break;
        default:
            matched = false;
            break;
        }


        /*
         * non-essential substitutions:
         *  invoke-{virtual,direct,static}[/range] --> execute-inline
         *  invoke-{virtual,super}[/range] --> invoke-*-quick
         */
        if (!matched && !essentialOnly) {
            switch (opc) {
            case OP_INVOKE_VIRTUAL:
                if (!rewriteExecuteInline(method, insns, METHOD_VIRTUAL)) {
                    rewriteVirtualInvoke(method, insns,
                        OP_INVOKE_VIRTUAL_QUICK);
                }
                break;
            case OP_INVOKE_VIRTUAL_RANGE:
                if (!rewriteExecuteInlineRange(method, insns, METHOD_VIRTUAL)) {
                    rewriteVirtualInvoke(method, insns,
                        OP_INVOKE_VIRTUAL_QUICK_RANGE);
                }
                break;
            case OP_INVOKE_SUPER:
                rewriteVirtualInvoke(method, insns, OP_INVOKE_SUPER_QUICK);
                break;
            case OP_INVOKE_SUPER_RANGE:
                rewriteVirtualInvoke(method, insns, OP_INVOKE_SUPER_QUICK_RANGE);
                break;
            case OP_INVOKE_DIRECT:
                rewriteExecuteInline(method, insns, METHOD_DIRECT);
                break;
            case OP_INVOKE_DIRECT_RANGE:
                rewriteExecuteInlineRange(method, insns, METHOD_DIRECT);
                break;
            case OP_INVOKE_STATIC:
                rewriteExecuteInline(method, insns, METHOD_STATIC);
                break;
            case OP_INVOKE_STATIC_RANGE:
                rewriteExecuteInlineRange(method, insns, METHOD_STATIC);
                break;
            default:
                /* nothing to do for this instruction */
                ;
            }
        }

        assert(width > 0);
        assert(width <= insnsSize);
        assert(width == dexGetWidthFromInstruction(insns));

        insns += width;
        insnsSize -= width;
    }

    assert(insnsSize == 0);
}

/*
 * Update a 16-bit code unit in "meth".  The way in which the DEX data was
 * loaded determines how we go about the write.
 *
 * This will be operating on post-byte-swap DEX data, so values will
 * be in host order.
 */
void dvmUpdateCodeUnit(const Method* meth, u2* ptr, u2 newVal)
{
    DvmDex* pDvmDex = meth->clazz->pDvmDex;

    if (!pDvmDex->isMappedReadOnly) {
        /* in-memory DEX (dexopt or byte[]), alter the output directly */
        *ptr = newVal;
    } else {
        /* memory-mapped file, toggle the page read/write status */
        dvmDexChangeDex2(pDvmDex, ptr, newVal);
    }
}

/*
 * Update an instruction's opcode.
 *
 * If "opcode" is an 8-bit op, we just replace that portion.  If it's a
 * 16-bit op, we convert the opcode from "packed" form (e.g. 0x0108) to
 * bytecode form (e.g. 0x08ff).
 */
static inline void updateOpcode(const Method* meth, u2* ptr, u2 opcode)
{
    if (opcode >= 256) {
        /* opcode low byte becomes high byte, low byte becomes 0xff */
        assert((ptr[0] & 0xff) == 0xff);
        dvmUpdateCodeUnit(meth, ptr, (opcode << 8) | 0x00ff);
    } else {
        /* 8-bit op, just replace the low byte */
        assert((ptr[0] & 0xff) != 0xff);
        dvmUpdateCodeUnit(meth, ptr, (ptr[0] & 0xff00) | opcode);
    }
}

/*
 * If "referrer" and "resClass" don't come from the same DEX file, and
 * the DEX we're working on is not destined for the bootstrap class path,
 * tweak the class loader so package-access checks work correctly.
 *
 * Only do this if we're doing pre-verification or optimization.
 */
static void tweakLoader(ClassObject* referrer, ClassObject* resClass)
{
    if (!gDvm.optimizing)
        return;
    assert(referrer->classLoader == NULL);
    assert(resClass->classLoader == NULL);

    if (!gDvm.optimizingBootstrapClass) {
        /* class loader for an array class comes from element type */
        if (dvmIsArrayClass(resClass))
            resClass = resClass->elementClass;
        if (referrer->pDvmDex != resClass->pDvmDex)
            resClass->classLoader = (Object*) 0xdead3333;
    }
}

/*
 * Undo the effects of tweakLoader.
 */
static void untweakLoader(ClassObject* referrer, ClassObject* resClass)
{
    if (!gDvm.optimizing || gDvm.optimizingBootstrapClass)
        return;

    if (dvmIsArrayClass(resClass))
        resClass = resClass->elementClass;
    resClass->classLoader = NULL;
}


/*
 * Alternate version of dvmResolveClass for use with verification and
 * optimization.  Performs access checks on every resolve, and refuses
 * to acknowledge the existence of classes defined in more than one DEX
 * file.
 *
 * Exceptions caused by failures are cleared before returning.
 *
 * On failure, returns NULL, and sets *pFailure if pFailure is not NULL.
 */
ClassObject* dvmOptResolveClass(ClassObject* referrer, u4 classIdx,
    VerifyError* pFailure)
{
    DvmDex* pDvmDex = referrer->pDvmDex;
    ClassObject* resClass;

    /*
     * Check the table first.  If not there, do the lookup by name.
     */
    resClass = dvmDexGetResolvedClass(pDvmDex, classIdx);
    if (resClass == NULL) {
        const char* className = dexStringByTypeIdx(pDvmDex->pDexFile, classIdx);
        if (className[0] != '\0' && className[1] == '\0') {
            /* primitive type */
            resClass = dvmFindPrimitiveClass(className[0]);
        } else {
            resClass = dvmFindClassNoInit(className, referrer->classLoader);
        }
        if (resClass == NULL) {
            /* not found, exception should be raised */
            ALOGV("DexOpt: class %d (%s) not found",
                classIdx,
                dexStringByTypeIdx(pDvmDex->pDexFile, classIdx));
            if (pFailure != NULL) {
                /* dig through the wrappers to find the original failure */
                Object* excep = dvmGetException(dvmThreadSelf());
                while (true) {
                    Object* cause = dvmGetExceptionCause(excep);
                    if (cause == NULL)
                        break;
                    excep = cause;
                }
                if (strcmp(excep->clazz->descriptor,
                    "Ljava/lang/IncompatibleClassChangeError;") == 0)
                {
                    *pFailure = VERIFY_ERROR_CLASS_CHANGE;
                } else {
                    *pFailure = VERIFY_ERROR_NO_CLASS;
                }
            }
            dvmClearOptException(dvmThreadSelf());
            return NULL;
        }

        /*
         * Add it to the resolved table so we're faster on the next lookup.
         */
        dvmDexSetResolvedClass(pDvmDex, classIdx, resClass);
    }

    /* multiple definitions? */
    if (IS_CLASS_FLAG_SET(resClass, CLASS_MULTIPLE_DEFS)) {
        ALOGI("DexOpt: not resolving ambiguous class '%s'",
            resClass->descriptor);
        if (pFailure != NULL)
            *pFailure = VERIFY_ERROR_NO_CLASS;
        return NULL;
    }

    /* access allowed? */
    tweakLoader(referrer, resClass);
    bool allowed = dvmCheckClassAccess(referrer, resClass);
    untweakLoader(referrer, resClass);
    if (!allowed) {
        ALOGW("DexOpt: resolve class illegal access: %s -> %s",
            referrer->descriptor, resClass->descriptor);
        if (pFailure != NULL)
            *pFailure = VERIFY_ERROR_ACCESS_CLASS;
        return NULL;
    }

    return resClass;
}

/*
 * Alternate version of dvmResolveInstField().
 *
 * On failure, returns NULL, and sets *pFailure if pFailure is not NULL.
 */
InstField* dvmOptResolveInstField(ClassObject* referrer, u4 ifieldIdx,
    VerifyError* pFailure)
{
    DvmDex* pDvmDex = referrer->pDvmDex;
    InstField* resField;

    resField = (InstField*) dvmDexGetResolvedField(pDvmDex, ifieldIdx);
    if (resField == NULL) {
        const DexFieldId* pFieldId;
        ClassObject* resClass;

        pFieldId = dexGetFieldId(pDvmDex->pDexFile, ifieldIdx);

        /*
         * Find the field's class.
         */
        resClass = dvmOptResolveClass(referrer, pFieldId->classIdx, pFailure);
        if (resClass == NULL) {
            //dvmClearOptException(dvmThreadSelf());
            assert(!dvmCheckException(dvmThreadSelf()));
            if (pFailure != NULL) { assert(!VERIFY_OK(*pFailure)); }
            return NULL;
        }

        resField = (InstField*)dvmFindFieldHier(resClass,
            dexStringById(pDvmDex->pDexFile, pFieldId->nameIdx),
            dexStringByTypeIdx(pDvmDex->pDexFile, pFieldId->typeIdx));
        if (resField == NULL) {
            ALOGD("DexOpt: couldn't find field %s.%s",
                resClass->descriptor,
                dexStringById(pDvmDex->pDexFile, pFieldId->nameIdx));
            if (pFailure != NULL)
                *pFailure = VERIFY_ERROR_NO_FIELD;
            return NULL;
        }
        if (dvmIsStaticField(resField)) {
            ALOGD("DexOpt: wanted instance, got static for field %s.%s",
                resClass->descriptor,
                dexStringById(pDvmDex->pDexFile, pFieldId->nameIdx));
            if (pFailure != NULL)
                *pFailure = VERIFY_ERROR_CLASS_CHANGE;
            return NULL;
        }

        /*
         * Add it to the resolved table so we're faster on the next lookup.
         */
        dvmDexSetResolvedField(pDvmDex, ifieldIdx, (Field*) resField);
    }

    /* access allowed? */
    tweakLoader(referrer, resField->clazz);
    bool allowed = dvmCheckFieldAccess(referrer, (Field*)resField);
    untweakLoader(referrer, resField->clazz);
    if (!allowed) {
        ALOGI("DexOpt: access denied from %s to field %s.%s",
            referrer->descriptor, resField->clazz->descriptor,
            resField->name);
        if (pFailure != NULL)
            *pFailure = VERIFY_ERROR_ACCESS_FIELD;
        return NULL;
    }

    return resField;
}

/*
 * Alternate version of dvmResolveStaticField().
 *
 * Does not force initialization of the resolved field's class.
 *
 * On failure, returns NULL, and sets *pFailure if pFailure is not NULL.
 */
StaticField* dvmOptResolveStaticField(ClassObject* referrer, u4 sfieldIdx,
    VerifyError* pFailure)
{
    DvmDex* pDvmDex = referrer->pDvmDex;
    StaticField* resField;

    resField = (StaticField*)dvmDexGetResolvedField(pDvmDex, sfieldIdx);
    if (resField == NULL) {
        const DexFieldId* pFieldId;
        ClassObject* resClass;

        pFieldId = dexGetFieldId(pDvmDex->pDexFile, sfieldIdx);

        /*
         * Find the field's class.
         */
        resClass = dvmOptResolveClass(referrer, pFieldId->classIdx, pFailure);
        if (resClass == NULL) {
            //dvmClearOptException(dvmThreadSelf());
            assert(!dvmCheckException(dvmThreadSelf()));
            if (pFailure != NULL) { assert(!VERIFY_OK(*pFailure)); }
            return NULL;
        }

        const char* fieldName =
            dexStringById(pDvmDex->pDexFile, pFieldId->nameIdx);

        resField = (StaticField*)dvmFindFieldHier(resClass, fieldName,
                    dexStringByTypeIdx(pDvmDex->pDexFile, pFieldId->typeIdx));
        if (resField == NULL) {
            ALOGD("DexOpt: couldn't find static field %s.%s",
                resClass->descriptor, fieldName);
            if (pFailure != NULL)
                *pFailure = VERIFY_ERROR_NO_FIELD;
            return NULL;
        }
        if (!dvmIsStaticField(resField)) {
            ALOGD("DexOpt: wanted static, got instance for field %s.%s",
                resClass->descriptor, fieldName);
            if (pFailure != NULL)
                *pFailure = VERIFY_ERROR_CLASS_CHANGE;
            return NULL;
        }

        /*
         * Add it to the resolved table so we're faster on the next lookup.
         *
         * We can only do this if we're in "dexopt", because the presence
         * of a valid value in the resolution table implies that the class
         * containing the static field has been initialized.
         */
        if (gDvm.optimizing)
            dvmDexSetResolvedField(pDvmDex, sfieldIdx, (Field*) resField);
    }

    /* access allowed? */
    tweakLoader(referrer, resField->clazz);
    bool allowed = dvmCheckFieldAccess(referrer, (Field*)resField);
    untweakLoader(referrer, resField->clazz);
    if (!allowed) {
        ALOGI("DexOpt: access denied from %s to field %s.%s",
            referrer->descriptor, resField->clazz->descriptor,
            resField->name);
        if (pFailure != NULL)
            *pFailure = VERIFY_ERROR_ACCESS_FIELD;
        return NULL;
    }

    return resField;
}


/*
 * Rewrite an iget/iput instruction if appropriate.  These all have the form:
 *   op vA, vB, field@CCCC
 *
 * Where vA holds the value, vB holds the object reference, and CCCC is
 * the field reference constant pool offset.  For a non-volatile field,
 * we want to replace the opcode with "quickOpc" and replace CCCC with
 * the byte offset from the start of the object.  For a volatile field,
 * we just want to replace the opcode with "volatileOpc".
 *
 * If "volatileOpc" is OP_NOP we don't check to see if it's a volatile
 * field.  If "quickOpc" is OP_NOP, and this is a non-volatile field,
 * we don't do anything.
 *
 * "method" is the referring method.
 */
static void rewriteInstField(Method* method, u2* insns, Opcode quickOpc,
    Opcode volatileOpc)
{
    ClassObject* clazz = method->clazz;
    u2 fieldIdx = insns[1];
    InstField* instField;

    instField = dvmOptResolveInstField(clazz, fieldIdx, NULL);
    if (instField == NULL) {
        ALOGI("DexOpt: unable to optimize instance field ref "
             "0x%04x at 0x%02x in %s.%s",
            fieldIdx, (int) (insns - method->insns), clazz->descriptor,
            method->name);
        return;
    }

    if (volatileOpc != OP_NOP && dvmIsVolatileField(instField)) {
        updateOpcode(method, insns, volatileOpc);
        ALOGV("DexOpt: rewrote ifield access %s.%s --> volatile",
            instField->clazz->descriptor, instField->name);
    } else if (quickOpc != OP_NOP && instField->byteOffset < 65536) {
        updateOpcode(method, insns, quickOpc);
        dvmUpdateCodeUnit(method, insns+1, (u2) instField->byteOffset);
        ALOGV("DexOpt: rewrote ifield access %s.%s --> %d",
            instField->clazz->descriptor, instField->name,
            instField->byteOffset);
    } else {
        ALOGV("DexOpt: no rewrite of ifield access %s.%s",
            instField->clazz->descriptor, instField->name);
    }

    return;
}

/*
 * Rewrite a static field access instruction if appropriate.  If
 * the target field is volatile, we replace the opcode with "volatileOpc".
 *
 * "method" is the referring method.
 */
static void rewriteStaticField0(Method* method, u2* insns, Opcode volatileOpc,
    u4 fieldIdx)
{
    ClassObject* clazz = method->clazz;
    StaticField* staticField;

    assert(volatileOpc != OP_NOP);

    staticField = dvmOptResolveStaticField(clazz, fieldIdx, NULL);
    if (staticField == NULL) {
        ALOGI("DexOpt: unable to optimize static field ref "
             "0x%04x at 0x%02x in %s.%s",
            fieldIdx, (int) (insns - method->insns), clazz->descriptor,
            method->name);
        return;
    }

    if (dvmIsVolatileField(staticField)) {
        updateOpcode(method, insns, volatileOpc);
        ALOGV("DexOpt: rewrote sfield access %s.%s --> volatile",
            staticField->clazz->descriptor, staticField->name);
    }
}

static void rewriteStaticField(Method* method, u2* insns, Opcode volatileOpc)
{
    u2 fieldIdx = insns[1];
    rewriteStaticField0(method, insns, volatileOpc, fieldIdx);
}

/*
 * Alternate version of dvmResolveMethod().
 *
 * Doesn't throw exceptions, and checks access on every lookup.
 *
 * On failure, returns NULL, and sets *pFailure if pFailure is not NULL.
 */
Method* dvmOptResolveMethod(ClassObject* referrer, u4 methodIdx,
    MethodType methodType, VerifyError* pFailure)
{
    DvmDex* pDvmDex = referrer->pDvmDex;
    Method* resMethod;

    assert(methodType == METHOD_DIRECT ||
           methodType == METHOD_VIRTUAL ||
           methodType == METHOD_STATIC);

    LOGVV("--- resolving method %u (referrer=%s)", methodIdx,
        referrer->descriptor);

    resMethod = dvmDexGetResolvedMethod(pDvmDex, methodIdx);
    if (resMethod == NULL) {
        const DexMethodId* pMethodId;
        ClassObject* resClass;

        pMethodId = dexGetMethodId(pDvmDex->pDexFile, methodIdx);

        resClass = dvmOptResolveClass(referrer, pMethodId->classIdx, pFailure);
        if (resClass == NULL) {
            /*
             * Can't find the class that the method is a part of, or don't
             * have permission to access the class.
             */
            ALOGV("DexOpt: can't find called method's class (?.%s)",
                dexStringById(pDvmDex->pDexFile, pMethodId->nameIdx));
            if (pFailure != NULL) { assert(!VERIFY_OK(*pFailure)); }
            return NULL;
        }
        if (dvmIsInterfaceClass(resClass)) {
            /* method is part of an interface; this is wrong method for that */
            ALOGW("DexOpt: method is in an interface");
            if (pFailure != NULL)
                *pFailure = VERIFY_ERROR_GENERIC;
            return NULL;
        }

        /*
         * We need to chase up the class hierarchy to find methods defined
         * in super-classes.  (We only want to check the current class
         * if we're looking for a constructor.)
         */
        DexProto proto;
        dexProtoSetFromMethodId(&proto, pDvmDex->pDexFile, pMethodId);

        if (methodType == METHOD_DIRECT) {
            resMethod = dvmFindDirectMethod(resClass,
                dexStringById(pDvmDex->pDexFile, pMethodId->nameIdx), &proto);
        } else {
            /* METHOD_STATIC or METHOD_VIRTUAL */
            resMethod = dvmFindMethodHier(resClass,
                dexStringById(pDvmDex->pDexFile, pMethodId->nameIdx), &proto);
        }

        if (resMethod == NULL) {
            ALOGV("DexOpt: couldn't find method '%s'",
                dexStringById(pDvmDex->pDexFile, pMethodId->nameIdx));
            if (pFailure != NULL)
                *pFailure = VERIFY_ERROR_NO_METHOD;
            return NULL;
        }
        if (methodType == METHOD_STATIC) {
            if (!dvmIsStaticMethod(resMethod)) {
                ALOGD("DexOpt: wanted static, got instance for method %s.%s",
                    resClass->descriptor, resMethod->name);
                if (pFailure != NULL)
                    *pFailure = VERIFY_ERROR_CLASS_CHANGE;
                return NULL;
            }
        } else if (methodType == METHOD_VIRTUAL) {
            if (dvmIsStaticMethod(resMethod)) {
                ALOGD("DexOpt: wanted instance, got static for method %s.%s",
                    resClass->descriptor, resMethod->name);
                if (pFailure != NULL)
                    *pFailure = VERIFY_ERROR_CLASS_CHANGE;
                return NULL;
            }
        }

        /* see if this is a pure-abstract method */
        if (dvmIsAbstractMethod(resMethod) && !dvmIsAbstractClass(resClass)) {
            ALOGW("DexOpt: pure-abstract method '%s' in %s",
                dexStringById(pDvmDex->pDexFile, pMethodId->nameIdx),
                resClass->descriptor);
            if (pFailure != NULL)
                *pFailure = VERIFY_ERROR_GENERIC;
            return NULL;
        }

        /*
         * Add it to the resolved table so we're faster on the next lookup.
         *
         * We can only do this for static methods if we're not in "dexopt",
         * because the presence of a valid value in the resolution table
         * implies that the class containing the static field has been
         * initialized.
         */
        if (methodType != METHOD_STATIC || gDvm.optimizing)
            dvmDexSetResolvedMethod(pDvmDex, methodIdx, resMethod);
    }

    LOGVV("--- found method %d (%s.%s)",
        methodIdx, resMethod->clazz->descriptor, resMethod->name);

    /* access allowed? */
    tweakLoader(referrer, resMethod->clazz);
    bool allowed = dvmCheckMethodAccess(referrer, resMethod);
    untweakLoader(referrer, resMethod->clazz);
    if (!allowed) {
        IF_ALOGI() {
            char* desc = dexProtoCopyMethodDescriptor(&resMethod->prototype);
            ALOGI("DexOpt: illegal method access (call %s.%s %s from %s)",
                resMethod->clazz->descriptor, resMethod->name, desc,
                referrer->descriptor);
            free(desc);
        }
        if (pFailure != NULL)
            *pFailure = VERIFY_ERROR_ACCESS_METHOD;
        return NULL;
    }

    return resMethod;
}

/*
 * Rewrite invoke-virtual, invoke-virtual/range, invoke-super, and
 * invoke-super/range if appropriate.  These all have the form:
 *   op vAA, meth@BBBB, reg stuff @CCCC
 *
 * We want to replace the method constant pool index BBBB with the
 * vtable index.
 */
static void rewriteVirtualInvoke(Method* method, u2* insns, Opcode newOpc)
{
    ClassObject* clazz = method->clazz;
    Method* baseMethod;
    u2 methodIdx = insns[1];

    baseMethod = dvmOptResolveMethod(clazz, methodIdx, METHOD_VIRTUAL, NULL);
    if (baseMethod == NULL) {
        ALOGD("DexOpt: unable to optimize virt call 0x%04x at 0x%02x in %s.%s",
            methodIdx,
            (int) (insns - method->insns), clazz->descriptor,
            method->name);
        return;
    }

    assert((insns[0] & 0xff) == OP_INVOKE_VIRTUAL ||
           (insns[0] & 0xff) == OP_INVOKE_VIRTUAL_RANGE ||
           (insns[0] & 0xff) == OP_INVOKE_SUPER ||
           (insns[0] & 0xff) == OP_INVOKE_SUPER_RANGE);

    /*
     * Note: Method->methodIndex is a u2 and is range checked during the
     * initial load.
     */
    updateOpcode(method, insns, newOpc);
    dvmUpdateCodeUnit(method, insns+1, baseMethod->methodIndex);

    //ALOGI("DexOpt: rewrote call to %s.%s --> %s.%s",
    //    method->clazz->descriptor, method->name,
    //    baseMethod->clazz->descriptor, baseMethod->name);

    return;
}

/*
 * Rewrite invoke-direct[/range] if the target is Object.<init>.
 *
 * This is useful as an optimization, because otherwise every object
 * instantiation will cause us to call a method that does nothing.
 * It also allows us to inexpensively mark objects as finalizable at the
 * correct time.
 *
 * TODO: verifier should ensure Object.<init> contains only return-void,
 * and issue a warning if not.
 */
static bool rewriteInvokeObjectInit(Method* method, u2* insns)
{
    ClassObject* clazz = method->clazz;
    Method* calledMethod;
    u2 methodIdx = insns[1];

    calledMethod = dvmOptResolveMethod(clazz, methodIdx, METHOD_DIRECT, NULL);
    if (calledMethod == NULL) {
        ALOGD("DexOpt: unable to opt direct call 0x%04x at 0x%02x in %s.%s",
            methodIdx, (int) (insns - method->insns),
            clazz->descriptor, method->name);
        return false;
    }

    if (calledMethod->clazz == gDvm.classJavaLangObject &&
        dvmCompareNameDescriptorAndMethod("<init>", "()V", calledMethod) == 0)
    {
        /*
         * Replace the instruction.  If the debugger is attached, the
         * interpreter will forward execution to the invoke-direct/range
         * handler.  If this was an invoke-direct/range instruction we can
         * just replace the opcode, but if it was an invoke-direct we
         * have to set the argument count (high 8 bits of first code unit)
         * to 1.
         */
        u1 origOp = insns[0] & 0xff;
        if (origOp == OP_INVOKE_DIRECT) {
            dvmUpdateCodeUnit(method, insns,
                OP_INVOKE_OBJECT_INIT_RANGE | 0x100);
        } else {
            assert(origOp == OP_INVOKE_DIRECT_RANGE);
            assert((insns[0] >> 8) == 1);
            updateOpcode(method, insns, OP_INVOKE_OBJECT_INIT_RANGE);
        }

        LOGVV("DexOpt: replaced Object.<init> in %s.%s",
            method->clazz->descriptor, method->name);
    }

    return true;
}

/*
 * Resolve an interface method reference.
 *
 * No method access check here -- interface methods are always public.
 *
 * Returns NULL if the method was not found.  Does not throw an exception.
 */
Method* dvmOptResolveInterfaceMethod(ClassObject* referrer, u4 methodIdx)
{
    DvmDex* pDvmDex = referrer->pDvmDex;
    Method* resMethod;

    LOGVV("--- resolving interface method %d (referrer=%s)",
        methodIdx, referrer->descriptor);

    resMethod = dvmDexGetResolvedMethod(pDvmDex, methodIdx);
    if (resMethod == NULL) {
        const DexMethodId* pMethodId;
        ClassObject* resClass;

        pMethodId = dexGetMethodId(pDvmDex->pDexFile, methodIdx);

        resClass = dvmOptResolveClass(referrer, pMethodId->classIdx, NULL);
        if (resClass == NULL) {
            /* can't find the class that the method is a part of */
            dvmClearOptException(dvmThreadSelf());
            return NULL;
        }
        if (!dvmIsInterfaceClass(resClass)) {
            /* whoops */
            ALOGI("Interface method not part of interface class");
            return NULL;
        }

        const char* methodName =
            dexStringById(pDvmDex->pDexFile, pMethodId->nameIdx);
        DexProto proto;
        dexProtoSetFromMethodId(&proto, pDvmDex->pDexFile, pMethodId);

        LOGVV("+++ looking for '%s' '%s' in resClass='%s'",
            methodName, methodSig, resClass->descriptor);
        resMethod = dvmFindInterfaceMethodHier(resClass, methodName, &proto);
        if (resMethod == NULL) {
            return NULL;
        }

        /* we're expecting this to be abstract */
        if (!dvmIsAbstractMethod(resMethod)) {
            char* desc = dexProtoCopyMethodDescriptor(&resMethod->prototype);
            ALOGW("Found non-abstract interface method %s.%s %s",
                resMethod->clazz->descriptor, resMethod->name, desc);
            free(desc);
            return NULL;
        }

        /*
         * Add it to the resolved table so we're faster on the next lookup.
         */
        dvmDexSetResolvedMethod(pDvmDex, methodIdx, resMethod);
    }

    LOGVV("--- found interface method %d (%s.%s)",
        methodIdx, resMethod->clazz->descriptor, resMethod->name);

    /* interface methods are always public; no need to check access */

    return resMethod;
}

/*
 * Replace invoke-virtual, invoke-direct, or invoke-static with an
 * execute-inline operation if appropriate.
 *
 * Returns "true" if we replace it.
 */
static bool rewriteExecuteInline(Method* method, u2* insns,
    MethodType methodType)
{
    const InlineSub* inlineSubs = gDvm.inlineSubs;
    ClassObject* clazz = method->clazz;
    Method* calledMethod;
    u2 methodIdx = insns[1];

    //return false;

    calledMethod = dvmOptResolveMethod(clazz, methodIdx, methodType, NULL);
    if (calledMethod == NULL) {
        ALOGV("+++ DexOpt inline: can't find %d", methodIdx);
        return false;
    }

    while (inlineSubs->method != NULL) {
        /*
        if (extra) {
            ALOGI("comparing %p vs %p %s.%s %s",
                inlineSubs->method, calledMethod,
                inlineSubs->method->clazz->descriptor,
                inlineSubs->method->name,
                inlineSubs->method->signature);
        }
        */
        if (inlineSubs->method == calledMethod) {
            assert((insns[0] & 0xff) == OP_INVOKE_DIRECT ||
                   (insns[0] & 0xff) == OP_INVOKE_STATIC ||
                   (insns[0] & 0xff) == OP_INVOKE_VIRTUAL);
            updateOpcode(method, insns, OP_EXECUTE_INLINE);
            dvmUpdateCodeUnit(method, insns+1, (u2) inlineSubs->inlineIdx);

            //ALOGI("DexOpt: execute-inline %s.%s --> %s.%s",
            //    method->clazz->descriptor, method->name,
            //    calledMethod->clazz->descriptor, calledMethod->name);
            return true;
        }

        inlineSubs++;
    }

    return false;
}

/*
 * Replace invoke-virtual/range, invoke-direct/range, or invoke-static/range
 * with an execute-inline operation if appropriate.
 *
 * Returns "true" if we replace it.
 */
static bool rewriteExecuteInlineRange(Method* method, u2* insns,
    MethodType methodType)
{
    const InlineSub* inlineSubs = gDvm.inlineSubs;
    ClassObject* clazz = method->clazz;
    Method* calledMethod;
    u2 methodIdx = insns[1];

    calledMethod = dvmOptResolveMethod(clazz, methodIdx, methodType, NULL);
    if (calledMethod == NULL) {
        ALOGV("+++ DexOpt inline/range: can't find %d", methodIdx);
        return false;
    }

    while (inlineSubs->method != NULL) {
        if (inlineSubs->method == calledMethod) {
            assert((insns[0] & 0xff) == OP_INVOKE_DIRECT_RANGE ||
                   (insns[0] & 0xff) == OP_INVOKE_STATIC_RANGE ||
                   (insns[0] & 0xff) == OP_INVOKE_VIRTUAL_RANGE);
            updateOpcode(method, insns, OP_EXECUTE_INLINE_RANGE);
            dvmUpdateCodeUnit(method, insns+1, (u2) inlineSubs->inlineIdx);

            //ALOGI("DexOpt: execute-inline/range %s.%s --> %s.%s",
            //    method->clazz->descriptor, method->name,
            //    calledMethod->clazz->descriptor, calledMethod->name);
            return true;
        }

        inlineSubs++;
    }

    return false;
}

/*
 * Returns "true" if the return-void instructions in this method should
 * be converted to return-void-barrier.
 *
 * This is needed to satisfy a Java Memory Model requirement regarding
 * the construction of objects with final fields.  (This does not apply
 * to <clinit> or static fields, since appropriate barriers are guaranteed
 * by the class initialization process.)
 */
static bool needsReturnBarrier(Method* method)
{
    if (!gDvm.dexOptForSmp)
        return false;
    if (strcmp(method->name, "<init>") != 0)
        return false;

    /*
     * Check to see if the class is finalizable.  The loader sets a flag
     * if the class or one of its superclasses overrides finalize().
     */
    const ClassObject* clazz = method->clazz;
    if (IS_CLASS_FLAG_SET(clazz, CLASS_ISFINALIZABLE))
        return true;

    /*
     * Check to see if the class has any final fields.  If not, we don't
     * need to generate a barrier instruction.
     *
     * In theory, we only need to do this if the method actually modifies
     * a final field.  In practice, non-constructor methods are allowed
     * to modify final fields, and there are 3rd-party tools that rely on
     * this behavior.  (The compiler does not allow it, but the VM does.)
     *
     * If we alter the verifier to restrict final-field updates to
     * constructors, we can tighten this up as well.
     */
    int idx = clazz->ifieldCount;
    while (--idx >= 0) {
        if (dvmIsFinalField(&clazz->ifields[idx]))
            return true;
    }

    return false;
}

/*
 * Convert a return-void to a return-void-barrier.
 */
static void rewriteReturnVoid(Method* method, u2* insns)
{
    assert((insns[0] & 0xff) == OP_RETURN_VOID);
    updateOpcode(method, insns, OP_RETURN_VOID_BARRIER);
}
