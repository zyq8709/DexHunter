/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include "../../CompilerInternals.h"
#include "libdex/DexOpcodes.h"
#include "ArmLIR.h"

static const char *shiftNames[4] = {
    "lsl",
    "lsr",
    "asr",
    "ror"};

/* Decode and print a ARM register name */
static char * decodeRegList(ArmOpcode opcode, int vector, char *buf)
{
    int i;
    bool printed = false;
    buf[0] = 0;
    for (i = 0; i < 16; i++, vector >>= 1) {
        if (vector & 0x1) {
            int regId = i;
            if (opcode == kThumbPush && i == 8) {
                regId = r14lr;
            } else if (opcode == kThumbPop && i == 8) {
                regId = r15pc;
            }
            if (printed) {
                sprintf(buf + strlen(buf), ", r%d", regId);
            } else {
                printed = true;
                sprintf(buf, "r%d", regId);
            }
        }
    }
    return buf;
}

static int expandImmediate(int value)
{
    int mode = (value & 0xf00) >> 8;
    u4 bits = value & 0xff;
    switch(mode) {
        case 0:
            return bits;
       case 1:
            return (bits << 16) | bits;
       case 2:
            return (bits << 24) | (bits << 8);
       case 3:
            return (bits << 24) | (bits << 16) | (bits << 8) | bits;
      default:
            break;
    }
    bits = (bits | 0x80) << 24;
    return bits >> (((value & 0xf80) >> 7) - 8);
}

/*
 * Interpret a format string and build a string no longer than size
 * See format key in Assemble.c.
 */
static void buildInsnString(const char *fmt, ArmLIR *lir, char* buf,
                            unsigned char *baseAddr, int size)
{
    int i;
    char *bufEnd = &buf[size-1];
    const char *fmtEnd = &fmt[strlen(fmt)];
    char tbuf[256];
    const char *name;
    char nc;
    while (fmt < fmtEnd) {
        int operand;
        if (*fmt == '!') {
            fmt++;
            assert(fmt < fmtEnd);
            nc = *fmt++;
            if (nc=='!') {
                strcpy(tbuf, "!");
            } else {
               assert(fmt < fmtEnd);
               assert((unsigned)(nc-'0') < 4);
               operand = lir->operands[nc-'0'];
               switch(*fmt++) {
                   case 'H':
                       if (operand != 0) {
                           sprintf(tbuf, ", %s %d",shiftNames[operand & 0x3],
                                   operand >> 2);
                       } else {
                           strcpy(tbuf,"");
                       }
                       break;
                   case 'B':
                       switch (operand) {
                           case kSY:
                               name = "sy";
                               break;
                           case kST:
                               name = "st";
                               break;
                           case kISH:
                               name = "ish";
                               break;
                           case kISHST:
                               name = "ishst";
                               break;
                           case kNSH:
                               name = "nsh";
                               break;
                           case kNSHST:
                               name = "shst";
                               break;
                           default:
                               name = "DecodeError";
                               break;
                       }
                       strcpy(tbuf, name);
                       break;
                   case 'b':
                       strcpy(tbuf,"0000");
                       for (i=3; i>= 0; i--) {
                           tbuf[i] += operand & 1;
                           operand >>= 1;
                       }
                       break;
                   case 'n':
                       operand = ~expandImmediate(operand);
                       sprintf(tbuf,"%d [%#x]", operand, operand);
                       break;
                   case 'm':
                       operand = expandImmediate(operand);
                       sprintf(tbuf,"%d [%#x]", operand, operand);
                       break;
                   case 's':
                       sprintf(tbuf,"s%d",operand & FP_REG_MASK);
                       break;
                   case 'S':
                       sprintf(tbuf,"d%d",(operand & FP_REG_MASK) >> 1);
                       break;
                   case 'h':
                       sprintf(tbuf,"%04x", operand);
                       break;
                   case 'M':
                   case 'd':
                       sprintf(tbuf,"%d", operand);
                       break;
                   case 'E':
                       sprintf(tbuf,"%d", operand*4);
                       break;
                   case 'F':
                       sprintf(tbuf,"%d", operand*2);
                       break;
                   case 'c':
                       switch (operand) {
                           case kArmCondEq:
                               strcpy(tbuf, "eq");
                               break;
                           case kArmCondNe:
                               strcpy(tbuf, "ne");
                               break;
                           case kArmCondLt:
                               strcpy(tbuf, "lt");
                               break;
                           case kArmCondGe:
                               strcpy(tbuf, "ge");
                               break;
                           case kArmCondGt:
                               strcpy(tbuf, "gt");
                               break;
                           case kArmCondLe:
                               strcpy(tbuf, "le");
                               break;
                           case kArmCondCs:
                               strcpy(tbuf, "cs");
                               break;
                           case kArmCondMi:
                               strcpy(tbuf, "mi");
                               break;
                           default:
                               strcpy(tbuf, "");
                               break;
                       }
                       break;
                   case 't':
                       sprintf(tbuf,"0x%08x (L%p)",
                               (int) baseAddr + lir->generic.offset + 4 +
                               (operand << 1),
                               lir->generic.target);
                       break;
                   case 'u': {
                       int offset_1 = lir->operands[0];
                       int offset_2 = NEXT_LIR(lir)->operands[0];
                       intptr_t target =
                           ((((intptr_t) baseAddr + lir->generic.offset + 4) &
                            ~3) + (offset_1 << 21 >> 9) + (offset_2 << 1)) &
                           0xfffffffc;
                       sprintf(tbuf, "%p", (void *) target);
                       break;
                    }

                   /* Nothing to print for BLX_2 */
                   case 'v':
                       strcpy(tbuf, "see above");
                       break;
                   case 'R':
                       decodeRegList(lir->opcode, operand, tbuf);
                       break;
                   default:
                       strcpy(tbuf,"DecodeError");
                       break;
               }
               if (buf+strlen(tbuf) <= bufEnd) {
                   strcpy(buf, tbuf);
                   buf += strlen(tbuf);
               } else {
                   break;
               }
            }
        } else {
           *buf++ = *fmt++;
        }
        if (buf == bufEnd)
            break;
    }
    *buf = 0;
}

void dvmDumpResourceMask(LIR *lir, u8 mask, const char *prefix)
{
    char buf[256];
    buf[0] = 0;
    ArmLIR *armLIR = (ArmLIR *) lir;

    if (mask == ENCODE_ALL) {
        strcpy(buf, "all");
    } else {
        char num[8];
        int i;

        for (i = 0; i < kRegEnd; i++) {
            if (mask & (1ULL << i)) {
                sprintf(num, "%d ", i);
                strcat(buf, num);
            }
        }

        if (mask & ENCODE_CCODE) {
            strcat(buf, "cc ");
        }
        if (mask & ENCODE_FP_STATUS) {
            strcat(buf, "fpcc ");
        }

        /* Memory bits */
        if (armLIR && (mask & ENCODE_DALVIK_REG)) {
            sprintf(buf + strlen(buf), "dr%d%s", armLIR->aliasInfo & 0xffff,
                    (armLIR->aliasInfo & 0x80000000) ? "(+1)" : "");
        }
        if (mask & ENCODE_LITERAL) {
            strcat(buf, "lit ");
        }

        if (mask & ENCODE_HEAP_REF) {
            strcat(buf, "heap ");
        }
        if (mask & ENCODE_MUST_NOT_ALIAS) {
            strcat(buf, "noalias ");
        }
    }
    if (buf[0]) {
        ALOGD("%s: %s", prefix, buf);
    }
}

/*
 * Debugging macros
 */
#define DUMP_RESOURCE_MASK(X)
#define DUMP_SSA_REP(X)

/* Pretty-print a LIR instruction */
void dvmDumpLIRInsn(LIR *arg, unsigned char *baseAddr)
{
    ArmLIR *lir = (ArmLIR *) arg;
    char buf[256];
    char opName[256];
    int offset = lir->generic.offset;
    int dest = lir->operands[0];
    const bool dumpNop = false;

    /* Handle pseudo-ops individually, and all regular insns as a group */
    switch(lir->opcode) {
        case kArmChainingCellBottom:
            ALOGD("-------- end of chaining cells (0x%04x)", offset);
            break;
        case kArmPseudoBarrier:
            ALOGD("-------- BARRIER");
            break;
        case kArmPseudoExtended:
            ALOGD("-------- %s", (char *) dest);
            break;
        case kArmPseudoSSARep:
            DUMP_SSA_REP(LOGD("-------- %s", (char *) dest));
            break;
        case kArmPseudoChainingCellBackwardBranch:
            ALOGD("L%p:", lir);
            ALOGD("-------- chaining cell (backward branch): 0x%04x", dest);
            break;
        case kArmPseudoChainingCellNormal:
            ALOGD("L%p:", lir);
            ALOGD("-------- chaining cell (normal): 0x%04x", dest);
            break;
        case kArmPseudoChainingCellHot:
            ALOGD("L%p:", lir);
            ALOGD("-------- chaining cell (hot): 0x%04x", dest);
            break;
        case kArmPseudoChainingCellInvokePredicted:
            ALOGD("L%p:", lir);
            ALOGD("-------- chaining cell (predicted): %s%s",
                 dest ? ((Method *) dest)->clazz->descriptor : "",
                 dest ? ((Method *) dest)->name : "N/A");
            break;
        case kArmPseudoChainingCellInvokeSingleton:
            ALOGD("L%p:", lir);
            ALOGD("-------- chaining cell (invoke singleton): %s%s/%p",
                 ((Method *)dest)->clazz->descriptor,
                 ((Method *)dest)->name,
                 ((Method *)dest)->insns);
            break;
        case kArmPseudoEntryBlock:
            ALOGD("-------- entry offset: 0x%04x", dest);
            break;
        case kArmPseudoDalvikByteCodeBoundary:
            ALOGD("-------- dalvik offset: 0x%04x @ %s", dest,
                 (char *) lir->operands[1]);
            break;
        case kArmPseudoExitBlock:
            ALOGD("-------- exit offset: 0x%04x", dest);
            break;
        case kArmPseudoPseudoAlign4:
            ALOGD("%p (%04x): .align4", baseAddr + offset, offset);
            break;
        case kArmPseudoPCReconstructionCell:
            ALOGD("L%p:", lir);
            ALOGD("-------- reconstruct dalvik PC : 0x%04x @ +0x%04x", dest,
                 lir->operands[1]);
            break;
        case kArmPseudoPCReconstructionBlockLabel:
            /* Do nothing */
            break;
        case kArmPseudoEHBlockLabel:
            ALOGD("Exception_Handling:");
            break;
        case kArmPseudoTargetLabel:
        case kArmPseudoNormalBlockLabel:
            ALOGD("L%p:", lir);
            break;
        default:
            if (lir->flags.isNop && !dumpNop) {
                break;
            }
            buildInsnString(EncodingMap[lir->opcode].name, lir, opName,
                            baseAddr, 256);
            buildInsnString(EncodingMap[lir->opcode].fmt, lir, buf, baseAddr,
                            256);
            ALOGD("%p (%04x): %-8s%s%s",
                 baseAddr + offset, offset, opName, buf,
                 lir->flags.isNop ? "(nop)" : "");
            break;
    }

    if (lir->useMask && (!lir->flags.isNop || dumpNop)) {
        DUMP_RESOURCE_MASK(dvmDumpResourceMask((LIR *) lir,
                                               lir->useMask, "use"));
    }
    if (lir->defMask && (!lir->flags.isNop || dumpNop)) {
        DUMP_RESOURCE_MASK(dvmDumpResourceMask((LIR *) lir,
                                               lir->defMask, "def"));
    }
}

/* Dump instructions and constant pool contents */
void dvmCompilerCodegenDump(CompilationUnit *cUnit)
{
    ALOGD("Dumping LIR insns");
    LIR *lirInsn;
    ArmLIR *armLIR;

    ALOGD("installed code is at %p", cUnit->baseAddr);
    ALOGD("total size is %d bytes", cUnit->totalSize);
    for (lirInsn = cUnit->firstLIRInsn; lirInsn; lirInsn = lirInsn->next) {
        dvmDumpLIRInsn(lirInsn, (unsigned char *) cUnit->baseAddr);
    }
    for (lirInsn = cUnit->classPointerList; lirInsn; lirInsn = lirInsn->next) {
        armLIR = (ArmLIR *) lirInsn;
        ALOGD("%p (%04x): .class (%s)",
             (char*)cUnit->baseAddr + armLIR->generic.offset,
             armLIR->generic.offset,
             ((CallsiteInfo *) armLIR->operands[0])->classDescriptor);
    }
    for (lirInsn = cUnit->literalList; lirInsn; lirInsn = lirInsn->next) {
        armLIR = (ArmLIR *) lirInsn;
        ALOGD("%p (%04x): .word (%#x)",
             (char*)cUnit->baseAddr + armLIR->generic.offset,
             armLIR->generic.offset,
             armLIR->operands[0]);
    }
}

/* Target-specific cache flushing */
void dvmCompilerCacheFlush(long start, long end, long flags)
{
    cacheflush(start, end, flags);
}

/* Target-specific cache clearing */
void dvmCompilerCacheClear(char *start, size_t size)
{
    /*
     * de is an invalid opcode for arm.
     * From gdb disassembly:  <UNDEFINED> instruction: 0xdede
     */

    memset(start, 0xde, size);
}
