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
 * This file contains codegen and support common to all supported
 * Mips variants.  It is included by:
 *
 *        Codegen-$(TARGET_ARCH_VARIANT).c
 *
 * which combines this common code with specific support found in the
 * applicable directory below this one.
 */

/*
 * Mark garbage collection card. Skip if the value we're storing is null.
 */
static void markCard(CompilationUnit *cUnit, int valReg, int tgtAddrReg)
{
    int regCardBase = dvmCompilerAllocTemp(cUnit);
    int regCardNo = dvmCompilerAllocTemp(cUnit);
    MipsLIR *branchOver = opCompareBranch(cUnit, kMipsBeq, valReg, r_ZERO);
    loadWordDisp(cUnit, rSELF, offsetof(Thread, cardTable),
                 regCardBase);
    opRegRegImm(cUnit, kOpLsr, regCardNo, tgtAddrReg, GC_CARD_SHIFT);
    storeBaseIndexed(cUnit, regCardBase, regCardNo, regCardBase, 0,
                     kUnsignedByte);
    MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
    target->defMask = ENCODE_ALL;
    branchOver->generic.target = (LIR *)target;
    dvmCompilerFreeTemp(cUnit, regCardBase);
    dvmCompilerFreeTemp(cUnit, regCardNo);
}

static bool genConversionCall(CompilationUnit *cUnit, MIR *mir, void *funct,
                                     int srcSize, int tgtSize)
{
    /*
     * Don't optimize the register usage since it calls out to template
     * functions
     */
    RegLocation rlSrc;
    RegLocation rlDest;
    int srcReg = 0;
    int srcRegHi = 0;
    dvmCompilerFlushAllRegs(cUnit);   /* Send everything to home location */

    if (srcSize == kWord) {
        srcReg = r_A0;
    } else if (srcSize == kSingle) {
#ifdef __mips_hard_float
        srcReg = r_F12;
#else
        srcReg = r_A0;
#endif
    } else if (srcSize == kLong) {
        srcReg = r_ARG0;
        srcRegHi = r_ARG1;
    } else if (srcSize == kDouble) {
#ifdef __mips_hard_float
        srcReg = r_FARG0;
        srcRegHi = r_FARG1;
#else
        srcReg = r_ARG0;
        srcRegHi = r_ARG1;
#endif
    }
    else {
        assert(0);
    }

    if (srcSize == kWord || srcSize == kSingle) {
        rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
        loadValueDirectFixed(cUnit, rlSrc, srcReg);
    } else {
        rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
        loadValueDirectWideFixed(cUnit, rlSrc, srcReg, srcRegHi);
    }
    LOAD_FUNC_ADDR(cUnit, r_T9, (int)funct);
    opReg(cUnit, kOpBlx, r_T9);
    newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
    dvmCompilerClobberCallRegs(cUnit);
    if (tgtSize == kWord || tgtSize == kSingle) {
        RegLocation rlResult;
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);
#ifdef __mips_hard_float
        if (tgtSize == kSingle)
            rlResult = dvmCompilerGetReturnAlt(cUnit);
        else
            rlResult = dvmCompilerGetReturn(cUnit);
#else
        rlResult = dvmCompilerGetReturn(cUnit);
#endif
        storeValue(cUnit, rlDest, rlResult);
    } else {
        RegLocation rlResult;
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
#ifdef __mips_hard_float
        if (tgtSize == kDouble)
            rlResult = dvmCompilerGetReturnWideAlt(cUnit);
        else
            rlResult = dvmCompilerGetReturnWide(cUnit);
#else
        rlResult = dvmCompilerGetReturnWide(cUnit);
#endif
        storeValueWide(cUnit, rlDest, rlResult);
    }
    return false;
}


static bool genArithOpFloatPortable(CompilationUnit *cUnit, MIR *mir,
                                    RegLocation rlDest, RegLocation rlSrc1,
                                    RegLocation rlSrc2)
{
    RegLocation rlResult;
    void* funct;

    switch (mir->dalvikInsn.opcode) {
        case OP_ADD_FLOAT_2ADDR:
        case OP_ADD_FLOAT:
            funct = (void*) __addsf3;
            break;
        case OP_SUB_FLOAT_2ADDR:
        case OP_SUB_FLOAT:
            funct = (void*) __subsf3;
            break;
        case OP_DIV_FLOAT_2ADDR:
        case OP_DIV_FLOAT:
            funct = (void*) __divsf3;
            break;
        case OP_MUL_FLOAT_2ADDR:
        case OP_MUL_FLOAT:
            funct = (void*) __mulsf3;
            break;
        case OP_REM_FLOAT_2ADDR:
        case OP_REM_FLOAT:
            funct = (void*) fmodf;
            break;
        case OP_NEG_FLOAT: {
            genNegFloat(cUnit, rlDest, rlSrc1);
            return false;
        }
        default:
            return true;
    }

    dvmCompilerFlushAllRegs(cUnit);   /* Send everything to home location */
#ifdef __mips_hard_float
    loadValueDirectFixed(cUnit, rlSrc1, r_F12);
    loadValueDirectFixed(cUnit, rlSrc2, r_F14);
#else
    loadValueDirectFixed(cUnit, rlSrc1, r_A0);
    loadValueDirectFixed(cUnit, rlSrc2, r_A1);
#endif
    LOAD_FUNC_ADDR(cUnit, r_T9, (int)funct);
    opReg(cUnit, kOpBlx, r_T9);
    newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
    dvmCompilerClobberCallRegs(cUnit);
#ifdef __mips_hard_float
    rlResult = dvmCompilerGetReturnAlt(cUnit);
#else
    rlResult = dvmCompilerGetReturn(cUnit);
#endif
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static bool genArithOpDoublePortable(CompilationUnit *cUnit, MIR *mir,
                                     RegLocation rlDest, RegLocation rlSrc1,
                                     RegLocation rlSrc2)
{
    RegLocation rlResult;
    void* funct;

    switch (mir->dalvikInsn.opcode) {
        case OP_ADD_DOUBLE_2ADDR:
        case OP_ADD_DOUBLE:
            funct = (void*) __adddf3;
            break;
        case OP_SUB_DOUBLE_2ADDR:
        case OP_SUB_DOUBLE:
            funct = (void*) __subdf3;
            break;
        case OP_DIV_DOUBLE_2ADDR:
        case OP_DIV_DOUBLE:
            funct = (void*) __divsf3;
            break;
        case OP_MUL_DOUBLE_2ADDR:
        case OP_MUL_DOUBLE:
            funct = (void*) __muldf3;
            break;
        case OP_REM_DOUBLE_2ADDR:
        case OP_REM_DOUBLE:
            funct = (void*) (double (*)(double, double)) fmod;
            break;
        case OP_NEG_DOUBLE: {
            genNegDouble(cUnit, rlDest, rlSrc1);
            return false;
        }
        default:
            return true;
    }
    dvmCompilerFlushAllRegs(cUnit);   /* Send everything to home location */
    LOAD_FUNC_ADDR(cUnit, r_T9, (int)funct);
#ifdef __mips_hard_float
    loadValueDirectWideFixed(cUnit, rlSrc1, r_F12, r_F13);
    loadValueDirectWideFixed(cUnit, rlSrc2, r_F14, r_F15);
#else
    loadValueDirectWideFixed(cUnit, rlSrc1, r_ARG0, r_ARG1);
    loadValueDirectWideFixed(cUnit, rlSrc2, r_ARG2, r_ARG3);
#endif
    opReg(cUnit, kOpBlx, r_T9);
    newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
    dvmCompilerClobberCallRegs(cUnit);
#ifdef __mips_hard_float
    rlResult = dvmCompilerGetReturnWideAlt(cUnit);
#else
    rlResult = dvmCompilerGetReturnWide(cUnit);
#endif
    storeValueWide(cUnit, rlDest, rlResult);
#if defined(WITH_SELF_VERIFICATION)
    cUnit->usesLinkRegister = true;
#endif
    return false;
}

static bool genConversionPortable(CompilationUnit *cUnit, MIR *mir)
{
    Opcode opcode = mir->dalvikInsn.opcode;

    switch (opcode) {
        case OP_INT_TO_FLOAT:
            return genConversionCall(cUnit, mir, (void*)__floatsisf, kWord, kSingle);
        case OP_FLOAT_TO_INT:
            return genConversionCall(cUnit, mir, (void*)__fixsfsi, kSingle, kWord);
        case OP_DOUBLE_TO_FLOAT:
            return genConversionCall(cUnit, mir, (void*)__truncdfsf2, kDouble, kSingle);
        case OP_FLOAT_TO_DOUBLE:
            return genConversionCall(cUnit, mir, (void*)__extendsfdf2, kSingle, kDouble);
        case OP_INT_TO_DOUBLE:
            return genConversionCall(cUnit, mir, (void*)__floatsidf, kWord, kDouble);
        case OP_DOUBLE_TO_INT:
            return genConversionCall(cUnit, mir, (void*)__fixdfsi, kDouble, kWord);
        case OP_FLOAT_TO_LONG:
            return genConversionCall(cUnit, mir, (void*)__fixsfdi, kSingle, kLong);
        case OP_LONG_TO_FLOAT:
            return genConversionCall(cUnit, mir, (void*)__floatdisf, kLong, kSingle);
        case OP_DOUBLE_TO_LONG:
            return genConversionCall(cUnit, mir, (void*)__fixdfdi, kDouble, kLong);
        case OP_LONG_TO_DOUBLE:
            return genConversionCall(cUnit, mir, (void*)__floatdidf, kLong, kDouble);
        default:
            return true;
    }
    return false;
}

#if defined(WITH_SELF_VERIFICATION)
static void selfVerificationBranchInsert(LIR *currentLIR, Mipsopcode opcode,
                          int dest, int src1)
{
assert(0); /* MIPSTODO port selfVerificationBranchInsert() */
     MipsLIR *insn = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
     insn->opcode = opcode;
     insn->operands[0] = dest;
     insn->operands[1] = src1;
     setupResourceMasks(insn);
     dvmCompilerInsertLIRBefore(currentLIR, (LIR *) insn);
}

/*
 * Example where r14 (LR) is preserved around a heap access under
 * self-verification mode in Thumb2:
 *
 * D/dalvikvm( 1538): 0x59414c5e (0026): ldr     r14, [r15pc, #220] <-hoisted
 * D/dalvikvm( 1538): 0x59414c62 (002a): mla     r4, r0, r8, r4
 * D/dalvikvm( 1538): 0x59414c66 (002e): adds    r3, r4, r3
 * D/dalvikvm( 1538): 0x59414c6a (0032): push    <r5, r14>    ---+
 * D/dalvikvm( 1538): 0x59414c6c (0034): blx_1   0x5940f494      |
 * D/dalvikvm( 1538): 0x59414c6e (0036): blx_2   see above       <-MEM_OP_DECODE
 * D/dalvikvm( 1538): 0x59414c70 (0038): ldr     r10, [r9, #0]   |
 * D/dalvikvm( 1538): 0x59414c74 (003c): pop     <r5, r14>    ---+
 * D/dalvikvm( 1538): 0x59414c78 (0040): mov     r11, r10
 * D/dalvikvm( 1538): 0x59414c7a (0042): asr     r12, r11, #31
 * D/dalvikvm( 1538): 0x59414c7e (0046): movs    r0, r2
 * D/dalvikvm( 1538): 0x59414c80 (0048): movs    r1, r3
 * D/dalvikvm( 1538): 0x59414c82 (004a): str     r2, [r5, #16]
 * D/dalvikvm( 1538): 0x59414c84 (004c): mov     r2, r11
 * D/dalvikvm( 1538): 0x59414c86 (004e): str     r3, [r5, #20]
 * D/dalvikvm( 1538): 0x59414c88 (0050): mov     r3, r12
 * D/dalvikvm( 1538): 0x59414c8a (0052): str     r11, [r5, #24]
 * D/dalvikvm( 1538): 0x59414c8e (0056): str     r12, [r5, #28]
 * D/dalvikvm( 1538): 0x59414c92 (005a): blx     r14             <-use of LR
 *
 */
static void selfVerificationBranchInsertPass(CompilationUnit *cUnit)
{
assert(0); /* MIPSTODO port selfVerificationBranchInsertPass() */
    MipsLIR *thisLIR;
    Templateopcode opcode = TEMPLATE_MEM_OP_DECODE;

    for (thisLIR = (MipsLIR *) cUnit->firstLIRInsn;
         thisLIR != (MipsLIR *) cUnit->lastLIRInsn;
         thisLIR = NEXT_LIR(thisLIR)) {
        if (!thisLIR->flags.isNop && thisLIR->flags.insertWrapper) {
            /*
             * Push r5(FP) and r14(LR) onto stack. We need to make sure that
             * SP is 8-byte aligned, and we use r5 as a temp to restore LR
             * for Thumb-only target since LR cannot be directly accessed in
             * Thumb mode. Another reason to choose r5 here is it is the Dalvik
             * frame pointer and cannot be the target of the emulated heap
             * load.
             */
            if (cUnit->usesLinkRegister) {
                genSelfVerificationPreBranch(cUnit, thisLIR);
            }

            /* Branch to mem op decode template */
            selfVerificationBranchInsert((LIR *) thisLIR, kThumbBlx1,
                       (int) gDvmJit.codeCache + templateEntryOffsets[opcode],
                       (int) gDvmJit.codeCache + templateEntryOffsets[opcode]);
            selfVerificationBranchInsert((LIR *) thisLIR, kThumbBlx2,
                       (int) gDvmJit.codeCache + templateEntryOffsets[opcode],
                       (int) gDvmJit.codeCache + templateEntryOffsets[opcode]);

            /* Restore LR */
            if (cUnit->usesLinkRegister) {
                genSelfVerificationPostBranch(cUnit, thisLIR);
            }
        }
    }
}
#endif

/* Generate conditional branch instructions */
static MipsLIR *genConditionalBranchMips(CompilationUnit *cUnit,
                                    MipsOpCode opc, int rs, int rt,
                                    MipsLIR *target)
{
    MipsLIR *branch = opCompareBranch(cUnit, opc, rs, rt);
    branch->generic.target = (LIR *) target;
    return branch;
}

/* Generate a unconditional branch to go to the interpreter */
static inline MipsLIR *genTrap(CompilationUnit *cUnit, int dOffset,
                                  MipsLIR *pcrLabel)
{
    MipsLIR *branch = opNone(cUnit, kOpUncondBr);
    return genCheckCommon(cUnit, dOffset, branch, pcrLabel);
}

/* Load a wide field from an object instance */
static void genIGetWide(CompilationUnit *cUnit, MIR *mir, int fieldOffset)
{
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    RegLocation rlResult;
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    int regPtr = dvmCompilerAllocTemp(cUnit);

    assert(rlDest.wide);

    genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg, mir->offset,
                 NULL);/* null object? */
    opRegRegImm(cUnit, kOpAdd, regPtr, rlObj.lowReg, fieldOffset);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);

    HEAP_ACCESS_SHADOW(true);
    loadPair(cUnit, regPtr, rlResult.lowReg, rlResult.highReg);
    HEAP_ACCESS_SHADOW(false);

    dvmCompilerFreeTemp(cUnit, regPtr);
    storeValueWide(cUnit, rlDest, rlResult);
}

/* Store a wide field to an object instance */
static void genIPutWide(CompilationUnit *cUnit, MIR *mir, int fieldOffset)
{
    RegLocation rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 2);
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    int regPtr;
    rlSrc = loadValueWide(cUnit, rlSrc, kAnyReg);
    genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg, mir->offset,
                 NULL);/* null object? */
    regPtr = dvmCompilerAllocTemp(cUnit);
    opRegRegImm(cUnit, kOpAdd, regPtr, rlObj.lowReg, fieldOffset);

    HEAP_ACCESS_SHADOW(true);
    storePair(cUnit, regPtr, rlSrc.lowReg, rlSrc.highReg);
    HEAP_ACCESS_SHADOW(false);

    dvmCompilerFreeTemp(cUnit, regPtr);
}

/*
 * Load a field from an object instance
 *
 */
static void genIGet(CompilationUnit *cUnit, MIR *mir, OpSize size,
                    int fieldOffset, bool isVolatile)
{
    RegLocation rlResult;
    RegisterClass regClass = dvmCompilerRegClassBySize(size);
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, regClass, true);
    genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg, mir->offset,
                 NULL);/* null object? */

    HEAP_ACCESS_SHADOW(true);
    loadBaseDisp(cUnit, mir, rlObj.lowReg, fieldOffset, rlResult.lowReg,
                 size, rlObj.sRegLow);
    HEAP_ACCESS_SHADOW(false);
    if (isVolatile) {
        dvmCompilerGenMemBarrier(cUnit, 0);
    }

    storeValue(cUnit, rlDest, rlResult);
}

/*
 * Store a field to an object instance
 *
 */
static void genIPut(CompilationUnit *cUnit, MIR *mir, OpSize size,
                    int fieldOffset, bool isObject, bool isVolatile)
{
    RegisterClass regClass = dvmCompilerRegClassBySize(size);
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 1);
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    rlSrc = loadValue(cUnit, rlSrc, regClass);
    genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg, mir->offset,
                 NULL);/* null object? */

    if (isVolatile) {
        dvmCompilerGenMemBarrier(cUnit, 0);
    }
    HEAP_ACCESS_SHADOW(true);
    storeBaseDisp(cUnit, rlObj.lowReg, fieldOffset, rlSrc.lowReg, size);
    HEAP_ACCESS_SHADOW(false);
    if (isVolatile) {
        dvmCompilerGenMemBarrier(cUnit, 0);
    }
    if (isObject) {
        /* NOTE: marking card based on object head */
        markCard(cUnit, rlSrc.lowReg, rlObj.lowReg);
    }
}


/*
 * Generate array load
 */
static void genArrayGet(CompilationUnit *cUnit, MIR *mir, OpSize size,
                        RegLocation rlArray, RegLocation rlIndex,
                        RegLocation rlDest, int scale)
{
    RegisterClass regClass = dvmCompilerRegClassBySize(size);
    int lenOffset = OFFSETOF_MEMBER(ArrayObject, length);
    int dataOffset = OFFSETOF_MEMBER(ArrayObject, contents);
    RegLocation rlResult;
    rlArray = loadValue(cUnit, rlArray, kCoreReg);
    rlIndex = loadValue(cUnit, rlIndex, kCoreReg);
    int regPtr;

    /* null object? */
    MipsLIR * pcrLabel = NULL;

    if (!(mir->OptimizationFlags & MIR_IGNORE_NULL_CHECK)) {
        pcrLabel = genNullCheck(cUnit, rlArray.sRegLow,
                                rlArray.lowReg, mir->offset, NULL);
    }

    regPtr = dvmCompilerAllocTemp(cUnit);

    assert(IS_SIMM16(dataOffset));
    if (scale) {
        opRegRegImm(cUnit, kOpLsl, regPtr, rlIndex.lowReg, scale);
    }

    if (!(mir->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        int regLen = dvmCompilerAllocTemp(cUnit);
        /* Get len */
        loadWordDisp(cUnit, rlArray.lowReg, lenOffset, regLen);
        genBoundsCheck(cUnit, rlIndex.lowReg, regLen, mir->offset,
                       pcrLabel);
        dvmCompilerFreeTemp(cUnit, regLen);
    }

    if (scale) {
        opRegReg(cUnit, kOpAdd, regPtr, rlArray.lowReg);
    } else {
        opRegRegReg(cUnit, kOpAdd, regPtr, rlArray.lowReg, rlIndex.lowReg);
    }

    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, regClass, true);
    if ((size == kLong) || (size == kDouble)) {
        HEAP_ACCESS_SHADOW(true);
        loadBaseDispWide(cUnit, mir, regPtr, dataOffset, rlResult.lowReg,
                         rlResult.highReg, INVALID_SREG);
        HEAP_ACCESS_SHADOW(false);
        dvmCompilerFreeTemp(cUnit, regPtr);
        storeValueWide(cUnit, rlDest, rlResult);
    } else {
        HEAP_ACCESS_SHADOW(true);
        loadBaseDisp(cUnit, mir, regPtr, dataOffset, rlResult.lowReg,
                     size, INVALID_SREG);
        HEAP_ACCESS_SHADOW(false);
        dvmCompilerFreeTemp(cUnit, regPtr);
        storeValue(cUnit, rlDest, rlResult);
    }
}

/*
 * Generate array store
 *
 */
static void genArrayPut(CompilationUnit *cUnit, MIR *mir, OpSize size,
                        RegLocation rlArray, RegLocation rlIndex,
                        RegLocation rlSrc, int scale)
{
    RegisterClass regClass = dvmCompilerRegClassBySize(size);
    int lenOffset = OFFSETOF_MEMBER(ArrayObject, length);
    int dataOffset = OFFSETOF_MEMBER(ArrayObject, contents);

    int regPtr;
    rlArray = loadValue(cUnit, rlArray, kCoreReg);
    rlIndex = loadValue(cUnit, rlIndex, kCoreReg);

    if (dvmCompilerIsTemp(cUnit, rlArray.lowReg)) {
        dvmCompilerClobber(cUnit, rlArray.lowReg);
        regPtr = rlArray.lowReg;
    } else {
        regPtr = dvmCompilerAllocTemp(cUnit);
        genRegCopy(cUnit, regPtr, rlArray.lowReg);
    }

    /* null object? */
    MipsLIR * pcrLabel = NULL;

    if (!(mir->OptimizationFlags & MIR_IGNORE_NULL_CHECK)) {
        pcrLabel = genNullCheck(cUnit, rlArray.sRegLow, rlArray.lowReg,
                                mir->offset, NULL);
    }

    assert(IS_SIMM16(dataOffset));
    int tReg = dvmCompilerAllocTemp(cUnit);
    if (scale) {
        opRegRegImm(cUnit, kOpLsl, tReg, rlIndex.lowReg, scale);
    }

    if (!(mir->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        int regLen = dvmCompilerAllocTemp(cUnit);
        //NOTE: max live temps(4) here.
        /* Get len */
        loadWordDisp(cUnit, rlArray.lowReg, lenOffset, regLen);
        genBoundsCheck(cUnit, rlIndex.lowReg, regLen, mir->offset,
                       pcrLabel);
        dvmCompilerFreeTemp(cUnit, regLen);
    }

    if (scale) {
        opRegReg(cUnit, kOpAdd, tReg, rlArray.lowReg);
    } else {
        opRegRegReg(cUnit, kOpAdd, tReg, rlArray.lowReg, rlIndex.lowReg);
    }

    /* at this point, tReg points to array, 2 live temps */
    if ((size == kLong) || (size == kDouble)) {
        rlSrc = loadValueWide(cUnit, rlSrc, regClass);
        HEAP_ACCESS_SHADOW(true);
        storeBaseDispWide(cUnit, tReg, dataOffset, rlSrc.lowReg, rlSrc.highReg)
        HEAP_ACCESS_SHADOW(false);
        dvmCompilerFreeTemp(cUnit, tReg);
        dvmCompilerFreeTemp(cUnit, regPtr);
    } else {
        rlSrc = loadValue(cUnit, rlSrc, regClass);
        HEAP_ACCESS_SHADOW(true);
        storeBaseDisp(cUnit, tReg, dataOffset, rlSrc.lowReg, size);
        dvmCompilerFreeTemp(cUnit, tReg);
        HEAP_ACCESS_SHADOW(false);
    }
}

/*
 * Generate array object store
 * Must use explicit register allocation here because of
 * call-out to dvmCanPutArrayElement
 */
static void genArrayObjectPut(CompilationUnit *cUnit, MIR *mir,
                              RegLocation rlArray, RegLocation rlIndex,
                              RegLocation rlSrc, int scale)
{
    int lenOffset = OFFSETOF_MEMBER(ArrayObject, length);
    int dataOffset = OFFSETOF_MEMBER(ArrayObject, contents);

    int regLen = r_A0;
    int regPtr = r_S0;  /* Preserved across call */
    int regArray = r_A1;
    int regIndex = r_S4;  /* Preserved across call */

    dvmCompilerFlushAllRegs(cUnit);
    // moved lock for r_S0 and r_S4 here from below since genBoundsCheck
    // allocates a temporary that can result in clobbering either of them
    dvmCompilerLockTemp(cUnit, regPtr);   // r_S0
    dvmCompilerLockTemp(cUnit, regIndex); // r_S4

    loadValueDirectFixed(cUnit, rlArray, regArray);
    loadValueDirectFixed(cUnit, rlIndex, regIndex);

    /* null object? */
    MipsLIR * pcrLabel = NULL;

    if (!(mir->OptimizationFlags & MIR_IGNORE_NULL_CHECK)) {
        pcrLabel = genNullCheck(cUnit, rlArray.sRegLow, regArray,
                                mir->offset, NULL);
    }

    if (!(mir->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        /* Get len */
        loadWordDisp(cUnit, regArray, lenOffset, regLen);
        /* regPtr -> array data */
        opRegRegImm(cUnit, kOpAdd, regPtr, regArray, dataOffset);
        genBoundsCheck(cUnit, regIndex, regLen, mir->offset,
                       pcrLabel);
    } else {
        /* regPtr -> array data */
        opRegRegImm(cUnit, kOpAdd, regPtr, regArray, dataOffset);
    }

    /* Get object to store */
    loadValueDirectFixed(cUnit, rlSrc, r_A0);
    LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmCanPutArrayElement);

    /* Are we storing null?  If so, avoid check */
    MipsLIR *branchOver = opCompareBranch(cUnit, kMipsBeqz, r_A0, -1);

    /* Make sure the types are compatible */
    loadWordDisp(cUnit, regArray, offsetof(Object, clazz), r_A1);
    loadWordDisp(cUnit, r_A0, offsetof(Object, clazz), r_A0);
    opReg(cUnit, kOpBlx, r_T9);
    newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
    dvmCompilerClobberCallRegs(cUnit);

    /*
     * Using fixed registers here, and counting on r_S0 and r_S4 being
     * preserved across the above call.  Tell the register allocation
     * utilities about the regs we are using directly
     */
    dvmCompilerLockTemp(cUnit, r_A0);
    dvmCompilerLockTemp(cUnit, r_A1);

    /* Bad? - roll back and re-execute if so */
    genRegImmCheck(cUnit, kMipsCondEq, r_V0, 0, mir->offset, pcrLabel);

    /* Resume here - must reload element & array, regPtr & index preserved */
    loadValueDirectFixed(cUnit, rlSrc, r_A0);
    loadValueDirectFixed(cUnit, rlArray, r_A1);

    MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
    target->defMask = ENCODE_ALL;
    branchOver->generic.target = (LIR *) target;

    HEAP_ACCESS_SHADOW(true);
    storeBaseIndexed(cUnit, regPtr, regIndex, r_A0,
                     scale, kWord);
    HEAP_ACCESS_SHADOW(false);

    dvmCompilerFreeTemp(cUnit, regPtr);
    dvmCompilerFreeTemp(cUnit, regIndex);

    /* NOTE: marking card here based on object head */
    markCard(cUnit, r_A0, r_A1);
}

static bool genShiftOpLong(CompilationUnit *cUnit, MIR *mir,
                           RegLocation rlDest, RegLocation rlSrc1,
                           RegLocation rlShift)
{
    /*
     * Don't mess with the regsiters here as there is a particular calling
     * convention to the out-of-line handler.
     */
    RegLocation rlResult;

    loadValueDirectWideFixed(cUnit, rlSrc1, r_ARG0, r_ARG1);
    loadValueDirect(cUnit, rlShift, r_A2);
    switch( mir->dalvikInsn.opcode) {
        case OP_SHL_LONG:
        case OP_SHL_LONG_2ADDR:
            genDispatchToHandler(cUnit, TEMPLATE_SHL_LONG);
            break;
        case OP_SHR_LONG:
        case OP_SHR_LONG_2ADDR:
            genDispatchToHandler(cUnit, TEMPLATE_SHR_LONG);
            break;
        case OP_USHR_LONG:
        case OP_USHR_LONG_2ADDR:
            genDispatchToHandler(cUnit, TEMPLATE_USHR_LONG);
            break;
        default:
            return true;
    }
    rlResult = dvmCompilerGetReturnWide(cUnit);
    storeValueWide(cUnit, rlDest, rlResult);
    return false;
}

static bool genArithOpLong(CompilationUnit *cUnit, MIR *mir,
                           RegLocation rlDest, RegLocation rlSrc1,
                           RegLocation rlSrc2)
{
    RegLocation rlResult;
    OpKind firstOp = kOpBkpt;
    OpKind secondOp = kOpBkpt;
    bool callOut = false;
    bool checkZero = false;
    void *callTgt;

    switch (mir->dalvikInsn.opcode) {
        case OP_NOT_LONG:
            rlSrc2 = loadValueWide(cUnit, rlSrc2, kCoreReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegReg(cUnit, kOpMvn, rlResult.lowReg, rlSrc2.lowReg);
            opRegReg(cUnit, kOpMvn, rlResult.highReg, rlSrc2.highReg);
            storeValueWide(cUnit, rlDest, rlResult);
            return false;
            break;
        case OP_ADD_LONG:
        case OP_ADD_LONG_2ADDR:
            firstOp = kOpAdd;
            secondOp = kOpAdc;
            break;
        case OP_SUB_LONG:
        case OP_SUB_LONG_2ADDR:
            firstOp = kOpSub;
            secondOp = kOpSbc;
            break;
        case OP_MUL_LONG:
        case OP_MUL_LONG_2ADDR:
            genMulLong(cUnit, rlDest, rlSrc1, rlSrc2);
            return false;
        case OP_DIV_LONG:
        case OP_DIV_LONG_2ADDR:
            callOut = true;
            checkZero = true;
            callTgt = (void*)__divdi3;
            break;
        case OP_REM_LONG:
        case OP_REM_LONG_2ADDR:
            callOut = true;
            callTgt = (void*)__moddi3;
            checkZero = true;
            break;
        case OP_AND_LONG_2ADDR:
        case OP_AND_LONG:
            firstOp = kOpAnd;
            secondOp = kOpAnd;
            break;
        case OP_OR_LONG:
        case OP_OR_LONG_2ADDR:
            firstOp = kOpOr;
            secondOp = kOpOr;
            break;
        case OP_XOR_LONG:
        case OP_XOR_LONG_2ADDR:
            firstOp = kOpXor;
            secondOp = kOpXor;
            break;
        case OP_NEG_LONG: {
            int tReg = dvmCompilerAllocTemp(cUnit);
            rlSrc2 = loadValueWide(cUnit, rlSrc2, kCoreReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            newLIR3(cUnit, kMipsSubu, rlResult.lowReg, r_ZERO, rlSrc2.lowReg);
            newLIR3(cUnit, kMipsSubu, tReg, r_ZERO, rlSrc2.highReg);
            newLIR3(cUnit, kMipsSltu, rlResult.highReg, r_ZERO, rlResult.lowReg);
            newLIR3(cUnit, kMipsSubu, rlResult.highReg, tReg, rlResult.highReg);
            dvmCompilerFreeTemp(cUnit, tReg);
            storeValueWide(cUnit, rlDest, rlResult);
            return false;
            break;
        }
        default:
            ALOGE("Invalid long arith op");
            dvmCompilerAbort(cUnit);
    }
    if (!callOut) {
        genLong3Addr(cUnit, mir, firstOp, secondOp, rlDest, rlSrc1, rlSrc2);
    } else {
        dvmCompilerFlushAllRegs(cUnit);   /* Send everything to home location */
        loadValueDirectWideFixed(cUnit, rlSrc2, r_ARG2, r_ARG3);
        loadValueDirectWideFixed(cUnit, rlSrc1, r_ARG0, r_ARG1);
        LOAD_FUNC_ADDR(cUnit, r_T9, (int) callTgt);
        if (checkZero) {
            int tReg = r_T1; // Using fixed registers during call sequence
            opRegRegReg(cUnit, kOpOr, tReg, r_ARG2, r_ARG3);
            genRegImmCheck(cUnit, kMipsCondEq, tReg, 0, mir->offset, NULL);
        }
        opReg(cUnit, kOpBlx, r_T9);
        newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
        dvmCompilerClobberCallRegs(cUnit);
        rlResult = dvmCompilerGetReturnWide(cUnit);
        storeValueWide(cUnit, rlDest, rlResult);
#if defined(WITH_SELF_VERIFICATION)
        cUnit->usesLinkRegister = true;
#endif
    }
    return false;
}

static bool genArithOpInt(CompilationUnit *cUnit, MIR *mir,
                          RegLocation rlDest, RegLocation rlSrc1,
                          RegLocation rlSrc2)
{
    OpKind op = kOpBkpt;
    bool checkZero = false;
    bool unary = false;
    RegLocation rlResult;
    bool shiftOp = false;
    int isDivRem = false;
    MipsOpCode opc;
    int divReg;

    switch (mir->dalvikInsn.opcode) {
        case OP_NEG_INT:
            op = kOpNeg;
            unary = true;
            break;
        case OP_NOT_INT:
            op = kOpMvn;
            unary = true;
            break;
        case OP_ADD_INT:
        case OP_ADD_INT_2ADDR:
            op = kOpAdd;
            break;
        case OP_SUB_INT:
        case OP_SUB_INT_2ADDR:
            op = kOpSub;
            break;
        case OP_MUL_INT:
        case OP_MUL_INT_2ADDR:
            op = kOpMul;
            break;
        case OP_DIV_INT:
        case OP_DIV_INT_2ADDR:
            isDivRem = true;
            checkZero = true;
            opc = kMipsMflo;
            divReg = r_LO;
            break;
        case OP_REM_INT:
        case OP_REM_INT_2ADDR:
            isDivRem = true;
            checkZero = true;
            opc = kMipsMfhi;
            divReg = r_HI;
            break;
        case OP_AND_INT:
        case OP_AND_INT_2ADDR:
            op = kOpAnd;
            break;
        case OP_OR_INT:
        case OP_OR_INT_2ADDR:
            op = kOpOr;
            break;
        case OP_XOR_INT:
        case OP_XOR_INT_2ADDR:
            op = kOpXor;
            break;
        case OP_SHL_INT:
        case OP_SHL_INT_2ADDR:
            shiftOp = true;
            op = kOpLsl;
            break;
        case OP_SHR_INT:
        case OP_SHR_INT_2ADDR:
            shiftOp = true;
            op = kOpAsr;
            break;
        case OP_USHR_INT:
        case OP_USHR_INT_2ADDR:
            shiftOp = true;
            op = kOpLsr;
            break;
        default:
            ALOGE("Invalid word arith op: %#x(%d)",
                 mir->dalvikInsn.opcode, mir->dalvikInsn.opcode);
            dvmCompilerAbort(cUnit);
    }

    rlSrc1 = loadValue(cUnit, rlSrc1, kCoreReg);
    if (unary) {
        rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
        opRegReg(cUnit, op, rlResult.lowReg,
                 rlSrc1.lowReg);
    } else if (isDivRem) {
        rlSrc2 = loadValue(cUnit, rlSrc2, kCoreReg);
        if (checkZero) {
            genNullCheck(cUnit, rlSrc2.sRegLow, rlSrc2.lowReg, mir->offset, NULL);
        }
        newLIR4(cUnit, kMipsDiv, r_HI, r_LO, rlSrc1.lowReg, rlSrc2.lowReg);
        rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
        newLIR2(cUnit, opc, rlResult.lowReg, divReg);
    } else {
        rlSrc2 = loadValue(cUnit, rlSrc2, kCoreReg);
        if (shiftOp) {
            int tReg = dvmCompilerAllocTemp(cUnit);
            opRegRegImm(cUnit, kOpAnd, tReg, rlSrc2.lowReg, 31);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegRegReg(cUnit, op, rlResult.lowReg,
                        rlSrc1.lowReg, tReg);
            dvmCompilerFreeTemp(cUnit, tReg);
        } else {
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegRegReg(cUnit, op, rlResult.lowReg,
                        rlSrc1.lowReg, rlSrc2.lowReg);
        }
    }
    storeValue(cUnit, rlDest, rlResult);

    return false;
}

static bool genArithOp(CompilationUnit *cUnit, MIR *mir)
{
    Opcode opcode = mir->dalvikInsn.opcode;
    RegLocation rlDest;
    RegLocation rlSrc1;
    RegLocation rlSrc2;
    /* Deduce sizes of operands */
    if (mir->ssaRep->numUses == 2) {
        rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 0);
        rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 1);
    } else if (mir->ssaRep->numUses == 3) {
        rlSrc1 = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
        rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 2);
    } else {
        rlSrc1 = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
        rlSrc2 = dvmCompilerGetSrcWide(cUnit, mir, 2, 3);
        assert(mir->ssaRep->numUses == 4);
    }
    if (mir->ssaRep->numDefs == 1) {
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);
    } else {
        assert(mir->ssaRep->numDefs == 2);
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    }

    if ((opcode >= OP_ADD_LONG_2ADDR) && (opcode <= OP_XOR_LONG_2ADDR)) {
        return genArithOpLong(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opcode >= OP_ADD_LONG) && (opcode <= OP_XOR_LONG)) {
        return genArithOpLong(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opcode >= OP_SHL_LONG_2ADDR) && (opcode <= OP_USHR_LONG_2ADDR)) {
        return genShiftOpLong(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opcode >= OP_SHL_LONG) && (opcode <= OP_USHR_LONG)) {
        return genShiftOpLong(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opcode >= OP_ADD_INT_2ADDR) && (opcode <= OP_USHR_INT_2ADDR)) {
        return genArithOpInt(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opcode >= OP_ADD_INT) && (opcode <= OP_USHR_INT)) {
        return genArithOpInt(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opcode >= OP_ADD_FLOAT_2ADDR) && (opcode <= OP_REM_FLOAT_2ADDR)) {
        return genArithOpFloat(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opcode >= OP_ADD_FLOAT) && (opcode <= OP_REM_FLOAT)) {
        return genArithOpFloat(cUnit, mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opcode >= OP_ADD_DOUBLE_2ADDR) && (opcode <= OP_REM_DOUBLE_2ADDR)) {
        return genArithOpDouble(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opcode >= OP_ADD_DOUBLE) && (opcode <= OP_REM_DOUBLE)) {
        return genArithOpDouble(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    return true;
}

/* Generate unconditional branch instructions */
static MipsLIR *genUnconditionalBranch(CompilationUnit *cUnit, MipsLIR *target)
{
    MipsLIR *branch = opNone(cUnit, kOpUncondBr);
    branch->generic.target = (LIR *) target;
    return branch;
}

/* Perform the actual operation for OP_RETURN_* */
void genReturnCommon(CompilationUnit *cUnit, MIR *mir)
{
    genDispatchToHandler(cUnit, gDvmJit.methodTraceSupport ?
                         TEMPLATE_RETURN_PROF : TEMPLATE_RETURN);
#if defined(WITH_JIT_TUNING)
    gDvmJit.returnOp++;
#endif
    int dPC = (int) (cUnit->method->insns + mir->offset);
    /* Insert branch, but defer setting of target */
    MipsLIR *branch = genUnconditionalBranch(cUnit, NULL);
    /* Set up the place holder to reconstruct this Dalvik PC */
    MipsLIR *pcrLabel = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
    pcrLabel->opcode = kMipsPseudoPCReconstructionCell;
    pcrLabel->operands[0] = dPC;
    pcrLabel->operands[1] = mir->offset;
    /* Insert the place holder to the growable list */
    dvmInsertGrowableList(&cUnit->pcReconstructionList, (intptr_t) pcrLabel);
    /* Branch to the PC reconstruction code */
    branch->generic.target = (LIR *) pcrLabel;
}

static void genProcessArgsNoRange(CompilationUnit *cUnit, MIR *mir,
                                  DecodedInstruction *dInsn,
                                  MipsLIR **pcrLabel)
{
    unsigned int i;
    unsigned int regMask = 0;
    RegLocation rlArg;
    int numDone = 0;

    /*
     * Load arguments to r_A0..r_T0.  Note that these registers may contain
     * live values, so we clobber them immediately after loading to prevent
     * them from being used as sources for subsequent loads.
     */
    dvmCompilerLockAllTemps(cUnit);
    for (i = 0; i < dInsn->vA; i++) {
        regMask |= 1 << i;
        rlArg = dvmCompilerGetSrc(cUnit, mir, numDone++);
        loadValueDirectFixed(cUnit, rlArg, i+r_A0); /* r_A0 thru r_T0 */
    }
    if (regMask) {
        /* Up to 5 args are pushed on top of FP - sizeofStackSaveArea */
        opRegRegImm(cUnit, kOpSub, r_S4, rFP,
                    sizeof(StackSaveArea) + (dInsn->vA << 2));
        /* generate null check */
        if (pcrLabel) {
            *pcrLabel = genNullCheck(cUnit, dvmCompilerSSASrc(mir, 0), r_A0,
                                     mir->offset, NULL);
        }
        storeMultiple(cUnit, r_S4, regMask);
    }
}

static void genProcessArgsRange(CompilationUnit *cUnit, MIR *mir,
                                DecodedInstruction *dInsn,
                                MipsLIR **pcrLabel)
{
    int srcOffset = dInsn->vC << 2;
    int numArgs = dInsn->vA;
    int regMask;

    /*
     * Note: here, all promoted registers will have been flushed
     * back to the Dalvik base locations, so register usage restrictins
     * are lifted.  All parms loaded from original Dalvik register
     * region - even though some might conceivably have valid copies
     * cached in a preserved register.
     */
    dvmCompilerLockAllTemps(cUnit);

    /*
     * r4PC     : &rFP[vC]
     * r_S4: &newFP[0]
     */
    opRegRegImm(cUnit, kOpAdd, r4PC, rFP, srcOffset);
    /* load [r_A0 up to r_A3)] */
    regMask = (1 << ((numArgs < 4) ? numArgs : 4)) - 1;
    /*
     * Protect the loadMultiple instruction from being reordered with other
     * Dalvik stack accesses.
     */
    if (numArgs != 0) loadMultiple(cUnit, r4PC, regMask);

    opRegRegImm(cUnit, kOpSub, r_S4, rFP,
                sizeof(StackSaveArea) + (numArgs << 2));
    /* generate null check */
    if (pcrLabel) {
        *pcrLabel = genNullCheck(cUnit, dvmCompilerSSASrc(mir, 0), r_A0,
                                 mir->offset, NULL);
    }

    /*
     * Handle remaining 4n arguments:
     * store previously loaded 4 values and load the next 4 values
     */
    if (numArgs >= 8) {
        MipsLIR *loopLabel = NULL;
        /*
         * r_A0 contains "this" and it will be used later, so push it to the stack
         * first. Pushing r_S1 (rFP) is just for stack alignment purposes.
         */

        newLIR2(cUnit, kMipsMove, r_T0, r_A0);
        newLIR2(cUnit, kMipsMove, r_T1, r_S1);

        /* No need to generate the loop structure if numArgs <= 11 */
        if (numArgs > 11) {
            loadConstant(cUnit, rFP, ((numArgs - 4) >> 2) << 2);
            loopLabel = newLIR0(cUnit, kMipsPseudoTargetLabel);
            loopLabel->defMask = ENCODE_ALL;
        }
        storeMultiple(cUnit, r_S4, regMask);
        /*
         * Protect the loadMultiple instruction from being reordered with other
         * Dalvik stack accesses.
         */
        loadMultiple(cUnit, r4PC, regMask);
        /* No need to generate the loop structure if numArgs <= 11 */
        if (numArgs > 11) {
            opRegImm(cUnit, kOpSub, rFP, 4);
            genConditionalBranchMips(cUnit, kMipsBne, rFP, r_ZERO, loopLabel);
        }
    }

    /* Save the last batch of loaded values */
    if (numArgs != 0) storeMultiple(cUnit, r_S4, regMask);

    /* Generate the loop epilogue - don't use r_A0 */
    if ((numArgs > 4) && (numArgs % 4)) {
        regMask = ((1 << (numArgs & 0x3)) - 1) << 1;
        /*
         * Protect the loadMultiple instruction from being reordered with other
         * Dalvik stack accesses.
         */
        loadMultiple(cUnit, r4PC, regMask);
    }
    if (numArgs >= 8) {
        newLIR2(cUnit, kMipsMove, r_A0, r_T0);
        newLIR2(cUnit, kMipsMove, r_S1, r_T1);
    }

    /* Save the modulo 4 arguments */
    if ((numArgs > 4) && (numArgs % 4)) {
        storeMultiple(cUnit, r_S4, regMask);
    }
}

/*
 * Generate code to setup the call stack then jump to the chaining cell if it
 * is not a native method.
 */
static void genInvokeSingletonCommon(CompilationUnit *cUnit, MIR *mir,
                                     BasicBlock *bb, MipsLIR *labelList,
                                     MipsLIR *pcrLabel,
                                     const Method *calleeMethod)
{
    /*
     * Note: all Dalvik register state should be flushed to
     * memory by the point, so register usage restrictions no
     * longer apply.  All temp & preserved registers may be used.
     */
    dvmCompilerLockAllTemps(cUnit);
    MipsLIR *retChainingCell = &labelList[bb->fallThrough->id];

    /* r_A1 = &retChainingCell */
    dvmCompilerLockTemp(cUnit, r_A1);
    MipsLIR *addrRetChain = newLIR2(cUnit, kMipsLahi, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;
    addrRetChain = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;

    /* r4PC = dalvikCallsite */
    loadConstant(cUnit, r4PC,
                 (int) (cUnit->method->insns + mir->offset));
    /*
     * r_A0 = calleeMethod (loaded upon calling genInvokeSingletonCommon)
     * r_A1 = &ChainingCell
     * r4PC = callsiteDPC
     */
    if (dvmIsNativeMethod(calleeMethod)) {
        genDispatchToHandler(cUnit, gDvmJit.methodTraceSupport ?
            TEMPLATE_INVOKE_METHOD_NATIVE_PROF :
            TEMPLATE_INVOKE_METHOD_NATIVE);
#if defined(WITH_JIT_TUNING)
        gDvmJit.invokeNative++;
#endif
    } else {
        genDispatchToHandler(cUnit, gDvmJit.methodTraceSupport ?
            TEMPLATE_INVOKE_METHOD_CHAIN_PROF :
            TEMPLATE_INVOKE_METHOD_CHAIN);
#if defined(WITH_JIT_TUNING)
        gDvmJit.invokeMonomorphic++;
#endif
        /* Branch to the chaining cell */
        genUnconditionalBranch(cUnit, &labelList[bb->taken->id]);
    }
    /* Handle exceptions using the interpreter */
    genTrap(cUnit, mir->offset, pcrLabel);
}

/*
 * Generate code to check the validity of a predicted chain and take actions
 * based on the result.
 *
 * 0x2f1304c4 :  lui      s0,0x2d22(11554)            # s0 <- dalvikPC
 * 0x2f1304c8 :  ori      s0,s0,0x2d22848c(757236876)
 * 0x2f1304cc :  lahi/lui a1,0x2f13(12051)            # a1 <- &retChainingCell
 * 0x2f1304d0 :  lalo/ori a1,a1,0x2f13055c(789775708)
 * 0x2f1304d4 :  lahi/lui a2,0x2f13(12051)            # a2 <- &predictedChainingCell
 * 0x2f1304d8 :  lalo/ori a2,a2,0x2f13056c(789775724)
 * 0x2f1304dc :  jal      0x2f12d1ec(789762540)       # call TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN
 * 0x2f1304e0 :  nop
 * 0x2f1304e4 :  b        0x2f13056c (L0x11ec10)      # off to the predicted chain
 * 0x2f1304e8 :  nop
 * 0x2f1304ec :  b        0x2f13054c (L0x11fc80)      # punt to the interpreter
 * 0x2f1304f0 :  lui      a0,0x2d22(11554)
 * 0x2f1304f4 :  lw       a0,156(s4)                  # a0 <- this->class->vtable[methodIdx]
 * 0x2f1304f8 :  bgtz     a1,0x2f13051c (L0x11fa40)   # if >0 don't rechain
 * 0x2f1304fc :  nop
 * 0x2f130500 :  lui      t9,0x2aba(10938)
 * 0x2f130504 :  ori      t9,t9,0x2abae3f8(716891128)
 * 0x2f130508 :  move     a1,s2
 * 0x2f13050c :  jalr     ra,t9                       # call dvmJitToPatchPredictedChain
 * 0x2f130510 :  nop
 * 0x2f130514 :  lw       gp,84(sp)
 * 0x2f130518 :  move     a0,v0
 * 0x2f13051c :  lahi/lui a1,0x2f13(12051)            # a1 <- &retChainingCell
 * 0x2f130520 :  lalo/ori a1,a1,0x2f13055c(789775708)
 * 0x2f130524 :  jal      0x2f12d0c4(789762244)       # call TEMPLATE_INVOKE_METHOD_NO_OPT
 * 0x2f130528 :  nop
 */
static void genInvokeVirtualCommon(CompilationUnit *cUnit, MIR *mir,
                                   int methodIndex,
                                   MipsLIR *retChainingCell,
                                   MipsLIR *predChainingCell,
                                   MipsLIR *pcrLabel)
{
    /*
     * Note: all Dalvik register state should be flushed to
     * memory by the point, so register usage restrictions no
     * longer apply.  Lock temps to prevent them from being
     * allocated by utility routines.
     */
    dvmCompilerLockAllTemps(cUnit);

    /*
     * For verbose printing, store the method pointer in operands[1] first as
     * operands[0] will be clobbered in dvmCompilerMIR2LIR.
     */
    predChainingCell->operands[1] = (int) mir->meta.callsiteInfo->method;

    /* "this" is already left in r_A0 by genProcessArgs* */

    /* r4PC = dalvikCallsite */
    loadConstant(cUnit, r4PC,
                 (int) (cUnit->method->insns + mir->offset));

    /* r_A1 = &retChainingCell */
    MipsLIR *addrRetChain = newLIR2(cUnit, kMipsLahi, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;
    addrRetChain = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;

    /* r_A2 = &predictedChainingCell */
    MipsLIR *predictedChainingCell = newLIR2(cUnit, kMipsLahi, r_A2, 0);
    predictedChainingCell->generic.target = (LIR *) predChainingCell;
    predictedChainingCell = newLIR3(cUnit, kMipsLalo, r_A2, r_A2, 0);
    predictedChainingCell->generic.target = (LIR *) predChainingCell;

    genDispatchToHandler(cUnit, gDvmJit.methodTraceSupport ?
        TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN_PROF :
        TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN);

    /* return through ra - jump to the chaining cell */
    genUnconditionalBranch(cUnit, predChainingCell);

    /*
     * null-check on "this" may have been eliminated, but we still need a PC-
     * reconstruction label for stack overflow bailout.
     */
    if (pcrLabel == NULL) {
        int dPC = (int) (cUnit->method->insns + mir->offset);
        pcrLabel = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
        pcrLabel->opcode = kMipsPseudoPCReconstructionCell;
        pcrLabel->operands[0] = dPC;
        pcrLabel->operands[1] = mir->offset;
        /* Insert the place holder to the growable list */
        dvmInsertGrowableList(&cUnit->pcReconstructionList,
                              (intptr_t) pcrLabel);
    }

    /* return through ra+8 - punt to the interpreter */
    genUnconditionalBranch(cUnit, pcrLabel);

    /*
     * return through ra+16 - fully resolve the callee method.
     * r_A1 <- count
     * r_A2 <- &predictedChainCell
     * r_A3 <- this->class
     * r4 <- dPC
     * r_S4 <- this->class->vtable
     */

    /* r_A0 <- calleeMethod */
    loadWordDisp(cUnit, r_S4, methodIndex * 4, r_A0);

    /* Check if rechain limit is reached */
    MipsLIR *bypassRechaining = opCompareBranch(cUnit, kMipsBgtz, r_A1, -1);

    LOAD_FUNC_ADDR(cUnit, r_T9, (int) dvmJitToPatchPredictedChain);

    genRegCopy(cUnit, r_A1, rSELF);

    /*
     * r_A0 = calleeMethod
     * r_A2 = &predictedChainingCell
     * r_A3 = class
     *
     * &returnChainingCell has been loaded into r_A1 but is not needed
     * when patching the chaining cell and will be clobbered upon
     * returning so it will be reconstructed again.
     */
    opReg(cUnit, kOpBlx, r_T9);
    newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
    newLIR2(cUnit, kMipsMove, r_A0, r_V0);

    /* r_A1 = &retChainingCell */
    addrRetChain = newLIR2(cUnit, kMipsLahi, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;
    bypassRechaining->generic.target = (LIR *) addrRetChain;
    addrRetChain = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;

    /*
     * r_A0 = calleeMethod,
     * r_A1 = &ChainingCell,
     * r4PC = callsiteDPC,
     */
    genDispatchToHandler(cUnit, gDvmJit.methodTraceSupport ?
        TEMPLATE_INVOKE_METHOD_NO_OPT_PROF :
        TEMPLATE_INVOKE_METHOD_NO_OPT);
#if defined(WITH_JIT_TUNING)
    gDvmJit.invokePolymorphic++;
#endif
    /* Handle exceptions using the interpreter */
    genTrap(cUnit, mir->offset, pcrLabel);
}

/* "this" pointer is already in r0 */
static void genInvokeVirtualWholeMethod(CompilationUnit *cUnit,
                                        MIR *mir,
                                        void *calleeAddr,
                                        MipsLIR *retChainingCell)
{
    CallsiteInfo *callsiteInfo = mir->meta.callsiteInfo;
    dvmCompilerLockAllTemps(cUnit);

    loadClassPointer(cUnit, r_A1, (int) callsiteInfo);

    loadWordDisp(cUnit, r_A0, offsetof(Object, clazz), r_A2);
    /*
     * Set the misPredBranchOver target so that it will be generated when the
     * code for the non-optimized invoke is generated.
     */
    /* Branch to the slow path if classes are not equal */
    MipsLIR *classCheck = opCompareBranch(cUnit, kMipsBne, r_A1, r_A2);

    /* a0 = the Dalvik PC of the callsite */
    loadConstant(cUnit, r_A0, (int) (cUnit->method->insns + mir->offset));

    newLIR1(cUnit, kMipsJal, (int) calleeAddr);
    genUnconditionalBranch(cUnit, retChainingCell);

    /* Target of slow path */
    MipsLIR *slowPathLabel = newLIR0(cUnit, kMipsPseudoTargetLabel);

    slowPathLabel->defMask = ENCODE_ALL;
    classCheck->generic.target = (LIR *) slowPathLabel;

    // FIXME
    cUnit->printMe = true;
}

static void genInvokeSingletonWholeMethod(CompilationUnit *cUnit,
                                          MIR *mir,
                                          void *calleeAddr,
                                          MipsLIR *retChainingCell)
{
    /* a0 = the Dalvik PC of the callsite */
    loadConstant(cUnit, r_A0, (int) (cUnit->method->insns + mir->offset));

    newLIR1(cUnit, kMipsJal, (int) calleeAddr);
    genUnconditionalBranch(cUnit, retChainingCell);

    // FIXME
    cUnit->printMe = true;
}

/* Geneate a branch to go back to the interpreter */
static void genPuntToInterp(CompilationUnit *cUnit, unsigned int offset)
{
    /* a0 = dalvik pc */
    dvmCompilerFlushAllRegs(cUnit);
    loadConstant(cUnit, r_A0, (int) (cUnit->method->insns + offset));
#if 0 /* MIPSTODO tempoary workaround unaligned access on sigma hardware
             this can removed when we're not punting to genInterpSingleStep
             for opcodes that haven't been activated yet */
    loadWordDisp(cUnit, r_A0, offsetof(Object, clazz), r_A3);
#endif
    loadWordDisp(cUnit, rSELF, offsetof(Thread,
                 jitToInterpEntries.dvmJitToInterpPunt), r_A1);

    opReg(cUnit, kOpBlx, r_A1);
}

/*
 * Attempt to single step one instruction using the interpreter and return
 * to the compiled code for the next Dalvik instruction
 */
static void genInterpSingleStep(CompilationUnit *cUnit, MIR *mir)
{
    int flags = dexGetFlagsFromOpcode(mir->dalvikInsn.opcode);
    int flagsToCheck = kInstrCanBranch | kInstrCanSwitch | kInstrCanReturn;

    // Single stepping is considered loop mode breaker
    if (cUnit->jitMode == kJitLoop) {
        cUnit->quitLoopMode = true;
        return;
    }

    //If already optimized out, just ignore
    if (mir->dalvikInsn.opcode == OP_NOP)
        return;

    //Ugly, but necessary.  Flush all Dalvik regs so Interp can find them
    dvmCompilerFlushAllRegs(cUnit);

    if ((mir->next == NULL) || (flags & flagsToCheck)) {
       genPuntToInterp(cUnit, mir->offset);
       return;
    }
    int entryAddr = offsetof(Thread,
                             jitToInterpEntries.dvmJitToInterpSingleStep);
    loadWordDisp(cUnit, rSELF, entryAddr, r_A2);
    /* a0 = dalvik pc */
    loadConstant(cUnit, r_A0, (int) (cUnit->method->insns + mir->offset));
    /* a1 = dalvik pc of following instruction */
    loadConstant(cUnit, r_A1, (int) (cUnit->method->insns + mir->next->offset));
    opReg(cUnit, kOpBlx, r_A2);
}

/*
 * To prevent a thread in a monitor wait from blocking the Jit from
 * resetting the code cache, heavyweight monitor lock will not
 * be allowed to return to an existing translation.  Instead, we will
 * handle them by branching to a handler, which will in turn call the
 * runtime lock routine and then branch directly back to the
 * interpreter main loop.  Given the high cost of the heavyweight
 * lock operation, this additional cost should be slight (especially when
 * considering that we expect the vast majority of lock operations to
 * use the fast-path thin lock bypass).
 */
static void genMonitorPortable(CompilationUnit *cUnit, MIR *mir)
{
    bool isEnter = (mir->dalvikInsn.opcode == OP_MONITOR_ENTER);
    genExportPC(cUnit, mir);
    dvmCompilerFlushAllRegs(cUnit);   /* Send everything to home location */
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    loadValueDirectFixed(cUnit, rlSrc, r_A1);
    genRegCopy(cUnit, r_A0, rSELF);
    genNullCheck(cUnit, rlSrc.sRegLow, r_A1, mir->offset, NULL);
    if (isEnter) {
        /* Get dPC of next insn */
        loadConstant(cUnit, r4PC, (int)(cUnit->method->insns + mir->offset +
                 dexGetWidthFromOpcode(OP_MONITOR_ENTER)));
        genDispatchToHandler(cUnit, TEMPLATE_MONITOR_ENTER);
    } else {
        LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmUnlockObject);
        /* Do the call */
        opReg(cUnit, kOpBlx, r_T9);
        newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
        /* Did we throw? */
        MipsLIR *branchOver = opCompareBranch(cUnit, kMipsBne, r_V0, r_ZERO);
        loadConstant(cUnit, r_A0,
                     (int) (cUnit->method->insns + mir->offset +
                     dexGetWidthFromOpcode(OP_MONITOR_EXIT)));
        genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
        MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
        target->defMask = ENCODE_ALL;
        branchOver->generic.target = (LIR *) target;
        dvmCompilerClobberCallRegs(cUnit);
    }
}
/*#endif*/

/*
 * Fetch *self->info.breakFlags. If the breakFlags are non-zero,
 * punt to the interpreter.
 */
static void genSuspendPoll(CompilationUnit *cUnit, MIR *mir)
{
    int rTemp = dvmCompilerAllocTemp(cUnit);
    MipsLIR *ld;
    ld = loadBaseDisp(cUnit, NULL, rSELF,
                      offsetof(Thread, interpBreak.ctl.breakFlags),
                      rTemp, kUnsignedByte, INVALID_SREG);
    setMemRefType(ld, true /* isLoad */, kMustNotAlias);
    genRegImmCheck(cUnit, kMipsCondNe, rTemp, 0, mir->offset, NULL);
}

/*
 * The following are the first-level codegen routines that analyze the format
 * of each bytecode then either dispatch special purpose codegen routines
 * or produce corresponding Thumb instructions directly.
 */

static bool handleFmt10t_Fmt20t_Fmt30t(CompilationUnit *cUnit, MIR *mir,
                                       BasicBlock *bb, MipsLIR *labelList)
{
    /* backward branch? */
    bool backwardBranch = (bb->taken->startOffset <= mir->offset);

    if (backwardBranch &&
        (gDvmJit.genSuspendPoll || cUnit->jitMode == kJitLoop)) {
        genSuspendPoll(cUnit, mir);
    }

    int numPredecessors = dvmCountSetBits(bb->taken->predecessors);
    /*
     * Things could be hoisted out of the taken block into the predecessor, so
     * make sure it is dominated by the predecessor.
     */
    if (numPredecessors == 1 && bb->taken->visited == false &&
        bb->taken->blockType == kDalvikByteCode) {
        cUnit->nextCodegenBlock = bb->taken;
    } else {
        /* For OP_GOTO, OP_GOTO_16, and OP_GOTO_32 */
        genUnconditionalBranch(cUnit, &labelList[bb->taken->id]);
    }
    return false;
}

static bool handleFmt10x(CompilationUnit *cUnit, MIR *mir)
{
    Opcode dalvikOpcode = mir->dalvikInsn.opcode;
    if ((dalvikOpcode >= OP_UNUSED_3E) && (dalvikOpcode <= OP_UNUSED_43)) {
        ALOGE("Codegen: got unused opcode %#x",dalvikOpcode);
        return true;
    }
    switch (dalvikOpcode) {
        case OP_RETURN_VOID_BARRIER:
            dvmCompilerGenMemBarrier(cUnit, 0);
            // Intentional fallthrough
        case OP_RETURN_VOID:
            genReturnCommon(cUnit,mir);
            break;
        case OP_UNUSED_73:
        case OP_UNUSED_79:
        case OP_UNUSED_7A:
        case OP_UNUSED_FF:
            ALOGE("Codegen: got unused opcode %#x",dalvikOpcode);
            return true;
        case OP_NOP:
            break;
        default:
            return true;
    }
    return false;
}

static bool handleFmt11n_Fmt31i(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlDest;
    RegLocation rlResult;
    if (mir->ssaRep->numDefs == 2) {
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    } else {
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);
    }

    switch (mir->dalvikInsn.opcode) {
        case OP_CONST:
        case OP_CONST_4: {
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);
            loadConstantNoClobber(cUnit, rlResult.lowReg, mir->dalvikInsn.vB);
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_CONST_WIDE_32: {
            //TUNING: single routine to load constant pair for support doubles
            //TUNING: load 0/-1 separately to avoid load dependency
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            loadConstantNoClobber(cUnit, rlResult.lowReg, mir->dalvikInsn.vB);
            opRegRegImm(cUnit, kOpAsr, rlResult.highReg,
                        rlResult.lowReg, 31);
            storeValueWide(cUnit, rlDest, rlResult);
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt21h(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlDest;
    RegLocation rlResult;
    if (mir->ssaRep->numDefs == 2) {
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    } else {
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);
    }
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);

    switch (mir->dalvikInsn.opcode) {
        case OP_CONST_HIGH16: {
            loadConstantNoClobber(cUnit, rlResult.lowReg,
                                  mir->dalvikInsn.vB << 16);
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_CONST_WIDE_HIGH16: {
            loadConstantValueWide(cUnit, rlResult.lowReg, rlResult.highReg,
                                  0, mir->dalvikInsn.vB << 16);
            storeValueWide(cUnit, rlDest, rlResult);
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt20bc(CompilationUnit *cUnit, MIR *mir)
{
    /* For OP_THROW_VERIFICATION_ERROR */
    genInterpSingleStep(cUnit, mir);
    return false;
}

static bool handleFmt21c_Fmt31c(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlResult;
    RegLocation rlDest;
    RegLocation rlSrc;

    switch (mir->dalvikInsn.opcode) {
        case OP_CONST_STRING_JUMBO:
        case OP_CONST_STRING: {
            void *strPtr = (void*)
              (cUnit->method->clazz->pDvmDex->pResStrings[mir->dalvikInsn.vB]);

            if (strPtr == NULL) {
                BAIL_LOOP_COMPILATION();
                ALOGE("Unexpected null string");
                dvmAbort();
            }

            rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            loadConstantNoClobber(cUnit, rlResult.lowReg, (int) strPtr );
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_CONST_CLASS: {
            void *classPtr = (void*)
              (cUnit->method->clazz->pDvmDex->pResClasses[mir->dalvikInsn.vB]);

            if (classPtr == NULL) {
                BAIL_LOOP_COMPILATION();
                ALOGE("Unexpected null class");
                dvmAbort();
            }

            rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            loadConstantNoClobber(cUnit, rlResult.lowReg, (int) classPtr );
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_SGET:
        case OP_SGET_VOLATILE:
        case OP_SGET_OBJECT:
        case OP_SGET_OBJECT_VOLATILE:
        case OP_SGET_BOOLEAN:
        case OP_SGET_CHAR:
        case OP_SGET_BYTE:
        case OP_SGET_SHORT: {
            int valOffset = OFFSETOF_MEMBER(StaticField, value);
            int tReg = dvmCompilerAllocTemp(cUnit);
            bool isVolatile;
            const Method *method = (mir->OptimizationFlags & MIR_CALLEE) ?
                mir->meta.calleeMethod : cUnit->method;
            void *fieldPtr = (void*)
              (method->clazz->pDvmDex->pResFields[mir->dalvikInsn.vB]);

            if (fieldPtr == NULL) {
                BAIL_LOOP_COMPILATION();
                ALOGE("Unexpected null static field");
                dvmAbort();
            }

            /*
             * On SMP systems, Dalvik opcodes found to be referencing
             * volatile fields are rewritten to their _VOLATILE variant.
             * However, this does not happen on non-SMP systems. The JIT
             * still needs to know about volatility to avoid unsafe
             * optimizations so we determine volatility based on either
             * the opcode or the field access flags.
             */
#if ANDROID_SMP != 0
            Opcode opcode = mir->dalvikInsn.opcode;
            isVolatile = (opcode == OP_SGET_VOLATILE) ||
                         (opcode == OP_SGET_OBJECT_VOLATILE);
            assert(isVolatile == dvmIsVolatileField((Field *) fieldPtr));
#else
            isVolatile = dvmIsVolatileField((Field *) fieldPtr);
#endif

            rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);
            loadConstant(cUnit, tReg,  (int) fieldPtr + valOffset);

            if (isVolatile) {
                dvmCompilerGenMemBarrier(cUnit, 0);
            }
            HEAP_ACCESS_SHADOW(true);
            loadWordDisp(cUnit, tReg, 0, rlResult.lowReg);
            HEAP_ACCESS_SHADOW(false);

            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_SGET_WIDE: {
            int valOffset = OFFSETOF_MEMBER(StaticField, value);
            const Method *method = (mir->OptimizationFlags & MIR_CALLEE) ?
                mir->meta.calleeMethod : cUnit->method;
            void *fieldPtr = (void*)
              (method->clazz->pDvmDex->pResFields[mir->dalvikInsn.vB]);

            if (fieldPtr == NULL) {
                BAIL_LOOP_COMPILATION();
                ALOGE("Unexpected null static field");
                dvmAbort();
            }

            int tReg = dvmCompilerAllocTemp(cUnit);
            rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);
            loadConstant(cUnit, tReg,  (int) fieldPtr + valOffset);

            HEAP_ACCESS_SHADOW(true);
            loadPair(cUnit, tReg, rlResult.lowReg, rlResult.highReg);
            HEAP_ACCESS_SHADOW(false);

            storeValueWide(cUnit, rlDest, rlResult);
            break;
        }
        case OP_SPUT:
        case OP_SPUT_VOLATILE:
        case OP_SPUT_OBJECT:
        case OP_SPUT_OBJECT_VOLATILE:
        case OP_SPUT_BOOLEAN:
        case OP_SPUT_CHAR:
        case OP_SPUT_BYTE:
        case OP_SPUT_SHORT: {
            int valOffset = OFFSETOF_MEMBER(StaticField, value);
            int tReg = dvmCompilerAllocTemp(cUnit);
            int objHead = 0;
            bool isVolatile;
            bool isSputObject;
            const Method *method = (mir->OptimizationFlags & MIR_CALLEE) ?
                mir->meta.calleeMethod : cUnit->method;
            void *fieldPtr = (void*)
              (method->clazz->pDvmDex->pResFields[mir->dalvikInsn.vB]);
            Opcode opcode = mir->dalvikInsn.opcode;

            if (fieldPtr == NULL) {
                BAIL_LOOP_COMPILATION();
                ALOGE("Unexpected null static field");
                dvmAbort();
            }

#if ANDROID_SMP != 0
            isVolatile = (opcode == OP_SPUT_VOLATILE) ||
                         (opcode == OP_SPUT_OBJECT_VOLATILE);
            assert(isVolatile == dvmIsVolatileField((Field *) fieldPtr));
#else
            isVolatile = dvmIsVolatileField((Field *) fieldPtr);
#endif

            isSputObject = (opcode == OP_SPUT_OBJECT) ||
                           (opcode == OP_SPUT_OBJECT_VOLATILE);

            rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            rlSrc = loadValue(cUnit, rlSrc, kAnyReg);
            loadConstant(cUnit, tReg,  (int) fieldPtr);
            if (isSputObject) {
                objHead = dvmCompilerAllocTemp(cUnit);
                loadWordDisp(cUnit, tReg, OFFSETOF_MEMBER(Field, clazz), objHead);
            }
            if (isVolatile) {
                dvmCompilerGenMemBarrier(cUnit, 0);
            }
            HEAP_ACCESS_SHADOW(true);
            storeWordDisp(cUnit, tReg, valOffset ,rlSrc.lowReg);
            dvmCompilerFreeTemp(cUnit, tReg);
            HEAP_ACCESS_SHADOW(false);
            if (isVolatile) {
                dvmCompilerGenMemBarrier(cUnit, 0);
            }
            if (isSputObject) {
                /* NOTE: marking card based sfield->clazz */
                markCard(cUnit, rlSrc.lowReg, objHead);
                dvmCompilerFreeTemp(cUnit, objHead);
            }

            break;
        }
        case OP_SPUT_WIDE: {
            int tReg = dvmCompilerAllocTemp(cUnit);
            int valOffset = OFFSETOF_MEMBER(StaticField, value);
            const Method *method = (mir->OptimizationFlags & MIR_CALLEE) ?
                mir->meta.calleeMethod : cUnit->method;
            void *fieldPtr = (void*)
              (method->clazz->pDvmDex->pResFields[mir->dalvikInsn.vB]);

            if (fieldPtr == NULL) {
                BAIL_LOOP_COMPILATION();
                ALOGE("Unexpected null static field");
                dvmAbort();
            }

            rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
            rlSrc = loadValueWide(cUnit, rlSrc, kAnyReg);
            loadConstant(cUnit, tReg,  (int) fieldPtr + valOffset);

            HEAP_ACCESS_SHADOW(true);
            storePair(cUnit, tReg, rlSrc.lowReg, rlSrc.highReg);
            HEAP_ACCESS_SHADOW(false);
            break;
        }
        case OP_NEW_INSTANCE: {
            /*
             * Obey the calling convention and don't mess with the register
             * usage.
             */
            ClassObject *classPtr = (ClassObject *)
              (cUnit->method->clazz->pDvmDex->pResClasses[mir->dalvikInsn.vB]);

            if (classPtr == NULL) {
                BAIL_LOOP_COMPILATION();
                ALOGE("Unexpected null class");
                dvmAbort();
            }

            /*
             * If it is going to throw, it should not make to the trace to begin
             * with.  However, Alloc might throw, so we need to genExportPC()
             */
            assert((classPtr->accessFlags & (ACC_INTERFACE|ACC_ABSTRACT)) == 0);
            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            genExportPC(cUnit, mir);
            LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmAllocObject);
            loadConstant(cUnit, r_A0, (int) classPtr);
            loadConstant(cUnit, r_A1, ALLOC_DONT_TRACK);
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /* generate a branch over if allocation is successful */
            MipsLIR *branchOver = opCompareBranch(cUnit, kMipsBne, r_V0, r_ZERO);

            /*
             * OOM exception needs to be thrown here and cannot re-execute
             */
            loadConstant(cUnit, r_A0,
                         (int) (cUnit->method->insns + mir->offset));
            genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
            /* noreturn */

            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            branchOver->generic.target = (LIR *) target;
            rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            rlResult = dvmCompilerGetReturn(cUnit);
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_CHECK_CAST: {
            /*
             * Obey the calling convention and don't mess with the register
             * usage.
             */
            ClassObject *classPtr =
              (cUnit->method->clazz->pDvmDex->pResClasses[mir->dalvikInsn.vB]);
            /*
             * Note: It is possible that classPtr is NULL at this point,
             * even though this instruction has been successfully interpreted.
             * If the previous interpretation had a null source, the
             * interpreter would not have bothered to resolve the clazz.
             * Bail out to the interpreter in this case, and log it
             * so that we can tell if it happens frequently.
             */
            if (classPtr == NULL) {
                BAIL_LOOP_COMPILATION();
                LOGVV("null clazz in OP_CHECK_CAST, single-stepping");
                genInterpSingleStep(cUnit, mir);
                return false;
            }
            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            loadConstant(cUnit, r_A1, (int) classPtr );
            rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            MipsLIR *branch1 = opCompareBranch(cUnit, kMipsBeqz, rlSrc.lowReg, -1);
            /*
             *  rlSrc.lowReg now contains object->clazz.  Note that
             *  it could have been allocated r_A0, but we're okay so long
             *  as we don't do anything desctructive until r_A0 is loaded
             *  with clazz.
             */
            /* r_A0 now contains object->clazz */
            loadWordDisp(cUnit, rlSrc.lowReg, offsetof(Object, clazz), r_A0);
            LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmInstanceofNonTrivial);
            MipsLIR *branch2 = opCompareBranch(cUnit, kMipsBeq, r_A0, r_A1);
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /*
             * If null, check cast failed - punt to the interpreter.  Because
             * interpreter will be the one throwing, we don't need to
             * genExportPC() here.
             */
            genRegCopy(cUnit, r_A0, r_V0);
            genZeroCheck(cUnit, r_V0, mir->offset, NULL);
            /* check cast passed - branch target here */
            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            branch1->generic.target = (LIR *)target;
            branch2->generic.target = (LIR *)target;
            break;
        }
        case OP_SGET_WIDE_VOLATILE:
        case OP_SPUT_WIDE_VOLATILE:
            genInterpSingleStep(cUnit, mir);
            break;
        default:
            return true;
    }
    return false;
}

static bool handleFmt11x(CompilationUnit *cUnit, MIR *mir)
{
    Opcode dalvikOpcode = mir->dalvikInsn.opcode;
    RegLocation rlResult;
    switch (dalvikOpcode) {
        case OP_MOVE_EXCEPTION: {
            int exOffset = offsetof(Thread, exception);
            int resetReg = dvmCompilerAllocTemp(cUnit);
            RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            loadWordDisp(cUnit, rSELF, exOffset, rlResult.lowReg);
            loadConstant(cUnit, resetReg, 0);
            storeWordDisp(cUnit, rSELF, exOffset, resetReg);
            storeValue(cUnit, rlDest, rlResult);
           break;
        }
        case OP_MOVE_RESULT:
        case OP_MOVE_RESULT_OBJECT: {
            /* An inlined move result is effectively no-op */
            if (mir->OptimizationFlags & MIR_INLINED)
                break;
            RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            RegLocation rlSrc = LOC_DALVIK_RETURN_VAL;
            rlSrc.fp = rlDest.fp;
            storeValue(cUnit, rlDest, rlSrc);
            break;
        }
        case OP_MOVE_RESULT_WIDE: {
            /* An inlined move result is effectively no-op */
            if (mir->OptimizationFlags & MIR_INLINED)
                break;
            RegLocation rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
            RegLocation rlSrc = LOC_DALVIK_RETURN_VAL_WIDE;
            rlSrc.fp = rlDest.fp;
            storeValueWide(cUnit, rlDest, rlSrc);
            break;
        }
        case OP_RETURN_WIDE: {
            RegLocation rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
            RegLocation rlDest = LOC_DALVIK_RETURN_VAL_WIDE;
            rlDest.fp = rlSrc.fp;
            storeValueWide(cUnit, rlDest, rlSrc);
            genReturnCommon(cUnit,mir);
            break;
        }
        case OP_RETURN:
        case OP_RETURN_OBJECT: {
            RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            RegLocation rlDest = LOC_DALVIK_RETURN_VAL;
            rlDest.fp = rlSrc.fp;
            storeValue(cUnit, rlDest, rlSrc);
            genReturnCommon(cUnit, mir);
            break;
        }
        case OP_MONITOR_EXIT:
        case OP_MONITOR_ENTER:
            genMonitor(cUnit, mir);
            break;
        case OP_THROW:
            genInterpSingleStep(cUnit, mir);
            break;
        default:
            return true;
    }
    return false;
}

static bool handleFmt12x(CompilationUnit *cUnit, MIR *mir)
{
    Opcode opcode = mir->dalvikInsn.opcode;
    RegLocation rlDest;
    RegLocation rlSrc;
    RegLocation rlResult;

    if ( (opcode >= OP_ADD_INT_2ADDR) && (opcode <= OP_REM_DOUBLE_2ADDR)) {
        return genArithOp( cUnit, mir );
    }

    if (mir->ssaRep->numUses == 2)
        rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
    else
        rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    if (mir->ssaRep->numDefs == 2)
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    else
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);

    switch (opcode) {
        case OP_DOUBLE_TO_INT:
        case OP_INT_TO_FLOAT:
        case OP_FLOAT_TO_INT:
        case OP_DOUBLE_TO_FLOAT:
        case OP_FLOAT_TO_DOUBLE:
        case OP_INT_TO_DOUBLE:
        case OP_FLOAT_TO_LONG:
        case OP_LONG_TO_FLOAT:
        case OP_DOUBLE_TO_LONG:
        case OP_LONG_TO_DOUBLE:
            return genConversion(cUnit, mir);
        case OP_NEG_INT:
        case OP_NOT_INT:
            return genArithOpInt(cUnit, mir, rlDest, rlSrc, rlSrc);
        case OP_NEG_LONG:
        case OP_NOT_LONG:
            return genArithOpLong(cUnit, mir, rlDest, rlSrc, rlSrc);
        case OP_NEG_FLOAT:
            return genArithOpFloat(cUnit, mir, rlDest, rlSrc, rlSrc);
        case OP_NEG_DOUBLE:
            return genArithOpDouble(cUnit, mir, rlDest, rlSrc, rlSrc);
        case OP_MOVE_WIDE:
            storeValueWide(cUnit, rlDest, rlSrc);
            break;
        case OP_INT_TO_LONG:
            rlSrc = dvmCompilerUpdateLoc(cUnit, rlSrc);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            //TUNING: shouldn't loadValueDirect already check for phys reg?
            if (rlSrc.location == kLocPhysReg) {
                genRegCopy(cUnit, rlResult.lowReg, rlSrc.lowReg);
            } else {
                loadValueDirect(cUnit, rlSrc, rlResult.lowReg);
            }
            opRegRegImm(cUnit, kOpAsr, rlResult.highReg,
                        rlResult.lowReg, 31);
            storeValueWide(cUnit, rlDest, rlResult);
            break;
        case OP_LONG_TO_INT:
            rlSrc = dvmCompilerUpdateLocWide(cUnit, rlSrc);
            rlSrc = dvmCompilerWideToNarrow(cUnit, rlSrc);
            // Intentional fallthrough
        case OP_MOVE:
        case OP_MOVE_OBJECT:
            storeValue(cUnit, rlDest, rlSrc);
            break;
        case OP_INT_TO_BYTE:
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegReg(cUnit, kOp2Byte, rlResult.lowReg, rlSrc.lowReg);
            storeValue(cUnit, rlDest, rlResult);
            break;
        case OP_INT_TO_SHORT:
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegReg(cUnit, kOp2Short, rlResult.lowReg, rlSrc.lowReg);
            storeValue(cUnit, rlDest, rlResult);
            break;
        case OP_INT_TO_CHAR:
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegReg(cUnit, kOp2Char, rlResult.lowReg, rlSrc.lowReg);
            storeValue(cUnit, rlDest, rlResult);
            break;
        case OP_ARRAY_LENGTH: {
            int lenOffset = OFFSETOF_MEMBER(ArrayObject, length);
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            genNullCheck(cUnit, rlSrc.sRegLow, rlSrc.lowReg,
                         mir->offset, NULL);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            loadWordDisp(cUnit, rlSrc.lowReg, lenOffset,
                         rlResult.lowReg);
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt21s(CompilationUnit *cUnit, MIR *mir)
{
    Opcode dalvikOpcode = mir->dalvikInsn.opcode;
    RegLocation rlDest;
    RegLocation rlResult;
    int BBBB = mir->dalvikInsn.vB;
    if (dalvikOpcode == OP_CONST_WIDE_16) {
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
        rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
        loadConstantNoClobber(cUnit, rlResult.lowReg, BBBB);
        //TUNING: do high separately to avoid load dependency
        opRegRegImm(cUnit, kOpAsr, rlResult.highReg, rlResult.lowReg, 31);
        storeValueWide(cUnit, rlDest, rlResult);
    } else if (dalvikOpcode == OP_CONST_16) {
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);
        rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);
        loadConstantNoClobber(cUnit, rlResult.lowReg, BBBB);
        storeValue(cUnit, rlDest, rlResult);
    } else
        return true;
    return false;
}

/* Compare agaist zero */
static bool handleFmt21t(CompilationUnit *cUnit, MIR *mir, BasicBlock *bb,
                         MipsLIR *labelList)
{
    Opcode dalvikOpcode = mir->dalvikInsn.opcode;
    MipsOpCode opc = kMipsNop;
    int rt = -1;
    /* backward branch? */
    bool backwardBranch = (bb->taken->startOffset <= mir->offset);

    if (backwardBranch &&
        (gDvmJit.genSuspendPoll || cUnit->jitMode == kJitLoop)) {
        genSuspendPoll(cUnit, mir);
    }

    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    rlSrc = loadValue(cUnit, rlSrc, kCoreReg);

    switch (dalvikOpcode) {
        case OP_IF_EQZ:
            opc = kMipsBeqz;
            break;
        case OP_IF_NEZ:
            opc = kMipsBne;
            rt = r_ZERO;
            break;
        case OP_IF_LTZ:
            opc = kMipsBltz;
            break;
        case OP_IF_GEZ:
            opc = kMipsBgez;
            break;
        case OP_IF_GTZ:
            opc = kMipsBgtz;
            break;
        case OP_IF_LEZ:
            opc = kMipsBlez;
            break;
        default:
            ALOGE("Unexpected opcode (%d) for Fmt21t", dalvikOpcode);
            dvmCompilerAbort(cUnit);
    }
    genConditionalBranchMips(cUnit, opc, rlSrc.lowReg, rt, &labelList[bb->taken->id]);
    /* This mostly likely will be optimized away in a later phase */
    genUnconditionalBranch(cUnit, &labelList[bb->fallThrough->id]);
    return false;
}

static bool isPowerOfTwo(int x)
{
    return (x & (x - 1)) == 0;
}

// Returns true if no more than two bits are set in 'x'.
static bool isPopCountLE2(unsigned int x)
{
    x &= x - 1;
    return (x & (x - 1)) == 0;
}

// Returns the index of the lowest set bit in 'x'.
static int lowestSetBit(unsigned int x) {
    int bit_posn = 0;
    while ((x & 0xf) == 0) {
        bit_posn += 4;
        x >>= 4;
    }
    while ((x & 1) == 0) {
        bit_posn++;
        x >>= 1;
    }
    return bit_posn;
}

// Returns true if it added instructions to 'cUnit' to divide 'rlSrc' by 'lit'
// and store the result in 'rlDest'.
static bool handleEasyDivide(CompilationUnit *cUnit, Opcode dalvikOpcode,
                             RegLocation rlSrc, RegLocation rlDest, int lit)
{
    if (lit < 2 || !isPowerOfTwo(lit)) {
        return false;
    }
    int k = lowestSetBit(lit);
    if (k >= 30) {
        // Avoid special cases.
        return false;
    }
    bool div = (dalvikOpcode == OP_DIV_INT_LIT8 || dalvikOpcode == OP_DIV_INT_LIT16);
    rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    if (div) {
        int tReg = dvmCompilerAllocTemp(cUnit);
        if (lit == 2) {
            // Division by 2 is by far the most common division by constant.
            opRegRegImm(cUnit, kOpLsr, tReg, rlSrc.lowReg, 32 - k);
            opRegRegReg(cUnit, kOpAdd, tReg, tReg, rlSrc.lowReg);
            opRegRegImm(cUnit, kOpAsr, rlResult.lowReg, tReg, k);
        } else {
            opRegRegImm(cUnit, kOpAsr, tReg, rlSrc.lowReg, 31);
            opRegRegImm(cUnit, kOpLsr, tReg, tReg, 32 - k);
            opRegRegReg(cUnit, kOpAdd, tReg, tReg, rlSrc.lowReg);
            opRegRegImm(cUnit, kOpAsr, rlResult.lowReg, tReg, k);
        }
    } else {
        int cReg = dvmCompilerAllocTemp(cUnit);
        loadConstant(cUnit, cReg, lit - 1);
        int tReg1 = dvmCompilerAllocTemp(cUnit);
        int tReg2 = dvmCompilerAllocTemp(cUnit);
        if (lit == 2) {
            opRegRegImm(cUnit, kOpLsr, tReg1, rlSrc.lowReg, 32 - k);
            opRegRegReg(cUnit, kOpAdd, tReg2, tReg1, rlSrc.lowReg);
            opRegRegReg(cUnit, kOpAnd, tReg2, tReg2, cReg);
            opRegRegReg(cUnit, kOpSub, rlResult.lowReg, tReg2, tReg1);
        } else {
            opRegRegImm(cUnit, kOpAsr, tReg1, rlSrc.lowReg, 31);
            opRegRegImm(cUnit, kOpLsr, tReg1, tReg1, 32 - k);
            opRegRegReg(cUnit, kOpAdd, tReg2, tReg1, rlSrc.lowReg);
            opRegRegReg(cUnit, kOpAnd, tReg2, tReg2, cReg);
            opRegRegReg(cUnit, kOpSub, rlResult.lowReg, tReg2, tReg1);
        }
    }
    storeValue(cUnit, rlDest, rlResult);
    return true;
}

// Returns true if it added instructions to 'cUnit' to multiply 'rlSrc' by 'lit'
// and store the result in 'rlDest'.
static bool handleEasyMultiply(CompilationUnit *cUnit,
                               RegLocation rlSrc, RegLocation rlDest, int lit)
{
    // Can we simplify this multiplication?
    bool powerOfTwo = false;
    bool popCountLE2 = false;
    bool powerOfTwoMinusOne = false;
    if (lit < 2) {
        // Avoid special cases.
        return false;
    } else if (isPowerOfTwo(lit)) {
        powerOfTwo = true;
    } else if (isPopCountLE2(lit)) {
        popCountLE2 = true;
    } else if (isPowerOfTwo(lit + 1)) {
        powerOfTwoMinusOne = true;
    } else {
        return false;
    }
    rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    if (powerOfTwo) {
        // Shift.
        opRegRegImm(cUnit, kOpLsl, rlResult.lowReg, rlSrc.lowReg,
                    lowestSetBit(lit));
    } else if (popCountLE2) {
        // Shift and add and shift.
        int firstBit = lowestSetBit(lit);
        int secondBit = lowestSetBit(lit ^ (1 << firstBit));
        genMultiplyByTwoBitMultiplier(cUnit, rlSrc, rlResult, lit,
                                      firstBit, secondBit);
    } else {
        // Reverse subtract: (src << (shift + 1)) - src.
        assert(powerOfTwoMinusOne);
        // TODO: rsb dst, src, src lsl#lowestSetBit(lit + 1)
        int tReg = dvmCompilerAllocTemp(cUnit);
        opRegRegImm(cUnit, kOpLsl, tReg, rlSrc.lowReg, lowestSetBit(lit + 1));
        opRegRegReg(cUnit, kOpSub, rlResult.lowReg, tReg, rlSrc.lowReg);
    }
    storeValue(cUnit, rlDest, rlResult);
    return true;
}

static bool handleFmt22b_Fmt22s(CompilationUnit *cUnit, MIR *mir)
{
    Opcode dalvikOpcode = mir->dalvikInsn.opcode;
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
    RegLocation rlResult;
    int lit = mir->dalvikInsn.vC;
    OpKind op = (OpKind)0;      /* Make gcc happy */
    int shiftOp = false;

    switch (dalvikOpcode) {
        case OP_RSUB_INT_LIT8:
        case OP_RSUB_INT: {
            int tReg;
            //TUNING: add support for use of Arm rsub op
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            tReg = dvmCompilerAllocTemp(cUnit);
            loadConstant(cUnit, tReg, lit);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegRegReg(cUnit, kOpSub, rlResult.lowReg,
                        tReg, rlSrc.lowReg);
            storeValue(cUnit, rlDest, rlResult);
            return false;
            break;
        }

        case OP_ADD_INT_LIT8:
        case OP_ADD_INT_LIT16:
            op = kOpAdd;
            break;
        case OP_MUL_INT_LIT8:
        case OP_MUL_INT_LIT16: {
            if (handleEasyMultiply(cUnit, rlSrc, rlDest, lit)) {
                return false;
            }
            op = kOpMul;
            break;
        }
        case OP_AND_INT_LIT8:
        case OP_AND_INT_LIT16:
            op = kOpAnd;
            break;
        case OP_OR_INT_LIT8:
        case OP_OR_INT_LIT16:
            op = kOpOr;
            break;
        case OP_XOR_INT_LIT8:
        case OP_XOR_INT_LIT16:
            op = kOpXor;
            break;
        case OP_SHL_INT_LIT8:
            lit &= 31;
            shiftOp = true;
            op = kOpLsl;
            break;
        case OP_SHR_INT_LIT8:
            lit &= 31;
            shiftOp = true;
            op = kOpAsr;
            break;
        case OP_USHR_INT_LIT8:
            lit &= 31;
            shiftOp = true;
            op = kOpLsr;
            break;

        case OP_DIV_INT_LIT8:
        case OP_DIV_INT_LIT16:
        case OP_REM_INT_LIT8:
        case OP_REM_INT_LIT16: {
            if (lit == 0) {
                /* Let the interpreter deal with div by 0 */
                genInterpSingleStep(cUnit, mir);
                return false;
            }
            if (handleEasyDivide(cUnit, dalvikOpcode, rlSrc, rlDest, lit)) {
                return false;
            }

            MipsOpCode opc;
            int divReg;

            if ((dalvikOpcode == OP_DIV_INT_LIT8) ||
                (dalvikOpcode == OP_DIV_INT_LIT16)) {
                opc = kMipsMflo;
                divReg = r_LO;
            } else {
                opc = kMipsMfhi;
                divReg = r_HI;
            }

            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            int tReg = dvmCompilerAllocTemp(cUnit);
            newLIR3(cUnit, kMipsAddiu, tReg, r_ZERO, lit);
            newLIR4(cUnit, kMipsDiv, r_HI, r_LO, rlSrc.lowReg, tReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            newLIR2(cUnit, opc, rlResult.lowReg, divReg);
            dvmCompilerFreeTemp(cUnit, tReg);
            storeValue(cUnit, rlDest, rlResult);
            return false;
            break;
        }
        default:
            return true;
    }
    rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    // Avoid shifts by literal 0 - no support in Thumb.  Change to copy
    if (shiftOp && (lit == 0)) {
        genRegCopy(cUnit, rlResult.lowReg, rlSrc.lowReg);
    } else {
        opRegRegImm(cUnit, op, rlResult.lowReg, rlSrc.lowReg, lit);
    }
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static bool handleFmt22c(CompilationUnit *cUnit, MIR *mir)
{
    Opcode dalvikOpcode = mir->dalvikInsn.opcode;
    int fieldOffset = -1;
    bool isVolatile = false;
    switch (dalvikOpcode) {
        /*
         * Wide volatiles currently handled via single step.
         * Add them here if generating in-line code.
         *     case OP_IGET_WIDE_VOLATILE:
         *     case OP_IPUT_WIDE_VOLATILE:
         */
        case OP_IGET_VOLATILE:
        case OP_IGET_OBJECT_VOLATILE:
        case OP_IPUT_VOLATILE:
        case OP_IPUT_OBJECT_VOLATILE:
#if ANDROID_SMP != 0
            isVolatile = true;
        // NOTE: intentional fallthrough
#endif
        case OP_IGET:
        case OP_IGET_WIDE:
        case OP_IGET_OBJECT:
        case OP_IGET_BOOLEAN:
        case OP_IGET_BYTE:
        case OP_IGET_CHAR:
        case OP_IGET_SHORT:
        case OP_IPUT:
        case OP_IPUT_WIDE:
        case OP_IPUT_OBJECT:
        case OP_IPUT_BOOLEAN:
        case OP_IPUT_BYTE:
        case OP_IPUT_CHAR:
        case OP_IPUT_SHORT: {
            const Method *method = (mir->OptimizationFlags & MIR_CALLEE) ?
                mir->meta.calleeMethod : cUnit->method;
            Field *fieldPtr =
                method->clazz->pDvmDex->pResFields[mir->dalvikInsn.vC];

            if (fieldPtr == NULL) {
                BAIL_LOOP_COMPILATION();
                ALOGE("Unexpected null instance field");
                dvmAbort();
            }
#if ANDROID_SMP != 0
            assert(isVolatile == dvmIsVolatileField((Field *) fieldPtr));
#else
            isVolatile = dvmIsVolatileField((Field *) fieldPtr);
#endif
            fieldOffset = ((InstField *)fieldPtr)->byteOffset;
            break;
        }
        default:
            break;
    }

    switch (dalvikOpcode) {
        case OP_NEW_ARRAY: {
#if 0 /* 080 triggers assert in Interp.c:1290 for out of memory exception.
             i think the assert is in error and should be disabled. With
             asserts disabled, 080 passes. */
genInterpSingleStep(cUnit, mir);
return false;
#endif
            // Generates a call - use explicit registers
            RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            RegLocation rlResult;
            void *classPtr = (void*)
              (cUnit->method->clazz->pDvmDex->pResClasses[mir->dalvikInsn.vC]);

            if (classPtr == NULL) {
                BAIL_LOOP_COMPILATION();
                ALOGE("Unexpected null class");
                dvmAbort();
            }

            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            genExportPC(cUnit, mir);
            loadValueDirectFixed(cUnit, rlSrc, r_A1);   /* Len */
            loadConstant(cUnit, r_A0, (int) classPtr );
            LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmAllocArrayByClass);
            /*
             * "len < 0": bail to the interpreter to re-execute the
             * instruction
             */
            genRegImmCheck(cUnit, kMipsCondMi, r_A1, 0, mir->offset, NULL);
            loadConstant(cUnit, r_A2, ALLOC_DONT_TRACK);
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /* generate a branch over if allocation is successful */
            MipsLIR *branchOver = opCompareBranch(cUnit, kMipsBne, r_V0, r_ZERO);
            /*
             * OOM exception needs to be thrown here and cannot re-execute
             */
            loadConstant(cUnit, r_A0,
                         (int) (cUnit->method->insns + mir->offset));
            genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
            /* noreturn */

            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            branchOver->generic.target = (LIR *) target;
            rlResult = dvmCompilerGetReturn(cUnit);
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_INSTANCE_OF: {
            // May generate a call - use explicit registers
            RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            RegLocation rlResult;
            ClassObject *classPtr =
              (cUnit->method->clazz->pDvmDex->pResClasses[mir->dalvikInsn.vC]);
            /*
             * Note: It is possible that classPtr is NULL at this point,
             * even though this instruction has been successfully interpreted.
             * If the previous interpretation had a null source, the
             * interpreter would not have bothered to resolve the clazz.
             * Bail out to the interpreter in this case, and log it
             * so that we can tell if it happens frequently.
             */
            if (classPtr == NULL) {
                BAIL_LOOP_COMPILATION();
                ALOGD("null clazz in OP_INSTANCE_OF, single-stepping");
                genInterpSingleStep(cUnit, mir);
                break;
            }
            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            loadValueDirectFixed(cUnit, rlSrc, r_V0);  /* Ref */
            loadConstant(cUnit, r_A2, (int) classPtr );
            /* When taken r_V0 has NULL which can be used for store directly */
            MipsLIR *branch1 = opCompareBranch(cUnit, kMipsBeqz, r_V0, -1);
            /* r_A1 now contains object->clazz */
            loadWordDisp(cUnit, r_V0, offsetof(Object, clazz), r_A1);
            /* r_A1 now contains object->clazz */
            LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmInstanceofNonTrivial);
            loadConstant(cUnit, r_V0, 1);                /* Assume true */
            MipsLIR *branch2 = opCompareBranch(cUnit, kMipsBeq, r_A1, r_A2);
            genRegCopy(cUnit, r_A0, r_A1);
            genRegCopy(cUnit, r_A1, r_A2);
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /* branch target here */
            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            rlResult = dvmCompilerGetReturn(cUnit);
            storeValue(cUnit, rlDest, rlResult);
            branch1->generic.target = (LIR *)target;
            branch2->generic.target = (LIR *)target;
            break;
        }
        case OP_IGET_WIDE:
            genIGetWide(cUnit, mir, fieldOffset);
            break;
        case OP_IGET_VOLATILE:
        case OP_IGET_OBJECT_VOLATILE:
        case OP_IGET:
        case OP_IGET_OBJECT:
        case OP_IGET_BOOLEAN:
        case OP_IGET_BYTE:
        case OP_IGET_CHAR:
        case OP_IGET_SHORT:
            genIGet(cUnit, mir, kWord, fieldOffset, isVolatile);
            break;
        case OP_IPUT_WIDE:
            genIPutWide(cUnit, mir, fieldOffset);
            break;
        case OP_IPUT_VOLATILE:
        case OP_IPUT:
        case OP_IPUT_BOOLEAN:
        case OP_IPUT_BYTE:
        case OP_IPUT_CHAR:
        case OP_IPUT_SHORT:
            genIPut(cUnit, mir, kWord, fieldOffset, false, isVolatile);
            break;
        case OP_IPUT_OBJECT_VOLATILE:
        case OP_IPUT_OBJECT:
            genIPut(cUnit, mir, kWord, fieldOffset, true, isVolatile);
            break;
        case OP_IGET_WIDE_VOLATILE:
        case OP_IPUT_WIDE_VOLATILE:
            genInterpSingleStep(cUnit, mir);
            break;
        default:
            return true;
    }
    return false;
}

static bool handleFmt22cs(CompilationUnit *cUnit, MIR *mir)
{
    Opcode dalvikOpcode = mir->dalvikInsn.opcode;
    int fieldOffset =  mir->dalvikInsn.vC;
    switch (dalvikOpcode) {
        case OP_IGET_QUICK:
        case OP_IGET_OBJECT_QUICK:
            genIGet(cUnit, mir, kWord, fieldOffset, false);
            break;
        case OP_IPUT_QUICK:
            genIPut(cUnit, mir, kWord, fieldOffset, false, false);
            break;
        case OP_IPUT_OBJECT_QUICK:
            genIPut(cUnit, mir, kWord, fieldOffset, true, false);
            break;
        case OP_IGET_WIDE_QUICK:
            genIGetWide(cUnit, mir, fieldOffset);
            break;
        case OP_IPUT_WIDE_QUICK:
            genIPutWide(cUnit, mir, fieldOffset);
            break;
        default:
            return true;
    }
    return false;

}

/* Compare against zero */
static bool handleFmt22t(CompilationUnit *cUnit, MIR *mir, BasicBlock *bb,
                         MipsLIR *labelList)
{
    Opcode dalvikOpcode = mir->dalvikInsn.opcode;
    MipsConditionCode cond;
    MipsOpCode opc = kMipsNop;
    MipsLIR * test = NULL;
    /* backward branch? */
    bool backwardBranch = (bb->taken->startOffset <= mir->offset);

    if (backwardBranch &&
        (gDvmJit.genSuspendPoll || cUnit->jitMode == kJitLoop)) {
        genSuspendPoll(cUnit, mir);
    }

    RegLocation rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 1);
    rlSrc1 = loadValue(cUnit, rlSrc1, kCoreReg);
    rlSrc2 = loadValue(cUnit, rlSrc2, kCoreReg);
    int reg1 = rlSrc1.lowReg;
    int reg2 = rlSrc2.lowReg;
    int tReg;

    switch (dalvikOpcode) {
        case OP_IF_EQ:
            opc = kMipsBeq;
            break;
        case OP_IF_NE:
            opc = kMipsBne;
            break;
        case OP_IF_LT:
            opc = kMipsBne;
            tReg = dvmCompilerAllocTemp(cUnit);
            test = newLIR3(cUnit, kMipsSlt, tReg, reg1, reg2);
            reg1 = tReg;
            reg2 = r_ZERO;
            break;
        case OP_IF_LE:
            opc = kMipsBeqz;
            tReg = dvmCompilerAllocTemp(cUnit);
            test = newLIR3(cUnit, kMipsSlt, tReg, reg2, reg1);
            reg1 = tReg;
            reg2 = -1;
            break;
        case OP_IF_GT:
            opc = kMipsBne;
            tReg = dvmCompilerAllocTemp(cUnit);
            test = newLIR3(cUnit, kMipsSlt, tReg, reg2, reg1);
            reg1 = tReg;
            reg2 = r_ZERO;
            break;
        case OP_IF_GE:
            opc = kMipsBeqz;
            tReg = dvmCompilerAllocTemp(cUnit);
            test = newLIR3(cUnit, kMipsSlt, tReg, reg1, reg2);
            reg1 = tReg;
            reg2 = -1;
            break;
        default:
            cond = (MipsConditionCode)0;
            ALOGE("Unexpected opcode (%d) for Fmt22t", dalvikOpcode);
            dvmCompilerAbort(cUnit);
    }

    genConditionalBranchMips(cUnit, opc, reg1, reg2, &labelList[bb->taken->id]);
    /* This mostly likely will be optimized away in a later phase */
    genUnconditionalBranch(cUnit, &labelList[bb->fallThrough->id]);
    return false;
}

static bool handleFmt22x_Fmt32x(CompilationUnit *cUnit, MIR *mir)
{
    Opcode opcode = mir->dalvikInsn.opcode;

    switch (opcode) {
        case OP_MOVE_16:
        case OP_MOVE_OBJECT_16:
        case OP_MOVE_FROM16:
        case OP_MOVE_OBJECT_FROM16: {
            storeValue(cUnit, dvmCompilerGetDest(cUnit, mir, 0),
                       dvmCompilerGetSrc(cUnit, mir, 0));
            break;
        }
        case OP_MOVE_WIDE_16:
        case OP_MOVE_WIDE_FROM16: {
            storeValueWide(cUnit, dvmCompilerGetDestWide(cUnit, mir, 0, 1),
                           dvmCompilerGetSrcWide(cUnit, mir, 0, 1));
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt23x(CompilationUnit *cUnit, MIR *mir)
{
    Opcode opcode = mir->dalvikInsn.opcode;
    RegLocation rlSrc1;
    RegLocation rlSrc2;
    RegLocation rlDest;

    if ((opcode >= OP_ADD_INT) && (opcode <= OP_REM_DOUBLE)) {
        return genArithOp( cUnit, mir );
    }

    /* APUTs have 3 sources and no targets */
    if (mir->ssaRep->numDefs == 0) {
        if (mir->ssaRep->numUses == 3) {
            rlDest = dvmCompilerGetSrc(cUnit, mir, 0);
            rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 1);
            rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 2);
        } else {
            assert(mir->ssaRep->numUses == 4);
            rlDest = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
            rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 2);
            rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 3);
        }
    } else {
        /* Two sources and 1 dest.  Deduce the operand sizes */
        if (mir->ssaRep->numUses == 4) {
            rlSrc1 = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
            rlSrc2 = dvmCompilerGetSrcWide(cUnit, mir, 2, 3);
        } else {
            assert(mir->ssaRep->numUses == 2);
            rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 0);
            rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 1);
        }
        if (mir->ssaRep->numDefs == 2) {
            rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
        } else {
            assert(mir->ssaRep->numDefs == 1);
            rlDest = dvmCompilerGetDest(cUnit, mir, 0);
        }
    }

    switch (opcode) {
        case OP_CMPL_FLOAT:
        case OP_CMPG_FLOAT:
        case OP_CMPL_DOUBLE:
        case OP_CMPG_DOUBLE:
            return genCmpFP(cUnit, mir, rlDest, rlSrc1, rlSrc2);
        case OP_CMP_LONG:
            genCmpLong(cUnit, mir, rlDest, rlSrc1, rlSrc2);
            break;
        case OP_AGET_WIDE:
            genArrayGet(cUnit, mir, kLong, rlSrc1, rlSrc2, rlDest, 3);
            break;
        case OP_AGET:
        case OP_AGET_OBJECT:
            genArrayGet(cUnit, mir, kWord, rlSrc1, rlSrc2, rlDest, 2);
            break;
        case OP_AGET_BOOLEAN:
            genArrayGet(cUnit, mir, kUnsignedByte, rlSrc1, rlSrc2, rlDest, 0);
            break;
        case OP_AGET_BYTE:
            genArrayGet(cUnit, mir, kSignedByte, rlSrc1, rlSrc2, rlDest, 0);
            break;
        case OP_AGET_CHAR:
            genArrayGet(cUnit, mir, kUnsignedHalf, rlSrc1, rlSrc2, rlDest, 1);
            break;
        case OP_AGET_SHORT:
            genArrayGet(cUnit, mir, kSignedHalf, rlSrc1, rlSrc2, rlDest, 1);
            break;
        case OP_APUT_WIDE:
            genArrayPut(cUnit, mir, kLong, rlSrc1, rlSrc2, rlDest, 3);
            break;
        case OP_APUT:
            genArrayPut(cUnit, mir, kWord, rlSrc1, rlSrc2, rlDest, 2);
            break;
        case OP_APUT_OBJECT:
            genArrayObjectPut(cUnit, mir, rlSrc1, rlSrc2, rlDest, 2);
            break;
        case OP_APUT_SHORT:
        case OP_APUT_CHAR:
            genArrayPut(cUnit, mir, kUnsignedHalf, rlSrc1, rlSrc2, rlDest, 1);
            break;
        case OP_APUT_BYTE:
        case OP_APUT_BOOLEAN:
            genArrayPut(cUnit, mir, kUnsignedByte, rlSrc1, rlSrc2, rlDest, 0);
            break;
        default:
            return true;
    }
    return false;
}

/*
 * Find the matching case.
 *
 * return values:
 * r_RESULT0 (low 32-bit): pc of the chaining cell corresponding to the resolved case,
 *    including default which is placed at MIN(size, MAX_CHAINED_SWITCH_CASES).
 * r_RESULT1 (high 32-bit): the branch offset of the matching case (only for indexes
 *    above MAX_CHAINED_SWITCH_CASES).
 *
 * Instructions around the call are:
 *
 * jalr &findPackedSwitchIndex
 * nop
 * lw gp, 84(sp) |
 * addu          | 20 bytes for these 5 instructions
 * move          | (NOTE: if this sequence is shortened or lengthened, then
 * jr            |  the 20 byte offset added below in 3 places must be changed
 * nop           |  accordingly.)
 * chaining cell for case 0 [16 bytes]
 * chaining cell for case 1 [16 bytes]
 *               :
 * chaining cell for case MIN(size, MAX_CHAINED_SWITCH_CASES)-1 [16 bytes]
 * chaining cell for case default [16 bytes]
 * noChain exit
 */
static u8 findPackedSwitchIndex(const u2* switchData, int testVal)
{
    int size;
    int firstKey;
    const int *entries;
    int index;
    int jumpIndex;
    uintptr_t caseDPCOffset = 0;

    /*
     * Packed switch data format:
     *  ushort ident = 0x0100   magic value
     *  ushort size             number of entries in the table
     *  int first_key           first (and lowest) switch case value
     *  int targets[size]       branch targets, relative to switch opcode
     *
     * Total size is (4+size*2) 16-bit code units.
     */
    size = switchData[1];
    assert(size > 0);

    firstKey = switchData[2];
    firstKey |= switchData[3] << 16;


    /* The entries are guaranteed to be aligned on a 32-bit boundary;
     * we can treat them as a native int array.
     */
    entries = (const int*) &switchData[4];
    assert(((u4)entries & 0x3) == 0);

    index = testVal - firstKey;

    /* Jump to the default cell */
    if (index < 0 || index >= size) {
        jumpIndex = MIN(size, MAX_CHAINED_SWITCH_CASES);
    /* Jump to the non-chaining exit point */
    } else if (index >= MAX_CHAINED_SWITCH_CASES) {
        jumpIndex = MAX_CHAINED_SWITCH_CASES + 1;
#ifdef HAVE_LITTLE_ENDIAN
        caseDPCOffset = entries[index];
#else
        caseDPCOffset = (unsigned int)entries[index] >> 16 | entries[index] << 16;
#endif
    /* Jump to the inline chaining cell */
    } else {
        jumpIndex = index;
    }

    return (((u8) caseDPCOffset) << 32) | (u8) (jumpIndex * CHAIN_CELL_NORMAL_SIZE + 20);
}

/* See comments for findPackedSwitchIndex */
static u8 findSparseSwitchIndex(const u2* switchData, int testVal)
{
    int size;
    const int *keys;
    const int *entries;
    /* In Thumb mode pc is 4 ahead of the "mov r2, pc" instruction */
    int i;

    /*
     * Sparse switch data format:
     *  ushort ident = 0x0200   magic value
     *  ushort size             number of entries in the table; > 0
     *  int keys[size]          keys, sorted low-to-high; 32-bit aligned
     *  int targets[size]       branch targets, relative to switch opcode
     *
     * Total size is (2+size*4) 16-bit code units.
     */

    size = switchData[1];
    assert(size > 0);

    /* The keys are guaranteed to be aligned on a 32-bit boundary;
     * we can treat them as a native int array.
     */
    keys = (const int*) &switchData[2];
    assert(((u4)keys & 0x3) == 0);

    /* The entries are guaranteed to be aligned on a 32-bit boundary;
     * we can treat them as a native int array.
     */
    entries = keys + size;
    assert(((u4)entries & 0x3) == 0);

    /*
     * Run through the list of keys, which are guaranteed to
     * be sorted low-to-high.
     *
     * Most tables have 3-4 entries.  Few have more than 10.  A binary
     * search here is probably not useful.
     */
    for (i = 0; i < size; i++) {
#ifdef HAVE_LITTLE_ENDIAN
        int k = keys[i];
        if (k == testVal) {
            /* MAX_CHAINED_SWITCH_CASES + 1 is the start of the overflow case */
            int jumpIndex = (i < MAX_CHAINED_SWITCH_CASES) ?
                           i : MAX_CHAINED_SWITCH_CASES + 1;
            return (((u8) entries[i]) << 32) | (u8) (jumpIndex * CHAIN_CELL_NORMAL_SIZE + 20);
#else
        int k = (unsigned int)keys[i] >> 16 | keys[i] << 16;
        if (k == testVal) {
            /* MAX_CHAINED_SWITCH_CASES + 1 is the start of the overflow case */
            int jumpIndex = (i < MAX_CHAINED_SWITCH_CASES) ?
                           i : MAX_CHAINED_SWITCH_CASES + 1;
            int temp = (unsigned int)entries[i] >> 16 | entries[i] << 16;
            return (((u8) temp) << 32) | (u8) (jumpIndex * CHAIN_CELL_NORMAL_SIZE + 20);
#endif
        } else if (k > testVal) {
            break;
        }
    }
    return MIN(size, MAX_CHAINED_SWITCH_CASES) * CHAIN_CELL_NORMAL_SIZE + 20;
}

static bool handleFmt31t(CompilationUnit *cUnit, MIR *mir)
{
    Opcode dalvikOpcode = mir->dalvikInsn.opcode;
    switch (dalvikOpcode) {
        case OP_FILL_ARRAY_DATA: {
            RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            // Making a call - use explicit registers
            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            genExportPC(cUnit, mir);
            loadValueDirectFixed(cUnit, rlSrc, r_A0);
            LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmInterpHandleFillArrayData);
            loadConstant(cUnit, r_A1,
               (int) (cUnit->method->insns + mir->offset + mir->dalvikInsn.vB));
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /* generate a branch over if successful */
            MipsLIR *branchOver = opCompareBranch(cUnit, kMipsBne, r_V0, r_ZERO);
            loadConstant(cUnit, r_A0,
                         (int) (cUnit->method->insns + mir->offset));
            genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            branchOver->generic.target = (LIR *) target;
            break;
        }
        /*
         * Compute the goto target of up to
         * MIN(switchSize, MAX_CHAINED_SWITCH_CASES) + 1 chaining cells.
         * See the comment before findPackedSwitchIndex for the code layout.
         */
        case OP_PACKED_SWITCH:
        case OP_SPARSE_SWITCH: {
            RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            loadValueDirectFixed(cUnit, rlSrc, r_A1);
            dvmCompilerLockAllTemps(cUnit);

            if (dalvikOpcode == OP_PACKED_SWITCH) {
                LOAD_FUNC_ADDR(cUnit, r_T9, (int)findPackedSwitchIndex);
            } else {
                LOAD_FUNC_ADDR(cUnit, r_T9, (int)findSparseSwitchIndex);
            }
            /* r_A0 <- Addr of the switch data */
            loadConstant(cUnit, r_A0,
               (int) (cUnit->method->insns + mir->offset + mir->dalvikInsn.vB));
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /* pc <- computed goto target using value in RA */
            newLIR3(cUnit, kMipsAddu, r_A0, r_RA, r_RESULT0);
            newLIR2(cUnit, kMipsMove, r_A1, r_RESULT1);
            newLIR1(cUnit, kMipsJr, r_A0);
            newLIR0(cUnit, kMipsNop); /* for maintaining 20 byte offset */
            break;
        }
        default:
            return true;
    }
    return false;
}

/*
 * See the example of predicted inlining listed before the
 * genValidationForPredictedInline function. The function here takes care the
 * branch over at 0x4858de78 and the misprediction target at 0x4858de7a.
 */
static void genLandingPadForMispredictedCallee(CompilationUnit *cUnit, MIR *mir,
                                               BasicBlock *bb,
                                               MipsLIR *labelList)
{
    BasicBlock *fallThrough = bb->fallThrough;

    /* Bypass the move-result block if there is one */
    if (fallThrough->firstMIRInsn) {
        assert(fallThrough->firstMIRInsn->OptimizationFlags & MIR_INLINED_PRED);
        fallThrough = fallThrough->fallThrough;
    }
    /* Generate a branch over if the predicted inlining is correct */
    genUnconditionalBranch(cUnit, &labelList[fallThrough->id]);

    /* Reset the register state */
    dvmCompilerResetRegPool(cUnit);
    dvmCompilerClobberAllRegs(cUnit);
    dvmCompilerResetNullCheck(cUnit);

    /* Target for the slow invoke path */
    MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
    target->defMask = ENCODE_ALL;
    /* Hook up the target to the verification branch */
    mir->meta.callsiteInfo->misPredBranchOver->target = (LIR *) target;
}

static bool handleFmt35c_3rc(CompilationUnit *cUnit, MIR *mir,
                             BasicBlock *bb, MipsLIR *labelList)
{
    MipsLIR *retChainingCell = NULL;
    MipsLIR *pcrLabel = NULL;

    /* An invoke with the MIR_INLINED is effectively a no-op */
    if (mir->OptimizationFlags & MIR_INLINED)
        return false;

    if (bb->fallThrough != NULL)
        retChainingCell = &labelList[bb->fallThrough->id];

    DecodedInstruction *dInsn = &mir->dalvikInsn;
    switch (mir->dalvikInsn.opcode) {
        /*
         * calleeMethod = this->clazz->vtable[
         *     method->clazz->pDvmDex->pResMethods[BBBB]->methodIndex
         * ]
         */
        case OP_INVOKE_VIRTUAL:
        case OP_INVOKE_VIRTUAL_RANGE: {
            MipsLIR *predChainingCell = &labelList[bb->taken->id];
            int methodIndex =
                cUnit->method->clazz->pDvmDex->pResMethods[dInsn->vB]->
                methodIndex;

            /*
             * If the invoke has non-null misPredBranchOver, we need to generate
             * the non-inlined version of the invoke here to handle the
             * mispredicted case.
             */
            if (mir->meta.callsiteInfo->misPredBranchOver) {
                genLandingPadForMispredictedCallee(cUnit, mir, bb, labelList);
            }

            if (mir->dalvikInsn.opcode == OP_INVOKE_VIRTUAL)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            genInvokeVirtualCommon(cUnit, mir, methodIndex,
                                   retChainingCell,
                                   predChainingCell,
                                   pcrLabel);
            break;
        }
        /*
         * calleeMethod = method->clazz->super->vtable[method->clazz->pDvmDex
         *                ->pResMethods[BBBB]->methodIndex]
         */
        case OP_INVOKE_SUPER:
        case OP_INVOKE_SUPER_RANGE: {
            /* Grab the method ptr directly from what the interpreter sees */
            const Method *calleeMethod = mir->meta.callsiteInfo->method;
            assert(calleeMethod == cUnit->method->clazz->super->vtable[
                                     cUnit->method->clazz->pDvmDex->
                                       pResMethods[dInsn->vB]->methodIndex]);

            if (mir->dalvikInsn.opcode == OP_INVOKE_SUPER)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            if (mir->OptimizationFlags & MIR_INVOKE_METHOD_JIT) {
                const Method *calleeMethod = mir->meta.callsiteInfo->method;
                void *calleeAddr = dvmJitGetMethodAddr(calleeMethod->insns);
                assert(calleeAddr);
                genInvokeSingletonWholeMethod(cUnit, mir, calleeAddr,
                                              retChainingCell);
            } else {
                /* r_A0 = calleeMethod */
                loadConstant(cUnit, r_A0, (int) calleeMethod);

                genInvokeSingletonCommon(cUnit, mir, bb, labelList, pcrLabel,
                                         calleeMethod);
            }
            break;
        }
        /* calleeMethod = method->clazz->pDvmDex->pResMethods[BBBB] */
        case OP_INVOKE_DIRECT:
        case OP_INVOKE_DIRECT_RANGE: {
            /* Grab the method ptr directly from what the interpreter sees */
            const Method *calleeMethod = mir->meta.callsiteInfo->method;
            assert(calleeMethod ==
                   cUnit->method->clazz->pDvmDex->pResMethods[dInsn->vB]);

            if (mir->dalvikInsn.opcode == OP_INVOKE_DIRECT)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            /* r_A0 = calleeMethod */
            loadConstant(cUnit, r_A0, (int) calleeMethod);

            genInvokeSingletonCommon(cUnit, mir, bb, labelList, pcrLabel,
                                     calleeMethod);
            break;
        }
        /* calleeMethod = method->clazz->pDvmDex->pResMethods[BBBB] */
        case OP_INVOKE_STATIC:
        case OP_INVOKE_STATIC_RANGE: {
            /* Grab the method ptr directly from what the interpreter sees */
            const Method *calleeMethod = mir->meta.callsiteInfo->method;
            assert(calleeMethod ==
                   cUnit->method->clazz->pDvmDex->pResMethods[dInsn->vB]);

            if (mir->dalvikInsn.opcode == OP_INVOKE_STATIC)
                genProcessArgsNoRange(cUnit, mir, dInsn,
                                      NULL /* no null check */);
            else
                genProcessArgsRange(cUnit, mir, dInsn,
                                    NULL /* no null check */);

            if (mir->OptimizationFlags & MIR_INVOKE_METHOD_JIT) {
                const Method *calleeMethod = mir->meta.callsiteInfo->method;
                void *calleeAddr = dvmJitGetMethodAddr(calleeMethod->insns);
                assert(calleeAddr);
                genInvokeSingletonWholeMethod(cUnit, mir, calleeAddr,
                                              retChainingCell);
            } else {
                /* r_A0 = calleeMethod */
                loadConstant(cUnit, r_A0, (int) calleeMethod);

                genInvokeSingletonCommon(cUnit, mir, bb, labelList, pcrLabel,
                                         calleeMethod);
            }
            break;
        }

        /*
         * calleeMethod = dvmFindInterfaceMethodInCache(this->clazz,
         *                    BBBB, method, method->clazz->pDvmDex)
         *
         * The following is an example of generated code for
         *      "invoke-interface v0"
         *
         * -------- dalvik offset: 0x000f @ invoke-interface (PI) v2
         * 0x2f140c54 : lw       a0,8(s1)                    # genProcessArgsNoRange
         * 0x2f140c58 : addiu    s4,s1,0xffffffe8(-24)
         * 0x2f140c5c : beqz     a0,0x2f140d5c (L0x11f864)
         * 0x2f140c60 : pref     1,0(s4)
         * -------- BARRIER
         * 0x2f140c64 : sw       a0,0(s4)
         * 0x2f140c68 : addiu    s4,s4,0x0004(4)
         * -------- BARRIER
         * 0x2f140c6c : lui      s0,0x2d23(11555)            # dalvikPC
         * 0x2f140c70 : ori      s0,s0,0x2d2365a6(757294502)
         * 0x2f140c74 : lahi/lui a1,0x2f14(12052)            # a1 <- &retChainingCell
         * 0x2f140c78 : lalo/ori a1,a1,0x2f140d38(789843256)
         * 0x2f140c7c : lahi/lui a2,0x2f14(12052)            # a2 <- &predictedChainingCell
         * 0x2f140c80 : lalo/ori a2,a2,0x2f140d80(789843328)
         * 0x2f140c84 : jal      0x2f1311ec(789778924)       # call TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN
         * 0x2f140c88 : nop
         * 0x2f140c8c : b        0x2f140d80 (L0x11efc0)      # off to the predicted chain
         * 0x2f140c90 : nop
         * 0x2f140c94 : b        0x2f140d60 (L0x12457c)      # punt to the interpreter
         * 0x2f140c98 : lui      a0,0x2d23(11555)
         * 0x2f140c9c : move     s5,a1                       # prepare for dvmFindInterfaceMethodInCache
         * 0x2f140ca0 : move     s6,a2
         * 0x2f140ca4 : move     s7,a3
         * 0x2f140ca8 : move     a0,a3
         * 0x2f140cac : ori      a1,zero,0x2b42(11074)
         * 0x2f140cb0 : lui      a2,0x2c92(11410)
         * 0x2f140cb4 : ori      a2,a2,0x2c92adf8(747810296)
         * 0x2f140cb8 : lui      a3,0x0009(9)
         * 0x2f140cbc : ori      a3,a3,0x924b8(599224)
         * 0x2f140cc0 : lui      t9,0x2ab2(10930)
         * 0x2f140cc4 : ori      t9,t9,0x2ab2a48c(716350604)
         * 0x2f140cc8 : jalr     ra,t9                       # call dvmFindInterfaceMethodInCache
         * 0x2f140ccc : nop
         * 0x2f140cd0 : lw       gp,84(sp)
         * 0x2f140cd4 : move     a0,v0
         * 0x2f140cd8 : bne      v0,zero,0x2f140cf0 (L0x120064)
         * 0x2f140cdc : nop
         * 0x2f140ce0 : lui      a0,0x2d23(11555)            # a0 <- dalvikPC
         * 0x2f140ce4 : ori      a0,a0,0x2d2365a6(757294502)
         * 0x2f140ce8 : jal      0x2f131720(789780256)       # call TEMPLATE_THROW_EXCEPTION_COMMON
         * 0x2f140cec : nop
         * 0x2f140cf0 : move     a1,s5                       # a1 <- &retChainingCell
         * 0x2f140cf4 : bgtz     s5,0x2f140d20 (L0x120324)   # >0? don't rechain
         * 0x2f140cf8 : nop
         * 0x2f140cfc : lui      t9,0x2aba(10938)            # prepare for dvmJitToPatchPredictedChain
         * 0x2f140d00 : ori      t9,t9,0x2abae3c4(716891076)
         * 0x2f140d04 : move     a1,s2
         * 0x2f140d08 : move     a2,s6
         * 0x2f140d0c : move     a3,s7
         * 0x2f140d10 : jalr     ra,t9                       # call dvmJitToPatchPredictedChain
         * 0x2f140d14 : nop
         * 0x2f140d18 : lw       gp,84(sp)
         * 0x2f140d1c : move     a0,v0
         * 0x2f140d20 : lahi/lui a1,0x2f14(12052)
         * 0x2f140d24 : lalo/ori a1,a1,0x2f140d38(789843256) # a1 <- &retChainingCell
         * 0x2f140d28 : jal      0x2f1310c4(789778628)       # call TEMPLATE_INVOKE_METHOD_NO_OPT
         * 0x2f140d2c : nop
         * 0x2f140d30 : b        0x2f140d60 (L0x12457c)
         * 0x2f140d34 : lui      a0,0x2d23(11555)
         * 0x2f140d38 : .align4
         * -------- dalvik offset: 0x0012 @ move-result (PI) v1, (#0), (#0)
         * 0x2f140d38 : lw       a2,16(s2)
         * 0x2f140d3c : sw       a2,4(s1)
         * 0x2f140d40 : b        0x2f140d74 (L0x1246fc)
         * 0x2f140d44 : lw       a0,116(s2)
         * 0x2f140d48 : undefined
         * -------- reconstruct dalvik PC : 0x2d2365a6 @ +0x000f
         * 0x2f140d4c : lui      a0,0x2d23(11555)
         * 0x2f140d50 : ori      a0,a0,0x2d2365a6(757294502)
         * 0x2f140d54 : b        0x2f140d68 (L0x12463c)
         * 0x2f140d58 : lw       a1,108(s2)
         * -------- reconstruct dalvik PC : 0x2d2365a6 @ +0x000f
         * 0x2f140d5c : lui      a0,0x2d23(11555)
         * 0x2f140d60 : ori      a0,a0,0x2d2365a6(757294502)
         * Exception_Handling:
         * 0x2f140d64 : lw       a1,108(s2)
         * 0x2f140d68 : jalr     ra,a1
         * 0x2f140d6c : nop
         * 0x2f140d70 : .align4
         * -------- chaining cell (hot): 0x0013
         * 0x2f140d70 : lw       a0,116(s2)
         * 0x2f140d74 : jalr     ra,a0
         * 0x2f140d78 : nop
         * 0x2f140d7c : data     0x2d2365ae(757294510)
         * 0x2f140d80 : .align4
         * -------- chaining cell (predicted): N/A
         * 0x2f140d80 : data     0xe7fe(59390)
         * 0x2f140d84 : data     0x0000(0)
         * 0x2f140d88 : data     0x0000(0)
         * 0x2f140d8c : data     0x0000(0)
         * 0x2f140d90 : data     0x0000(0)
         * -------- end of chaining cells (0x0190)
         */
        case OP_INVOKE_INTERFACE:
        case OP_INVOKE_INTERFACE_RANGE: {
            MipsLIR *predChainingCell = &labelList[bb->taken->id];

            /*
             * If the invoke has non-null misPredBranchOver, we need to generate
             * the non-inlined version of the invoke here to handle the
             * mispredicted case.
             */
            if (mir->meta.callsiteInfo->misPredBranchOver) {
                genLandingPadForMispredictedCallee(cUnit, mir, bb, labelList);
            }

            if (mir->dalvikInsn.opcode == OP_INVOKE_INTERFACE)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            /* "this" is already left in r_A0 by genProcessArgs* */

            /* r4PC = dalvikCallsite */
            loadConstant(cUnit, r4PC,
                         (int) (cUnit->method->insns + mir->offset));

            /* r_A1 = &retChainingCell */
            MipsLIR *addrRetChain = newLIR2(cUnit, kMipsLahi, r_A1, 0);
            addrRetChain->generic.target = (LIR *) retChainingCell;
            addrRetChain = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
            addrRetChain->generic.target = (LIR *) retChainingCell;


            /* r_A2 = &predictedChainingCell */
            MipsLIR *predictedChainingCell = newLIR2(cUnit, kMipsLahi, r_A2, 0);
            predictedChainingCell->generic.target = (LIR *) predChainingCell;
            predictedChainingCell = newLIR3(cUnit, kMipsLalo, r_A2, r_A2, 0);
            predictedChainingCell->generic.target = (LIR *) predChainingCell;

            genDispatchToHandler(cUnit, gDvmJit.methodTraceSupport ?
                TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN_PROF :
                TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN);

            /* return through ra - jump to the chaining cell */
            genUnconditionalBranch(cUnit, predChainingCell);

            /*
             * null-check on "this" may have been eliminated, but we still need
             * a PC-reconstruction label for stack overflow bailout.
             */
            if (pcrLabel == NULL) {
                int dPC = (int) (cUnit->method->insns + mir->offset);
                pcrLabel = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
                pcrLabel->opcode = kMipsPseudoPCReconstructionCell;
                pcrLabel->operands[0] = dPC;
                pcrLabel->operands[1] = mir->offset;
                /* Insert the place holder to the growable list */
                dvmInsertGrowableList(&cUnit->pcReconstructionList,
                                      (intptr_t) pcrLabel);
            }

            /* return through ra+8 - punt to the interpreter */
            genUnconditionalBranch(cUnit, pcrLabel);

            /*
             * return through ra+16 - fully resolve the callee method.
             * r_A1 <- count
             * r_A2 <- &predictedChainCell
             * r_A3 <- this->class
             * r4 <- dPC
             * r_S4 <- this->class->vtable
             */

            /* Save count, &predictedChainCell, and class to high regs first */
            genRegCopy(cUnit, r_S5, r_A1);
            genRegCopy(cUnit, r_S6, r_A2);
            genRegCopy(cUnit, r_S7, r_A3);

            /* r_A0 now contains this->clazz */
            genRegCopy(cUnit, r_A0, r_A3);

            /* r_A1 = BBBB */
            loadConstant(cUnit, r_A1, dInsn->vB);

            /* r_A2 = method (caller) */
            loadConstant(cUnit, r_A2, (int) cUnit->method);

            /* r_A3 = pDvmDex */
            loadConstant(cUnit, r_A3, (int) cUnit->method->clazz->pDvmDex);

            LOAD_FUNC_ADDR(cUnit, r_T9,
                           (intptr_t) dvmFindInterfaceMethodInCache);
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            /* r_V0 = calleeMethod (returned from dvmFindInterfaceMethodInCache */
            genRegCopy(cUnit, r_A0, r_V0);

            dvmCompilerClobberCallRegs(cUnit);
            /* generate a branch over if the interface method is resolved */
            MipsLIR *branchOver = opCompareBranch(cUnit, kMipsBne, r_V0, r_ZERO);
            /*
             * calleeMethod == NULL -> throw
             */
            loadConstant(cUnit, r_A0,
                         (int) (cUnit->method->insns + mir->offset));
            genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
            /* noreturn */

            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            branchOver->generic.target = (LIR *) target;

            genRegCopy(cUnit, r_A1, r_S5);

            /* Check if rechain limit is reached */
            MipsLIR *bypassRechaining = opCompareBranch(cUnit, kMipsBgtz, r_S5, -1);

            LOAD_FUNC_ADDR(cUnit, r_T9, (int) dvmJitToPatchPredictedChain);

            genRegCopy(cUnit, r_A1, rSELF);
            genRegCopy(cUnit, r_A2, r_S6);
            genRegCopy(cUnit, r_A3, r_S7);

            /*
             * r_A0 = calleeMethod
             * r_A2 = &predictedChainingCell
             * r_A3 = class
             *
             * &returnChainingCell has been loaded into r_A1 but is not needed
             * when patching the chaining cell and will be clobbered upon
             * returning so it will be reconstructed again.
             */
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            genRegCopy(cUnit, r_A0, r_V0);

            /* r_A1 = &retChainingCell */
            addrRetChain = newLIR2(cUnit, kMipsLahi, r_A1, 0);
            addrRetChain->generic.target = (LIR *) retChainingCell;
            bypassRechaining->generic.target = (LIR *) addrRetChain;
            addrRetChain = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
            addrRetChain->generic.target = (LIR *) retChainingCell;


            /*
             * r_A0 = this, r_A1 = calleeMethod,
             * r_A1 = &ChainingCell,
             * r4PC = callsiteDPC,
             */
            genDispatchToHandler(cUnit, gDvmJit.methodTraceSupport ?
                TEMPLATE_INVOKE_METHOD_NO_OPT_PROF :
                TEMPLATE_INVOKE_METHOD_NO_OPT);

#if defined(WITH_JIT_TUNING)
            gDvmJit.invokePolymorphic++;
#endif
            /* Handle exceptions using the interpreter */
            genTrap(cUnit, mir->offset, pcrLabel);
            break;
        }
        case OP_INVOKE_OBJECT_INIT_RANGE:
        case OP_FILLED_NEW_ARRAY:
        case OP_FILLED_NEW_ARRAY_RANGE: {
            /* Just let the interpreter deal with these */
            genInterpSingleStep(cUnit, mir);
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt35ms_3rms(CompilationUnit *cUnit, MIR *mir,
                               BasicBlock *bb, MipsLIR *labelList)
{
    MipsLIR *pcrLabel = NULL;

    /* An invoke with the MIR_INLINED is effectively a no-op */
    if (mir->OptimizationFlags & MIR_INLINED)
        return false;

    DecodedInstruction *dInsn = &mir->dalvikInsn;
    switch (mir->dalvikInsn.opcode) {
        /* calleeMethod = this->clazz->vtable[BBBB] */
        case OP_INVOKE_VIRTUAL_QUICK_RANGE:
        case OP_INVOKE_VIRTUAL_QUICK: {
            int methodIndex = dInsn->vB;
            MipsLIR *retChainingCell = &labelList[bb->fallThrough->id];
            MipsLIR *predChainingCell = &labelList[bb->taken->id];

            /*
             * If the invoke has non-null misPredBranchOver, we need to generate
             * the non-inlined version of the invoke here to handle the
             * mispredicted case.
             */
            if (mir->meta.callsiteInfo->misPredBranchOver) {
                genLandingPadForMispredictedCallee(cUnit, mir, bb, labelList);
            }

            if (mir->dalvikInsn.opcode == OP_INVOKE_VIRTUAL_QUICK)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            if (mir->OptimizationFlags & MIR_INVOKE_METHOD_JIT) {
                const Method *calleeMethod = mir->meta.callsiteInfo->method;
                void *calleeAddr = dvmJitGetMethodAddr(calleeMethod->insns);
                assert(calleeAddr);
                genInvokeVirtualWholeMethod(cUnit, mir, calleeAddr,
                                            retChainingCell);
            }

            genInvokeVirtualCommon(cUnit, mir, methodIndex,
                                   retChainingCell,
                                   predChainingCell,
                                   pcrLabel);
            break;
        }
        /* calleeMethod = method->clazz->super->vtable[BBBB] */
        case OP_INVOKE_SUPER_QUICK:
        case OP_INVOKE_SUPER_QUICK_RANGE: {
            /* Grab the method ptr directly from what the interpreter sees */
            const Method *calleeMethod = mir->meta.callsiteInfo->method;
            assert(calleeMethod ==
                   cUnit->method->clazz->super->vtable[dInsn->vB]);

            if (mir->dalvikInsn.opcode == OP_INVOKE_SUPER_QUICK)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            /* r_A0 = calleeMethod */
            loadConstant(cUnit, r_A0, (int) calleeMethod);

            genInvokeSingletonCommon(cUnit, mir, bb, labelList, pcrLabel,
                                     calleeMethod);
            break;
        }
        default:
            return true;
    }
    return false;
}

/*
 * This operation is complex enough that we'll do it partly inline
 * and partly with a handler.  NOTE: the handler uses hardcoded
 * values for string object offsets and must be revisitied if the
 * layout changes.
 */
static bool genInlinedCompareTo(CompilationUnit *cUnit, MIR *mir)
{
#if defined(USE_GLOBAL_STRING_DEFS)
    return handleExecuteInlineC(cUnit, mir);
#else
    MipsLIR *rollback;
    RegLocation rlThis = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlComp = dvmCompilerGetSrc(cUnit, mir, 1);

    loadValueDirectFixed(cUnit, rlThis, r_A0);
    loadValueDirectFixed(cUnit, rlComp, r_A1);
    /* Test objects for NULL */
    rollback = genNullCheck(cUnit, rlThis.sRegLow, r_A0, mir->offset, NULL);
    genNullCheck(cUnit, rlComp.sRegLow, r_A1, mir->offset, rollback);
    /*
     * TUNING: we could check for object pointer equality before invoking
     * handler. Unclear whether the gain would be worth the added code size
     * expansion.
     */
    genDispatchToHandler(cUnit, TEMPLATE_STRING_COMPARETO);
    storeValue(cUnit, inlinedTarget(cUnit, mir, false),
               dvmCompilerGetReturn(cUnit));
    return false;
#endif
}

static bool genInlinedFastIndexOf(CompilationUnit *cUnit, MIR *mir)
{
#if defined(USE_GLOBAL_STRING_DEFS)
    return handleExecuteInlineC(cUnit, mir);
#else
    RegLocation rlThis = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlChar = dvmCompilerGetSrc(cUnit, mir, 1);

    loadValueDirectFixed(cUnit, rlThis, r_A0);
    loadValueDirectFixed(cUnit, rlChar, r_A1);

    RegLocation rlStart = dvmCompilerGetSrc(cUnit, mir, 2);
    loadValueDirectFixed(cUnit, rlStart, r_A2);

    /* Test objects for NULL */
    genNullCheck(cUnit, rlThis.sRegLow, r_A0, mir->offset, NULL);
    genDispatchToHandler(cUnit, TEMPLATE_STRING_INDEXOF);
    storeValue(cUnit, inlinedTarget(cUnit, mir, false),
               dvmCompilerGetReturn(cUnit));
    return false;
#endif
}

// Generates an inlined String.isEmpty or String.length.
static bool genInlinedStringIsEmptyOrLength(CompilationUnit *cUnit, MIR *mir,
                                            bool isEmpty)
{
    // dst = src.length();
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlDest = inlinedTarget(cUnit, mir, false);
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg, mir->offset, NULL);
    loadWordDisp(cUnit, rlObj.lowReg, gDvm.offJavaLangString_count,
                 rlResult.lowReg);
    if (isEmpty) {
        // dst = (dst == 0);
        int tReg = dvmCompilerAllocTemp(cUnit);
        newLIR3(cUnit, kMipsSltu, tReg, r_ZERO, rlResult.lowReg);
        opRegRegImm(cUnit, kOpXor, rlResult.lowReg, tReg, 1);
    }
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static bool genInlinedStringLength(CompilationUnit *cUnit, MIR *mir)
{
    return genInlinedStringIsEmptyOrLength(cUnit, mir, false);
}

static bool genInlinedStringIsEmpty(CompilationUnit *cUnit, MIR *mir)
{
    return genInlinedStringIsEmptyOrLength(cUnit, mir, true);
}

static bool genInlinedStringCharAt(CompilationUnit *cUnit, MIR *mir)
{
    int contents = OFFSETOF_MEMBER(ArrayObject, contents);
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlIdx = dvmCompilerGetSrc(cUnit, mir, 1);
    RegLocation rlDest = inlinedTarget(cUnit, mir, false);
    RegLocation rlResult;
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    rlIdx = loadValue(cUnit, rlIdx, kCoreReg);
    int regMax = dvmCompilerAllocTemp(cUnit);
    int regOff = dvmCompilerAllocTemp(cUnit);
    int regPtr = dvmCompilerAllocTemp(cUnit);
    MipsLIR *pcrLabel = genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg,
                                    mir->offset, NULL);
    loadWordDisp(cUnit, rlObj.lowReg, gDvm.offJavaLangString_count, regMax);
    loadWordDisp(cUnit, rlObj.lowReg, gDvm.offJavaLangString_offset, regOff);
    loadWordDisp(cUnit, rlObj.lowReg, gDvm.offJavaLangString_value, regPtr);
    genBoundsCheck(cUnit, rlIdx.lowReg, regMax, mir->offset, pcrLabel);
    dvmCompilerFreeTemp(cUnit, regMax);
    opRegImm(cUnit, kOpAdd, regPtr, contents);
    opRegReg(cUnit, kOpAdd, regOff, rlIdx.lowReg);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    loadBaseIndexed(cUnit, regPtr, regOff, rlResult.lowReg, 1, kUnsignedHalf);
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static bool genInlinedAbsInt(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
    RegLocation rlDest = inlinedTarget(cUnit, mir, false);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    int signReg = dvmCompilerAllocTemp(cUnit);
    /*
     * abs(x) = y<=x>>31, (x+y)^y.
     * Thumb2's IT block also yields 3 instructions, but imposes
     * scheduling constraints.
     */
    opRegRegImm(cUnit, kOpAsr, signReg, rlSrc.lowReg, 31);
    opRegRegReg(cUnit, kOpAdd, rlResult.lowReg, rlSrc.lowReg, signReg);
    opRegReg(cUnit, kOpXor, rlResult.lowReg, signReg);
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static bool genInlinedAbsLong(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
    RegLocation rlDest = inlinedTargetWide(cUnit, mir, false);
    rlSrc = loadValueWide(cUnit, rlSrc, kCoreReg);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    int signReg = dvmCompilerAllocTemp(cUnit);
    int tReg = dvmCompilerAllocTemp(cUnit);
    /*
     * abs(x) = y<=x>>31, (x+y)^y.
     * Thumb2 IT block allows slightly shorter sequence,
     * but introduces a scheduling barrier.  Stick with this
     * mechanism for now.
     */
    opRegRegImm(cUnit, kOpAsr, signReg, rlSrc.highReg, 31);
    opRegRegReg(cUnit, kOpAdd, rlResult.lowReg, rlSrc.lowReg, signReg);
    newLIR3(cUnit, kMipsSltu, tReg, rlResult.lowReg, signReg);
    opRegRegReg(cUnit, kOpAdd, rlResult.highReg, rlSrc.highReg, signReg);
    opRegRegReg(cUnit, kOpAdd, rlResult.highReg, rlResult.highReg, tReg);
    opRegReg(cUnit, kOpXor, rlResult.lowReg, signReg);
    opRegReg(cUnit, kOpXor, rlResult.highReg, signReg);
    dvmCompilerFreeTemp(cUnit, signReg);
    dvmCompilerFreeTemp(cUnit, tReg);
    storeValueWide(cUnit, rlDest, rlResult);
    return false;
}

static bool genInlinedIntFloatConversion(CompilationUnit *cUnit, MIR *mir)
{
    // Just move from source to destination...
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlDest = inlinedTarget(cUnit, mir, false);
    storeValue(cUnit, rlDest, rlSrc);
    return false;
}

static bool genInlinedLongDoubleConversion(CompilationUnit *cUnit, MIR *mir)
{
    // Just move from source to destination...
    RegLocation rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
    RegLocation rlDest = inlinedTargetWide(cUnit, mir, false);
    storeValueWide(cUnit, rlDest, rlSrc);
    return false;
}
/*
 * JITs a call to a C function.
 * TODO: use this for faster native method invocation for simple native
 * methods (http://b/3069458).
 */
static bool handleExecuteInlineC(CompilationUnit *cUnit, MIR *mir)
{
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    int operation = dInsn->vB;
    unsigned int i;
    const InlineOperation* inLineTable = dvmGetInlineOpsTable();
    uintptr_t fn = (int) inLineTable[operation].func;
    if (fn == 0) {
        dvmCompilerAbort(cUnit);
    }
    dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
    dvmCompilerClobberCallRegs(cUnit);
    dvmCompilerClobber(cUnit, r4PC);
    dvmCompilerClobber(cUnit, rINST);
    int offset = offsetof(Thread, interpSave.retval);
    opRegRegImm(cUnit, kOpAdd, r4PC, rSELF, offset);
    newLIR3(cUnit, kMipsSw, r4PC, 16, r_SP); /* sp has plenty of space */
    genExportPC(cUnit, mir);
    assert(dInsn->vA <= 4);
    for (i=0; i < dInsn->vA; i++) {
        loadValueDirect(cUnit, dvmCompilerGetSrc(cUnit, mir, i), i+r_A0);
    }
    LOAD_FUNC_ADDR(cUnit, r_T9, fn);
    opReg(cUnit, kOpBlx, r_T9);
    newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
    /* NULL? */
    MipsLIR *branchOver = opCompareBranch(cUnit, kMipsBne, r_V0, r_ZERO);
    loadConstant(cUnit, r_A0, (int) (cUnit->method->insns + mir->offset));
    genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
    MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
    target->defMask = ENCODE_ALL;
    branchOver->generic.target = (LIR *) target;
    return false;
}

/*
 * NOTE: Handles both range and non-range versions (arguments
 * have already been normalized by this point).
 */
static bool handleExecuteInline(CompilationUnit *cUnit, MIR *mir)
{
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    assert(dInsn->opcode == OP_EXECUTE_INLINE_RANGE ||
           dInsn->opcode == OP_EXECUTE_INLINE);
    switch (dInsn->vB) {
        case INLINE_EMPTYINLINEMETHOD:
            return false;  /* Nop */

        /* These ones we potentially JIT inline. */

        case INLINE_STRING_CHARAT:
            return genInlinedStringCharAt(cUnit, mir);
        case INLINE_STRING_LENGTH:
            return genInlinedStringLength(cUnit, mir);
        case INLINE_STRING_IS_EMPTY:
            return genInlinedStringIsEmpty(cUnit, mir);
        case INLINE_STRING_COMPARETO:
            return genInlinedCompareTo(cUnit, mir);
        case INLINE_STRING_FASTINDEXOF_II:
            return genInlinedFastIndexOf(cUnit, mir);

        case INLINE_MATH_ABS_INT:
        case INLINE_STRICT_MATH_ABS_INT:
            return genInlinedAbsInt(cUnit, mir);
        case INLINE_MATH_ABS_LONG:
        case INLINE_STRICT_MATH_ABS_LONG:
            return genInlinedAbsLong(cUnit, mir);
        case INLINE_MATH_MIN_INT:
        case INLINE_STRICT_MATH_MIN_INT:
            return genInlinedMinMaxInt(cUnit, mir, true);
        case INLINE_MATH_MAX_INT:
        case INLINE_STRICT_MATH_MAX_INT:
            return genInlinedMinMaxInt(cUnit, mir, false);
        case INLINE_MATH_SQRT:
        case INLINE_STRICT_MATH_SQRT:
            return genInlineSqrt(cUnit, mir);
        case INLINE_MATH_ABS_FLOAT:
        case INLINE_STRICT_MATH_ABS_FLOAT:
            return genInlinedAbsFloat(cUnit, mir);
        case INLINE_MATH_ABS_DOUBLE:
        case INLINE_STRICT_MATH_ABS_DOUBLE:
            return genInlinedAbsDouble(cUnit, mir);

        case INLINE_FLOAT_TO_RAW_INT_BITS:
        case INLINE_INT_BITS_TO_FLOAT:
            return genInlinedIntFloatConversion(cUnit, mir);
        case INLINE_DOUBLE_TO_RAW_LONG_BITS:
        case INLINE_LONG_BITS_TO_DOUBLE:
            return genInlinedLongDoubleConversion(cUnit, mir);

        /*
         * These ones we just JIT a call to a C function for.
         * TODO: special-case these in the other "invoke" call paths.
         */
        case INLINE_STRING_EQUALS:
        case INLINE_MATH_COS:
        case INLINE_MATH_SIN:
        case INLINE_FLOAT_TO_INT_BITS:
        case INLINE_DOUBLE_TO_LONG_BITS:
            return handleExecuteInlineC(cUnit, mir);
    }
    dvmCompilerAbort(cUnit);
    return false; // Not reachable; keeps compiler happy.
}

static bool handleFmt51l(CompilationUnit *cUnit, MIR *mir)
{
    //TUNING: We're using core regs here - not optimal when target is a double
    RegLocation rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    loadConstantNoClobber(cUnit, rlResult.lowReg,
                          mir->dalvikInsn.vB_wide & 0xFFFFFFFFUL);
    loadConstantNoClobber(cUnit, rlResult.highReg,
                          (mir->dalvikInsn.vB_wide>>32) & 0xFFFFFFFFUL);
    storeValueWide(cUnit, rlDest, rlResult);
    return false;
}

/*
 * The following are special processing routines that handle transfer of
 * controls between compiled code and the interpreter. Certain VM states like
 * Dalvik PC and special-purpose registers are reconstructed here.
 */

/* Chaining cell for code that may need warmup. */
static void handleNormalChainingCell(CompilationUnit *cUnit,
                                     unsigned int offset)
{
    newLIR3(cUnit, kMipsLw, r_A0,
        offsetof(Thread, jitToInterpEntries.dvmJitToInterpNormal),
        rSELF);
    newLIR2(cUnit, kMipsJalr, r_RA, r_A0);
    addWordData(cUnit, NULL, (int) (cUnit->method->insns + offset));
}

/*
 * Chaining cell for instructions that immediately following already translated
 * code.
 */
static void handleHotChainingCell(CompilationUnit *cUnit,
                                  unsigned int offset)
{
    newLIR3(cUnit, kMipsLw, r_A0,
        offsetof(Thread, jitToInterpEntries.dvmJitToInterpTraceSelect),
        rSELF);
    newLIR2(cUnit, kMipsJalr, r_RA, r_A0);
    addWordData(cUnit, NULL, (int) (cUnit->method->insns + offset));
}

/* Chaining cell for branches that branch back into the same basic block */
static void handleBackwardBranchChainingCell(CompilationUnit *cUnit,
                                             unsigned int offset)
{
    /*
     * Use raw instruction constructors to guarantee that the generated
     * instructions fit the predefined cell size.
     */
#if defined(WITH_SELF_VERIFICATION)
    newLIR3(cUnit, kMipsLw, r_A0,
        offsetof(Thread, jitToInterpEntries.dvmJitToInterpBackwardBranch),
        rSELF);
#else
    newLIR3(cUnit, kMipsLw, r_A0,
        offsetof(Thread, jitToInterpEntries.dvmJitToInterpNormal),
        rSELF);
#endif
    newLIR2(cUnit, kMipsJalr, r_RA, r_A0);
    addWordData(cUnit, NULL, (int) (cUnit->method->insns + offset));
}

/* Chaining cell for monomorphic method invocations. */
static void handleInvokeSingletonChainingCell(CompilationUnit *cUnit,
                                              const Method *callee)
{
    newLIR3(cUnit, kMipsLw, r_A0,
        offsetof(Thread, jitToInterpEntries.dvmJitToInterpTraceSelect),
        rSELF);
    newLIR2(cUnit, kMipsJalr, r_RA, r_A0);
    addWordData(cUnit, NULL, (int) (callee->insns));
}

/* Chaining cell for monomorphic method invocations. */
static void handleInvokePredictedChainingCell(CompilationUnit *cUnit)
{
    /* Should not be executed in the initial state */
    addWordData(cUnit, NULL, PREDICTED_CHAIN_BX_PAIR_INIT);
    /* branch delay slot nop */
    addWordData(cUnit, NULL, PREDICTED_CHAIN_DELAY_SLOT_INIT);
    /* To be filled: class */
    addWordData(cUnit, NULL, PREDICTED_CHAIN_CLAZZ_INIT);
    /* To be filled: method */
    addWordData(cUnit, NULL, PREDICTED_CHAIN_METHOD_INIT);
    /*
     * Rechain count. The initial value of 0 here will trigger chaining upon
     * the first invocation of this callsite.
     */
    addWordData(cUnit, NULL, PREDICTED_CHAIN_COUNTER_INIT);
}

/* Load the Dalvik PC into a0 and jump to the specified target */
static void handlePCReconstruction(CompilationUnit *cUnit,
                                   MipsLIR *targetLabel)
{
    MipsLIR **pcrLabel =
        (MipsLIR **) cUnit->pcReconstructionList.elemList;
    int numElems = cUnit->pcReconstructionList.numUsed;
    int i;

    /*
     * We should never reach here through fall-through code, so insert
     * a bomb to signal troubles immediately.
     */
    if (numElems) {
        newLIR0(cUnit, kMipsUndefined);
    }

    for (i = 0; i < numElems; i++) {
        dvmCompilerAppendLIR(cUnit, (LIR *) pcrLabel[i]);
        /* a0 = dalvik PC */
        loadConstant(cUnit, r_A0, pcrLabel[i]->operands[0]);
        genUnconditionalBranch(cUnit, targetLabel);
    }
}

static const char *extendedMIROpNames[kMirOpLast - kMirOpFirst] = {
    "kMirOpPhi",
    "kMirOpNullNRangeUpCheck",
    "kMirOpNullNRangeDownCheck",
    "kMirOpLowerBound",
    "kMirOpPunt",
    "kMirOpCheckInlinePrediction",
};

/*
 * vA = arrayReg;
 * vB = idxReg;
 * vC = endConditionReg;
 * arg[0] = maxC
 * arg[1] = minC
 * arg[2] = loopBranchConditionCode
 */
static void genHoistedChecksForCountUpLoop(CompilationUnit *cUnit, MIR *mir)
{
    /*
     * NOTE: these synthesized blocks don't have ssa names assigned
     * for Dalvik registers.  However, because they dominate the following
     * blocks we can simply use the Dalvik name w/ subscript 0 as the
     * ssa name.
     */
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    const int lenOffset = OFFSETOF_MEMBER(ArrayObject, length);
    const int maxC = dInsn->arg[0];
    int regLength;
    RegLocation rlArray = cUnit->regLocation[mir->dalvikInsn.vA];
    RegLocation rlIdxEnd = cUnit->regLocation[mir->dalvikInsn.vC];

    /* regArray <- arrayRef */
    rlArray = loadValue(cUnit, rlArray, kCoreReg);
    rlIdxEnd = loadValue(cUnit, rlIdxEnd, kCoreReg);
    genRegImmCheck(cUnit, kMipsCondEq, rlArray.lowReg, 0, 0,
                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);

    /* regLength <- len(arrayRef) */
    regLength = dvmCompilerAllocTemp(cUnit);
    loadWordDisp(cUnit, rlArray.lowReg, lenOffset, regLength);

    int delta = maxC;
    /*
     * If the loop end condition is ">=" instead of ">", then the largest value
     * of the index is "endCondition - 1".
     */
    if (dInsn->arg[2] == OP_IF_GE) {
        delta--;
    }

    if (delta) {
        int tReg = dvmCompilerAllocTemp(cUnit);
        opRegRegImm(cUnit, kOpAdd, tReg, rlIdxEnd.lowReg, delta);
        rlIdxEnd.lowReg = tReg;
        dvmCompilerFreeTemp(cUnit, tReg);
    }
    /* Punt if "regIdxEnd < len(Array)" is false */
    genRegRegCheck(cUnit, kMipsCondGe, rlIdxEnd.lowReg, regLength, 0,
                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);
}

/*
 * vA = arrayReg;
 * vB = idxReg;
 * vC = endConditionReg;
 * arg[0] = maxC
 * arg[1] = minC
 * arg[2] = loopBranchConditionCode
 */
static void genHoistedChecksForCountDownLoop(CompilationUnit *cUnit, MIR *mir)
{
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    const int lenOffset = OFFSETOF_MEMBER(ArrayObject, length);
    const int regLength = dvmCompilerAllocTemp(cUnit);
    const int maxC = dInsn->arg[0];
    RegLocation rlArray = cUnit->regLocation[mir->dalvikInsn.vA];
    RegLocation rlIdxInit = cUnit->regLocation[mir->dalvikInsn.vB];

    /* regArray <- arrayRef */
    rlArray = loadValue(cUnit, rlArray, kCoreReg);
    rlIdxInit = loadValue(cUnit, rlIdxInit, kCoreReg);
    genRegImmCheck(cUnit, kMipsCondEq, rlArray.lowReg, 0, 0,
                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);

    /* regLength <- len(arrayRef) */
    loadWordDisp(cUnit, rlArray.lowReg, lenOffset, regLength);

    if (maxC) {
        int tReg = dvmCompilerAllocTemp(cUnit);
        opRegRegImm(cUnit, kOpAdd, tReg, rlIdxInit.lowReg, maxC);
        rlIdxInit.lowReg = tReg;
        dvmCompilerFreeTemp(cUnit, tReg);
    }

    /* Punt if "regIdxInit < len(Array)" is false */
    genRegRegCheck(cUnit, kMipsCondGe, rlIdxInit.lowReg, regLength, 0,
                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);
}

/*
 * vA = idxReg;
 * vB = minC;
 */
static void genHoistedLowerBoundCheck(CompilationUnit *cUnit, MIR *mir)
{
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    const int minC = dInsn->vB;
    RegLocation rlIdx = cUnit->regLocation[mir->dalvikInsn.vA];

    /* regIdx <- initial index value */
    rlIdx = loadValue(cUnit, rlIdx, kCoreReg);

    /* Punt if "regIdxInit + minC >= 0" is false */
    genRegImmCheck(cUnit, kMipsCondLt, rlIdx.lowReg, -minC, 0,
                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);
}

/*
 * vC = this
 *
 * A predicted inlining target looks like the following, where instructions
 * between 0x2f130d24 and 0x2f130d40 are checking if the predicted class
 * matches "this", and the verificaion code is generated by this routine.
 *
 * (C) means the instruction is inlined from the callee, and (PI) means the
 * instruction is the predicted inlined invoke, whose corresponding
 * instructions are still generated to handle the mispredicted case.
 *
 * D/dalvikvm( 2377): -------- kMirOpCheckInlinePrediction
 * D/dalvikvm( 2377): 0x2f130d24 (0020):  lw       v0,16(s1)
 * D/dalvikvm( 2377): 0x2f130d28 (0024):  lui      v1,0x0011(17)
 * D/dalvikvm( 2377): 0x2f130d2c (0028):  ori      v1,v1,0x11e418(1172504)
 * D/dalvikvm( 2377): 0x2f130d30 (002c):  beqz     v0,0x2f130df0 (L0x11f1f0)
 * D/dalvikvm( 2377): 0x2f130d34 (0030):  pref     0,0(v0)
 * D/dalvikvm( 2377): 0x2f130d38 (0034):  lw       a0,0(v0)
 * D/dalvikvm( 2377): 0x2f130d3c (0038):  bne      v1,a0,0x2f130d54 (L0x11f518)
 * D/dalvikvm( 2377): 0x2f130d40 (003c):  pref     0,8(v0)
 * D/dalvikvm( 2377): -------- dalvik offset: 0x000a @ +iget-object-quick (C) v3, v4, (#8)
 * D/dalvikvm( 2377): 0x2f130d44 (0040):  lw       a1,8(v0)
 * D/dalvikvm( 2377): -------- dalvik offset: 0x000a @ +invoke-virtual-quick (PI) v4
 * D/dalvikvm( 2377): 0x2f130d48 (0044):  sw       a1,12(s1)
 * D/dalvikvm( 2377): 0x2f130d4c (0048):  b        0x2f130e18 (L0x120150)
 * D/dalvikvm( 2377): 0x2f130d50 (004c):  lw       a0,116(s2)
 * D/dalvikvm( 2377): L0x11f518:
 * D/dalvikvm( 2377): 0x2f130d54 (0050):  lw       a0,16(s1)
 * D/dalvikvm( 2377): 0x2f130d58 (0054):  addiu    s4,s1,0xffffffe8(-24)
 * D/dalvikvm( 2377): 0x2f130d5c (0058):  beqz     a0,0x2f130e00 (L0x11f618)
 * D/dalvikvm( 2377): 0x2f130d60 (005c):  pref     1,0(s4)
 * D/dalvikvm( 2377): -------- BARRIER
 * D/dalvikvm( 2377): 0x2f130d64 (0060):  sw       a0,0(s4)
 * D/dalvikvm( 2377): 0x2f130d68 (0064):  addiu    s4,s4,0x0004(4)
 * D/dalvikvm( 2377): -------- BARRIER
 * D/dalvikvm( 2377): 0x2f130d6c (0068):  lui      s0,0x2d22(11554)
 * D/dalvikvm( 2377): 0x2f130d70 (006c):  ori      s0,s0,0x2d228464(757236836)
 * D/dalvikvm( 2377): 0x2f130d74 (0070):  lahi/lui a1,0x2f13(12051)
 * D/dalvikvm( 2377): 0x2f130d78 (0074):  lalo/ori a1,a1,0x2f130ddc(789777884)
 * D/dalvikvm( 2377): 0x2f130d7c (0078):  lahi/lui a2,0x2f13(12051)
 * D/dalvikvm( 2377): 0x2f130d80 (007c):  lalo/ori a2,a2,0x2f130e24(789777956)
 * D/dalvikvm( 2377): 0x2f130d84 (0080):  jal      0x2f12d1ec(789762540)
 * D/dalvikvm( 2377): 0x2f130d88 (0084):  nop
 * D/dalvikvm( 2377): 0x2f130d8c (0088):  b        0x2f130e24 (L0x11ed6c)
 * D/dalvikvm( 2377): 0x2f130d90 (008c):  nop
 * D/dalvikvm( 2377): 0x2f130d94 (0090):  b        0x2f130e04 (L0x11ffd0)
 * D/dalvikvm( 2377): 0x2f130d98 (0094):  lui      a0,0x2d22(11554)
 * D/dalvikvm( 2377): 0x2f130d9c (0098):  lw       a0,44(s4)
 * D/dalvikvm( 2377): 0x2f130da0 (009c):  bgtz     a1,0x2f130dc4 (L0x11fb98)
 * D/dalvikvm( 2377): 0x2f130da4 (00a0):  nop
 * D/dalvikvm( 2377): 0x2f130da8 (00a4):  lui      t9,0x2aba(10938)
 * D/dalvikvm( 2377): 0x2f130dac (00a8):  ori      t9,t9,0x2abae3f8(716891128)
 * D/dalvikvm( 2377): 0x2f130db0 (00ac):  move     a1,s2
 * D/dalvikvm( 2377): 0x2f130db4 (00b0):  jalr     ra,t9
 * D/dalvikvm( 2377): 0x2f130db8 (00b4):  nop
 * D/dalvikvm( 2377): 0x2f130dbc (00b8):  lw       gp,84(sp)
 * D/dalvikvm( 2377): 0x2f130dc0 (00bc):  move     a0,v0
 * D/dalvikvm( 2377): 0x2f130dc4 (00c0):  lahi/lui a1,0x2f13(12051)
 * D/dalvikvm( 2377): 0x2f130dc8 (00c4):  lalo/ori a1,a1,0x2f130ddc(789777884)
 * D/dalvikvm( 2377): 0x2f130dcc (00c8):  jal      0x2f12d0c4(789762244)
 * D/dalvikvm( 2377): 0x2f130dd0 (00cc):  nop
 * D/dalvikvm( 2377): 0x2f130dd4 (00d0):  b        0x2f130e04 (L0x11ffd0)
 * D/dalvikvm( 2377): 0x2f130dd8 (00d4):  lui      a0,0x2d22(11554)
 * D/dalvikvm( 2377): 0x2f130ddc (00d8): .align4
 * D/dalvikvm( 2377): L0x11ed2c:
 * D/dalvikvm( 2377): -------- dalvik offset: 0x000d @ move-result-object (PI) v3, (#0), (#0)
 * D/dalvikvm( 2377): 0x2f130ddc (00d8):  lw       a2,16(s2)
 * D/dalvikvm( 2377): 0x2f130de0 (00dc):  sw       a2,12(s1)
 * D/dalvikvm( 2377): 0x2f130de4 (00e0):  b        0x2f130e18 (L0x120150)
 * D/dalvikvm( 2377): 0x2f130de8 (00e4):  lw       a0,116(s2)
 * D/dalvikvm( 2377): 0x2f130dec (00e8):  undefined
 * D/dalvikvm( 2377): L0x11f1f0:
 * D/dalvikvm( 2377): -------- reconstruct dalvik PC : 0x2d228464 @ +0x000a
 * D/dalvikvm( 2377): 0x2f130df0 (00ec):  lui      a0,0x2d22(11554)
 * D/dalvikvm( 2377): 0x2f130df4 (00f0):  ori      a0,a0,0x2d228464(757236836)
 * D/dalvikvm( 2377): 0x2f130df8 (00f4):  b        0x2f130e0c (L0x120090)
 * D/dalvikvm( 2377): 0x2f130dfc (00f8):  lw       a1,108(s2)
 * D/dalvikvm( 2377): L0x11f618:
 * D/dalvikvm( 2377): -------- reconstruct dalvik PC : 0x2d228464 @ +0x000a
 * D/dalvikvm( 2377): 0x2f130e00 (00fc):  lui      a0,0x2d22(11554)
 * D/dalvikvm( 2377): 0x2f130e04 (0100):  ori      a0,a0,0x2d228464(757236836)
 * D/dalvikvm( 2377): Exception_Handling:
 * D/dalvikvm( 2377): 0x2f130e08 (0104):  lw       a1,108(s2)
 * D/dalvikvm( 2377): 0x2f130e0c (0108):  jalr     ra,a1
 * D/dalvikvm( 2377): 0x2f130e10 (010c):  nop
 * D/dalvikvm( 2377): 0x2f130e14 (0110): .align4
 * D/dalvikvm( 2377): L0x11edac:
 * D/dalvikvm( 2377): -------- chaining cell (hot): 0x000e
 * D/dalvikvm( 2377): 0x2f130e14 (0110):  lw       a0,116(s2)
 * D/dalvikvm( 2377): 0x2f130e18 (0114):  jalr     ra,a0
 * D/dalvikvm( 2377): 0x2f130e1c (0118):  nop
 * D/dalvikvm( 2377): 0x2f130e20 (011c):  data     0x2d22846c(757236844)
 * D/dalvikvm( 2377): 0x2f130e24 (0120): .align4
 * D/dalvikvm( 2377): L0x11ed6c:
 * D/dalvikvm( 2377): -------- chaining cell (predicted)
 * D/dalvikvm( 2377): 0x2f130e24 (0120):  data     0xe7fe(59390)
 * D/dalvikvm( 2377): 0x2f130e28 (0124):  data     0x0000(0)
 * D/dalvikvm( 2377): 0x2f130e2c (0128):  data     0x0000(0)
 * D/dalvikvm( 2377): 0x2f130e30 (012c):  data     0x0000(0)
 * D/dalvikvm( 2377): 0x2f130e34 (0130):  data     0x0000(0)
 */
static void genValidationForPredictedInline(CompilationUnit *cUnit, MIR *mir)
{
    CallsiteInfo *callsiteInfo = mir->meta.callsiteInfo;
    RegLocation rlThis = cUnit->regLocation[mir->dalvikInsn.vC];

    rlThis = loadValue(cUnit, rlThis, kCoreReg);
    int regPredictedClass = dvmCompilerAllocTemp(cUnit);
    loadClassPointer(cUnit, regPredictedClass, (int) callsiteInfo);
    genNullCheck(cUnit, rlThis.sRegLow, rlThis.lowReg, mir->offset,
                 NULL);/* null object? */
    int regActualClass = dvmCompilerAllocTemp(cUnit);
    loadWordDisp(cUnit, rlThis.lowReg, offsetof(Object, clazz), regActualClass);
//    opRegReg(cUnit, kOpCmp, regPredictedClass, regActualClass);
    /*
     * Set the misPredBranchOver target so that it will be generated when the
     * code for the non-optimized invoke is generated.
     */
    callsiteInfo->misPredBranchOver = (LIR *) opCompareBranch(cUnit, kMipsBne, regPredictedClass, regActualClass);
}

/* Extended MIR instructions like PHI */
static void handleExtendedMIR(CompilationUnit *cUnit, MIR *mir)
{
    int opOffset = mir->dalvikInsn.opcode - kMirOpFirst;
    char *msg = (char *)dvmCompilerNew(strlen(extendedMIROpNames[opOffset]) + 1,
                                       false);
    strcpy(msg, extendedMIROpNames[opOffset]);
    newLIR1(cUnit, kMipsPseudoExtended, (int) msg);

    switch ((ExtendedMIROpcode)mir->dalvikInsn.opcode) {
        case kMirOpPhi: {
            char *ssaString = dvmCompilerGetSSAString(cUnit, mir->ssaRep);
            newLIR1(cUnit, kMipsPseudoSSARep, (int) ssaString);
            break;
        }
        case kMirOpNullNRangeUpCheck: {
            genHoistedChecksForCountUpLoop(cUnit, mir);
            break;
        }
        case kMirOpNullNRangeDownCheck: {
            genHoistedChecksForCountDownLoop(cUnit, mir);
            break;
        }
        case kMirOpLowerBound: {
            genHoistedLowerBoundCheck(cUnit, mir);
            break;
        }
        case kMirOpPunt: {
            genUnconditionalBranch(cUnit,
                                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);
            break;
        }
        case kMirOpCheckInlinePrediction: {
            genValidationForPredictedInline(cUnit, mir);
            break;
        }
        default:
            break;
    }
}

/*
 * Create a PC-reconstruction cell for the starting offset of this trace.
 * Since the PCR cell is placed near the end of the compiled code which is
 * usually out of range for a conditional branch, we put two branches (one
 * branch over to the loop body and one layover branch to the actual PCR) at the
 * end of the entry block.
 */
static void setupLoopEntryBlock(CompilationUnit *cUnit, BasicBlock *entry,
                                MipsLIR *bodyLabel)
{
    /* Set up the place holder to reconstruct this Dalvik PC */
    MipsLIR *pcrLabel = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
    pcrLabel->opcode = kMipsPseudoPCReconstructionCell;
    pcrLabel->operands[0] =
        (int) (cUnit->method->insns + entry->startOffset);
    pcrLabel->operands[1] = entry->startOffset;
    /* Insert the place holder to the growable list */
    dvmInsertGrowableList(&cUnit->pcReconstructionList, (intptr_t) pcrLabel);

    /*
     * Next, create two branches - one branch over to the loop body and the
     * other branch to the PCR cell to punt.
     */
    MipsLIR *branchToBody = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
    branchToBody->opcode = kMipsB;
    branchToBody->generic.target = (LIR *) bodyLabel;
    setupResourceMasks(branchToBody);
    cUnit->loopAnalysis->branchToBody = (LIR *) branchToBody;

    MipsLIR *branchToPCR = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
    branchToPCR->opcode = kMipsB;
    branchToPCR->generic.target = (LIR *) pcrLabel;
    setupResourceMasks(branchToPCR);
    cUnit->loopAnalysis->branchToPCR = (LIR *) branchToPCR;
}

#if defined(WITH_SELF_VERIFICATION)
static bool selfVerificationPuntOps(MIR *mir)
{
assert(0); /* MIPSTODO port selfVerificationPuntOps() */
    DecodedInstruction *decInsn = &mir->dalvikInsn;

    /*
     * All opcodes that can throw exceptions and use the
     * TEMPLATE_THROW_EXCEPTION_COMMON template should be excluded in the trace
     * under self-verification mode.
     */
    switch (decInsn->opcode) {
        case OP_MONITOR_ENTER:
        case OP_MONITOR_EXIT:
        case OP_NEW_INSTANCE:
        case OP_NEW_ARRAY:
        case OP_CHECK_CAST:
        case OP_MOVE_EXCEPTION:
        case OP_FILL_ARRAY_DATA:
        case OP_EXECUTE_INLINE:
        case OP_EXECUTE_INLINE_RANGE:
            return true;
        default:
            return false;
    }
}
#endif

void dvmCompilerMIR2LIR(CompilationUnit *cUnit)
{
    /* Used to hold the labels of each block */
    MipsLIR *labelList =
        (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR) * cUnit->numBlocks, true);
    MipsLIR *headLIR = NULL;
    GrowableList chainingListByType[kChainingCellGap];
    int i;

    /*
     * Initialize various types chaining lists.
     */
    for (i = 0; i < kChainingCellGap; i++) {
        dvmInitGrowableList(&chainingListByType[i], 2);
    }

    /* Clear the visited flag for each block */
    dvmCompilerDataFlowAnalysisDispatcher(cUnit, dvmCompilerClearVisitedFlag,
                                          kAllNodes, false /* isIterative */);

    GrowableListIterator iterator;
    dvmGrowableListIteratorInit(&cUnit->blockList, &iterator);

    /* Traces start with a profiling entry point.  Generate it here */
    cUnit->profileCodeSize = genTraceProfileEntry(cUnit);

    /* Handle the content in each basic block */
    for (i = 0; ; i++) {
        MIR *mir;
        BasicBlock *bb = (BasicBlock *) dvmGrowableListIteratorNext(&iterator);
        if (bb == NULL) break;
        if (bb->visited == true) continue;

        labelList[i].operands[0] = bb->startOffset;

        if (bb->blockType >= kChainingCellGap) {
            if (bb->isFallThroughFromInvoke == true) {
                /* Align this block first since it is a return chaining cell */
                newLIR0(cUnit, kMipsPseudoPseudoAlign4);
            }
            /*
             * Append the label pseudo LIR first. Chaining cells will be handled
             * separately afterwards.
             */
            dvmCompilerAppendLIR(cUnit, (LIR *) &labelList[i]);
        }

        if (bb->blockType == kEntryBlock) {
            labelList[i].opcode = kMipsPseudoEntryBlock;
            if (bb->firstMIRInsn == NULL) {
                continue;
            } else {
              setupLoopEntryBlock(cUnit, bb,
                                  &labelList[bb->fallThrough->id]);
            }
        } else if (bb->blockType == kExitBlock) {
            labelList[i].opcode = kMipsPseudoExitBlock;
            goto gen_fallthrough;
        } else if (bb->blockType == kDalvikByteCode) {
            if (bb->hidden == true) continue;
            labelList[i].opcode = kMipsPseudoNormalBlockLabel;
            /* Reset the register state */
            dvmCompilerResetRegPool(cUnit);
            dvmCompilerClobberAllRegs(cUnit);
            dvmCompilerResetNullCheck(cUnit);
        } else {
            switch (bb->blockType) {
                case kChainingCellNormal:
                    labelList[i].opcode = kMipsPseudoChainingCellNormal;
                    /* handle the codegen later */
                    dvmInsertGrowableList(
                        &chainingListByType[kChainingCellNormal], i);
                    break;
                case kChainingCellInvokeSingleton:
                    labelList[i].opcode =
                        kMipsPseudoChainingCellInvokeSingleton;
                    labelList[i].operands[0] =
                        (int) bb->containingMethod;
                    /* handle the codegen later */
                    dvmInsertGrowableList(
                        &chainingListByType[kChainingCellInvokeSingleton], i);
                    break;
                case kChainingCellInvokePredicted:
                    labelList[i].opcode =
                        kMipsPseudoChainingCellInvokePredicted;
                    /*
                     * Move the cached method pointer from operand 1 to 0.
                     * Operand 0 was clobbered earlier in this routine to store
                     * the block starting offset, which is not applicable to
                     * predicted chaining cell.
                     */
                    labelList[i].operands[0] = labelList[i].operands[1];
                    /* handle the codegen later */
                    dvmInsertGrowableList(
                        &chainingListByType[kChainingCellInvokePredicted], i);
                    break;
                case kChainingCellHot:
                    labelList[i].opcode =
                        kMipsPseudoChainingCellHot;
                    /* handle the codegen later */
                    dvmInsertGrowableList(
                        &chainingListByType[kChainingCellHot], i);
                    break;
                case kPCReconstruction:
                    /* Make sure exception handling block is next */
                    labelList[i].opcode =
                        kMipsPseudoPCReconstructionBlockLabel;
                    handlePCReconstruction(cUnit,
                                           &labelList[cUnit->puntBlock->id]);
                    break;
                case kExceptionHandling:
                    labelList[i].opcode = kMipsPseudoEHBlockLabel;
                    if (cUnit->pcReconstructionList.numUsed) {
                        loadWordDisp(cUnit, rSELF, offsetof(Thread,
                                     jitToInterpEntries.dvmJitToInterpPunt),
                                     r_A1);
                        opReg(cUnit, kOpBlx, r_A1);
                    }
                    break;
                case kChainingCellBackwardBranch:
                    labelList[i].opcode =
                        kMipsPseudoChainingCellBackwardBranch;
                    /* handle the codegen later */
                    dvmInsertGrowableList(
                        &chainingListByType[kChainingCellBackwardBranch],
                        i);
                    break;
                default:
                    break;
            }
            continue;
        }

        /*
         * Try to build a longer optimization unit. Currently if the previous
         * block ends with a goto, we continue adding instructions and don't
         * reset the register allocation pool.
         */
        for (BasicBlock *nextBB = bb; nextBB != NULL; nextBB = cUnit->nextCodegenBlock) {
            bb = nextBB;
            bb->visited = true;
            cUnit->nextCodegenBlock = NULL;

            for (mir = bb->firstMIRInsn; mir; mir = mir->next) {

                dvmCompilerResetRegPool(cUnit);
                if (gDvmJit.disableOpt & (1 << kTrackLiveTemps)) {
                    dvmCompilerClobberAllRegs(cUnit);
                }

                if (gDvmJit.disableOpt & (1 << kSuppressLoads)) {
                    dvmCompilerResetDefTracking(cUnit);
                }

                if ((int)mir->dalvikInsn.opcode >= (int)kMirOpFirst) {
                    handleExtendedMIR(cUnit, mir);
                    continue;
                }

                Opcode dalvikOpcode = mir->dalvikInsn.opcode;
                InstructionFormat dalvikFormat =
                    dexGetFormatFromOpcode(dalvikOpcode);
                const char *note;
                if (mir->OptimizationFlags & MIR_INLINED) {
                    note = " (I)";
                } else if (mir->OptimizationFlags & MIR_INLINED_PRED) {
                    note = " (PI)";
                } else if (mir->OptimizationFlags & MIR_CALLEE) {
                    note = " (C)";
                } else {
                    note = NULL;
                }

                MipsLIR *boundaryLIR =
                    newLIR2(cUnit, kMipsPseudoDalvikByteCodeBoundary,
                            mir->offset,
                            (int) dvmCompilerGetDalvikDisassembly(&mir->dalvikInsn,
                                                                  note));
                if (mir->ssaRep) {
                    char *ssaString = dvmCompilerGetSSAString(cUnit, mir->ssaRep);
                    newLIR1(cUnit, kMipsPseudoSSARep, (int) ssaString);
                }

                /* Remember the first LIR for this block */
                if (headLIR == NULL) {
                    headLIR = boundaryLIR;
                    /* Set the first boundaryLIR as a scheduling barrier */
                    headLIR->defMask = ENCODE_ALL;
                }

                bool notHandled;
                /*
                 * Debugging: screen the opcode first to see if it is in the
                 * do[-not]-compile list
                 */
                bool singleStepMe = SINGLE_STEP_OP(dalvikOpcode);
#if defined(WITH_SELF_VERIFICATION)
              if (singleStepMe == false) {
                  singleStepMe = selfVerificationPuntOps(mir);
              }
#endif
                if (singleStepMe || cUnit->allSingleStep) {
                    notHandled = false;
                    genInterpSingleStep(cUnit, mir);
                } else {
                    opcodeCoverage[dalvikOpcode]++;
                    switch (dalvikFormat) {
                        case kFmt10t:
                        case kFmt20t:
                        case kFmt30t:
                            notHandled = handleFmt10t_Fmt20t_Fmt30t(cUnit,
                                      mir, bb, labelList);
                            break;
                        case kFmt10x:
                            notHandled = handleFmt10x(cUnit, mir);
                            break;
                        case kFmt11n:
                        case kFmt31i:
                            notHandled = handleFmt11n_Fmt31i(cUnit, mir);
                            break;
                        case kFmt11x:
                            notHandled = handleFmt11x(cUnit, mir);
                            break;
                        case kFmt12x:
                            notHandled = handleFmt12x(cUnit, mir);
                            break;
                        case kFmt20bc:
                            notHandled = handleFmt20bc(cUnit, mir);
                            break;
                        case kFmt21c:
                        case kFmt31c:
                            notHandled = handleFmt21c_Fmt31c(cUnit, mir);
                            break;
                        case kFmt21h:
                            notHandled = handleFmt21h(cUnit, mir);
                            break;
                        case kFmt21s:
                            notHandled = handleFmt21s(cUnit, mir);
                            break;
                        case kFmt21t:
                            notHandled = handleFmt21t(cUnit, mir, bb,
                                                      labelList);
                            break;
                        case kFmt22b:
                        case kFmt22s:
                            notHandled = handleFmt22b_Fmt22s(cUnit, mir);
                            break;
                        case kFmt22c:
                            notHandled = handleFmt22c(cUnit, mir);
                            break;
                        case kFmt22cs:
                            notHandled = handleFmt22cs(cUnit, mir);
                            break;
                        case kFmt22t:
                            notHandled = handleFmt22t(cUnit, mir, bb,
                                                      labelList);
                            break;
                        case kFmt22x:
                        case kFmt32x:
                            notHandled = handleFmt22x_Fmt32x(cUnit, mir);
                            break;
                        case kFmt23x:
                            notHandled = handleFmt23x(cUnit, mir);
                            break;
                        case kFmt31t:
                            notHandled = handleFmt31t(cUnit, mir);
                            break;
                        case kFmt3rc:
                        case kFmt35c:
                            notHandled = handleFmt35c_3rc(cUnit, mir, bb,
                                                          labelList);
                            break;
                        case kFmt3rms:
                        case kFmt35ms:
                            notHandled = handleFmt35ms_3rms(cUnit, mir,bb,
                                                            labelList);
                            break;
                        case kFmt35mi:
                        case kFmt3rmi:
                            notHandled = handleExecuteInline(cUnit, mir);
                            break;
                        case kFmt51l:
                            notHandled = handleFmt51l(cUnit, mir);
                            break;
                        default:
                            notHandled = true;
                            break;
                    }
                }
                if (notHandled) {
                    ALOGE("%#06x: Opcode %#x (%s) / Fmt %d not handled",
                         mir->offset,
                         dalvikOpcode, dexGetOpcodeName(dalvikOpcode),
                         dalvikFormat);
                    dvmCompilerAbort(cUnit);
                    break;
                }
            }
        }

        if (bb->blockType == kEntryBlock) {
            dvmCompilerAppendLIR(cUnit,
                                 (LIR *) cUnit->loopAnalysis->branchToBody);
            dvmCompilerAppendLIR(cUnit,
                                 (LIR *) cUnit->loopAnalysis->branchToPCR);
        }

        if (headLIR) {
            /*
             * Eliminate redundant loads/stores and delay stores into later
             * slots
             */
            dvmCompilerApplyLocalOptimizations(cUnit, (LIR *) headLIR,
                                               cUnit->lastLIRInsn);
            /* Reset headLIR which is also the optimization boundary */
            headLIR = NULL;
        }

gen_fallthrough:
        /*
         * Check if the block is terminated due to trace length constraint -
         * insert an unconditional branch to the chaining cell.
         */
        if (bb->needFallThroughBranch) {
            genUnconditionalBranch(cUnit, &labelList[bb->fallThrough->id]);
        }
    }

    /* Handle the chaining cells in predefined order */
    for (i = 0; i < kChainingCellGap; i++) {
        size_t j;
        int *blockIdList = (int *) chainingListByType[i].elemList;

        cUnit->numChainingCells[i] = chainingListByType[i].numUsed;

        /* No chaining cells of this type */
        if (cUnit->numChainingCells[i] == 0)
            continue;

        /* Record the first LIR for a new type of chaining cell */
        cUnit->firstChainingLIR[i] = (LIR *) &labelList[blockIdList[0]];

        for (j = 0; j < chainingListByType[i].numUsed; j++) {
            int blockId = blockIdList[j];
            BasicBlock *chainingBlock =
                (BasicBlock *) dvmGrowableListGetElement(&cUnit->blockList,
                                                         blockId);

            /* Align this chaining cell first */
            newLIR0(cUnit, kMipsPseudoPseudoAlign4);

            /* Insert the pseudo chaining instruction */
            dvmCompilerAppendLIR(cUnit, (LIR *) &labelList[blockId]);


            switch (chainingBlock->blockType) {
                case kChainingCellNormal:
                    handleNormalChainingCell(cUnit, chainingBlock->startOffset);
                    break;
                case kChainingCellInvokeSingleton:
                    handleInvokeSingletonChainingCell(cUnit,
                        chainingBlock->containingMethod);
                    break;
                case kChainingCellInvokePredicted:
                    handleInvokePredictedChainingCell(cUnit);
                    break;
                case kChainingCellHot:
                    handleHotChainingCell(cUnit, chainingBlock->startOffset);
                    break;
                case kChainingCellBackwardBranch:
                    handleBackwardBranchChainingCell(cUnit,
                        chainingBlock->startOffset);
                    break;
                default:
                    ALOGE("Bad blocktype %d", chainingBlock->blockType);
                    dvmCompilerAbort(cUnit);
            }
        }
    }

    /* Mark the bottom of chaining cells */
    cUnit->chainingCellBottom = (LIR *) newLIR0(cUnit, kMipsChainingCellBottom);

    /*
     * Generate the branch to the dvmJitToInterpNoChain entry point at the end
     * of all chaining cells for the overflow cases.
     */
    if (cUnit->switchOverflowPad) {
        loadConstant(cUnit, r_A0, (int) cUnit->switchOverflowPad);
        loadWordDisp(cUnit, rSELF, offsetof(Thread,
                     jitToInterpEntries.dvmJitToInterpNoChain), r_A2);
        opRegReg(cUnit, kOpAdd, r_A1, r_A1);
        opRegRegReg(cUnit, kOpAdd, r4PC, r_A0, r_A1);
#if defined(WITH_JIT_TUNING)
        loadConstant(cUnit, r_A0, kSwitchOverflow);
#endif
        opReg(cUnit, kOpBlx, r_A2);
    }

    dvmCompilerApplyGlobalOptimizations(cUnit);

#if defined(WITH_SELF_VERIFICATION)
    selfVerificationBranchInsertPass(cUnit);
#endif
}

/*
 * Accept the work and start compiling.  Returns true if compilation
 * is attempted.
 */
bool dvmCompilerDoWork(CompilerWorkOrder *work)
{
    JitTraceDescription *desc;
    bool isCompile;
    bool success = true;

    if (gDvmJit.codeCacheFull) {
        return false;
    }

    switch (work->kind) {
        case kWorkOrderTrace:
            isCompile = true;
            /* Start compilation with maximally allowed trace length */
            desc = (JitTraceDescription *)work->info;
            success = dvmCompileTrace(desc, JIT_MAX_TRACE_LEN, &work->result,
                                        work->bailPtr, 0 /* no hints */);
            break;
        case kWorkOrderTraceDebug: {
            bool oldPrintMe = gDvmJit.printMe;
            gDvmJit.printMe = true;
            isCompile = true;
            /* Start compilation with maximally allowed trace length */
            desc = (JitTraceDescription *)work->info;
            success = dvmCompileTrace(desc, JIT_MAX_TRACE_LEN, &work->result,
                                        work->bailPtr, 0 /* no hints */);
            gDvmJit.printMe = oldPrintMe;
            break;
        }
        case kWorkOrderProfileMode:
            dvmJitChangeProfileMode((TraceProfilingModes)(int)work->info);
            isCompile = false;
            break;
        default:
            isCompile = false;
            ALOGE("Jit: unknown work order type");
            assert(0);  // Bail if debug build, discard otherwise
    }
    if (!success)
        work->result.codeAddress = NULL;
    return isCompile;
}

/* Architectural-specific debugging helpers go here */
void dvmCompilerArchDump(void)
{
    /* Print compiled opcode in this VM instance */
    int i, start, streak;
    char buf[1024];

    streak = i = 0;
    buf[0] = 0;
    while (opcodeCoverage[i] == 0 && i < 256) {
        i++;
    }
    if (i == 256) {
        return;
    }
    for (start = i++, streak = 1; i < 256; i++) {
        if (opcodeCoverage[i]) {
            streak++;
        } else {
            if (streak == 1) {
                sprintf(buf+strlen(buf), "%x,", start);
            } else {
                sprintf(buf+strlen(buf), "%x-%x,", start, start + streak - 1);
            }
            streak = 0;
            while (opcodeCoverage[i] == 0 && i < 256) {
                i++;
            }
            if (i < 256) {
                streak = 1;
                start = i;
            }
        }
    }
    if (streak) {
        if (streak == 1) {
            sprintf(buf+strlen(buf), "%x", start);
        } else {
            sprintf(buf+strlen(buf), "%x-%x", start, start + streak - 1);
        }
    }
    if (strlen(buf)) {
        ALOGD("dalvik.vm.jit.op = %s", buf);
    }
}

/* Common initialization routine for an architecture family */
bool dvmCompilerArchInit()
{
    int i;

    for (i = 0; i < kMipsLast; i++) {
        if (EncodingMap[i].opcode != i) {
            ALOGE("Encoding order for %s is wrong: expecting %d, seeing %d",
                 EncodingMap[i].name, i, EncodingMap[i].opcode);
            dvmAbort();  // OK to dvmAbort - build error
        }
    }

    return dvmCompilerArchVariantInit();
}

void *dvmCompilerGetInterpretTemplate()
{
      return (void*) ((int)gDvmJit.codeCache +
                      templateEntryOffsets[TEMPLATE_INTERPRET]);
}

JitInstructionSetType dvmCompilerGetInterpretTemplateSet()
{
    return DALVIK_JIT_MIPS;
}

/* Needed by the Assembler */
void dvmCompilerSetupResourceMasks(MipsLIR *lir)
{
    setupResourceMasks(lir);
}

/* Needed by the ld/st optmizatons */
MipsLIR* dvmCompilerRegCopyNoInsert(CompilationUnit *cUnit, int rDest, int rSrc)
{
    return genRegCopyNoInsert(cUnit, rDest, rSrc);
}

/* Needed by the register allocator */
MipsLIR* dvmCompilerRegCopy(CompilationUnit *cUnit, int rDest, int rSrc)
{
    return genRegCopy(cUnit, rDest, rSrc);
}

/* Needed by the register allocator */
void dvmCompilerRegCopyWide(CompilationUnit *cUnit, int destLo, int destHi,
                            int srcLo, int srcHi)
{
    genRegCopyWide(cUnit, destLo, destHi, srcLo, srcHi);
}

void dvmCompilerFlushRegImpl(CompilationUnit *cUnit, int rBase,
                             int displacement, int rSrc, OpSize size)
{
    storeBaseDisp(cUnit, rBase, displacement, rSrc, size);
}

void dvmCompilerFlushRegWideImpl(CompilationUnit *cUnit, int rBase,
                                 int displacement, int rSrcLo, int rSrcHi)
{
    storeBaseDispWide(cUnit, rBase, displacement, rSrcLo, rSrcHi);
}
