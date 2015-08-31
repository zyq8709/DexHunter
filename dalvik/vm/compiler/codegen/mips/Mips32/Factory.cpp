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

static int coreTemps[] = {r_V0, r_V1, r_A0, r_A1, r_A2, r_A3, r_T0, r_T1, r_T2,
                          r_T3, r_T4, r_T5, r_T6, r_T7, r_T8, r_T9, r_S0, r_S4};
#ifdef __mips_hard_float
static int fpTemps[] = {r_F0, r_F1, r_F2, r_F3, r_F4, r_F5, r_F6, r_F7,
                        r_F8, r_F9, r_F10, r_F11, r_F12, r_F13, r_F14, r_F15};
#endif

static void storePair(CompilationUnit *cUnit, int base, int lowReg,
                      int highReg);
static void loadPair(CompilationUnit *cUnit, int base, int lowReg, int highReg);
static MipsLIR *loadWordDisp(CompilationUnit *cUnit, int rBase, int displacement,
                            int rDest);
static MipsLIR *storeWordDisp(CompilationUnit *cUnit, int rBase,
                             int displacement, int rSrc);
static MipsLIR *genRegRegCheck(CompilationUnit *cUnit,
                              MipsConditionCode cond,
                              int reg1, int reg2, int dOffset,
                              MipsLIR *pcrLabel);
static MipsLIR *loadConstant(CompilationUnit *cUnit, int rDest, int value);

#ifdef __mips_hard_float
static MipsLIR *fpRegCopy(CompilationUnit *cUnit, int rDest, int rSrc)
{
    MipsLIR* res = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
    res->operands[0] = rDest;
    res->operands[1] = rSrc;
    if (rDest == rSrc) {
        res->flags.isNop = true;
    } else {
        /* must be both DOUBLE or both not DOUBLE */
        assert(DOUBLEREG(rDest) == DOUBLEREG(rSrc));
        if (DOUBLEREG(rDest)) {
            res->opcode = kMipsFmovd;
        } else {
            if (SINGLEREG(rDest)) {
                if (SINGLEREG(rSrc)) {
                    res->opcode = kMipsFmovs;
                } else {
                    /* note the operands are swapped for the mtc1 instr */
                    res->opcode = kMipsMtc1;
                    res->operands[0] = rSrc;
                    res->operands[1] = rDest;
                }
            } else {
                assert(SINGLEREG(rSrc));
                res->opcode = kMipsMfc1;
            }
        }
    }
    setupResourceMasks(res);
    return res;
}
#endif

/*
 * Load a immediate using a shortcut if possible; otherwise
 * grab from the per-translation literal pool.  If target is
 * a high register, build constant into a low register and copy.
 *
 * No additional register clobbering operation performed. Use this version when
 * 1) rDest is freshly returned from dvmCompilerAllocTemp or
 * 2) The codegen is under fixed register usage
 */
static MipsLIR *loadConstantNoClobber(CompilationUnit *cUnit, int rDest,
                                     int value)
{
    MipsLIR *res;

#ifdef __mips_hard_float
    int rDestSave = rDest;
    int isFpReg = FPREG(rDest);
    if (isFpReg) {
        assert(SINGLEREG(rDest));
        rDest = dvmCompilerAllocTemp(cUnit);
    }
#endif

    /* See if the value can be constructed cheaply */
    if (value == 0) {
        res = newLIR2(cUnit, kMipsMove, rDest, r_ZERO);
    } else if ((value > 0) && (value <= 65535)) {
        res = newLIR3(cUnit, kMipsOri, rDest, r_ZERO, value);
    } else if ((value < 0) && (value >= -32768)) {
        res = newLIR3(cUnit, kMipsAddiu, rDest, r_ZERO, value);
    } else {
        res = newLIR2(cUnit, kMipsLui, rDest, value>>16);
        if (value & 0xffff)
	    newLIR3(cUnit, kMipsOri, rDest, rDest, value);
    }

#ifdef __mips_hard_float
    if (isFpReg) {
        newLIR2(cUnit, kMipsMtc1, rDest, rDestSave);
        dvmCompilerFreeTemp(cUnit, rDest);
    }
#endif

    return res;
}

/*
 * Load an immediate value into a fixed or temp register.  Target
 * register is clobbered, and marked inUse.
 */
static MipsLIR *loadConstant(CompilationUnit *cUnit, int rDest, int value)
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
static MipsLIR *loadClassPointer(CompilationUnit *cUnit, int rDest, int value)
{
    MipsLIR *res;
    if (dvmCompilerIsTemp(cUnit, rDest)) {
        dvmCompilerClobber(cUnit, rDest);
        dvmCompilerMarkInUse(cUnit, rDest);
    }
    res = newLIR2(cUnit, kMipsLui, rDest, value>>16);
    if (value & 0xffff)
        newLIR3(cUnit, kMipsOri, rDest, rDest, value);
    return res;
}

static MipsLIR *opNone(CompilationUnit *cUnit, OpKind op)
{
    MipsLIR *res;
    MipsOpCode opcode = kMipsNop;
    switch (op) {
        case kOpUncondBr:
            opcode = kMipsB;
            break;
        default:
            ALOGE("Jit: bad case in opNone");
            dvmCompilerAbort(cUnit);
    }
    res = newLIR0(cUnit, opcode);
    return res;
}

static MipsLIR *opCompareBranch(CompilationUnit *cUnit, MipsOpCode opc, int rs, int rt)
{
    MipsLIR *res;
    if (rt < 0) {
      assert(opc >= kMipsBeqz && opc <= kMipsBnez);
      res = newLIR1(cUnit, opc, rs);
    } else  {
      assert(opc == kMipsBeq || opc == kMipsBne);
      res = newLIR2(cUnit, opc, rs, rt);
    }
    return res;
}

static MipsLIR *loadMultiple(CompilationUnit *cUnit, int rBase, int rMask);

static MipsLIR *opReg(CompilationUnit *cUnit, OpKind op, int rDestSrc)
{
    MipsOpCode opcode = kMipsNop;
    switch (op) {
        case kOpBlx:
            opcode = kMipsJalr;
            break;
        default:
            assert(0);
    }
    return newLIR2(cUnit, opcode, r_RA, rDestSrc);
}

static MipsLIR *opRegRegImm(CompilationUnit *cUnit, OpKind op, int rDest,
                           int rSrc1, int value);
static MipsLIR *opRegImm(CompilationUnit *cUnit, OpKind op, int rDestSrc1,
                        int value)
{
    MipsLIR *res;
    bool neg = (value < 0);
    int absValue = (neg) ? -value : value;
    bool shortForm = (absValue & 0xff) == absValue;
    MipsOpCode opcode = kMipsNop;
    switch (op) {
        case kOpAdd:
            return opRegRegImm(cUnit, op, rDestSrc1, rDestSrc1, value);
            break;
        case kOpSub:
            return opRegRegImm(cUnit, op, rDestSrc1, rDestSrc1, value);
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

static MipsLIR *opRegRegReg(CompilationUnit *cUnit, OpKind op, int rDest,
                           int rSrc1, int rSrc2)
{
    MipsOpCode opcode = kMipsNop;
    switch (op) {
        case kOpAdd:
            opcode = kMipsAddu;
            break;
        case kOpSub:
            opcode = kMipsSubu;
            break;
        case kOpAnd:
            opcode = kMipsAnd;
            break;
        case kOpMul:
            opcode = kMipsMul;
            break;
        case kOpOr:
            opcode = kMipsOr;
            break;
        case kOpXor:
            opcode = kMipsXor;
            break;
        case kOpLsl:
            opcode = kMipsSllv;
            break;
        case kOpLsr:
            opcode = kMipsSrlv;
            break;
        case kOpAsr:
            opcode = kMipsSrav;
            break;
        default:
            ALOGE("Jit: bad case in opRegRegReg");
            dvmCompilerAbort(cUnit);
            break;
    }
    return newLIR3(cUnit, opcode, rDest, rSrc1, rSrc2);
}

static MipsLIR *opRegRegImm(CompilationUnit *cUnit, OpKind op, int rDest,
                           int rSrc1, int value)
{
    MipsLIR *res;
    MipsOpCode opcode = kMipsNop;
    bool shortForm = true;

    switch(op) {
        case kOpAdd:
            if (IS_SIMM16(value)) {
                opcode = kMipsAddiu;
            }
            else {
                shortForm = false;
                opcode = kMipsAddu;
            }
            break;
        case kOpSub:
            if (IS_SIMM16((-value))) {
                value = -value;
                opcode = kMipsAddiu;
            }
            else {
                shortForm = false;
                opcode = kMipsSubu;
            }
            break;
        case kOpLsl:
                assert(value >= 0 && value <= 31);
                opcode = kMipsSll;
                break;
        case kOpLsr:
                assert(value >= 0 && value <= 31);
                opcode = kMipsSrl;
                break;
        case kOpAsr:
                assert(value >= 0 && value <= 31);
                opcode = kMipsSra;
                break;
        case kOpAnd:
            if (IS_UIMM16((value))) {
                opcode = kMipsAndi;
            }
            else {
                shortForm = false;
                opcode = kMipsAnd;
            }
            break;
        case kOpOr:
            if (IS_UIMM16((value))) {
                opcode = kMipsOri;
            }
            else {
                shortForm = false;
                opcode = kMipsOr;
            }
            break;
        case kOpXor:
            if (IS_UIMM16((value))) {
                opcode = kMipsXori;
            }
            else {
                shortForm = false;
                opcode = kMipsXor;
            }
            break;
        case kOpMul:
            shortForm = false;
            opcode = kMipsMul;
            break;
        default:
            ALOGE("Jit: bad case in opRegRegImm");
            dvmCompilerAbort(cUnit);
            break;
    }

    if (shortForm)
        res = newLIR3(cUnit, opcode, rDest, rSrc1, value);
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

static MipsLIR *opRegReg(CompilationUnit *cUnit, OpKind op, int rDestSrc1,
                        int rSrc2)
{
    MipsOpCode opcode = kMipsNop;
    MipsLIR *res;
    switch (op) {
        case kOpMov:
            opcode = kMipsMove;
            break;
        case kOpMvn:
            return newLIR3(cUnit, kMipsNor, rDestSrc1, rSrc2, r_ZERO);
        case kOpNeg:
            return newLIR3(cUnit, kMipsSubu, rDestSrc1, r_ZERO, rSrc2);
        case kOpAdd:
        case kOpAnd:
        case kOpMul:
        case kOpOr:
        case kOpSub:
        case kOpXor:
            return opRegRegReg(cUnit, op, rDestSrc1, rDestSrc1, rSrc2);
        case kOp2Byte:
#if __mips_isa_rev>=2
            res = newLIR2(cUnit, kMipsSeb, rDestSrc1, rSrc2);
#else
            res = opRegRegImm(cUnit, kOpLsl, rDestSrc1, rSrc2, 24);
            opRegRegImm(cUnit, kOpAsr, rDestSrc1, rDestSrc1, 24);
#endif
            return res;
        case kOp2Short:
#if __mips_isa_rev>=2
            res = newLIR2(cUnit, kMipsSeh, rDestSrc1, rSrc2);
#else
            res = opRegRegImm(cUnit, kOpLsl, rDestSrc1, rSrc2, 16);
            opRegRegImm(cUnit, kOpAsr, rDestSrc1, rDestSrc1, 16);
#endif
            return res;
        case kOp2Char:
             return newLIR3(cUnit, kMipsAndi, rDestSrc1, rSrc2, 0xFFFF);
        default:
            ALOGE("Jit: bad case in opRegReg");
            dvmCompilerAbort(cUnit);
            break;
    }
    return newLIR2(cUnit, opcode, rDestSrc1, rSrc2);
}

static MipsLIR *loadConstantValueWide(CompilationUnit *cUnit, int rDestLo,
                                     int rDestHi, int valLo, int valHi)
{
    MipsLIR *res;
    res = loadConstantNoClobber(cUnit, rDestLo, valLo);
    loadConstantNoClobber(cUnit, rDestHi, valHi);
    return res;
}

/* Load value from base + scaled index. */
static MipsLIR *loadBaseIndexed(CompilationUnit *cUnit, int rBase,
                               int rIndex, int rDest, int scale, OpSize size)
{
    MipsLIR *first = NULL;
    MipsLIR *res;
    MipsOpCode opcode = kMipsNop;
    int tReg = dvmCompilerAllocTemp(cUnit);

#ifdef __mips_hard_float
    if (FPREG(rDest)) {
        assert(SINGLEREG(rDest));
        assert((size == kWord) || (size == kSingle));
        size = kSingle;
    } else {
        if (size == kSingle)
            size = kWord;
    }
#endif

    if (!scale) {
        first = newLIR3(cUnit, kMipsAddu, tReg , rBase, rIndex);
    } else {
        first = opRegRegImm(cUnit, kOpLsl, tReg, rIndex, scale);
        newLIR3(cUnit, kMipsAddu, tReg , rBase, tReg);
    }

    switch (size) {
#ifdef __mips_hard_float
        case kSingle:
            opcode = kMipsFlwc1;
            break;
#endif
        case kWord:
            opcode = kMipsLw;
            break;
        case kUnsignedHalf:
            opcode = kMipsLhu;
            break;
        case kSignedHalf:
            opcode = kMipsLh;
            break;
        case kUnsignedByte:
            opcode = kMipsLbu;
            break;
        case kSignedByte:
            opcode = kMipsLb;
            break;
        default:
            ALOGE("Jit: bad case in loadBaseIndexed");
            dvmCompilerAbort(cUnit);
    }

    res = newLIR3(cUnit, opcode, rDest, 0, tReg);
#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->flags.insertWrapper = true;
#endif
    dvmCompilerFreeTemp(cUnit, tReg);
    return (first) ? first : res;
}

/* store value base base + scaled index. */
static MipsLIR *storeBaseIndexed(CompilationUnit *cUnit, int rBase,
                                int rIndex, int rSrc, int scale, OpSize size)
{
    MipsLIR *first = NULL;
    MipsLIR *res;
    MipsOpCode opcode = kMipsNop;
    int rNewIndex = rIndex;
    int tReg = dvmCompilerAllocTemp(cUnit);

#ifdef __mips_hard_float
    if (FPREG(rSrc)) {
        assert(SINGLEREG(rSrc));
        assert((size == kWord) || (size == kSingle));
        size = kSingle;
    } else {
        if (size == kSingle)
            size = kWord;
    }
#endif

    if (!scale) {
        first = newLIR3(cUnit, kMipsAddu, tReg , rBase, rIndex);
    } else {
        first = opRegRegImm(cUnit, kOpLsl, tReg, rIndex, scale);
        newLIR3(cUnit, kMipsAddu, tReg , rBase, tReg);
    }

    switch (size) {
#ifdef __mips_hard_float
        case kSingle:
            opcode = kMipsFswc1;
            break;
#endif
        case kWord:
            opcode = kMipsSw;
            break;
        case kUnsignedHalf:
        case kSignedHalf:
            opcode = kMipsSh;
            break;
        case kUnsignedByte:
        case kSignedByte:
            opcode = kMipsSb;
            break;
        default:
            ALOGE("Jit: bad case in storeBaseIndexed");
            dvmCompilerAbort(cUnit);
    }
    res = newLIR3(cUnit, opcode, rSrc, 0, tReg);
#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->flags.insertWrapper = true;
#endif
    dvmCompilerFreeTemp(cUnit, rNewIndex);
    return first;
}

static MipsLIR *loadMultiple(CompilationUnit *cUnit, int rBase, int rMask)
{
    int i;
    int loadCnt = 0;
    MipsLIR *res = NULL ;
    genBarrier(cUnit);

    for (i = 0; i < 8; i++, rMask >>= 1) {
        if (rMask & 0x1) { /* map r0 to MIPS r_A0 */
            newLIR3(cUnit, kMipsLw, i+r_A0, loadCnt*4, rBase);
            loadCnt++;
        }
    }

    if (loadCnt) {/* increment after */
        newLIR3(cUnit, kMipsAddiu, rBase, rBase, loadCnt*4);
    }

#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->flags.insertWrapper = true;
#endif
    genBarrier(cUnit);
    return res; /* NULL always returned which should be ok since no callers use it */
}

static MipsLIR *storeMultiple(CompilationUnit *cUnit, int rBase, int rMask)
{
    int i;
    int storeCnt = 0;
    MipsLIR *res = NULL ;
    genBarrier(cUnit);

    for (i = 0; i < 8; i++, rMask >>= 1) {
        if (rMask & 0x1) { /* map r0 to MIPS r_A0 */
            newLIR3(cUnit, kMipsSw, i+r_A0, storeCnt*4, rBase);
            storeCnt++;
        }
    }

    if (storeCnt) { /* increment after */
        newLIR3(cUnit, kMipsAddiu, rBase, rBase, storeCnt*4);
    }

#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->flags.insertWrapper = true;
#endif
    genBarrier(cUnit);
    return res; /* NULL always returned which should be ok since no callers use it */
}

static MipsLIR *loadBaseDispBody(CompilationUnit *cUnit, MIR *mir, int rBase,
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
    MipsLIR *res;
    MipsLIR *load = NULL;
    MipsLIR *load2 = NULL;
    MipsOpCode opcode = kMipsNop;
    bool shortForm = IS_SIMM16(displacement);
    bool pair = false;

    switch (size) {
        case kLong:
        case kDouble:
            pair = true;
            opcode = kMipsLw;
#ifdef __mips_hard_float
            if (FPREG(rDest)) {
                opcode = kMipsFlwc1;
                if (DOUBLEREG(rDest)) {
                    rDest = rDest - FP_DOUBLE;
                } else {
                    assert(FPREG(rDestHi));
                    assert(rDest == (rDestHi - 1));
                }
                rDestHi = rDest + 1;
            }
#endif
            shortForm = IS_SIMM16_2WORD(displacement);
            assert((displacement & 0x3) == 0);
            break;
        case kWord:
        case kSingle:
            opcode = kMipsLw;
#ifdef __mips_hard_float
            if (FPREG(rDest)) {
                opcode = kMipsFlwc1;
                assert(SINGLEREG(rDest));
            }
#endif
            assert((displacement & 0x3) == 0);
            break;
        case kUnsignedHalf:
            opcode = kMipsLhu;
            assert((displacement & 0x1) == 0);
            break;
        case kSignedHalf:
            opcode = kMipsLh;
            assert((displacement & 0x1) == 0);
            break;
        case kUnsignedByte:
            opcode = kMipsLbu;
            break;
        case kSignedByte:
            opcode = kMipsLb;
            break;
        default:
            ALOGE("Jit: bad case in loadBaseIndexedBody");
            dvmCompilerAbort(cUnit);
    }

    if (shortForm) {
        if (!pair) {
            load = res = newLIR3(cUnit, opcode, rDest, displacement, rBase);
        } else {
            load = res = newLIR3(cUnit, opcode, rDest, displacement + LOWORD_OFFSET, rBase);
            load2 = newLIR3(cUnit, opcode, rDestHi, displacement + HIWORD_OFFSET, rBase);
        }
    } else {
        if (pair) {
            int rTmp = dvmCompilerAllocFreeTemp(cUnit);
            res = opRegRegImm(cUnit, kOpAdd, rTmp, rBase, displacement);
            load = newLIR3(cUnit, opcode, rDest, LOWORD_OFFSET, rTmp);
            load2 = newLIR3(cUnit, opcode, rDestHi, HIWORD_OFFSET, rTmp);
            dvmCompilerFreeTemp(cUnit, rTmp);
        } else {
            int rTmp = (rBase == rDest) ? dvmCompilerAllocFreeTemp(cUnit)
                                        : rDest;
            res = loadConstant(cUnit, rTmp, displacement);
            load = newLIR3(cUnit, opcode, rDest, rBase, rTmp);
            if (rTmp != rDest)
                dvmCompilerFreeTemp(cUnit, rTmp);
        }
    }

    if (rBase == rFP) {
        if (load != NULL)
            annotateDalvikRegAccess(load, (displacement + (pair ? LOWORD_OFFSET : 0)) >> 2,
                                    true /* isLoad */);
        if (load2 != NULL)
            annotateDalvikRegAccess(load2, (displacement + HIWORD_OFFSET) >> 2,
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

static MipsLIR *loadBaseDisp(CompilationUnit *cUnit, MIR *mir, int rBase,
                            int displacement, int rDest, OpSize size,
                            int sReg)
{
    return loadBaseDispBody(cUnit, mir, rBase, displacement, rDest, -1,
                            size, sReg);
}

static MipsLIR *loadBaseDispWide(CompilationUnit *cUnit, MIR *mir, int rBase,
                                int displacement, int rDestLo, int rDestHi,
                                int sReg)
{
    return loadBaseDispBody(cUnit, mir, rBase, displacement, rDestLo, rDestHi,
                            kLong, sReg);
}

static MipsLIR *storeBaseDispBody(CompilationUnit *cUnit, int rBase,
                                 int displacement, int rSrc, int rSrcHi,
                                 OpSize size)
{
    MipsLIR *res;
    MipsLIR *store = NULL;
    MipsLIR *store2 = NULL;
    MipsOpCode opcode = kMipsNop;
    bool shortForm = IS_SIMM16(displacement);
    bool pair = false;

    switch (size) {
        case kLong:
        case kDouble:
            pair = true;
            opcode = kMipsSw;
#ifdef __mips_hard_float
            if (FPREG(rSrc)) {
                opcode = kMipsFswc1;
                if (DOUBLEREG(rSrc)) {
                    rSrc = rSrc - FP_DOUBLE;
                } else {
                    assert(FPREG(rSrcHi));
                    assert(rSrc == (rSrcHi - 1));
                }
                rSrcHi = rSrc + 1;
            }
#endif
            shortForm = IS_SIMM16_2WORD(displacement);
            assert((displacement & 0x3) == 0);
            break;
        case kWord:
        case kSingle:
            opcode = kMipsSw;
#ifdef __mips_hard_float
            if (FPREG(rSrc)) {
                opcode = kMipsFswc1;
                assert(SINGLEREG(rSrc));
            }
#endif
            assert((displacement & 0x3) == 0);
            break;
        case kUnsignedHalf:
        case kSignedHalf:
            opcode = kMipsSh;
            assert((displacement & 0x1) == 0);
            break;
        case kUnsignedByte:
        case kSignedByte:
            opcode = kMipsSb;
            break;
        default:
            ALOGE("Jit: bad case in storeBaseIndexedBody");
            dvmCompilerAbort(cUnit);
    }

    if (shortForm) {
        if (!pair) {
            store = res = newLIR3(cUnit, opcode, rSrc, displacement, rBase);
        } else {
            store = res = newLIR3(cUnit, opcode, rSrc, displacement + LOWORD_OFFSET, rBase);
            store2 = newLIR3(cUnit, opcode, rSrcHi, displacement + HIWORD_OFFSET, rBase);
        }
    } else {
        int rScratch = dvmCompilerAllocTemp(cUnit);
        res = opRegRegImm(cUnit, kOpAdd, rScratch, rBase, displacement);
        if (!pair) {
            store =  newLIR3(cUnit, opcode, rSrc, 0, rScratch);
        } else {
            store =  newLIR3(cUnit, opcode, rSrc, LOWORD_OFFSET, rScratch);
            store2 = newLIR3(cUnit, opcode, rSrcHi, HIWORD_OFFSET, rScratch);
        }
        dvmCompilerFreeTemp(cUnit, rScratch);
    }

    if (rBase == rFP) {
        if (store != NULL)
            annotateDalvikRegAccess(store, (displacement + (pair ? LOWORD_OFFSET : 0)) >> 2,
                                    false /* isLoad */);
        if (store2 != NULL)
            annotateDalvikRegAccess(store2, (displacement + HIWORD_OFFSET) >> 2,
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

static MipsLIR *storeBaseDisp(CompilationUnit *cUnit, int rBase,
                             int displacement, int rSrc, OpSize size)
{
    return storeBaseDispBody(cUnit, rBase, displacement, rSrc, -1, size);
}

static MipsLIR *storeBaseDispWide(CompilationUnit *cUnit, int rBase,
                                 int displacement, int rSrcLo, int rSrcHi)
{
    return storeBaseDispBody(cUnit, rBase, displacement, rSrcLo, rSrcHi, kLong);
}

static void storePair(CompilationUnit *cUnit, int base, int lowReg, int highReg)
{
    storeWordDisp(cUnit, base, LOWORD_OFFSET, lowReg);
    storeWordDisp(cUnit, base, HIWORD_OFFSET, highReg);
}

static void loadPair(CompilationUnit *cUnit, int base, int lowReg, int highReg)
{
    loadWordDisp(cUnit, base, LOWORD_OFFSET , lowReg);
    loadWordDisp(cUnit, base, HIWORD_OFFSET , highReg);
}

static MipsLIR* genRegCopyNoInsert(CompilationUnit *cUnit, int rDest, int rSrc)
{
    MipsLIR* res;
    MipsOpCode opcode;
#ifdef __mips_hard_float
    if (FPREG(rDest) || FPREG(rSrc))
        return fpRegCopy(cUnit, rDest, rSrc);
#endif
    res = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
    opcode = kMipsMove;
    assert(LOWREG(rDest) && LOWREG(rSrc));
    res->operands[0] = rDest;
    res->operands[1] = rSrc;
    res->opcode = opcode;
    setupResourceMasks(res);
    if (rDest == rSrc) {
        res->flags.isNop = true;
    }
    return res;
}

static MipsLIR* genRegCopy(CompilationUnit *cUnit, int rDest, int rSrc)
{
    MipsLIR *res = genRegCopyNoInsert(cUnit, rDest, rSrc);
    dvmCompilerAppendLIR(cUnit, (LIR*)res);
    return res;
}

static void genRegCopyWide(CompilationUnit *cUnit, int destLo, int destHi,
                           int srcLo, int srcHi)
{
#ifdef __mips_hard_float
    bool destFP = FPREG(destLo) && FPREG(destHi);
    bool srcFP = FPREG(srcLo) && FPREG(srcHi);
    assert(FPREG(srcLo) == FPREG(srcHi));
    assert(FPREG(destLo) == FPREG(destHi));
    if (destFP) {
        if (srcFP) {
            genRegCopy(cUnit, S2D(destLo, destHi), S2D(srcLo, srcHi));
        } else {
           /* note the operands are swapped for the mtc1 instr */
            newLIR2(cUnit, kMipsMtc1, srcLo, destLo);
            newLIR2(cUnit, kMipsMtc1, srcHi, destHi);
        }
    } else {
        if (srcFP) {
            newLIR2(cUnit, kMipsMfc1, destLo, srcLo);
            newLIR2(cUnit, kMipsMfc1, destHi, srcHi);
        } else {
            // Handle overlap
            if (srcHi == destLo) {
                genRegCopy(cUnit, destHi, srcHi);
                genRegCopy(cUnit, destLo, srcLo);
            } else {
                genRegCopy(cUnit, destLo, srcLo);
                genRegCopy(cUnit, destHi, srcHi);
            }
        }
    }
#else
    // Handle overlap
    if (srcHi == destLo) {
        genRegCopy(cUnit, destHi, srcHi);
        genRegCopy(cUnit, destLo, srcLo);
    } else {
        genRegCopy(cUnit, destLo, srcLo);
        genRegCopy(cUnit, destHi, srcHi);
    }
#endif
}

static inline MipsLIR *genRegImmCheck(CompilationUnit *cUnit,
                                     MipsConditionCode cond, int reg,
                                     int checkValue, int dOffset,
                                     MipsLIR *pcrLabel)
{
    MipsLIR *branch = NULL;

    if (checkValue == 0) {
        MipsOpCode opc = kMipsNop;
        if (cond == kMipsCondEq) {
            opc = kMipsBeqz;
	} else if (cond == kMipsCondNe) {
            opc = kMipsBnez;
        } else if (cond == kMipsCondLt || cond == kMipsCondMi) {
            opc = kMipsBltz;
        } else if (cond == kMipsCondLe) {
            opc = kMipsBlez;
        } else if (cond == kMipsCondGt) {
            opc = kMipsBgtz;
        } else if (cond == kMipsCondGe) {
            opc = kMipsBgez;
        } else {
            ALOGE("Jit: bad case in genRegImmCheck");
            dvmCompilerAbort(cUnit);
        }
        branch = opCompareBranch(cUnit, opc, reg, -1);
    } else if (IS_SIMM16(checkValue)) {
        if (cond == kMipsCondLt) {
            int tReg = dvmCompilerAllocTemp(cUnit);
            newLIR3(cUnit, kMipsSlti, tReg, reg, checkValue);
            branch = opCompareBranch(cUnit, kMipsBne, tReg, r_ZERO);
            dvmCompilerFreeTemp(cUnit, tReg);
        } else {
            ALOGE("Jit: bad case in genRegImmCheck");
            dvmCompilerAbort(cUnit);
        }
    } else {
        ALOGE("Jit: bad case in genRegImmCheck");
        dvmCompilerAbort(cUnit);
    }

    if (cUnit->jitMode == kJitMethod) {
        BasicBlock *bb = cUnit->curBlock;
        if (bb->taken) {
            MipsLIR  *exceptionLabel = (MipsLIR *) cUnit->blockLabelList;
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

#if defined(WITH_SELF_VERIFICATION)
static void genSelfVerificationPreBranch(CompilationUnit *cUnit,
                                         MipsLIR *origLIR) {
// DOUGLAS - this still needs to be implemented for MIPS.
#if 0
    /*
     * We need two separate pushes, since we want r5 to be pushed first.
     * Store multiple will push LR first.
     */
    MipsLIR *pushFP = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
    pushFP->opcode = kThumbPush;
    pushFP->operands[0] = 1 << r5FP;
    setupResourceMasks(pushFP);
    dvmCompilerInsertLIRBefore((LIR *) origLIR, (LIR *) pushFP);

    MipsLIR *pushLR = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
    pushLR->opcode = kThumbPush;
    /* Thumb push can handle LR, but is encoded differently at bit 8 */
    pushLR->operands[0] = 1 << 8;
    setupResourceMasks(pushLR);
    dvmCompilerInsertLIRBefore((LIR *) origLIR, (LIR *) pushLR);
#endif
}

static void genSelfVerificationPostBranch(CompilationUnit *cUnit,
                                         MipsLIR *origLIR) {
// DOUGLAS - this still needs to be implemented for MIPS.
#if 0
    /*
     * Since Thumb cannot pop memory content into LR, we have to pop LR
     * to a temp first (r5 in this case). Then we move r5 to LR, then pop the
     * original r5 from stack.
     */
    /* Pop memory content(LR) into r5 first */
    MipsLIR *popForLR = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
    popForLR->opcode = kThumbPop;
    popForLR->operands[0] = 1 << r5FP;
    setupResourceMasks(popForLR);
    dvmCompilerInsertLIRAfter((LIR *) origLIR, (LIR *) popForLR);

    MipsLIR *copy = genRegCopyNoInsert(cUnit, r14lr, r5FP);
    dvmCompilerInsertLIRAfter((LIR *) popForLR, (LIR *) copy);

    /* Now restore the original r5 */
    MipsLIR *popFP = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
    popFP->opcode = kThumbPop;
    popFP->operands[0] = 1 << r5FP;
    setupResourceMasks(popFP);
    dvmCompilerInsertLIRAfter((LIR *) copy, (LIR *) popFP);
#endif
}
#endif
