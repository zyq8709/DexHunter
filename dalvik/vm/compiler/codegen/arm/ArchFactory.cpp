/*
 * Copyright (C) 2010 The Android Open Source Project
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
 * This file contains arm-specific codegen factory support.
 * It is included by
 *
 *        Codegen-$(TARGET_ARCH_VARIANT).c
 *
 */

/*
 * Perform a "reg cmp imm" operation and jump to the PCR region if condition
 * satisfies.
 */
static TGT_LIR *genRegImmCheck(CompilationUnit *cUnit,
                               ArmConditionCode cond, int reg,
                               int checkValue, int dOffset,
                               TGT_LIR *pcrLabel)
{
    TGT_LIR *branch = genCmpImmBranch(cUnit, cond, reg, checkValue);
    if (cUnit->jitMode == kJitMethod) {
        BasicBlock *bb = cUnit->curBlock;
        if (bb->taken) {
            ArmLIR  *exceptionLabel = (ArmLIR *) cUnit->blockLabelList;
            exceptionLabel += bb->taken->id;
            branch->generic.target = (LIR *) exceptionLabel;
            return exceptionLabel;
        } else {
            ALOGE("Catch blocks not handled yet");
            dvmAbort();
            return NULL;
        }
    } else {
        return genCheckCommon(cUnit, dOffset, branch, pcrLabel);
    }
}

/*
 * Perform null-check on a register. sReg is the ssa register being checked,
 * and mReg is the machine register holding the actual value. If internal state
 * indicates that sReg has been checked before the check request is ignored.
 */
static TGT_LIR *genNullCheck(CompilationUnit *cUnit, int sReg, int mReg,
                             int dOffset, TGT_LIR *pcrLabel)
{
    /* This particular Dalvik register has been null-checked */
    if (dvmIsBitSet(cUnit->regPool->nullCheckedRegs, sReg)) {
        return pcrLabel;
    }
    dvmSetBit(cUnit->regPool->nullCheckedRegs, sReg);
    return genRegImmCheck(cUnit, kArmCondEq, mReg, 0, dOffset, pcrLabel);
}

/*
 * Perform a "reg cmp reg" operation and jump to the PCR region if condition
 * satisfies.
 */
static TGT_LIR *genRegRegCheck(CompilationUnit *cUnit,
                               ArmConditionCode cond,
                               int reg1, int reg2, int dOffset,
                               TGT_LIR *pcrLabel)
{
    TGT_LIR *res;
    res = opRegReg(cUnit, kOpCmp, reg1, reg2);
    TGT_LIR *branch = opCondBranch(cUnit, cond);
    genCheckCommon(cUnit, dOffset, branch, pcrLabel);
    return res;
}

/*
 * Perform zero-check on a register. Similar to genNullCheck but the value being
 * checked does not have a corresponding Dalvik register.
 */
static TGT_LIR *genZeroCheck(CompilationUnit *cUnit, int mReg,
                             int dOffset, TGT_LIR *pcrLabel)
{
    return genRegImmCheck(cUnit, kArmCondEq, mReg, 0, dOffset, pcrLabel);
}

/* Perform bound check on two registers */
static TGT_LIR *genBoundsCheck(CompilationUnit *cUnit, int rIndex,
                               int rBound, int dOffset, TGT_LIR *pcrLabel)
{
    return genRegRegCheck(cUnit, kArmCondCs, rIndex, rBound, dOffset,
                          pcrLabel);
}

/*
 * Jump to the out-of-line handler in ARM mode to finish executing the
 * remaining of more complex instructions.
 */
static void genDispatchToHandler(CompilationUnit *cUnit, TemplateOpcode opcode)
{
    /*
     * NOTE - In practice BLX only needs one operand, but since the assembler
     * may abort itself and retry due to other out-of-range conditions we
     * cannot really use operand[0] to store the absolute target address since
     * it may get clobbered by the final relative offset. Therefore,
     * we fake BLX_1 is a two operand instruction and the absolute target
     * address is stored in operand[1].
     */
    dvmCompilerClobberHandlerRegs(cUnit);
    newLIR2(cUnit, kThumbBlx1,
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode],
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode]);
    newLIR2(cUnit, kThumbBlx2,
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode],
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode]);
}
