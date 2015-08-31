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
 * This file contains codegen for the Thumb ISA and is intended to be
 * includes by:
 *
 *        Codegen-$(TARGET_ARCH_VARIANT).c
 *
 */

static int coreTemps[] = {r0, r1, r2, r3, r4PC, r7};

static void storePair(CompilationUnit *cUnit, int base, int lowReg,
                      int highReg);
static void loadPair(CompilationUnit *cUnit, int base, int lowReg, int highReg);
static ArmLIR *loadWordDisp(CompilationUnit *cUnit, int rBase, int displacement,
                            int rDest);
static ArmLIR *storeWordDisp(CompilationUnit *cUnit, int rBase,
                             int displacement, int rSrc);
static ArmLIR *genRegRegCheck(CompilationUnit *cUnit,
                              ArmConditionCode cond,
                              int reg1, int reg2, int dOffset,
                              ArmLIR *pcrLabel);


/*
 * Load a immediate using a shortcut if possible; otherwise
 * grab from the per-translation literal pool.  If target is
 * a high register, build constant into a low register and copy.
 *
 * No additional register clobbering operation performed. Use this version when
 * 1) rDest is freshly returned from dvmCompilerAllocTemp or
 * 2) The codegen is under fixed register usage
 */
static ArmLIR *loadConstantNoClobber(CompilationUnit *cUnit, int rDest,
                                     int value)
{
    ArmLIR *res;
    int tDest = LOWREG(rDest) ? rDest : dvmCompilerAllocTemp(cUnit);
    /* See if the value can be constructed cheaply */
    if ((value >= 0) && (value <= 255)) {
        res = newLIR2(cUnit, kThumbMovImm, tDest, value);
        if (rDest != tDest) {
           opRegReg(cUnit, kOpMov, rDest, tDest);
           dvmCompilerFreeTemp(cUnit, tDest);
        }
        return res;
    } else if ((value & 0xFFFFFF00) == 0xFFFFFF00) {
        res = newLIR2(cUnit, kThumbMovImm, tDest, ~value);
        newLIR2(cUnit, kThumbMvn, tDest, tDest);
        if (rDest != tDest) {
           opRegReg(cUnit, kOpMov, rDest, tDest);
           dvmCompilerFreeTemp(cUnit, tDest);
        }
        return res;
    }
    /* No shortcut - go ahead and use literal pool */
    ArmLIR *dataTarget = scanLiteralPool(cUnit->literalList, value, 255);
    if (dataTarget == NULL) {
        dataTarget = addWordData(cUnit, &cUnit->literalList, value);
    }
    ArmLIR *loadPcRel = (ArmLIR *) dvmCompilerNew(sizeof(ArmLIR), true);
    loadPcRel->opcode = kThumbLdrPcRel;
    loadPcRel->generic.target = (LIR *) dataTarget;
    loadPcRel->operands[0] = tDest;
    setupResourceMasks(loadPcRel);
    setMemRefType(loadPcRel, true, kLiteral);
    loadPcRel->aliasInfo = dataTarget->operands[0];
    res = loadPcRel;
    dvmCompilerAppendLIR(cUnit, (LIR *) loadPcRel);

    /*
     * To save space in the constant pool, we use the ADD_RRI8 instruction to
     * add up to 255 to an existing constant value.
     */
    if (dataTarget->operands[0] != value) {
        newLIR2(cUnit, kThumbAddRI8, tDest, value - dataTarget->operands[0]);
    }
    if (rDest != tDest) {
       opRegReg(cUnit, kOpMov, rDest, tDest);
       dvmCompilerFreeTemp(cUnit, tDest);
    }
    return res;
}

/*
 * Load an immediate value into a fixed or temp register.  Target
 * register is clobbered, and marked inUse.
 */
static ArmLIR *loadConstant(CompilationUnit *cUnit, int rDest, int value)
{
    if (dvmCompilerIsTemp(cUnit, rDest)) {
        dvmCompilerClobber(cUnit, rDest);
        dvmCompilerMarkInUse(cUnit, rDest);
    }
    return loadConstantNoClobber(cUnit, rDest, value);
}

/*
 * Load a class pointer value into a fixed or temp register.  Target
 * register is clobbered, and marked inUse.
 */
static ArmLIR *loadClassPointer(CompilationUnit *cUnit, int rDest, int value)
{
    ArmLIR *res;
    cUnit->hasClassLiterals = true;
    if (dvmCompilerIsTemp(cUnit, rDest)) {
        dvmCompilerClobber(cUnit, rDest);
        dvmCompilerMarkInUse(cUnit, rDest);
    }
    ArmLIR *dataTarget = scanLiteralPool(cUnit->classPointerList, value, 0);
    if (dataTarget == NULL) {
        dataTarget = addWordData(cUnit, &cUnit->classPointerList, value);
        /* Counts the number of class pointers in this translation */
        cUnit->numClassPointers++;
    }
    ArmLIR *loadPcRel = (ArmLIR *) dvmCompilerNew(sizeof(ArmLIR), true);
    loadPcRel->opcode = kThumbLdrPcRel;
    loadPcRel->generic.target = (LIR *) dataTarget;
    loadPcRel->operands[0] = rDest;
    setupResourceMasks(loadPcRel);
    setMemRefType(loadPcRel, true, kLiteral);
    loadPcRel->aliasInfo = dataTarget->operands[0];
    res = loadPcRel;
    dvmCompilerAppendLIR(cUnit, (LIR *) loadPcRel);
    return res;
}

static ArmLIR *opNone(CompilationUnit *cUnit, OpKind op)
{
    ArmOpcode opcode = kThumbBkpt;
    switch (op) {
        case kOpUncondBr:
            opcode = kThumbBUncond;
            break;
        default:
            ALOGE("Jit: bad case in opNone");
            dvmCompilerAbort(cUnit);
    }
    return newLIR0(cUnit, opcode);
}

static ArmLIR *opCondBranch(CompilationUnit *cUnit, ArmConditionCode cc)
{
    return newLIR2(cUnit, kThumbBCond, 0 /* offset to be patched */, cc);
}

static ArmLIR *opImm(CompilationUnit *cUnit, OpKind op, int value)
{
    ArmOpcode opcode = kThumbBkpt;
    switch (op) {
        case kOpPush:
            opcode = kThumbPush;
            break;
        case kOpPop:
            opcode = kThumbPop;
            break;
        default:
            ALOGE("Jit: bad case in opCondBranch");
            dvmCompilerAbort(cUnit);
    }
    return newLIR1(cUnit, opcode, value);
}

static ArmLIR *opReg(CompilationUnit *cUnit, OpKind op, int rDestSrc)
{
    ArmOpcode opcode = kThumbBkpt;
    switch (op) {
        case kOpBlx:
            opcode = kThumbBlxR;
            break;
        default:
            ALOGE("Jit: bad case in opReg");
            dvmCompilerAbort(cUnit);
    }
    return newLIR1(cUnit, opcode, rDestSrc);
}

static ArmLIR *opRegImm(CompilationUnit *cUnit, OpKind op, int rDestSrc1,
                        int value)
{
    ArmLIR *res;
    bool neg = (value < 0);
    int absValue = (neg) ? -value : value;
    bool shortForm = (absValue & 0xff) == absValue;
    ArmOpcode opcode = kThumbBkpt;
    switch (op) {
        case kOpAdd:
            if ( !neg && (rDestSrc1 == r13sp) && (value <= 508)) { /* sp */
                assert((value & 0x3) == 0);
                return newLIR1(cUnit, kThumbAddSpI7, value >> 2);
            } else if (shortForm) {
                opcode = (neg) ? kThumbSubRI8 : kThumbAddRI8;
            } else
                opcode = kThumbAddRRR;
            break;
        case kOpSub:
            if (!neg && (rDestSrc1 == r13sp) && (value <= 508)) { /* sp */
                assert((value & 0x3) == 0);
                return newLIR1(cUnit, kThumbSubSpI7, value >> 2);
            } else if (shortForm) {
                opcode = (neg) ? kThumbAddRI8 : kThumbSubRI8;
            } else
                opcode = kThumbSubRRR;
            break;
        case kOpCmp:
            if (neg)
               shortForm = false;
            if (LOWREG(rDestSrc1) && shortForm) {
                opcode = kThumbCmpRI8;
            } else if (LOWREG(rDestSrc1)) {
                opcode = kThumbCmpRR;
            } else {
                shortForm = false;
                opcode = kThumbCmpHL;
            }
            break;
        default:
            ALOGE("Jit: bad case in opRegImm");
            dvmCompilerAbort(cUnit);
            break;
    }
    if (shortForm)
        res = newLIR2(cUnit, opcode, rDestSrc1, absValue);
    else {
        int rScratch = dvmCompilerAllocTemp(cUnit);
        res = loadConstant(cUnit, rScratch, value);
        if (op == kOpCmp)
            newLIR2(cUnit, opcode, rDestSrc1, rScratch);
        else
            newLIR3(cUnit, opcode, rDestSrc1, rDestSrc1, rScratch);
    }
    return res;
}

static ArmLIR *opRegRegReg(CompilationUnit *cUnit, OpKind op, int rDest,
                           int rSrc1, int rSrc2)
{
    ArmOpcode opcode = kThumbBkpt;
    switch (op) {
        case kOpAdd:
            opcode = kThumbAddRRR;
            break;
        case kOpSub:
            opcode = kThumbSubRRR;
            break;
        default:
            if (rDest == rSrc1) {
                return opRegReg(cUnit, op, rDest, rSrc2);
            } else if (rDest == rSrc2) {
                assert(dvmCompilerIsTemp(cUnit, rSrc1));
                dvmCompilerClobber(cUnit, rSrc1);
                opRegReg(cUnit, op, rSrc1, rSrc2);
                return opRegReg(cUnit, kOpMov, rDest, rSrc1);
            } else {
                opRegReg(cUnit, kOpMov, rDest, rSrc1);
                return opRegReg(cUnit, op, rDest, rSrc2);
            }
            break;
    }
    return newLIR3(cUnit, opcode, rDest, rSrc1, rSrc2);
}

static ArmLIR *opRegRegImm(CompilationUnit *cUnit, OpKind op, int rDest,
                           int rSrc1, int value)
{
    ArmLIR *res;
    bool neg = (value < 0);
    int absValue = (neg) ? -value : value;
    ArmOpcode opcode = kThumbBkpt;
    bool shortForm = (absValue & 0x7) == absValue;
    switch(op) {
        case kOpAdd:
            if (rDest == rSrc1)
                return opRegImm(cUnit, op, rDest, value);
            if ((rSrc1 == r13sp) && (value <= 1020)) { /* sp */
                assert((value & 0x3) == 0);
                shortForm = true;
                opcode = kThumbAddSpRel;
                value >>= 2;
            } else if ((rSrc1 == r15pc) && (value <= 1020)) { /* pc */
                assert((value & 0x3) == 0);
                shortForm = true;
                opcode = kThumbAddPcRel;
                value >>= 2;
            } else if (shortForm) {
                opcode = (neg) ? kThumbSubRRI3 : kThumbAddRRI3;
            } else if ((absValue > 0) && (absValue <= (255 + 7))) {
                /* Two shots - 1st handle the 7 */
                opcode = (neg) ? kThumbSubRRI3 : kThumbAddRRI3;
                res = newLIR3(cUnit, opcode, rDest, rSrc1, 7);
                opcode = (neg) ? kThumbSubRI8 : kThumbAddRI8;
                newLIR2(cUnit, opcode, rDest, absValue - 7);
                return res;
            } else
                opcode = kThumbAddRRR;
            break;

        case kOpSub:
            if (rDest == rSrc1)
                return opRegImm(cUnit, op, rDest, value);
            if (shortForm) {
                opcode = (neg) ? kThumbAddRRI3 : kThumbSubRRI3;
            } else if ((absValue > 0) && (absValue <= (255 + 7))) {
                /* Two shots - 1st handle the 7 */
                opcode = (neg) ? kThumbAddRRI3 : kThumbSubRRI3;
                res = newLIR3(cUnit, opcode, rDest, rSrc1, 7);
                opcode = (neg) ? kThumbAddRI8 : kThumbSubRI8;
                newLIR2(cUnit, opcode, rDest, absValue - 7);
                return res;
            } else
                opcode = kThumbSubRRR;
            break;
        case kOpLsl:
                shortForm = (!neg && value <= 31);
                opcode = kThumbLslRRI5;
                break;
        case kOpLsr:
                shortForm = (!neg && value <= 31);
                opcode = kThumbLsrRRI5;
                break;
        case kOpAsr:
                shortForm = (!neg && value <= 31);
                opcode = kThumbAsrRRI5;
                break;
        case kOpMul:
        case kOpAnd:
        case kOpOr:
        case kOpXor:
                if (rDest == rSrc1) {
                    int rScratch = dvmCompilerAllocTemp(cUnit);
                    res = loadConstant(cUnit, rScratch, value);
                    opRegReg(cUnit, op, rDest, rScratch);
                } else {
                    res = loadConstant(cUnit, rDest, value);
                    opRegReg(cUnit, op, rDest, rSrc1);
                }
                return res;
        default:
            ALOGE("Jit: bad case in opRegRegImm");
            dvmCompilerAbort(cUnit);
            break;
    }
    if (shortForm)
        res = newLIR3(cUnit, opcode, rDest, rSrc1, absValue);
    else {
        if (rDest != rSrc1) {
            res = loadConstant(cUnit, rDest, value);
            newLIR3(cUnit, opcode, rDest, rSrc1, rDest);
        } else {
            int rScratch = dvmCompilerAllocTemp(cUnit);
            res = loadConstant(cUnit, rScratch, value);
            newLIR3(cUnit, opcode, rDest, rSrc1, rScratch);
        }
    }
    return res;
}

static ArmLIR *opRegReg(CompilationUnit *cUnit, OpKind op, int rDestSrc1,
                        int rSrc2)
{
    ArmLIR *res;
    ArmOpcode opcode = kThumbBkpt;
    switch (op) {
        case kOpAdc:
            opcode = kThumbAdcRR;
            break;
        case kOpAnd:
            opcode = kThumbAndRR;
            break;
        case kOpBic:
            opcode = kThumbBicRR;
            break;
        case kOpCmn:
            opcode = kThumbCmnRR;
            break;
        case kOpCmp:
            opcode = kThumbCmpRR;
            break;
        case kOpXor:
            opcode = kThumbEorRR;
            break;
        case kOpMov:
            if (LOWREG(rDestSrc1) && LOWREG(rSrc2))
                opcode = kThumbMovRR;
            else if (!LOWREG(rDestSrc1) && !LOWREG(rSrc2))
                opcode = kThumbMovRR_H2H;
            else if (LOWREG(rDestSrc1))
                opcode = kThumbMovRR_H2L;
            else
                opcode = kThumbMovRR_L2H;
            break;
        case kOpMul:
            opcode = kThumbMul;
            break;
        case kOpMvn:
            opcode = kThumbMvn;
            break;
        case kOpNeg:
            opcode = kThumbNeg;
            break;
        case kOpOr:
            opcode = kThumbOrr;
            break;
        case kOpSbc:
            opcode = kThumbSbc;
            break;
        case kOpTst:
            opcode = kThumbTst;
            break;
        case kOpLsl:
            opcode = kThumbLslRR;
            break;
        case kOpLsr:
            opcode = kThumbLsrRR;
            break;
        case kOpAsr:
            opcode = kThumbAsrRR;
            break;
        case kOpRor:
            opcode = kThumbRorRR;
        case kOpAdd:
        case kOpSub:
            return opRegRegReg(cUnit, op, rDestSrc1, rDestSrc1, rSrc2);
        case kOp2Byte:
             res = opRegRegImm(cUnit, kOpLsl, rDestSrc1, rSrc2, 24);
             opRegRegImm(cUnit, kOpAsr, rDestSrc1, rDestSrc1, 24);
             return res;
        case kOp2Short:
             res = opRegRegImm(cUnit, kOpLsl, rDestSrc1, rSrc2, 16);
             opRegRegImm(cUnit, kOpAsr, rDestSrc1, rDestSrc1, 16);
             return res;
        case kOp2Char:
             res = opRegRegImm(cUnit, kOpLsl, rDestSrc1, rSrc2, 16);
             opRegRegImm(cUnit, kOpLsr, rDestSrc1, rDestSrc1, 16);
             return res;
        default:
            ALOGE("Jit: bad case in opRegReg");
            dvmCompilerAbort(cUnit);
            break;
    }
    return newLIR2(cUnit, opcode, rDestSrc1, rSrc2);
}

static ArmLIR *loadConstantValueWide(CompilationUnit *cUnit, int rDestLo,
                                     int rDestHi, int valLo, int valHi)
{
    ArmLIR *res;
    res = loadConstantNoClobber(cUnit, rDestLo, valLo);
    loadConstantNoClobber(cUnit, rDestHi, valHi);
    return res;
}

/* Load value from base + scaled index. */
static ArmLIR *loadBaseIndexed(CompilationUnit *cUnit, int rBase,
                               int rIndex, int rDest, int scale, OpSize size)
{
    ArmLIR *first = NULL;
    ArmLIR *res;
    ArmOpcode opcode = kThumbBkpt;
    int rNewIndex = rIndex;
    if (scale) {
        // Scale the index, but can't trash the original.
        rNewIndex = dvmCompilerAllocTemp(cUnit);
        first = opRegRegImm(cUnit, kOpLsl, rNewIndex, rIndex, scale);
    }
    switch (size) {
        case kWord:
            opcode = kThumbLdrRRR;
            break;
        case kUnsignedHalf:
            opcode = kThumbLdrhRRR;
            break;
        case kSignedHalf:
            opcode = kThumbLdrshRRR;
            break;
        case kUnsignedByte:
            opcode = kThumbLdrbRRR;
            break;
        case kSignedByte:
            opcode = kThumbLdrsbRRR;
            break;
        default:
            ALOGE("Jit: bad case in loadBaseIndexed");
            dvmCompilerAbort(cUnit);
    }
    res = newLIR3(cUnit, opcode, rDest, rBase, rNewIndex);
#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->flags.insertWrapper = true;
#endif
    if (scale)
        dvmCompilerFreeTemp(cUnit, rNewIndex);
    return (first) ? first : res;
}

/* store value base base + scaled index. */
static ArmLIR *storeBaseIndexed(CompilationUnit *cUnit, int rBase,
                                int rIndex, int rSrc, int scale, OpSize size)
{
    ArmLIR *first = NULL;
    ArmLIR *res;
    ArmOpcode opcode = kThumbBkpt;
    int rNewIndex = rIndex;
    if (scale) {
        rNewIndex = dvmCompilerAllocTemp(cUnit);
        first = opRegRegImm(cUnit, kOpLsl, rNewIndex, rIndex, scale);
    }
    switch (size) {
        case kWord:
            opcode = kThumbStrRRR;
            break;
        case kUnsignedHalf:
        case kSignedHalf:
            opcode = kThumbStrhRRR;
            break;
        case kUnsignedByte:
        case kSignedByte:
            opcode = kThumbStrbRRR;
            break;
        default:
            ALOGE("Jit: bad case in storeBaseIndexed");
            dvmCompilerAbort(cUnit);
    }
    res = newLIR3(cUnit, opcode, rSrc, rBase, rNewIndex);
#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->flags.insertWrapper = true;
#endif
    if (scale)
        dvmCompilerFreeTemp(cUnit, rNewIndex);
    return (first) ? first : res;
}

static ArmLIR *loadMultiple(CompilationUnit *cUnit, int rBase, int rMask)
{
    ArmLIR *res;
    genBarrier(cUnit);
    res = newLIR2(cUnit, kThumbLdmia, rBase, rMask);
#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->flags.insertWrapper = true;
#endif
    genBarrier(cUnit);
    return res;
}

static ArmLIR *storeMultiple(CompilationUnit *cUnit, int rBase, int rMask)
{
    ArmLIR *res;
    genBarrier(cUnit);
    res = newLIR2(cUnit, kThumbStmia, rBase, rMask);
#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->flags.insertWrapper = true;
#endif
    genBarrier(cUnit);
    return res;
}

static ArmLIR *loadBaseDispBody(CompilationUnit *cUnit, MIR *mir, int rBase,
                                int displacement, int rDest, int rDestHi,
                                OpSize size, int sReg)
/*
 * Load value from base + displacement.  Optionally perform null check
 * on base (which must have an associated sReg and MIR).  If not
 * performing null check, incoming MIR can be null. IMPORTANT: this
 * code must not allocate any new temps.  If a new register is needed
 * and base and dest are the same, spill some other register to
 * rlp and then restore.
 */
{
    ArmLIR *res;
    ArmLIR *load = NULL;
    ArmLIR *load2 = NULL;
    ArmOpcode opcode = kThumbBkpt;
    bool shortForm = false;
    int encodedDisp = displacement;
    bool pair = false;

    switch (size) {
        case kLong:
        case kDouble:
            pair = true;
            if ((displacement < 124) && (displacement >= 0)) {
                assert((displacement & 0x3) == 0);
                shortForm = true;
                encodedDisp >>= 2;
                opcode = kThumbLdrRRI5;
            } else {
                opcode = kThumbLdrRRR;
            }
            break;
        case kWord:
            if (LOWREG(rDest) && (rBase == r15pc) &&
                (displacement <= 1020) && (displacement >= 0)) {
                shortForm = true;
                encodedDisp >>= 2;
                opcode = kThumbLdrPcRel;
            } else if (LOWREG(rDest) && (rBase == r13sp) &&
                      (displacement <= 1020) && (displacement >= 0)) {
                shortForm = true;
                encodedDisp >>= 2;
                opcode = kThumbLdrSpRel;
            } else if (displacement < 128 && displacement >= 0) {
                assert((displacement & 0x3) == 0);
                shortForm = true;
                encodedDisp >>= 2;
                opcode = kThumbLdrRRI5;
            } else {
                opcode = kThumbLdrRRR;
            }
            break;
        case kUnsignedHalf:
            if (displacement < 64 && displacement >= 0) {
                assert((displacement & 0x1) == 0);
                shortForm = true;
                encodedDisp >>= 1;
                opcode = kThumbLdrhRRI5;
            } else {
                opcode = kThumbLdrhRRR;
            }
            break;
        case kSignedHalf:
            opcode = kThumbLdrshRRR;
            break;
        case kUnsignedByte:
            if (displacement < 32 && displacement >= 0) {
                shortForm = true;
                opcode = kThumbLdrbRRI5;
            } else {
                opcode = kThumbLdrbRRR;
            }
            break;
        case kSignedByte:
            opcode = kThumbLdrsbRRR;
            break;
        default:
            ALOGE("Jit: bad case in loadBaseIndexedBody");
            dvmCompilerAbort(cUnit);
    }
    if (shortForm) {
        load = res = newLIR3(cUnit, opcode, rDest, rBase, encodedDisp);
        if (pair) {
            load2 = newLIR3(cUnit, opcode, rDestHi, rBase, encodedDisp+1);
        }
    } else {
        if (pair) {
            int rTmp = dvmCompilerAllocFreeTemp(cUnit);
            res = opRegRegImm(cUnit, kOpAdd, rTmp, rBase, displacement);
            load = newLIR3(cUnit, kThumbLdrRRI5, rDest, rTmp, 0);
            load2 = newLIR3(cUnit, kThumbLdrRRI5, rDestHi, rTmp, 1);
            dvmCompilerFreeTemp(cUnit, rTmp);
        } else {
            int rTmp = (rBase == rDest) ? dvmCompilerAllocFreeTemp(cUnit)
                                        : rDest;
            res = loadConstant(cUnit, rTmp, displacement);
            load = newLIR3(cUnit, opcode, rDest, rBase, rTmp);
            if (rBase == r5FP)
                annotateDalvikRegAccess(load, displacement >> 2,
                                        true /* isLoad */);
            if (rTmp != rDest)
                dvmCompilerFreeTemp(cUnit, rTmp);
        }
    }
    if (rBase == r5FP) {
        if (load != NULL)
            annotateDalvikRegAccess(load, displacement >> 2,
                                    true /* isLoad */);
        if (load2 != NULL)
            annotateDalvikRegAccess(load2, (displacement >> 2) + 1,
                                    true /* isLoad */);
    }
#if defined(WITH_SELF_VERIFICATION)
    if (load != NULL && cUnit->heapMemOp)
        load->flags.insertWrapper = true;
    if (load2 != NULL && cUnit->heapMemOp)
        load2->flags.insertWrapper = true;
#endif
    return load;
}

static ArmLIR *loadBaseDisp(CompilationUnit *cUnit, MIR *mir, int rBase,
                            int displacement, int rDest, OpSize size,
                            int sReg)
{
    return loadBaseDispBody(cUnit, mir, rBase, displacement, rDest, -1,
                            size, sReg);
}

static ArmLIR *loadBaseDispWide(CompilationUnit *cUnit, MIR *mir, int rBase,
                                int displacement, int rDestLo, int rDestHi,
                                int sReg)
{
    return loadBaseDispBody(cUnit, mir, rBase, displacement, rDestLo, rDestHi,
                            kLong, sReg);
}

static ArmLIR *storeBaseDispBody(CompilationUnit *cUnit, int rBase,
                                 int displacement, int rSrc, int rSrcHi,
                                 OpSize size)
{
    ArmLIR *res;
    ArmLIR *store = NULL;
    ArmLIR *store2 = NULL;
    ArmOpcode opcode = kThumbBkpt;
    bool shortForm = false;
    int encodedDisp = displacement;
    bool pair = false;

    switch (size) {
        case kLong:
        case kDouble:
            pair = true;
            if ((displacement < 124) && (displacement >= 0)) {
                assert((displacement & 0x3) == 0);
                pair = true;
                shortForm = true;
                encodedDisp >>= 2;
                opcode = kThumbStrRRI5;
            } else {
                opcode = kThumbStrRRR;
            }
            break;
        case kWord:
            if (displacement < 128 && displacement >= 0) {
                assert((displacement & 0x3) == 0);
                shortForm = true;
                encodedDisp >>= 2;
                opcode = kThumbStrRRI5;
            } else {
                opcode = kThumbStrRRR;
            }
            break;
        case kUnsignedHalf:
        case kSignedHalf:
            if (displacement < 64 && displacement >= 0) {
                assert((displacement & 0x1) == 0);
                shortForm = true;
                encodedDisp >>= 1;
                opcode = kThumbStrhRRI5;
            } else {
                opcode = kThumbStrhRRR;
            }
            break;
        case kUnsignedByte:
        case kSignedByte:
            if (displacement < 32 && displacement >= 0) {
                shortForm = true;
                opcode = kThumbStrbRRI5;
            } else {
                opcode = kThumbStrbRRR;
            }
            break;
        default:
            ALOGE("Jit: bad case in storeBaseIndexedBody");
            dvmCompilerAbort(cUnit);
    }
    if (shortForm) {
        store = res = newLIR3(cUnit, opcode, rSrc, rBase, encodedDisp);
        if (pair) {
            store2 = newLIR3(cUnit, opcode, rSrcHi, rBase, encodedDisp + 1);
        }
    } else {
        int rScratch = dvmCompilerAllocTemp(cUnit);
        if (pair) {
            res = opRegRegImm(cUnit, kOpAdd, rScratch, rBase, displacement);
            store =  newLIR3(cUnit, kThumbStrRRI5, rSrc, rScratch, 0);
            store2 = newLIR3(cUnit, kThumbStrRRI5, rSrcHi, rScratch, 1);
        } else {
            res = loadConstant(cUnit, rScratch, displacement);
            store = newLIR3(cUnit, opcode, rSrc, rBase, rScratch);
        }
        dvmCompilerFreeTemp(cUnit, rScratch);
    }
    if (rBase == r5FP) {
        if (store != NULL)
            annotateDalvikRegAccess(store, displacement >> 2,
                                    false /* isLoad */);
        if (store2 != NULL)
            annotateDalvikRegAccess(store2, (displacement >> 2) + 1,
                                    false /* isLoad */);
    }
#if defined(WITH_SELF_VERIFICATION)
    if (store != NULL && cUnit->heapMemOp)
        store->flags.insertWrapper = true;
    if (store2 != NULL && cUnit->heapMemOp)
        store2->flags.insertWrapper = true;
#endif
    return res;
}

static ArmLIR *storeBaseDisp(CompilationUnit *cUnit, int rBase,
                             int displacement, int rSrc, OpSize size)
{
    return storeBaseDispBody(cUnit, rBase, displacement, rSrc, -1, size);
}

static ArmLIR *storeBaseDispWide(CompilationUnit *cUnit, int rBase,
                                 int displacement, int rSrcLo, int rSrcHi)
{
    return storeBaseDispBody(cUnit, rBase, displacement, rSrcLo, rSrcHi, kLong);
}

static void storePair(CompilationUnit *cUnit, int base, int lowReg, int highReg)
{
    if (lowReg < highReg) {
        storeMultiple(cUnit, base, (1 << lowReg) | (1 << highReg));
    } else {
        storeWordDisp(cUnit, base, 0, lowReg);
        storeWordDisp(cUnit, base, 4, highReg);
    }
}

static void loadPair(CompilationUnit *cUnit, int base, int lowReg, int highReg)
{
    if (lowReg < highReg) {
        loadMultiple(cUnit, base, (1 << lowReg) | (1 << highReg));
    } else {
        loadWordDisp(cUnit, base, 0 , lowReg);
        loadWordDisp(cUnit, base, 4 , highReg);
    }
}

static ArmLIR* genRegCopyNoInsert(CompilationUnit *cUnit, int rDest, int rSrc)
{
    ArmLIR* res;
    ArmOpcode opcode;
    res = (ArmLIR *) dvmCompilerNew(sizeof(ArmLIR), true);
    if (LOWREG(rDest) && LOWREG(rSrc))
        opcode = kThumbMovRR;
    else if (!LOWREG(rDest) && !LOWREG(rSrc))
         opcode = kThumbMovRR_H2H;
    else if (LOWREG(rDest))
         opcode = kThumbMovRR_H2L;
    else
         opcode = kThumbMovRR_L2H;

    res->operands[0] = rDest;
    res->operands[1] = rSrc;
    res->opcode = opcode;
    setupResourceMasks(res);
    if (rDest == rSrc) {
        res->flags.isNop = true;
    }
    return res;
}

static ArmLIR* genRegCopy(CompilationUnit *cUnit, int rDest, int rSrc)
{
    ArmLIR *res = genRegCopyNoInsert(cUnit, rDest, rSrc);
    dvmCompilerAppendLIR(cUnit, (LIR*)res);
    return res;
}

static void genRegCopyWide(CompilationUnit *cUnit, int destLo, int destHi,
                           int srcLo, int srcHi)
{
    // Handle overlap
    if (srcHi == destLo) {
        genRegCopy(cUnit, destHi, srcHi);
        genRegCopy(cUnit, destLo, srcLo);
    } else {
        genRegCopy(cUnit, destLo, srcLo);
        genRegCopy(cUnit, destHi, srcHi);
    }
}

static ArmLIR *genCmpImmBranch(CompilationUnit *cUnit,
                                     ArmConditionCode cond, int reg,
                                     int checkValue)
{
    if ((checkValue & 0xff) != checkValue) {
        int tReg = dvmCompilerAllocTemp(cUnit);
        loadConstant(cUnit, tReg, checkValue);
        newLIR2(cUnit, kThumbCmpRR, reg, tReg);
        dvmCompilerFreeTemp(cUnit, tReg);
    } else {
        newLIR2(cUnit, kThumbCmpRI8, reg, checkValue);
    }
    ArmLIR *branch = newLIR2(cUnit, kThumbBCond, 0, cond);
    return branch;
}

#if defined(WITH_SELF_VERIFICATION)
static void genSelfVerificationPreBranch(CompilationUnit *cUnit,
                                         ArmLIR *origLIR) {
    /*
     * We need two separate pushes, since we want r5 to be pushed first.
     * Store multiple will push LR first.
     */
    ArmLIR *pushFP = (ArmLIR *) dvmCompilerNew(sizeof(ArmLIR), true);
    pushFP->opcode = kThumbPush;
    pushFP->operands[0] = 1 << r5FP;
    setupResourceMasks(pushFP);
    dvmCompilerInsertLIRBefore((LIR *) origLIR, (LIR *) pushFP);

    ArmLIR *pushLR = (ArmLIR *) dvmCompilerNew(sizeof(ArmLIR), true);
    pushLR->opcode = kThumbPush;
    /* Thumb push can handle LR, but is encoded differently at bit 8 */
    pushLR->operands[0] = 1 << 8;
    setupResourceMasks(pushLR);
    dvmCompilerInsertLIRBefore((LIR *) origLIR, (LIR *) pushLR);
}

static void genSelfVerificationPostBranch(CompilationUnit *cUnit,
                                         ArmLIR *origLIR) {
    /*
     * Since Thumb cannot pop memory content into LR, we have to pop LR
     * to a temp first (r5 in this case). Then we move r5 to LR, then pop the
     * original r5 from stack.
     */
    /* Pop memory content(LR) into r5 first */
    ArmLIR *popForLR = (ArmLIR *) dvmCompilerNew(sizeof(ArmLIR), true);
    popForLR->opcode = kThumbPop;
    popForLR->operands[0] = 1 << r5FP;
    setupResourceMasks(popForLR);
    dvmCompilerInsertLIRAfter((LIR *) origLIR, (LIR *) popForLR);

    ArmLIR *copy = genRegCopyNoInsert(cUnit, r14lr, r5FP);
    dvmCompilerInsertLIRAfter((LIR *) popForLR, (LIR *) copy);

    /* Now restore the original r5 */
    ArmLIR *popFP = (ArmLIR *) dvmCompilerNew(sizeof(ArmLIR), true);
    popFP->opcode = kThumbPop;
    popFP->operands[0] = 1 << r5FP;
    setupResourceMasks(popFP);
    dvmCompilerInsertLIRAfter((LIR *) copy, (LIR *) popFP);
}
#endif
