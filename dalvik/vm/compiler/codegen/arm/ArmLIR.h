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

#ifndef DALVIK_VM_COMPILER_CODEGEN_ARM_ARMLIR_H_
#define DALVIK_VM_COMPILER_CODEGEN_ARM_ARMLIR_H_

#include "Dalvik.h"
#include "compiler/CompilerInternals.h"

/*
 * r0, r1, r2, r3 are always scratch
 * r4 (rPC) is scratch for Jit, but most be restored when resuming interp
 * r5 (rFP) is reserved [holds Dalvik frame pointer]
 * r6 (rSELF) is reserved [holds current &Thread]
 * r7 (rINST) is scratch for Jit
 * r8 (rIBASE) is scratch for Jit, but must be restored when resuming interp
 * r9 is reserved
 * r10 is always scratch
 * r11 (fp) used by gcc unless -fomit-frame-pointer set [available for jit?]
 * r12 is always scratch
 * r13 (sp) is reserved
 * r14 (lr) is scratch for Jit
 * r15 (pc) is reserved
 *
 * Preserved across C calls: r4, r5, r6, r7, r8, r10, r11
 * Trashed across C calls: r0, r1, r2, r3, r12, r14
 *
 * Floating pointer registers
 * s0-s31
 * d0-d15, where d0={s0,s1}, d1={s2,s3}, ... , d15={s30,s31}
 *
 * s16-s31 (d8-d15) preserved across C calls
 * s0-s15 (d0-d7) trashed across C calls
 *
 * For Thumb code use:
 *       r0, r1, r2, r3 to hold operands/results
 *       r4, r7 for temps
 *
 * For Thumb2 code use:
 *       r0, r1, r2, r3, r8, r9, r10, r11, r12, r14 for operands/results
 *       r4, r7 for temps
 *       s16-s31/d8-d15 for operands/results
 *       s0-s15/d0-d7 for temps
 *
 * When transitioning from code cache to interp:
 *       restore rIBASE
 *       restore rPC
 *       restore r11?
 */

/* Offset to distingish FP regs */
#define FP_REG_OFFSET 32
/* Offset to distinguish DP FP regs */
#define FP_DOUBLE 64
/* Reg types */
#define REGTYPE(x) (x & (FP_REG_OFFSET | FP_DOUBLE))
#define FPREG(x) ((x & FP_REG_OFFSET) == FP_REG_OFFSET)
#define LOWREG(x) ((x & 0x7) == x)
#define DOUBLEREG(x) ((x & FP_DOUBLE) == FP_DOUBLE)
#define SINGLEREG(x) (FPREG(x) && !DOUBLEREG(x))
/*
 * Note: the low register of a floating point pair is sufficient to
 * create the name of a double, but require both names to be passed to
 * allow for asserts to verify that the pair is consecutive if significant
 * rework is done in this area.  Also, it is a good reminder in the calling
 * code that reg locations always describe doubles as a pair of singles.
 */
#define S2D(x,y) ((x) | FP_DOUBLE)
/* Mask to strip off fp flags */
#define FP_REG_MASK (FP_REG_OFFSET-1)
/* non-existent Dalvik register */
#define vNone   (-1)
/* non-existant physical register */
#define rNone   (-1)

/* RegisterLocation templates return values (r0, or r0/r1) */
#define LOC_C_RETURN {kLocPhysReg, 0, 0, r0, 0, -1}
#define LOC_C_RETURN_WIDE {kLocPhysReg, 1, 0, r0, r1, -1}
/* RegisterLocation templates for interpState->retVal; */
#define LOC_DALVIK_RETURN_VAL {kLocRetval, 0, 0, 0, 0, -1}
#define LOC_DALVIK_RETURN_VAL_WIDE {kLocRetval, 1, 0, 0, 0, -1}

 /*
 * Data structure tracking the mapping between a Dalvik register (pair) and a
 * native register (pair). The idea is to reuse the previously loaded value
 * if possible, otherwise to keep the value in a native register as long as
 * possible.
 */
typedef struct RegisterInfo {
    int reg;                    // Reg number
    bool inUse;                 // Has it been allocated?
    bool pair;                  // Part of a register pair?
    int partner;                // If pair, other reg of pair
    bool live;                  // Is there an associated SSA name?
    bool dirty;                 // If live, is it dirty?
    int sReg;                   // Name of live value
    struct LIR *defStart;       // Starting inst in last def sequence
    struct LIR *defEnd;         // Ending inst in last def sequence
} RegisterInfo;

typedef struct RegisterPool {
    BitVector *nullCheckedRegs; // Track which registers have been null-checked
    int numCoreTemps;
    RegisterInfo *coreTemps;
    int nextCoreTemp;
    int numFPTemps;
    RegisterInfo *FPTemps;
    int nextFPTemp;
} RegisterPool;

typedef enum ResourceEncodingPos {
    kGPReg0     = 0,
    kRegSP      = 13,
    kRegLR      = 14,
    kRegPC      = 15,
    kFPReg0     = 16,
    kRegEnd     = 48,
    kCCode      = kRegEnd,
    kFPStatus,          // FP status word
    // The following four bits are for memory disambiguation
    kDalvikReg,         // 1 Dalvik Frame (can be fully disambiguated)
    kLiteral,           // 2 Literal pool (can be fully disambiguated)
    kHeapRef,           // 3 Somewhere on the heap (alias with any other heap)
    kMustNotAlias,      // 4 Guaranteed to be non-alias (eg *(r6+x))
} ResourceEncodingPos;

#define ENCODE_REG_LIST(N)      ((u8) N)
#define ENCODE_REG_SP           (1ULL << kRegSP)
#define ENCODE_REG_LR           (1ULL << kRegLR)
#define ENCODE_REG_PC           (1ULL << kRegPC)
#define ENCODE_CCODE            (1ULL << kCCode)
#define ENCODE_FP_STATUS        (1ULL << kFPStatus)

/* Abstract memory locations */
#define ENCODE_DALVIK_REG       (1ULL << kDalvikReg)
#define ENCODE_LITERAL          (1ULL << kLiteral)
#define ENCODE_HEAP_REF         (1ULL << kHeapRef)
#define ENCODE_MUST_NOT_ALIAS   (1ULL << kMustNotAlias)

#define ENCODE_ALL              (~0ULL)
#define ENCODE_MEM              (ENCODE_DALVIK_REG | ENCODE_LITERAL | \
                                 ENCODE_HEAP_REF | ENCODE_MUST_NOT_ALIAS)

#define DECODE_ALIAS_INFO_REG(X)        (X & 0xffff)
#define DECODE_ALIAS_INFO_WIDE(X)       ((X & 0x80000000) ? 1 : 0)

typedef enum OpSize {
    kWord,
    kLong,
    kSingle,
    kDouble,
    kUnsignedHalf,
    kSignedHalf,
    kUnsignedByte,
    kSignedByte,
} OpSize;

typedef enum OpKind {
    kOpMov,
    kOpMvn,
    kOpCmp,
    kOpLsl,
    kOpLsr,
    kOpAsr,
    kOpRor,
    kOpNot,
    kOpAnd,
    kOpOr,
    kOpXor,
    kOpNeg,
    kOpAdd,
    kOpAdc,
    kOpSub,
    kOpSbc,
    kOpRsub,
    kOpMul,
    kOpDiv,
    kOpRem,
    kOpBic,
    kOpCmn,
    kOpTst,
    kOpBkpt,
    kOpBlx,
    kOpPush,
    kOpPop,
    kOp2Char,
    kOp2Short,
    kOp2Byte,
    kOpCondBr,
    kOpUncondBr,
} OpKind;

/*
 * Annotate special-purpose core registers:
 *   - VM: r4PC, r5FP, and r6SELF
 *   - ARM architecture: r13sp, r14lr, and r15pc
 *
 * rPC, rFP, and rSELF are for architecture-independent code to use.
 */
typedef enum NativeRegisterPool {
    r0     = 0,
    r1     = 1,
    r2     = 2,
    r3     = 3,
    rPC    = 4,
    r4PC   = rPC,
    rFP    = 5,
    r5FP   = rFP,
    rSELF  = 6,
    r6SELF = rSELF,
    r7     = 7,
    r8     = 8,
    r9     = 9,
    r10    = 10,
    r11    = 11,
    r12    = 12,
    r13sp  = 13,
    r14lr  = 14,
    r15pc  = 15,
    fr0  =  0 + FP_REG_OFFSET,
    fr1  =  1 + FP_REG_OFFSET,
    fr2  =  2 + FP_REG_OFFSET,
    fr3  =  3 + FP_REG_OFFSET,
    fr4  =  4 + FP_REG_OFFSET,
    fr5  =  5 + FP_REG_OFFSET,
    fr6  =  6 + FP_REG_OFFSET,
    fr7  =  7 + FP_REG_OFFSET,
    fr8  =  8 + FP_REG_OFFSET,
    fr9  =  9 + FP_REG_OFFSET,
    fr10 = 10 + FP_REG_OFFSET,
    fr11 = 11 + FP_REG_OFFSET,
    fr12 = 12 + FP_REG_OFFSET,
    fr13 = 13 + FP_REG_OFFSET,
    fr14 = 14 + FP_REG_OFFSET,
    fr15 = 15 + FP_REG_OFFSET,
    fr16 = 16 + FP_REG_OFFSET,
    fr17 = 17 + FP_REG_OFFSET,
    fr18 = 18 + FP_REG_OFFSET,
    fr19 = 19 + FP_REG_OFFSET,
    fr20 = 20 + FP_REG_OFFSET,
    fr21 = 21 + FP_REG_OFFSET,
    fr22 = 22 + FP_REG_OFFSET,
    fr23 = 23 + FP_REG_OFFSET,
    fr24 = 24 + FP_REG_OFFSET,
    fr25 = 25 + FP_REG_OFFSET,
    fr26 = 26 + FP_REG_OFFSET,
    fr27 = 27 + FP_REG_OFFSET,
    fr28 = 28 + FP_REG_OFFSET,
    fr29 = 29 + FP_REG_OFFSET,
    fr30 = 30 + FP_REG_OFFSET,
    fr31 = 31 + FP_REG_OFFSET,
    dr0 = fr0 + FP_DOUBLE,
    dr1 = fr2 + FP_DOUBLE,
    dr2 = fr4 + FP_DOUBLE,
    dr3 = fr6 + FP_DOUBLE,
    dr4 = fr8 + FP_DOUBLE,
    dr5 = fr10 + FP_DOUBLE,
    dr6 = fr12 + FP_DOUBLE,
    dr7 = fr14 + FP_DOUBLE,
    dr8 = fr16 + FP_DOUBLE,
    dr9 = fr18 + FP_DOUBLE,
    dr10 = fr20 + FP_DOUBLE,
    dr11 = fr22 + FP_DOUBLE,
    dr12 = fr24 + FP_DOUBLE,
    dr13 = fr26 + FP_DOUBLE,
    dr14 = fr28 + FP_DOUBLE,
    dr15 = fr30 + FP_DOUBLE,
} NativeRegisterPool;

/* Shift encodings */
typedef enum ArmShiftEncodings {
    kArmLsl = 0x0,
    kArmLsr = 0x1,
    kArmAsr = 0x2,
    kArmRor = 0x3
} ArmShiftEncodings;

/* Thumb condition encodings */
typedef enum ArmConditionCode {
    kArmCondEq = 0x0,    /* 0000 */
    kArmCondNe = 0x1,    /* 0001 */
    kArmCondCs = 0x2,    /* 0010 */
    kArmCondCc = 0x3,    /* 0011 */
    kArmCondMi = 0x4,    /* 0100 */
    kArmCondPl = 0x5,    /* 0101 */
    kArmCondVs = 0x6,    /* 0110 */
    kArmCondVc = 0x7,    /* 0111 */
    kArmCondHi = 0x8,    /* 1000 */
    kArmCondLs = 0x9,    /* 1001 */
    kArmCondGe = 0xa,    /* 1010 */
    kArmCondLt = 0xb,    /* 1011 */
    kArmCondGt = 0xc,    /* 1100 */
    kArmCondLe = 0xd,    /* 1101 */
    kArmCondAl = 0xe,    /* 1110 */
    kArmCondNv = 0xf,    /* 1111 */
} ArmConditionCode;

#define isPseudoOpcode(opcode) ((int)(opcode) < 0)

/*
 * The following enum defines the list of supported Thumb instructions by the
 * assembler. Their corresponding snippet positions will be defined in
 * Assemble.c.
 */
typedef enum ArmOpcode {
    kArmChainingCellBottom = -18,
    kArmPseudoBarrier = -17,
    kArmPseudoExtended = -16,
    kArmPseudoSSARep = -15,
    kArmPseudoEntryBlock = -14,
    kArmPseudoExitBlock = -13,
    kArmPseudoTargetLabel = -12,
    kArmPseudoChainingCellBackwardBranch = -11,
    kArmPseudoChainingCellHot = -10,
    kArmPseudoChainingCellInvokePredicted = -9,
    kArmPseudoChainingCellInvokeSingleton = -8,
    kArmPseudoChainingCellNormal = -7,
    kArmPseudoDalvikByteCodeBoundary = -6,
    kArmPseudoPseudoAlign4 = -5,
    kArmPseudoPCReconstructionCell = -4,
    kArmPseudoPCReconstructionBlockLabel = -3,
    kArmPseudoEHBlockLabel = -2,
    kArmPseudoNormalBlockLabel = -1,
    /************************************************************************/
    kArm16BitData,       /* DATA   [0] rd[15..0] */
    kThumbAdcRR,         /* adc     [0100000101] rm[5..3] rd[2..0] */
    kThumbAddRRI3,       /* add(1)  [0001110] imm_3[8..6] rn[5..3] rd[2..0]*/
    kThumbAddRI8,        /* add(2)  [00110] rd[10..8] imm_8[7..0] */
    kThumbAddRRR,        /* add(3)  [0001100] rm[8..6] rn[5..3] rd[2..0] */
    kThumbAddRRLH,       /* add(4)  [01000100] H12[01] rm[5..3] rd[2..0] */
    kThumbAddRRHL,       /* add(4)  [01001000] H12[10] rm[5..3] rd[2..0] */
    kThumbAddRRHH,       /* add(4)  [01001100] H12[11] rm[5..3] rd[2..0] */
    kThumbAddPcRel,      /* add(5)  [10100] rd[10..8] imm_8[7..0] */
    kThumbAddSpRel,      /* add(6)  [10101] rd[10..8] imm_8[7..0] */
    kThumbAddSpI7,       /* add(7)  [101100000] imm_7[6..0] */
    kThumbAndRR,         /* and     [0100000000] rm[5..3] rd[2..0] */
    kThumbAsrRRI5,       /* asr(1)  [00010] imm_5[10..6] rm[5..3] rd[2..0] */
    kThumbAsrRR,         /* asr(2)  [0100000100] rs[5..3] rd[2..0] */
    kThumbBCond,         /* b(1)    [1101] cond[11..8] offset_8[7..0] */
    kThumbBUncond,       /* b(2)    [11100] offset_11[10..0] */
    kThumbBicRR,         /* bic     [0100001110] rm[5..3] rd[2..0] */
    kThumbBkpt,          /* bkpt    [10111110] imm_8[7..0] */
    kThumbBlx1,          /* blx(1)  [111] H[10] offset_11[10..0] */
    kThumbBlx2,          /* blx(1)  [111] H[01] offset_11[10..0] */
    kThumbBl1,           /* blx(1)  [111] H[10] offset_11[10..0] */
    kThumbBl2,           /* blx(1)  [111] H[11] offset_11[10..0] */
    kThumbBlxR,          /* blx(2)  [010001111] rm[6..3] [000] */
    kThumbBx,            /* bx      [010001110] H2[6..6] rm[5..3] SBZ[000] */
    kThumbCmnRR,         /* cmn     [0100001011] rm[5..3] rd[2..0] */
    kThumbCmpRI8,        /* cmp(1)  [00101] rn[10..8] imm_8[7..0] */
    kThumbCmpRR,         /* cmp(2)  [0100001010] rm[5..3] rd[2..0] */
    kThumbCmpLH,         /* cmp(3)  [01000101] H12[01] rm[5..3] rd[2..0] */
    kThumbCmpHL,         /* cmp(3)  [01000110] H12[10] rm[5..3] rd[2..0] */
    kThumbCmpHH,         /* cmp(3)  [01000111] H12[11] rm[5..3] rd[2..0] */
    kThumbEorRR,         /* eor     [0100000001] rm[5..3] rd[2..0] */
    kThumbLdmia,         /* ldmia   [11001] rn[10..8] reglist [7..0] */
    kThumbLdrRRI5,       /* ldr(1)  [01101] imm_5[10..6] rn[5..3] rd[2..0] */
    kThumbLdrRRR,        /* ldr(2)  [0101100] rm[8..6] rn[5..3] rd[2..0] */
    kThumbLdrPcRel,      /* ldr(3)  [01001] rd[10..8] imm_8[7..0] */
    kThumbLdrSpRel,      /* ldr(4)  [10011] rd[10..8] imm_8[7..0] */
    kThumbLdrbRRI5,      /* ldrb(1) [01111] imm_5[10..6] rn[5..3] rd[2..0] */
    kThumbLdrbRRR,       /* ldrb(2) [0101110] rm[8..6] rn[5..3] rd[2..0] */
    kThumbLdrhRRI5,      /* ldrh(1) [10001] imm_5[10..6] rn[5..3] rd[2..0] */
    kThumbLdrhRRR,       /* ldrh(2) [0101101] rm[8..6] rn[5..3] rd[2..0] */
    kThumbLdrsbRRR,      /* ldrsb   [0101011] rm[8..6] rn[5..3] rd[2..0] */
    kThumbLdrshRRR,      /* ldrsh   [0101111] rm[8..6] rn[5..3] rd[2..0] */
    kThumbLslRRI5,       /* lsl(1)  [00000] imm_5[10..6] rm[5..3] rd[2..0] */
    kThumbLslRR,         /* lsl(2)  [0100000010] rs[5..3] rd[2..0] */
    kThumbLsrRRI5,       /* lsr(1)  [00001] imm_5[10..6] rm[5..3] rd[2..0] */
    kThumbLsrRR,         /* lsr(2)  [0100000011] rs[5..3] rd[2..0] */
    kThumbMovImm,        /* mov(1)  [00100] rd[10..8] imm_8[7..0] */
    kThumbMovRR,         /* mov(2)  [0001110000] rn[5..3] rd[2..0] */
    kThumbMovRR_H2H,     /* mov(3)  [01000111] H12[11] rm[5..3] rd[2..0] */
    kThumbMovRR_H2L,     /* mov(3)  [01000110] H12[01] rm[5..3] rd[2..0] */
    kThumbMovRR_L2H,     /* mov(3)  [01000101] H12[10] rm[5..3] rd[2..0] */
    kThumbMul,           /* mul     [0100001101] rm[5..3] rd[2..0] */
    kThumbMvn,           /* mvn     [0100001111] rm[5..3] rd[2..0] */
    kThumbNeg,           /* neg     [0100001001] rm[5..3] rd[2..0] */
    kThumbOrr,           /* orr     [0100001100] rm[5..3] rd[2..0] */
    kThumbPop,           /* pop     [1011110] r[8..8] rl[7..0] */
    kThumbPush,          /* push    [1011010] r[8..8] rl[7..0] */
    kThumbRorRR,         /* ror     [0100000111] rs[5..3] rd[2..0] */
    kThumbSbc,           /* sbc     [0100000110] rm[5..3] rd[2..0] */
    kThumbStmia,         /* stmia   [11000] rn[10..8] reglist [7.. 0] */
    kThumbStrRRI5,       /* str(1)  [01100] imm_5[10..6] rn[5..3] rd[2..0] */
    kThumbStrRRR,        /* str(2)  [0101000] rm[8..6] rn[5..3] rd[2..0] */
    kThumbStrSpRel,      /* str(3)  [10010] rd[10..8] imm_8[7..0] */
    kThumbStrbRRI5,      /* strb(1) [01110] imm_5[10..6] rn[5..3] rd[2..0] */
    kThumbStrbRRR,       /* strb(2) [0101010] rm[8..6] rn[5..3] rd[2..0] */
    kThumbStrhRRI5,      /* strh(1) [10000] imm_5[10..6] rn[5..3] rd[2..0] */
    kThumbStrhRRR,       /* strh(2) [0101001] rm[8..6] rn[5..3] rd[2..0] */
    kThumbSubRRI3,       /* sub(1)  [0001111] imm_3[8..6] rn[5..3] rd[2..0]*/
    kThumbSubRI8,        /* sub(2)  [00111] rd[10..8] imm_8[7..0] */
    kThumbSubRRR,        /* sub(3)  [0001101] rm[8..6] rn[5..3] rd[2..0] */
    kThumbSubSpI7,       /* sub(4)  [101100001] imm_7[6..0] */
    kThumbSwi,           /* swi     [11011111] imm_8[7..0] */
    kThumbTst,           /* tst     [0100001000] rm[5..3] rn[2..0] */
    kThumb2Vldrs,        /* vldr low  sx [111011011001] rn[19..16] rd[15-12]
                                    [1010] imm_8[7..0] */
    kThumb2Vldrd,        /* vldr low  dx [111011011001] rn[19..16] rd[15-12]
                                    [1011] imm_8[7..0] */
    kThumb2Vmuls,        /* vmul vd, vn, vm [111011100010] rn[19..16]
                                    rd[15-12] [10100000] rm[3..0] */
    kThumb2Vmuld,        /* vmul vd, vn, vm [111011100010] rn[19..16]
                                    rd[15-12] [10110000] rm[3..0] */
    kThumb2Vstrs,        /* vstr low  sx [111011011000] rn[19..16] rd[15-12]
                                    [1010] imm_8[7..0] */
    kThumb2Vstrd,        /* vstr low  dx [111011011000] rn[19..16] rd[15-12]
                                    [1011] imm_8[7..0] */
    kThumb2Vsubs,        /* vsub vd, vn, vm [111011100011] rn[19..16]
                                    rd[15-12] [10100040] rm[3..0] */
    kThumb2Vsubd,        /* vsub vd, vn, vm [111011100011] rn[19..16]
                                    rd[15-12] [10110040] rm[3..0] */
    kThumb2Vadds,        /* vadd vd, vn, vm [111011100011] rn[19..16]
                                    rd[15-12] [10100000] rm[3..0] */
    kThumb2Vaddd,        /* vadd vd, vn, vm [111011100011] rn[19..16]
                                    rd[15-12] [10110000] rm[3..0] */
    kThumb2Vdivs,        /* vdiv vd, vn, vm [111011101000] rn[19..16]
                                    rd[15-12] [10100000] rm[3..0] */
    kThumb2Vdivd,        /* vdiv vd, vn, vm [111011101000] rn[19..16]
                                    rd[15-12] [10110000] rm[3..0] */
    kThumb2VcvtIF,       /* vcvt.F32 vd, vm [1110111010111000] vd[15..12]
                                    [10101100] vm[3..0] */
    kThumb2VcvtID,       /* vcvt.F64 vd, vm [1110111010111000] vd[15..12]
                                       [10111100] vm[3..0] */
    kThumb2VcvtFI,       /* vcvt.S32.F32 vd, vm [1110111010111101] vd[15..12]
                                       [10101100] vm[3..0] */
    kThumb2VcvtDI,       /* vcvt.S32.F32 vd, vm [1110111010111101] vd[15..12]
                                       [10111100] vm[3..0] */
    kThumb2VcvtFd,       /* vcvt.F64.F32 vd, vm [1110111010110111] vd[15..12]
                                       [10101100] vm[3..0] */
    kThumb2VcvtDF,       /* vcvt.F32.F64 vd, vm [1110111010110111] vd[15..12]
                                       [10111100] vm[3..0] */
    kThumb2Vsqrts,       /* vsqrt.f32 vd, vm [1110111010110001] vd[15..12]
                                       [10101100] vm[3..0] */
    kThumb2Vsqrtd,       /* vsqrt.f64 vd, vm [1110111010110001] vd[15..12]
                                       [10111100] vm[3..0] */
    kThumb2MovImmShift,  /* mov(T2) rd, #<const> [11110] i [00001001111]
                                       imm3 rd[11..8] imm8 */
    kThumb2MovImm16,     /* mov(T3) rd, #<const> [11110] i [0010100] imm4 [0]
                                       imm3 rd[11..8] imm8 */
    kThumb2StrRRI12,     /* str(Imm,T3) rd,[rn,#imm12] [111110001100]
                                       rn[19..16] rt[15..12] imm12[11..0] */
    kThumb2LdrRRI12,     /* str(Imm,T3) rd,[rn,#imm12] [111110001100]
                                       rn[19..16] rt[15..12] imm12[11..0] */
    kThumb2StrRRI8Predec, /* str(Imm,T4) rd,[rn,#-imm8] [111110000100]
                                       rn[19..16] rt[15..12] [1100] imm[7..0]*/
    kThumb2LdrRRI8Predec, /* ldr(Imm,T4) rd,[rn,#-imm8] [111110000101]
                                       rn[19..16] rt[15..12] [1100] imm[7..0]*/
    kThumb2Cbnz,         /* cbnz rd,<label> [101110] i [1] imm5[7..3]
                                       rn[2..0] */
    kThumb2Cbz,          /* cbn rd,<label> [101100] i [1] imm5[7..3]
                                       rn[2..0] */
    kThumb2AddRRI12,     /* add rd, rn, #imm12 [11110] i [100000] rn[19..16]
                                       [0] imm3[14..12] rd[11..8] imm8[7..0] */
    kThumb2MovRR,        /* mov rd, rm [11101010010011110000] rd[11..8]
                                       [0000] rm[3..0] */
    kThumb2Vmovs,        /* vmov.f32 vd, vm [111011101] D [110000]
                                       vd[15..12] 101001] M [0] vm[3..0] */
    kThumb2Vmovd,        /* vmov.f64 vd, vm [111011101] D [110000]
                                       vd[15..12] 101101] M [0] vm[3..0] */
    kThumb2Ldmia,        /* ldmia  [111010001001[ rn[19..16] mask[15..0] */
    kThumb2Stmia,        /* stmia  [111010001000[ rn[19..16] mask[15..0] */
    kThumb2AddRRR,       /* add [111010110000] rn[19..16] [0000] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2SubRRR,       /* sub [111010111010] rn[19..16] [0000] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2SbcRRR,       /* sbc [111010110110] rn[19..16] [0000] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2CmpRR,        /* cmp [111010111011] rn[19..16] [0000] [1111]
                                   [0000] rm[3..0] */
    kThumb2SubRRI12,     /* sub rd, rn, #imm12 [11110] i [01010] rn[19..16]
                                       [0] imm3[14..12] rd[11..8] imm8[7..0] */
    kThumb2MvnImmShift,  /* mov(T2) rd, #<const> [11110] i [00011011110]
                                       imm3 rd[11..8] imm8 */
    kThumb2Sel,          /* sel rd, rn, rm [111110101010] rn[19-16] rd[11-8]
                                       rm[3-0] */
    kThumb2Ubfx,         /* ubfx rd,rn,#lsb,#width [111100111100] rn[19..16]
                                       [0] imm3[14-12] rd[11-8] w[4-0] */
    kThumb2Sbfx,         /* ubfx rd,rn,#lsb,#width [111100110100] rn[19..16]
                                       [0] imm3[14-12] rd[11-8] w[4-0] */
    kThumb2LdrRRR,       /* ldr rt,[rn,rm,LSL #imm] [111110000101] rn[19-16]
                                       rt[15-12] [000000] imm[5-4] rm[3-0] */
    kThumb2LdrhRRR,      /* ldrh rt,[rn,rm,LSL #imm] [111110000101] rn[19-16]
                                       rt[15-12] [000000] imm[5-4] rm[3-0] */
    kThumb2LdrshRRR,     /* ldrsh rt,[rn,rm,LSL #imm] [111110000101] rn[19-16]
                                       rt[15-12] [000000] imm[5-4] rm[3-0] */
    kThumb2LdrbRRR,      /* ldrb rt,[rn,rm,LSL #imm] [111110000101] rn[19-16]
                                       rt[15-12] [000000] imm[5-4] rm[3-0] */
    kThumb2LdrsbRRR,     /* ldrsb rt,[rn,rm,LSL #imm] [111110000101] rn[19-16]
                                       rt[15-12] [000000] imm[5-4] rm[3-0] */
    kThumb2StrRRR,       /* str rt,[rn,rm,LSL #imm] [111110000100] rn[19-16]
                                       rt[15-12] [000000] imm[5-4] rm[3-0] */
    kThumb2StrhRRR,      /* str rt,[rn,rm,LSL #imm] [111110000010] rn[19-16]
                                       rt[15-12] [000000] imm[5-4] rm[3-0] */
    kThumb2StrbRRR,      /* str rt,[rn,rm,LSL #imm] [111110000000] rn[19-16]
                                       rt[15-12] [000000] imm[5-4] rm[3-0] */
    kThumb2LdrhRRI12,    /* ldrh rt,[rn,#imm12] [111110001011]
                                       rt[15..12] rn[19..16] imm12[11..0] */
    kThumb2LdrshRRI12,   /* ldrsh rt,[rn,#imm12] [111110011011]
                                       rt[15..12] rn[19..16] imm12[11..0] */
    kThumb2LdrbRRI12,    /* ldrb rt,[rn,#imm12] [111110001001]
                                       rt[15..12] rn[19..16] imm12[11..0] */
    kThumb2LdrsbRRI12,   /* ldrsb rt,[rn,#imm12] [111110011001]
                                       rt[15..12] rn[19..16] imm12[11..0] */
    kThumb2StrhRRI12,    /* strh rt,[rn,#imm12] [111110001010]
                                       rt[15..12] rn[19..16] imm12[11..0] */
    kThumb2StrbRRI12,    /* strb rt,[rn,#imm12] [111110001000]
                                       rt[15..12] rn[19..16] imm12[11..0] */
    kThumb2Pop,          /* pop     [1110100010111101] list[15-0]*/
    kThumb2Push,         /* push    [1110100100101101] list[15-0]*/
    kThumb2CmpRI8,       /* cmp rn, #<const> [11110] i [011011] rn[19-16] [0]
                                       imm3 [1111] imm8[7..0] */
    kThumb2AdcRRR,       /* adc [111010110101] rn[19..16] [0000] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2AndRRR,       /* and [111010100000] rn[19..16] [0000] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2BicRRR,       /* bic [111010100010] rn[19..16] [0000] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2CmnRR,        /* cmn [111010110001] rn[19..16] [0000] [1111]
                                   [0000] rm[3..0] */
    kThumb2EorRRR,       /* eor [111010101000] rn[19..16] [0000] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2MulRRR,       /* mul [111110110000] rn[19..16] [1111] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2MnvRR,        /* mvn [11101010011011110] rd[11-8] [0000]
                                   rm[3..0] */
    kThumb2RsubRRI8,     /* rsub [111100011100] rn[19..16] [0000] rd[11..8]
                                   imm8[7..0] */
    kThumb2NegRR,        /* actually rsub rd, rn, #0 */
    kThumb2OrrRRR,       /* orr [111010100100] rn[19..16] [0000] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2TstRR,        /* tst [111010100001] rn[19..16] [0000] [1111]
                                   [0000] rm[3..0] */
    kThumb2LslRRR,       /* lsl [111110100000] rn[19..16] [1111] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2LsrRRR,       /* lsr [111110100010] rn[19..16] [1111] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2AsrRRR,       /* asr [111110100100] rn[19..16] [1111] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2RorRRR,       /* ror [111110100110] rn[19..16] [1111] rd[11..8]
                                   [0000] rm[3..0] */
    kThumb2LslRRI5,      /* lsl [11101010010011110] imm[14.12] rd[11..8]
                                   [00] rm[3..0] */
    kThumb2LsrRRI5,      /* lsr [11101010010011110] imm[14.12] rd[11..8]
                                   [01] rm[3..0] */
    kThumb2AsrRRI5,      /* asr [11101010010011110] imm[14.12] rd[11..8]
                                   [10] rm[3..0] */
    kThumb2RorRRI5,      /* ror [11101010010011110] imm[14.12] rd[11..8]
                                   [11] rm[3..0] */
    kThumb2BicRRI8,      /* bic [111100000010] rn[19..16] [0] imm3
                                   rd[11..8] imm8 */
    kThumb2AndRRI8,      /* bic [111100000000] rn[19..16] [0] imm3
                                   rd[11..8] imm8 */
    kThumb2OrrRRI8,      /* orr [111100000100] rn[19..16] [0] imm3
                                   rd[11..8] imm8 */
    kThumb2EorRRI8,      /* eor [111100001000] rn[19..16] [0] imm3
                                   rd[11..8] imm8 */
    kThumb2AddRRI8,      /* add [111100001000] rn[19..16] [0] imm3
                                   rd[11..8] imm8 */
    kThumb2AdcRRI8,      /* adc [111100010101] rn[19..16] [0] imm3
                                   rd[11..8] imm8 */
    kThumb2SubRRI8,      /* sub [111100011011] rn[19..16] [0] imm3
                                   rd[11..8] imm8 */
    kThumb2SbcRRI8,      /* sbc [111100010111] rn[19..16] [0] imm3
                                   rd[11..8] imm8 */
    kThumb2It,           /* it [10111111] firstcond[7-4] mask[3-0] */
    kThumb2Fmstat,       /* fmstat [11101110111100011111101000010000] */
    kThumb2Vcmpd,        /* vcmp [111011101] D [11011] rd[15-12] [1011]
                                   E [1] M [0] rm[3-0] */
    kThumb2Vcmps,        /* vcmp [111011101] D [11010] rd[15-12] [1011]
                                   E [1] M [0] rm[3-0] */
    kThumb2LdrPcRel12,   /* ldr rd,[pc,#imm12] [1111100011011111] rt[15-12]
                                  imm12[11-0] */
    kThumb2BCond,        /* b<c> [1110] S cond[25-22] imm6[21-16] [10]
                                  J1 [0] J2 imm11[10..0] */
    kThumb2Vmovd_RR,     /* vmov [111011101] D [110000] vd[15-12 [101101]
                                  M [0] vm[3-0] */
    kThumb2Vmovs_RR,     /* vmov [111011101] D [110000] vd[15-12 [101001]
                                  M [0] vm[3-0] */
    kThumb2Fmrs,         /* vmov [111011100000] vn[19-16] rt[15-12] [1010]
                                  N [0010000] */
    kThumb2Fmsr,         /* vmov [111011100001] vn[19-16] rt[15-12] [1010]
                                  N [0010000] */
    kThumb2Fmrrd,        /* vmov [111011000100] rt2[19-16] rt[15-12]
                                  [101100] M [1] vm[3-0] */
    kThumb2Fmdrr,        /* vmov [111011000101] rt2[19-16] rt[15-12]
                                  [101100] M [1] vm[3-0] */
    kThumb2Vabsd,        /* vabs.f64 [111011101] D [110000] rd[15-12]
                                  [1011110] M [0] vm[3-0] */
    kThumb2Vabss,        /* vabs.f32 [111011101] D [110000] rd[15-12]
                                  [1010110] M [0] vm[3-0] */
    kThumb2Vnegd,        /* vneg.f64 [111011101] D [110000] rd[15-12]
                                  [1011110] M [0] vm[3-0] */
    kThumb2Vnegs,        /* vneg.f32 [111011101] D [110000] rd[15-12]
                                 [1010110] M [0] vm[3-0] */
    kThumb2Vmovs_IMM8,   /* vmov.f32 [111011101] D [11] imm4h[19-16] vd[15-12]
                                  [10100000] imm4l[3-0] */
    kThumb2Vmovd_IMM8,   /* vmov.f64 [111011101] D [11] imm4h[19-16] vd[15-12]
                                  [10110000] imm4l[3-0] */
    kThumb2Mla,          /* mla [111110110000] rn[19-16] ra[15-12] rd[7-4]
                                  [0000] rm[3-0] */
    kThumb2Umull,        /* umull [111110111010] rn[19-16], rdlo[15-12]
                                  rdhi[11-8] [0000] rm[3-0] */
    kThumb2Ldrex,        /* ldrex [111010000101] rn[19-16] rt[11-8] [1111]
                                  imm8[7-0] */
    kThumb2Strex,        /* strex [111010000100] rn[19-16] rt[11-8] rd[11-8]
                                  imm8[7-0] */
    kThumb2Clrex,        /* clrex [111100111011111110000111100101111] */
    kThumb2Bfi,          /* bfi [111100110110] rn[19-16] [0] imm3[14-12]
                                  rd[11-8] imm2[7-6] [0] msb[4-0] */
    kThumb2Bfc,          /* bfc [11110011011011110] [0] imm3[14-12]
                                  rd[11-8] imm2[7-6] [0] msb[4-0] */
    kThumb2Dmb,          /* dmb [1111001110111111100011110101] option[3-0] */
    kThumb2LdrPcReln12,  /* ldr rd,[pc,-#imm12] [1111100011011111] rt[15-12]
                                  imm12[11-0] */
    kThumb2RsbRRR,       /* rsb [111010111101] rn[19..16] [0000] rd[11..8]
                                  [0000] rm[3..0] */
    kThumbUndefined,     /* undefined [11011110xxxxxxxx] */
    kArmLast,
} ArmOpcode;

/* DMB option encodings */
typedef enum ArmOpDmbOptions {
    kSY = 0xf,
    kST = 0xe,
    kISH = 0xb,
    kISHST = 0xa,
    kNSH = 0x7,
    kNSHST = 0x6
} ArmOpDmbOptions;

/* Bit flags describing the behavior of each native opcode */
typedef enum ArmOpFeatureFlags {
    kIsBranch = 0,
    kRegDef0,
    kRegDef1,
    kRegDefSP,
    kRegDefLR,
    kRegDefList0,
    kRegDefList1,
    kRegUse0,
    kRegUse1,
    kRegUse2,
    kRegUse3,
    kRegUseSP,
    kRegUsePC,
    kRegUseList0,
    kRegUseList1,
    kNoOperand,
    kIsUnaryOp,
    kIsBinaryOp,
    kIsTertiaryOp,
    kIsQuadOp,
    kIsIT,
    kSetsCCodes,
    kUsesCCodes,
    kMemLoad,
    kMemStore,
} ArmOpFeatureFlags;

#define IS_LOAD         (1 << kMemLoad)
#define IS_STORE        (1 << kMemStore)
#define IS_BRANCH       (1 << kIsBranch)
#define REG_DEF0        (1 << kRegDef0)
#define REG_DEF1        (1 << kRegDef1)
#define REG_DEF_SP      (1 << kRegDefSP)
#define REG_DEF_LR      (1 << kRegDefLR)
#define REG_DEF_LIST0   (1 << kRegDefList0)
#define REG_DEF_LIST1   (1 << kRegDefList1)
#define REG_USE0        (1 << kRegUse0)
#define REG_USE1        (1 << kRegUse1)
#define REG_USE2        (1 << kRegUse2)
#define REG_USE3        (1 << kRegUse3)
#define REG_USE_SP      (1 << kRegUseSP)
#define REG_USE_PC      (1 << kRegUsePC)
#define REG_USE_LIST0   (1 << kRegUseList0)
#define REG_USE_LIST1   (1 << kRegUseList1)
#define NO_OPERAND      (1 << kNoOperand)
#define IS_UNARY_OP     (1 << kIsUnaryOp)
#define IS_BINARY_OP    (1 << kIsBinaryOp)
#define IS_TERTIARY_OP  (1 << kIsTertiaryOp)
#define IS_QUAD_OP      (1 << kIsQuadOp)
#define IS_IT           (1 << kIsIT)
#define SETS_CCODES     (1 << kSetsCCodes)
#define USES_CCODES     (1 << kUsesCCodes)

/* Common combo register usage patterns */
#define REG_USE01       (REG_USE0 | REG_USE1)
#define REG_USE012      (REG_USE01 | REG_USE2)
#define REG_USE12       (REG_USE1 | REG_USE2)
#define REG_DEF0_USE0   (REG_DEF0 | REG_USE0)
#define REG_DEF0_USE1   (REG_DEF0 | REG_USE1)
#define REG_DEF0_USE01  (REG_DEF0 | REG_USE01)
#define REG_DEF0_USE12  (REG_DEF0 | REG_USE12)
#define REG_DEF01_USE2  (REG_DEF0 | REG_DEF1 | REG_USE2)

/* Instruction assembly fieldLoc kind */
typedef enum ArmEncodingKind {
    kFmtUnused,
    kFmtBitBlt,        /* Bit string using end/start */
    kFmtDfp,           /* Double FP reg */
    kFmtSfp,           /* Single FP reg */
    kFmtModImm,        /* Shifted 8-bit immed using [26,14..12,7..0] */
    kFmtImm16,         /* Zero-extended immed using [26,19..16,14..12,7..0] */
    kFmtImm6,          /* Encoded branch target using [9,7..3]0 */
    kFmtImm12,         /* Zero-extended immediate using [26,14..12,7..0] */
    kFmtShift,         /* Shift descriptor, [14..12,7..4] */
    kFmtLsb,           /* least significant bit using [14..12][7..6] */
    kFmtBWidth,        /* bit-field width, encoded as width-1 */
    kFmtShift5,        /* Shift count, [14..12,7..6] */
    kFmtBrOffset,      /* Signed extended [26,11,13,21-16,10-0]:0 */
    kFmtFPImm,         /* Encoded floating point immediate */
} ArmEncodingKind;

/* Struct used to define the snippet positions for each Thumb opcode */
typedef struct ArmEncodingMap {
    u4 skeleton;
    struct {
        ArmEncodingKind kind;
        int end;   /* end for kFmtBitBlt, 1-bit slice end for FP regs */
        int start; /* start for kFmtBitBlt, 4-bit slice end for FP regs */
    } fieldLoc[4];
    ArmOpcode opcode;
    int flags;
    const char* name;
    const char* fmt;
    int size;
} ArmEncodingMap;

/* Keys for target-specific scheduling and other optimization hints */
typedef enum ArmTargetOptHints {
    kMaxHoistDistance,
} ArmTargetOptHints;

extern ArmEncodingMap EncodingMap[kArmLast];

/*
 * Each instance of this struct holds a pseudo or real LIR instruction:
 * - pseudo ones (eg labels and marks) and will be discarded by the assembler.
 * - real ones will be assembled into Thumb instructions.
 *
 * Machine resources are encoded into a 64-bit vector, where the encodings are
 * as following:
 * - [ 0..15]: general purpose registers including PC, SP, and LR
 * - [16..47]: floating-point registers where d0 is expanded to s[01] and s0
 *   starts at bit 16
 * - [48]: IT block
 * - [49]: integer condition code
 * - [50]: floatint-point status word
 */
typedef struct ArmLIR {
    LIR generic;
    ArmOpcode opcode;
    int operands[4];            // [0..3] = [dest, src1, src2, extra]
    struct {
        bool isNop:1;           // LIR is optimized away
        bool insertWrapper:1;   // insert branch to emulate memory accesses
        unsigned int age:4;     // default is 0, set lazily by the optimizer
        unsigned int size:3;    // bytes (2 for thumb, 2/4 for thumb2)
        unsigned int unused:23;
    } flags;
    int aliasInfo;              // For Dalvik register & litpool disambiguation
    u8 useMask;                 // Resource mask for use
    u8 defMask;                 // Resource mask for def
} ArmLIR;

/* Init values when a predicted chain is initially assembled */
/* E7FE is branch to self */
#define PREDICTED_CHAIN_BX_PAIR_INIT     0xe7fe

/* Utility macros to traverse the LIR/ArmLIR list */
#define NEXT_LIR(lir) ((ArmLIR *) lir->generic.next)
#define PREV_LIR(lir) ((ArmLIR *) lir->generic.prev)

#define NEXT_LIR_LVALUE(lir) (lir)->generic.next
#define PREV_LIR_LVALUE(lir) (lir)->generic.prev

#define CHAIN_CELL_OFFSET_TAG   0xcdab

#define CHAIN_CELL_NORMAL_SIZE 12
#define CHAIN_CELL_PREDICTED_SIZE 16

#endif  // DALVIK_VM_COMPILER_CODEGEN_ARM_ARMLIR_H_
