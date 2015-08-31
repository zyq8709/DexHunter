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

/*
 * This file contains codegen for the Thumb2 ISA and is intended to be
 * includes by:
 *
 *        Codegen-$(TARGET_ARCH_VARIANT).c
 *
 */

/*
 * Reserve 6 bytes at the beginning of the trace
 *        +----------------------------+
 *        | prof count addr (4 bytes)  |
 *        +----------------------------+
 *        | chain cell offset (2 bytes)|
 *        +----------------------------+
 *
 * ...and then code to increment the execution
 *
 * For continuous profiling (10 bytes)
 *       ldr   r0, [pc-8]   @ get prof count addr    [4 bytes]
 *       ldr   r1, [r0]     @ load counter           [2 bytes]
 *       add   r1, #1       @ increment              [2 bytes]
 *       str   r1, [r0]     @ store                  [2 bytes]
 *
 * For periodic profiling (4 bytes)
 *       call  TEMPLATE_PERIODIC_PROFILING
 *
 * and return the size (in bytes) of the generated code.
 */

static int genTraceProfileEntry(CompilationUnit *cUnit)
{
    intptr_t addr = (intptr_t)dvmJitNextTraceCounter();
    assert(__BYTE_ORDER == __LITTLE_ENDIAN);
    newLIR1(cUnit, kArm16BitData, addr & 0xffff);
    newLIR1(cUnit, kArm16BitData, (addr >> 16) & 0xffff);
    cUnit->chainCellOffsetLIR =
        (LIR *) newLIR1(cUnit, kArm16BitData, CHAIN_CELL_OFFSET_TAG);
    cUnit->headerSize = 6;
    if ((gDvmJit.profileMode == kTraceProfilingContinuous) ||
        (gDvmJit.profileMode == kTraceProfilingDisabled)) {
        /* Thumb[2] instruction used directly here to ensure correct size */
        newLIR2(cUnit, kThumb2LdrPcReln12, r0, 8);
        newLIR3(cUnit, kThumbLdrRRI5, r1, r0, 0);
        newLIR2(cUnit, kThumbAddRI8, r1, 1);
        newLIR3(cUnit, kThumbStrRRI5, r1, r0, 0);
        return 10;
    } else {
        int opcode = TEMPLATE_PERIODIC_PROFILING;
        newLIR2(cUnit, kThumbBlx1,
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode],
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode]);
        newLIR2(cUnit, kThumbBlx2,
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode],
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode]);
        return 4;
    }
}

static void genNegFloat(CompilationUnit *cUnit, RegLocation rlDest,
                        RegLocation rlSrc)
{
    RegLocation rlResult;
    rlSrc = loadValue(cUnit, rlSrc, kFPReg);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kFPReg, true);
    newLIR2(cUnit, kThumb2Vnegs, rlResult.lowReg, rlSrc.lowReg);
    storeValue(cUnit, rlDest, rlResult);
}

static void genNegDouble(CompilationUnit *cUnit, RegLocation rlDest,
                         RegLocation rlSrc)
{
    RegLocation rlResult;
    rlSrc = loadValueWide(cUnit, rlSrc, kFPReg);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kFPReg, true);
    newLIR2(cUnit, kThumb2Vnegd, S2D(rlResult.lowReg, rlResult.highReg),
            S2D(rlSrc.lowReg, rlSrc.highReg));
    storeValueWide(cUnit, rlDest, rlResult);
}

/*
 * To avoid possible conflicts, we use a lot of temps here.  Note that
 * our usage of Thumb2 instruction forms avoids the problems with register
 * reuse for multiply instructions prior to arm6.
 */
static void genMulLong(CompilationUnit *cUnit, RegLocation rlDest,
                       RegLocation rlSrc1, RegLocation rlSrc2)
{
    RegLocation rlResult;
    int resLo = dvmCompilerAllocTemp(cUnit);
    int resHi = dvmCompilerAllocTemp(cUnit);
    int tmp1 = dvmCompilerAllocTemp(cUnit);

    rlSrc1 = loadValueWide(cUnit, rlSrc1, kCoreReg);
    rlSrc2 = loadValueWide(cUnit, rlSrc2, kCoreReg);

    newLIR3(cUnit, kThumb2MulRRR, tmp1, rlSrc2.lowReg, rlSrc1.highReg);
    newLIR4(cUnit, kThumb2Umull, resLo, resHi, rlSrc2.lowReg, rlSrc1.lowReg);
    newLIR4(cUnit, kThumb2Mla, tmp1, rlSrc1.lowReg, rlSrc2.highReg, tmp1);
    newLIR4(cUnit, kThumb2AddRRR, resHi, tmp1, resHi, 0);
    dvmCompilerFreeTemp(cUnit, tmp1);

    rlResult = dvmCompilerGetReturnWide(cUnit);  // Just as a template, will patch
    rlResult.lowReg = resLo;
    rlResult.highReg = resHi;
    storeValueWide(cUnit, rlDest, rlResult);
}

static void genLong3Addr(CompilationUnit *cUnit, MIR *mir, OpKind firstOp,
                         OpKind secondOp, RegLocation rlDest,
                         RegLocation rlSrc1, RegLocation rlSrc2)
{
    RegLocation rlResult;
    rlSrc1 = loadValueWide(cUnit, rlSrc1, kCoreReg);
    rlSrc2 = loadValueWide(cUnit, rlSrc2, kCoreReg);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    opRegRegReg(cUnit, firstOp, rlResult.lowReg, rlSrc1.lowReg, rlSrc2.lowReg);
    opRegRegReg(cUnit, secondOp, rlResult.highReg, rlSrc1.highReg,
                rlSrc2.highReg);
    storeValueWide(cUnit, rlDest, rlResult);
}

void dvmCompilerInitializeRegAlloc(CompilationUnit *cUnit)
{
    int numTemps = sizeof(coreTemps)/sizeof(int);
    int numFPTemps = sizeof(fpTemps)/sizeof(int);
    RegisterPool *pool = (RegisterPool *)dvmCompilerNew(sizeof(*pool), true);
    cUnit->regPool = pool;
    pool->numCoreTemps = numTemps;
    pool->coreTemps = (RegisterInfo *)
            dvmCompilerNew(numTemps * sizeof(*cUnit->regPool->coreTemps), true);
    pool->numFPTemps = numFPTemps;
    pool->FPTemps = (RegisterInfo *)
            dvmCompilerNew(numFPTemps * sizeof(*cUnit->regPool->FPTemps), true);
    dvmCompilerInitPool(pool->coreTemps, coreTemps, pool->numCoreTemps);
    dvmCompilerInitPool(pool->FPTemps, fpTemps, pool->numFPTemps);
    pool->nullCheckedRegs =
        dvmCompilerAllocBitVector(cUnit->numSSARegs, false);
}

/*
 * Generate a Thumb2 IT instruction, which can nullify up to
 * four subsequent instructions based on a condition and its
 * inverse.  The condition applies to the first instruction, which
 * is executed if the condition is met.  The string "guide" consists
 * of 0 to 3 chars, and applies to the 2nd through 4th instruction.
 * A "T" means the instruction is executed if the condition is
 * met, and an "E" means the instruction is executed if the condition
 * is not met.
 */
static ArmLIR *genIT(CompilationUnit *cUnit, ArmConditionCode code,
                     const char *guide)
{
    int mask;
    int condBit = code & 1;
    int altBit = condBit ^ 1;
    int mask3 = 0;
    int mask2 = 0;
    int mask1 = 0;

    //Note: case fallthroughs intentional
    switch(strlen(guide)) {
        case 3:
            mask1 = (guide[2] == 'T') ? condBit : altBit;
        case 2:
            mask2 = (guide[1] == 'T') ? condBit : altBit;
        case 1:
            mask3 = (guide[0] == 'T') ? condBit : altBit;
            break;
        case 0:
            break;
        default:
            ALOGE("Jit: bad case in genIT");
            dvmCompilerAbort(cUnit);
    }
    mask = (mask3 << 3) | (mask2 << 2) | (mask1 << 1) |
           (1 << (3 - strlen(guide)));
    return newLIR2(cUnit, kThumb2It, code, mask);
}

/* Export the Dalvik PC assicated with an instruction to the StackSave area */
static ArmLIR *genExportPC(CompilationUnit *cUnit, MIR *mir)
{
    ArmLIR *res;
    int offset = offsetof(StackSaveArea, xtra.currentPc);
    int rDPC = dvmCompilerAllocTemp(cUnit);
    res = loadConstant(cUnit, rDPC, (int) (cUnit->method->insns + mir->offset));
    newLIR3(cUnit, kThumb2StrRRI8Predec, rDPC, r5FP,
            sizeof(StackSaveArea) - offset);
    dvmCompilerFreeTemp(cUnit, rDPC);
    return res;
}

/*
 * Handle simple case (thin lock) inline.  If it's complicated, bail
 * out to the heavyweight lock/unlock routines.  We'll use dedicated
 * registers here in order to be in the right position in case we
 * to bail to dvm[Lock/Unlock]Object(self, object)
 *
 * r0 -> self pointer [arg0 for dvm[Lock/Unlock]Object
 * r1 -> object [arg1 for dvm[Lock/Unlock]Object
 * r2 -> intial contents of object->lock, later result of strex
 * r3 -> self->threadId
 * r7 -> temp to hold new lock value [unlock only]
 * r4 -> allow to be used by utilities as general temp
 *
 * The result of the strex is 0 if we acquire the lock.
 *
 * See comments in Sync.c for the layout of the lock word.
 * Of particular interest to this code is the test for the
 * simple case - which we handle inline.  For monitor enter, the
 * simple case is thin lock, held by no-one.  For monitor exit,
 * the simple case is thin lock, held by the unlocking thread with
 * a recurse count of 0.
 *
 * A minor complication is that there is a field in the lock word
 * unrelated to locking: the hash state.  This field must be ignored, but
 * preserved.
 *
 */
static void genMonitorEnter(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    ArmLIR *target;
    ArmLIR *hopTarget;
    ArmLIR *branch;
    ArmLIR *hopBranch;

    assert(LW_SHAPE_THIN == 0);
    loadValueDirectFixed(cUnit, rlSrc, r1);  // Get obj
    dvmCompilerLockAllTemps(cUnit);  // Prepare for explicit register usage
    dvmCompilerFreeTemp(cUnit, r4PC);  // Free up r4 for general use
    genNullCheck(cUnit, rlSrc.sRegLow, r1, mir->offset, NULL);
    loadWordDisp(cUnit, r6SELF, offsetof(Thread, threadId), r3); // Get threadId
    newLIR3(cUnit, kThumb2Ldrex, r2, r1,
            offsetof(Object, lock) >> 2); // Get object->lock
    opRegImm(cUnit, kOpLsl, r3, LW_LOCK_OWNER_SHIFT); // Align owner
    // Is lock unheld on lock or held by us (==threadId) on unlock?
    newLIR4(cUnit, kThumb2Bfi, r3, r2, 0, LW_LOCK_OWNER_SHIFT - 1);
    newLIR3(cUnit, kThumb2Bfc, r2, LW_HASH_STATE_SHIFT,
            LW_LOCK_OWNER_SHIFT - 1);
    hopBranch = newLIR2(cUnit, kThumb2Cbnz, r2, 0);
    newLIR4(cUnit, kThumb2Strex, r2, r3, r1, offsetof(Object, lock) >> 2);
    dvmCompilerGenMemBarrier(cUnit, kSY);
    branch = newLIR2(cUnit, kThumb2Cbz, r2, 0);

    hopTarget = newLIR0(cUnit, kArmPseudoTargetLabel);
    hopTarget->defMask = ENCODE_ALL;
    hopBranch->generic.target = (LIR *)hopTarget;

    // Export PC (part 1)
    loadConstant(cUnit, r3, (int) (cUnit->method->insns + mir->offset));

    /* Get dPC of next insn */
    loadConstant(cUnit, r4PC, (int)(cUnit->method->insns + mir->offset +
                 dexGetWidthFromOpcode(OP_MONITOR_ENTER)));
    // Export PC (part 2)
    newLIR3(cUnit, kThumb2StrRRI8Predec, r3, r5FP,
            sizeof(StackSaveArea) -
            offsetof(StackSaveArea, xtra.currentPc));
    /* Call template, and don't return */
    genRegCopy(cUnit, r0, r6SELF);
    genDispatchToHandler(cUnit, TEMPLATE_MONITOR_ENTER);
    // Resume here
    target = newLIR0(cUnit, kArmPseudoTargetLabel);
    target->defMask = ENCODE_ALL;
    branch->generic.target = (LIR *)target;
}

/*
 * For monitor unlock, we don't have to use ldrex/strex.  Once
 * we've determined that the lock is thin and that we own it with
 * a zero recursion count, it's safe to punch it back to the
 * initial, unlock thin state with a store word.
 */
static void genMonitorExit(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    ArmLIR *target;
    ArmLIR *branch;
    ArmLIR *hopTarget;
    ArmLIR *hopBranch;

    assert(LW_SHAPE_THIN == 0);
    loadValueDirectFixed(cUnit, rlSrc, r1);  // Get obj
    dvmCompilerLockAllTemps(cUnit);  // Prepare for explicit register usage
    dvmCompilerFreeTemp(cUnit, r4PC);  // Free up r4 for general use
    genNullCheck(cUnit, rlSrc.sRegLow, r1, mir->offset, NULL);
    loadWordDisp(cUnit, r1, offsetof(Object, lock), r2); // Get object->lock
    loadWordDisp(cUnit, r6SELF, offsetof(Thread, threadId), r3); // Get threadId
    // Is lock unheld on lock or held by us (==threadId) on unlock?
    opRegRegImm(cUnit, kOpAnd, r7, r2,
                (LW_HASH_STATE_MASK << LW_HASH_STATE_SHIFT));
    opRegImm(cUnit, kOpLsl, r3, LW_LOCK_OWNER_SHIFT); // Align owner
    newLIR3(cUnit, kThumb2Bfc, r2, LW_HASH_STATE_SHIFT,
            LW_LOCK_OWNER_SHIFT - 1);
    opRegReg(cUnit, kOpSub, r2, r3);
    hopBranch = opCondBranch(cUnit, kArmCondNe);
    dvmCompilerGenMemBarrier(cUnit, kSY);
    storeWordDisp(cUnit, r1, offsetof(Object, lock), r7);
    branch = opNone(cUnit, kOpUncondBr);

    hopTarget = newLIR0(cUnit, kArmPseudoTargetLabel);
    hopTarget->defMask = ENCODE_ALL;
    hopBranch->generic.target = (LIR *)hopTarget;

    // Export PC (part 1)
    loadConstant(cUnit, r3, (int) (cUnit->method->insns + mir->offset));

    LOAD_FUNC_ADDR(cUnit, r7, (int)dvmUnlockObject);
    genRegCopy(cUnit, r0, r6SELF);
    // Export PC (part 2)
    newLIR3(cUnit, kThumb2StrRRI8Predec, r3, r5FP,
            sizeof(StackSaveArea) -
            offsetof(StackSaveArea, xtra.currentPc));
    opReg(cUnit, kOpBlx, r7);
    /* Did we throw? */
    ArmLIR *branchOver = genCmpImmBranch(cUnit, kArmCondNe, r0, 0);
    loadConstant(cUnit, r0,
                 (int) (cUnit->method->insns + mir->offset +
                 dexGetWidthFromOpcode(OP_MONITOR_EXIT)));
    genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);

    // Resume here
    target = newLIR0(cUnit, kArmPseudoTargetLabel);
    target->defMask = ENCODE_ALL;
    branch->generic.target = (LIR *)target;
    branchOver->generic.target = (LIR *) target;
}

static void genMonitor(CompilationUnit *cUnit, MIR *mir)
{
    if (mir->dalvikInsn.opcode == OP_MONITOR_ENTER)
        genMonitorEnter(cUnit, mir);
    else
        genMonitorExit(cUnit, mir);
}

/*
 * 64-bit 3way compare function.
 *     mov   r7, #-1
 *     cmp   op1hi, op2hi
 *     blt   done
 *     bgt   flip
 *     sub   r7, op1lo, op2lo (treat as unsigned)
 *     beq   done
 *     ite   hi
 *     mov(hi)   r7, #-1
 *     mov(!hi)  r7, #1
 * flip:
 *     neg   r7
 * done:
 */
static void genCmpLong(CompilationUnit *cUnit, MIR *mir,
                       RegLocation rlDest, RegLocation rlSrc1,
                       RegLocation rlSrc2)
{
    RegLocation rlTemp = LOC_C_RETURN; // Just using as template, will change
    ArmLIR *target1;
    ArmLIR *target2;
    rlSrc1 = loadValueWide(cUnit, rlSrc1, kCoreReg);
    rlSrc2 = loadValueWide(cUnit, rlSrc2, kCoreReg);
    rlTemp.lowReg = dvmCompilerAllocTemp(cUnit);
    loadConstant(cUnit, rlTemp.lowReg, -1);
    opRegReg(cUnit, kOpCmp, rlSrc1.highReg, rlSrc2.highReg);
    ArmLIR *branch1 = opCondBranch(cUnit, kArmCondLt);
    ArmLIR *branch2 = opCondBranch(cUnit, kArmCondGt);
    opRegRegReg(cUnit, kOpSub, rlTemp.lowReg, rlSrc1.lowReg, rlSrc2.lowReg);
    ArmLIR *branch3 = opCondBranch(cUnit, kArmCondEq);

    genIT(cUnit, kArmCondHi, "E");
    newLIR2(cUnit, kThumb2MovImmShift, rlTemp.lowReg, modifiedImmediate(-1));
    loadConstant(cUnit, rlTemp.lowReg, 1);
    genBarrier(cUnit);

    target2 = newLIR0(cUnit, kArmPseudoTargetLabel);
    target2->defMask = -1;
    opRegReg(cUnit, kOpNeg, rlTemp.lowReg, rlTemp.lowReg);

    target1 = newLIR0(cUnit, kArmPseudoTargetLabel);
    target1->defMask = -1;

    storeValue(cUnit, rlDest, rlTemp);

    branch1->generic.target = (LIR *)target1;
    branch2->generic.target = (LIR *)target2;
    branch3->generic.target = branch1->generic.target;
}

static bool genInlinedAbsFloat(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlDest = inlinedTarget(cUnit, mir, true);
    rlSrc = loadValue(cUnit, rlSrc, kFPReg);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kFPReg, true);
    newLIR2(cUnit, kThumb2Vabss, rlResult.lowReg, rlSrc.lowReg);
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static bool genInlinedAbsDouble(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
    RegLocation rlDest = inlinedTargetWide(cUnit, mir, true);
    rlSrc = loadValueWide(cUnit, rlSrc, kFPReg);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kFPReg, true);
    newLIR2(cUnit, kThumb2Vabsd, S2D(rlResult.lowReg, rlResult.highReg),
            S2D(rlSrc.lowReg, rlSrc.highReg));
    storeValueWide(cUnit, rlDest, rlResult);
    return false;
}

static bool genInlinedMinMaxInt(CompilationUnit *cUnit, MIR *mir, bool isMin)
{
    RegLocation rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 1);
    rlSrc1 = loadValue(cUnit, rlSrc1, kCoreReg);
    rlSrc2 = loadValue(cUnit, rlSrc2, kCoreReg);
    RegLocation rlDest = inlinedTarget(cUnit, mir, false);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    opRegReg(cUnit, kOpCmp, rlSrc1.lowReg, rlSrc2.lowReg);
    genIT(cUnit, (isMin) ? kArmCondGt : kArmCondLt, "E");
    opRegReg(cUnit, kOpMov, rlResult.lowReg, rlSrc2.lowReg);
    opRegReg(cUnit, kOpMov, rlResult.lowReg, rlSrc1.lowReg);
    genBarrier(cUnit);
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static void genMultiplyByTwoBitMultiplier(CompilationUnit *cUnit,
        RegLocation rlSrc, RegLocation rlResult, int lit,
        int firstBit, int secondBit)
{
    opRegRegRegShift(cUnit, kOpAdd, rlResult.lowReg, rlSrc.lowReg, rlSrc.lowReg,
                     encodeShift(kArmLsl, secondBit - firstBit));
    if (firstBit != 0) {
        opRegRegImm(cUnit, kOpLsl, rlResult.lowReg, rlResult.lowReg, firstBit);
    }
}

static void genMultiplyByShiftAndReverseSubtract(CompilationUnit *cUnit,
        RegLocation rlSrc, RegLocation rlResult, int lit)
{
    newLIR4(cUnit, kThumb2RsbRRR, rlResult.lowReg, rlSrc.lowReg, rlSrc.lowReg,
            encodeShift(kArmLsl, lit));
}
