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

#ifndef ART_COMPILER_DEX_QUICK_ARM_ARM_LIR_H_
#define ART_COMPILER_DEX_QUICK_ARM_ARM_LIR_H_

#include "dex/compiler_internals.h"

namespace art {

/*
 * Runtime register usage conventions.
 *
 * r0-r3: Argument registers in both Dalvik and C/C++ conventions.
 *        However, for Dalvik->Dalvik calls we'll pass the target's Method*
 *        pointer in r0 as a hidden arg0. Otherwise used as codegen scratch
 *        registers.
 * r0-r1: As in C/C++ r0 is 32-bit return register and r0/r1 is 64-bit
 * r4   : (rARM_SUSPEND) is reserved (suspend check/debugger assist)
 * r5   : Callee save (promotion target)
 * r6   : Callee save (promotion target)
 * r7   : Callee save (promotion target)
 * r8   : Callee save (promotion target)
 * r9   : (rARM_SELF) is reserved (pointer to thread-local storage)
 * r10  : Callee save (promotion target)
 * r11  : Callee save (promotion target)
 * r12  : Scratch, may be trashed by linkage stubs
 * r13  : (sp) is reserved
 * r14  : (lr) is reserved
 * r15  : (pc) is reserved
 *
 * 5 core temps that codegen can use (r0, r1, r2, r3, r12)
 * 7 core registers that can be used for promotion
 *
 * Floating pointer registers
 * s0-s31
 * d0-d15, where d0={s0,s1}, d1={s2,s3}, ... , d15={s30,s31}
 *
 * s16-s31 (d8-d15) preserved across C calls
 * s0-s15 (d0-d7) trashed across C calls
 *
 * s0-s15/d0-d7 used as codegen temp/scratch
 * s16-s31/d8-d31 can be used for promotion.
 *
 * Calling convention
 *     o On a call to a Dalvik method, pass target's Method* in r0
 *     o r1-r3 will be used for up to the first 3 words of arguments
 *     o Arguments past the first 3 words will be placed in appropriate
 *       out slots by the caller.
 *     o If a 64-bit argument would span the register/memory argument
 *       boundary, it will instead be fully passed in the frame.
 *     o Maintain a 16-byte stack alignment
 *
 *  Stack frame diagram (stack grows down, higher addresses at top):
 *
 * +------------------------+
 * | IN[ins-1]              |  {Note: resides in caller's frame}
 * |       .                |
 * | IN[0]                  |
 * | caller's Method*       |
 * +========================+  {Note: start of callee's frame}
 * | spill region           |  {variable sized - will include lr if non-leaf.}
 * +------------------------+
 * | ...filler word...      |  {Note: used as 2nd word of V[locals-1] if long]
 * +------------------------+
 * | V[locals-1]            |
 * | V[locals-2]            |
 * |      .                 |
 * |      .                 |
 * | V[1]                   |
 * | V[0]                   |
 * +------------------------+
 * |  0 to 3 words padding  |
 * +------------------------+
 * | OUT[outs-1]            |
 * | OUT[outs-2]            |
 * |       .                |
 * | OUT[0]                 |
 * | cur_method*            | <<== sp w/ 16-byte alignment
 * +========================+
 */

// Offset to distingish FP regs.
#define ARM_FP_REG_OFFSET 32
// Offset to distinguish DP FP regs.
#define ARM_FP_DOUBLE 64
// First FP callee save.
#define ARM_FP_CALLEE_SAVE_BASE 16
// Reg types.
#define ARM_REGTYPE(x) (x & (ARM_FP_REG_OFFSET | ARM_FP_DOUBLE))
#define ARM_FPREG(x) ((x & ARM_FP_REG_OFFSET) == ARM_FP_REG_OFFSET)
#define ARM_LOWREG(x) ((x & 0x7) == x)
#define ARM_DOUBLEREG(x) ((x & ARM_FP_DOUBLE) == ARM_FP_DOUBLE)
#define ARM_SINGLEREG(x) (ARM_FPREG(x) && !ARM_DOUBLEREG(x))

/*
 * Note: the low register of a floating point pair is sufficient to
 * create the name of a double, but require both names to be passed to
 * allow for asserts to verify that the pair is consecutive if significant
 * rework is done in this area.  Also, it is a good reminder in the calling
 * code that reg locations always describe doubles as a pair of singles.
 */
#define ARM_S2D(x, y) ((x) | ARM_FP_DOUBLE)
// Mask to strip off fp flags.
#define ARM_FP_REG_MASK (ARM_FP_REG_OFFSET-1)

// RegisterLocation templates return values (r0, or r0/r1).
#define ARM_LOC_C_RETURN {kLocPhysReg, 0, 0, 0, 0, 0, 0, 0, 1, r0, INVALID_REG, \
                          INVALID_SREG, INVALID_SREG}
#define ARM_LOC_C_RETURN_WIDE {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, r0, r1, \
                               INVALID_SREG, INVALID_SREG}
#define ARM_LOC_C_RETURN_FLOAT  ARM_LOC_C_RETURN
#define ARM_LOC_C_RETURN_DOUBLE  ARM_LOC_C_RETURN_WIDE

enum ArmResourceEncodingPos {
  kArmGPReg0   = 0,
  kArmRegSP    = 13,
  kArmRegLR    = 14,
  kArmRegPC    = 15,
  kArmFPReg0   = 16,
  kArmFPReg16  = 32,
  kArmRegEnd   = 48,
};

#define ENCODE_ARM_REG_LIST(N)      (static_cast<uint64_t>(N))
#define ENCODE_ARM_REG_SP           (1ULL << kArmRegSP)
#define ENCODE_ARM_REG_LR           (1ULL << kArmRegLR)
#define ENCODE_ARM_REG_PC           (1ULL << kArmRegPC)
#define ENCODE_ARM_REG_FPCS_LIST(N) (static_cast<uint64_t>(N) << kArmFPReg16)

enum ArmNativeRegisterPool {
  r0   = 0,
  r1   = 1,
  r2   = 2,
  r3   = 3,
  rARM_SUSPEND = 4,
  r5   = 5,
  r6   = 6,
  r7   = 7,
  r8   = 8,
  rARM_SELF  = 9,
  r10  = 10,
  r11  = 11,
  r12  = 12,
  r13sp  = 13,
  rARM_SP  = 13,
  r14lr  = 14,
  rARM_LR  = 14,
  r15pc  = 15,
  rARM_PC  = 15,
  fr0  =  0 + ARM_FP_REG_OFFSET,
  fr1  =  1 + ARM_FP_REG_OFFSET,
  fr2  =  2 + ARM_FP_REG_OFFSET,
  fr3  =  3 + ARM_FP_REG_OFFSET,
  fr4  =  4 + ARM_FP_REG_OFFSET,
  fr5  =  5 + ARM_FP_REG_OFFSET,
  fr6  =  6 + ARM_FP_REG_OFFSET,
  fr7  =  7 + ARM_FP_REG_OFFSET,
  fr8  =  8 + ARM_FP_REG_OFFSET,
  fr9  =  9 + ARM_FP_REG_OFFSET,
  fr10 = 10 + ARM_FP_REG_OFFSET,
  fr11 = 11 + ARM_FP_REG_OFFSET,
  fr12 = 12 + ARM_FP_REG_OFFSET,
  fr13 = 13 + ARM_FP_REG_OFFSET,
  fr14 = 14 + ARM_FP_REG_OFFSET,
  fr15 = 15 + ARM_FP_REG_OFFSET,
  fr16 = 16 + ARM_FP_REG_OFFSET,
  fr17 = 17 + ARM_FP_REG_OFFSET,
  fr18 = 18 + ARM_FP_REG_OFFSET,
  fr19 = 19 + ARM_FP_REG_OFFSET,
  fr20 = 20 + ARM_FP_REG_OFFSET,
  fr21 = 21 + ARM_FP_REG_OFFSET,
  fr22 = 22 + ARM_FP_REG_OFFSET,
  fr23 = 23 + ARM_FP_REG_OFFSET,
  fr24 = 24 + ARM_FP_REG_OFFSET,
  fr25 = 25 + ARM_FP_REG_OFFSET,
  fr26 = 26 + ARM_FP_REG_OFFSET,
  fr27 = 27 + ARM_FP_REG_OFFSET,
  fr28 = 28 + ARM_FP_REG_OFFSET,
  fr29 = 29 + ARM_FP_REG_OFFSET,
  fr30 = 30 + ARM_FP_REG_OFFSET,
  fr31 = 31 + ARM_FP_REG_OFFSET,
  dr0 = fr0 + ARM_FP_DOUBLE,
  dr1 = fr2 + ARM_FP_DOUBLE,
  dr2 = fr4 + ARM_FP_DOUBLE,
  dr3 = fr6 + ARM_FP_DOUBLE,
  dr4 = fr8 + ARM_FP_DOUBLE,
  dr5 = fr10 + ARM_FP_DOUBLE,
  dr6 = fr12 + ARM_FP_DOUBLE,
  dr7 = fr14 + ARM_FP_DOUBLE,
  dr8 = fr16 + ARM_FP_DOUBLE,
  dr9 = fr18 + ARM_FP_DOUBLE,
  dr10 = fr20 + ARM_FP_DOUBLE,
  dr11 = fr22 + ARM_FP_DOUBLE,
  dr12 = fr24 + ARM_FP_DOUBLE,
  dr13 = fr26 + ARM_FP_DOUBLE,
  dr14 = fr28 + ARM_FP_DOUBLE,
  dr15 = fr30 + ARM_FP_DOUBLE,
};

// Target-independent aliases.
#define rARM_ARG0 r0
#define rARM_ARG1 r1
#define rARM_ARG2 r2
#define rARM_ARG3 r3
#define rARM_FARG0 r0
#define rARM_FARG1 r1
#define rARM_FARG2 r2
#define rARM_FARG3 r3
#define rARM_RET0 r0
#define rARM_RET1 r1
#define rARM_INVOKE_TGT rARM_LR
#define rARM_COUNT INVALID_REG

enum ArmShiftEncodings {
  kArmLsl = 0x0,
  kArmLsr = 0x1,
  kArmAsr = 0x2,
  kArmRor = 0x3
};

/*
 * The following enum defines the list of supported Thumb instructions by the
 * assembler. Their corresponding EncodingMap positions will be defined in
 * Assemble.cc.
 */
enum ArmOpcode {
  kArmFirst = 0,
  kArm16BitData = kArmFirst,  // DATA   [0] rd[15..0].
  kThumbAdcRR,       // adc   [0100000101] rm[5..3] rd[2..0].
  kThumbAddRRI3,     // add(1)  [0001110] imm_3[8..6] rn[5..3] rd[2..0]*/
  kThumbAddRI8,      // add(2)  [00110] rd[10..8] imm_8[7..0].
  kThumbAddRRR,      // add(3)  [0001100] rm[8..6] rn[5..3] rd[2..0].
  kThumbAddRRLH,     // add(4)  [01000100] H12[01] rm[5..3] rd[2..0].
  kThumbAddRRHL,     // add(4)  [01001000] H12[10] rm[5..3] rd[2..0].
  kThumbAddRRHH,     // add(4)  [01001100] H12[11] rm[5..3] rd[2..0].
  kThumbAddPcRel,    // add(5)  [10100] rd[10..8] imm_8[7..0].
  kThumbAddSpRel,    // add(6)  [10101] rd[10..8] imm_8[7..0].
  kThumbAddSpI7,     // add(7)  [101100000] imm_7[6..0].
  kThumbAndRR,       // and   [0100000000] rm[5..3] rd[2..0].
  kThumbAsrRRI5,     // asr(1)  [00010] imm_5[10..6] rm[5..3] rd[2..0].
  kThumbAsrRR,       // asr(2)  [0100000100] rs[5..3] rd[2..0].
  kThumbBCond,       // b(1)  [1101] cond[11..8] offset_8[7..0].
  kThumbBUncond,     // b(2)  [11100] offset_11[10..0].
  kThumbBicRR,       // bic   [0100001110] rm[5..3] rd[2..0].
  kThumbBkpt,        // bkpt  [10111110] imm_8[7..0].
  kThumbBlx1,        // blx(1)  [111] H[10] offset_11[10..0].
  kThumbBlx2,        // blx(1)  [111] H[01] offset_11[10..0].
  kThumbBl1,         // blx(1)  [111] H[10] offset_11[10..0].
  kThumbBl2,         // blx(1)  [111] H[11] offset_11[10..0].
  kThumbBlxR,        // blx(2)  [010001111] rm[6..3] [000].
  kThumbBx,          // bx    [010001110] H2[6..6] rm[5..3] SBZ[000].
  kThumbCmnRR,       // cmn   [0100001011] rm[5..3] rd[2..0].
  kThumbCmpRI8,      // cmp(1)  [00101] rn[10..8] imm_8[7..0].
  kThumbCmpRR,       // cmp(2)  [0100001010] rm[5..3] rd[2..0].
  kThumbCmpLH,       // cmp(3)  [01000101] H12[01] rm[5..3] rd[2..0].
  kThumbCmpHL,       // cmp(3)  [01000110] H12[10] rm[5..3] rd[2..0].
  kThumbCmpHH,       // cmp(3)  [01000111] H12[11] rm[5..3] rd[2..0].
  kThumbEorRR,       // eor   [0100000001] rm[5..3] rd[2..0].
  kThumbLdmia,       // ldmia   [11001] rn[10..8] reglist [7..0].
  kThumbLdrRRI5,     // ldr(1)  [01101] imm_5[10..6] rn[5..3] rd[2..0].
  kThumbLdrRRR,      // ldr(2)  [0101100] rm[8..6] rn[5..3] rd[2..0].
  kThumbLdrPcRel,    // ldr(3)  [01001] rd[10..8] imm_8[7..0].
  kThumbLdrSpRel,    // ldr(4)  [10011] rd[10..8] imm_8[7..0].
  kThumbLdrbRRI5,    // ldrb(1) [01111] imm_5[10..6] rn[5..3] rd[2..0].
  kThumbLdrbRRR,     // ldrb(2) [0101110] rm[8..6] rn[5..3] rd[2..0].
  kThumbLdrhRRI5,    // ldrh(1) [10001] imm_5[10..6] rn[5..3] rd[2..0].
  kThumbLdrhRRR,     // ldrh(2) [0101101] rm[8..6] rn[5..3] rd[2..0].
  kThumbLdrsbRRR,    // ldrsb   [0101011] rm[8..6] rn[5..3] rd[2..0].
  kThumbLdrshRRR,    // ldrsh   [0101111] rm[8..6] rn[5..3] rd[2..0].
  kThumbLslRRI5,     // lsl(1)  [00000] imm_5[10..6] rm[5..3] rd[2..0].
  kThumbLslRR,       // lsl(2)  [0100000010] rs[5..3] rd[2..0].
  kThumbLsrRRI5,     // lsr(1)  [00001] imm_5[10..6] rm[5..3] rd[2..0].
  kThumbLsrRR,       // lsr(2)  [0100000011] rs[5..3] rd[2..0].
  kThumbMovImm,      // mov(1)  [00100] rd[10..8] imm_8[7..0].
  kThumbMovRR,       // mov(2)  [0001110000] rn[5..3] rd[2..0].
  kThumbMovRR_H2H,   // mov(3)  [01000111] H12[11] rm[5..3] rd[2..0].
  kThumbMovRR_H2L,   // mov(3)  [01000110] H12[01] rm[5..3] rd[2..0].
  kThumbMovRR_L2H,   // mov(3)  [01000101] H12[10] rm[5..3] rd[2..0].
  kThumbMul,         // mul   [0100001101] rm[5..3] rd[2..0].
  kThumbMvn,         // mvn   [0100001111] rm[5..3] rd[2..0].
  kThumbNeg,         // neg   [0100001001] rm[5..3] rd[2..0].
  kThumbOrr,         // orr   [0100001100] rm[5..3] rd[2..0].
  kThumbPop,         // pop   [1011110] r[8..8] rl[7..0].
  kThumbPush,        // push  [1011010] r[8..8] rl[7..0].
  kThumbRorRR,       // ror   [0100000111] rs[5..3] rd[2..0].
  kThumbSbc,         // sbc   [0100000110] rm[5..3] rd[2..0].
  kThumbStmia,       // stmia   [11000] rn[10..8] reglist [7.. 0].
  kThumbStrRRI5,     // str(1)  [01100] imm_5[10..6] rn[5..3] rd[2..0].
  kThumbStrRRR,      // str(2)  [0101000] rm[8..6] rn[5..3] rd[2..0].
  kThumbStrSpRel,    // str(3)  [10010] rd[10..8] imm_8[7..0].
  kThumbStrbRRI5,    // strb(1) [01110] imm_5[10..6] rn[5..3] rd[2..0].
  kThumbStrbRRR,     // strb(2) [0101010] rm[8..6] rn[5..3] rd[2..0].
  kThumbStrhRRI5,    // strh(1) [10000] imm_5[10..6] rn[5..3] rd[2..0].
  kThumbStrhRRR,     // strh(2) [0101001] rm[8..6] rn[5..3] rd[2..0].
  kThumbSubRRI3,     // sub(1)  [0001111] imm_3[8..6] rn[5..3] rd[2..0]*/
  kThumbSubRI8,      // sub(2)  [00111] rd[10..8] imm_8[7..0].
  kThumbSubRRR,      // sub(3)  [0001101] rm[8..6] rn[5..3] rd[2..0].
  kThumbSubSpI7,     // sub(4)  [101100001] imm_7[6..0].
  kThumbSwi,         // swi   [11011111] imm_8[7..0].
  kThumbTst,         // tst   [0100001000] rm[5..3] rn[2..0].
  kThumb2Vldrs,      // vldr low  sx [111011011001] rn[19..16] rd[15-12] [1010] imm_8[7..0].
  kThumb2Vldrd,      // vldr low  dx [111011011001] rn[19..16] rd[15-12] [1011] imm_8[7..0].
  kThumb2Vmuls,      // vmul vd, vn, vm [111011100010] rn[19..16] rd[15-12] [10100000] rm[3..0].
  kThumb2Vmuld,      // vmul vd, vn, vm [111011100010] rn[19..16] rd[15-12] [10110000] rm[3..0].
  kThumb2Vstrs,      // vstr low  sx [111011011000] rn[19..16] rd[15-12] [1010] imm_8[7..0].
  kThumb2Vstrd,      // vstr low  dx [111011011000] rn[19..16] rd[15-12] [1011] imm_8[7..0].
  kThumb2Vsubs,      // vsub vd, vn, vm [111011100011] rn[19..16] rd[15-12] [10100040] rm[3..0].
  kThumb2Vsubd,      // vsub vd, vn, vm [111011100011] rn[19..16] rd[15-12] [10110040] rm[3..0].
  kThumb2Vadds,      // vadd vd, vn, vm [111011100011] rn[19..16] rd[15-12] [10100000] rm[3..0].
  kThumb2Vaddd,      // vadd vd, vn, vm [111011100011] rn[19..16] rd[15-12] [10110000] rm[3..0].
  kThumb2Vdivs,      // vdiv vd, vn, vm [111011101000] rn[19..16] rd[15-12] [10100000] rm[3..0].
  kThumb2Vdivd,      // vdiv vd, vn, vm [111011101000] rn[19..16] rd[15-12] [10110000] rm[3..0].
  kThumb2VcvtIF,     // vcvt.F32 vd, vm [1110111010111000] vd[15..12] [10101100] vm[3..0].
  kThumb2VcvtID,     // vcvt.F64 vd, vm [1110111010111000] vd[15..12] [10111100] vm[3..0].
  kThumb2VcvtFI,     // vcvt.S32.F32 vd, vm [1110111010111101] vd[15..12] [10101100] vm[3..0].
  kThumb2VcvtDI,     // vcvt.S32.F32 vd, vm [1110111010111101] vd[15..12] [10111100] vm[3..0].
  kThumb2VcvtFd,     // vcvt.F64.F32 vd, vm [1110111010110111] vd[15..12] [10101100] vm[3..0].
  kThumb2VcvtDF,     // vcvt.F32.F64 vd, vm [1110111010110111] vd[15..12] [10111100] vm[3..0].
  kThumb2Vsqrts,     // vsqrt.f32 vd, vm [1110111010110001] vd[15..12] [10101100] vm[3..0].
  kThumb2Vsqrtd,     // vsqrt.f64 vd, vm [1110111010110001] vd[15..12] [10111100] vm[3..0].
  kThumb2MovImmShift,  // mov(T2) rd, #<const> [11110] i [00001001111] imm3 rd[11..8] imm8.
  kThumb2MovImm16,   // mov(T3) rd, #<const> [11110] i [0010100] imm4 [0] imm3 rd[11..8] imm8.
  kThumb2StrRRI12,   // str(Imm,T3) rd,[rn,#imm12] [111110001100] rn[19..16] rt[15..12] imm12[11..0].
  kThumb2LdrRRI12,   // str(Imm,T3) rd,[rn,#imm12] [111110001100] rn[19..16] rt[15..12] imm12[11..0].
  kThumb2StrRRI8Predec,  // str(Imm,T4) rd,[rn,#-imm8] [111110000100] rn[19..16] rt[15..12] [1100] imm[7..0]*/
  kThumb2LdrRRI8Predec,  // ldr(Imm,T4) rd,[rn,#-imm8] [111110000101] rn[19..16] rt[15..12] [1100] imm[7..0]*/
  kThumb2Cbnz,       // cbnz rd,<label> [101110] i [1] imm5[7..3] rn[2..0].
  kThumb2Cbz,        // cbn rd,<label> [101100] i [1] imm5[7..3] rn[2..0].
  kThumb2AddRRI12,   // add rd, rn, #imm12 [11110] i [100000] rn[19..16] [0] imm3[14..12] rd[11..8] imm8[7..0].
  kThumb2MovRR,      // mov rd, rm [11101010010011110000] rd[11..8] [0000] rm[3..0].
  kThumb2Vmovs,      // vmov.f32 vd, vm [111011101] D [110000] vd[15..12] 101001] M [0] vm[3..0].
  kThumb2Vmovd,      // vmov.f64 vd, vm [111011101] D [110000] vd[15..12] 101101] M [0] vm[3..0].
  kThumb2Ldmia,      // ldmia  [111010001001[ rn[19..16] mask[15..0].
  kThumb2Stmia,      // stmia  [111010001000[ rn[19..16] mask[15..0].
  kThumb2AddRRR,     // add [111010110000] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2SubRRR,     // sub [111010111010] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2SbcRRR,     // sbc [111010110110] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2CmpRR,      // cmp [111010111011] rn[19..16] [0000] [1111] [0000] rm[3..0].
  kThumb2SubRRI12,   // sub rd, rn, #imm12 [11110] i [01010] rn[19..16] [0] imm3[14..12] rd[11..8] imm8[7..0].
  kThumb2MvnImm12,   // mov(T2) rd, #<const> [11110] i [00011011110] imm3 rd[11..8] imm8.
  kThumb2Sel,        // sel rd, rn, rm [111110101010] rn[19-16] rd[11-8] rm[3-0].
  kThumb2Ubfx,       // ubfx rd,rn,#lsb,#width [111100111100] rn[19..16] [0] imm3[14-12] rd[11-8] w[4-0].
  kThumb2Sbfx,       // ubfx rd,rn,#lsb,#width [111100110100] rn[19..16] [0] imm3[14-12] rd[11-8] w[4-0].
  kThumb2LdrRRR,     // ldr rt,[rn,rm,LSL #imm] [111110000101] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2LdrhRRR,    // ldrh rt,[rn,rm,LSL #imm] [111110000101] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2LdrshRRR,   // ldrsh rt,[rn,rm,LSL #imm] [111110000101] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2LdrbRRR,    // ldrb rt,[rn,rm,LSL #imm] [111110000101] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2LdrsbRRR,   // ldrsb rt,[rn,rm,LSL #imm] [111110000101] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2StrRRR,     // str rt,[rn,rm,LSL #imm] [111110000100] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2StrhRRR,    // str rt,[rn,rm,LSL #imm] [111110000010] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2StrbRRR,    // str rt,[rn,rm,LSL #imm] [111110000000] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2LdrhRRI12,  // ldrh rt,[rn,#imm12] [111110001011] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2LdrshRRI12,  // ldrsh rt,[rn,#imm12] [111110011011] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2LdrbRRI12,  // ldrb rt,[rn,#imm12] [111110001001] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2LdrsbRRI12,  // ldrsb rt,[rn,#imm12] [111110011001] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2StrhRRI12,  // strh rt,[rn,#imm12] [111110001010] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2StrbRRI12,  // strb rt,[rn,#imm12] [111110001000] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2Pop,        // pop   [1110100010111101] list[15-0]*/
  kThumb2Push,       // push  [1110100100101101] list[15-0]*/
  kThumb2CmpRI12,    // cmp rn, #<const> [11110] i [011011] rn[19-16] [0] imm3 [1111] imm8[7..0].
  kThumb2AdcRRR,     // adc [111010110101] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2AndRRR,     // and [111010100000] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2BicRRR,     // bic [111010100010] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2CmnRR,      // cmn [111010110001] rn[19..16] [0000] [1111] [0000] rm[3..0].
  kThumb2EorRRR,     // eor [111010101000] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2MulRRR,     // mul [111110110000] rn[19..16] [1111] rd[11..8] [0000] rm[3..0].
  kThumb2MnvRR,      // mvn [11101010011011110] rd[11-8] [0000] rm[3..0].
  kThumb2RsubRRI8,   // rsub [111100011100] rn[19..16] [0000] rd[11..8] imm8[7..0].
  kThumb2NegRR,      // actually rsub rd, rn, #0.
  kThumb2OrrRRR,     // orr [111010100100] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2TstRR,      // tst [111010100001] rn[19..16] [0000] [1111] [0000] rm[3..0].
  kThumb2LslRRR,     // lsl [111110100000] rn[19..16] [1111] rd[11..8] [0000] rm[3..0].
  kThumb2LsrRRR,     // lsr [111110100010] rn[19..16] [1111] rd[11..8] [0000] rm[3..0].
  kThumb2AsrRRR,     // asr [111110100100] rn[19..16] [1111] rd[11..8] [0000] rm[3..0].
  kThumb2RorRRR,     // ror [111110100110] rn[19..16] [1111] rd[11..8] [0000] rm[3..0].
  kThumb2LslRRI5,    // lsl [11101010010011110] imm[14.12] rd[11..8] [00] rm[3..0].
  kThumb2LsrRRI5,    // lsr [11101010010011110] imm[14.12] rd[11..8] [01] rm[3..0].
  kThumb2AsrRRI5,    // asr [11101010010011110] imm[14.12] rd[11..8] [10] rm[3..0].
  kThumb2RorRRI5,    // ror [11101010010011110] imm[14.12] rd[11..8] [11] rm[3..0].
  kThumb2BicRRI8,    // bic [111100000010] rn[19..16] [0] imm3 rd[11..8] imm8.
  kThumb2AndRRI8,    // bic [111100000000] rn[19..16] [0] imm3 rd[11..8] imm8.
  kThumb2OrrRRI8,    // orr [111100000100] rn[19..16] [0] imm3 rd[11..8] imm8.
  kThumb2EorRRI8,    // eor [111100001000] rn[19..16] [0] imm3 rd[11..8] imm8.
  kThumb2AddRRI8,    // add [111100001000] rn[19..16] [0] imm3 rd[11..8] imm8.
  kThumb2AdcRRI8,    // adc [111100010101] rn[19..16] [0] imm3 rd[11..8] imm8.
  kThumb2SubRRI8,    // sub [111100011011] rn[19..16] [0] imm3 rd[11..8] imm8.
  kThumb2SbcRRI8,    // sbc [111100010111] rn[19..16] [0] imm3 rd[11..8] imm8.
  kThumb2It,         // it [10111111] firstcond[7-4] mask[3-0].
  kThumb2Fmstat,     // fmstat [11101110111100011111101000010000].
  kThumb2Vcmpd,      // vcmp [111011101] D [11011] rd[15-12] [1011] E [1] M [0] rm[3-0].
  kThumb2Vcmps,      // vcmp [111011101] D [11010] rd[15-12] [1011] E [1] M [0] rm[3-0].
  kThumb2LdrPcRel12,  // ldr rd,[pc,#imm12] [1111100011011111] rt[15-12] imm12[11-0].
  kThumb2BCond,      // b<c> [1110] S cond[25-22] imm6[21-16] [10] J1 [0] J2 imm11[10..0].
  kThumb2Vmovd_RR,   // vmov [111011101] D [110000] vd[15-12 [101101] M [0] vm[3-0].
  kThumb2Vmovs_RR,   // vmov [111011101] D [110000] vd[15-12 [101001] M [0] vm[3-0].
  kThumb2Fmrs,       // vmov [111011100000] vn[19-16] rt[15-12] [1010] N [0010000].
  kThumb2Fmsr,       // vmov [111011100001] vn[19-16] rt[15-12] [1010] N [0010000].
  kThumb2Fmrrd,      // vmov [111011000100] rt2[19-16] rt[15-12] [101100] M [1] vm[3-0].
  kThumb2Fmdrr,      // vmov [111011000101] rt2[19-16] rt[15-12] [101100] M [1] vm[3-0].
  kThumb2Vabsd,      // vabs.f64 [111011101] D [110000] rd[15-12] [1011110] M [0] vm[3-0].
  kThumb2Vabss,      // vabs.f32 [111011101] D [110000] rd[15-12] [1010110] M [0] vm[3-0].
  kThumb2Vnegd,      // vneg.f64 [111011101] D [110000] rd[15-12] [1011110] M [0] vm[3-0].
  kThumb2Vnegs,      // vneg.f32 [111011101] D [110000] rd[15-12] [1010110] M [0] vm[3-0].
  kThumb2Vmovs_IMM8,  // vmov.f32 [111011101] D [11] imm4h[19-16] vd[15-12] [10100000] imm4l[3-0].
  kThumb2Vmovd_IMM8,  // vmov.f64 [111011101] D [11] imm4h[19-16] vd[15-12] [10110000] imm4l[3-0].
  kThumb2Mla,        // mla [111110110000] rn[19-16] ra[15-12] rd[7-4] [0000] rm[3-0].
  kThumb2Umull,      // umull [111110111010] rn[19-16], rdlo[15-12] rdhi[11-8] [0000] rm[3-0].
  kThumb2Ldrex,      // ldrex [111010000101] rn[19-16] rt[11-8] [1111] imm8[7-0].
  kThumb2Strex,      // strex [111010000100] rn[19-16] rt[11-8] rd[11-8] imm8[7-0].
  kThumb2Clrex,      // clrex [111100111011111110000111100101111].
  kThumb2Bfi,        // bfi [111100110110] rn[19-16] [0] imm3[14-12] rd[11-8] imm2[7-6] [0] msb[4-0].
  kThumb2Bfc,        // bfc [11110011011011110] [0] imm3[14-12] rd[11-8] imm2[7-6] [0] msb[4-0].
  kThumb2Dmb,        // dmb [1111001110111111100011110101] option[3-0].
  kThumb2LdrPcReln12,  // ldr rd,[pc,-#imm12] [1111100011011111] rt[15-12] imm12[11-0].
  kThumb2Stm,        // stm <list> [111010010000] rn[19-16] 000 rl[12-0].
  kThumbUndefined,   // undefined [11011110xxxxxxxx].
  kThumb2VPopCS,     // vpop <list of callee save fp singles (s16+).
  kThumb2VPushCS,    // vpush <list callee save fp singles (s16+).
  kThumb2Vldms,      // vldms rd, <list>.
  kThumb2Vstms,      // vstms rd, <list>.
  kThumb2BUncond,    // b <label>.
  kThumb2MovImm16H,  // similar to kThumb2MovImm16, but target high hw.
  kThumb2AddPCR,     // Thumb2 2-operand add with hard-coded PC target.
  kThumb2Adr,        // Special purpose encoding of ADR for switch tables.
  kThumb2MovImm16LST,  // Special purpose version for switch table use.
  kThumb2MovImm16HST,  // Special purpose version for switch table use.
  kThumb2LdmiaWB,    // ldmia  [111010011001[ rn[19..16] mask[15..0].
  kThumb2SubsRRI12,  // setflags encoding.
  kThumb2OrrRRRs,    // orrx [111010100101] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2Push1,      // t3 encoding of push.
  kThumb2Pop1,       // t3 encoding of pop.
  kThumb2RsubRRR,    // rsb [111010111101] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2Smull,      // smull [111110111000] rn[19-16], rdlo[15-12] rdhi[11-8] [0000] rm[3-0].
  kThumb2LdrdPcRel8,  // ldrd rt, rt2, pc +-/1024.
  kThumb2LdrdI8,     // ldrd rt, rt2, [rn +-/1024].
  kThumb2StrdI8,     // strd rt, rt2, [rn +-/1024].
  kArmLast,
};

enum ArmOpDmbOptions {
  kSY = 0xf,
  kST = 0xe,
  kISH = 0xb,
  kISHST = 0xa,
  kNSH = 0x7,
  kNSHST = 0x6
};

// Instruction assembly field_loc kind.
enum ArmEncodingKind {
  kFmtUnused,
  kFmtBitBlt,    // Bit string using end/start.
  kFmtDfp,       // Double FP reg.
  kFmtSfp,       // Single FP reg.
  kFmtModImm,    // Shifted 8-bit immed using [26,14..12,7..0].
  kFmtImm16,     // Zero-extended immed using [26,19..16,14..12,7..0].
  kFmtImm6,      // Encoded branch target using [9,7..3]0.
  kFmtImm12,     // Zero-extended immediate using [26,14..12,7..0].
  kFmtShift,     // Shift descriptor, [14..12,7..4].
  kFmtLsb,       // least significant bit using [14..12][7..6].
  kFmtBWidth,    // bit-field width, encoded as width-1.
  kFmtShift5,    // Shift count, [14..12,7..6].
  kFmtBrOffset,  // Signed extended [26,11,13,21-16,10-0]:0.
  kFmtFPImm,     // Encoded floating point immediate.
  kFmtOff24,     // 24-bit Thumb2 unconditional branch encoding.
};

// Struct used to define the snippet positions for each Thumb opcode.
struct ArmEncodingMap {
  uint32_t skeleton;
  struct {
    ArmEncodingKind kind;
    int end;   // end for kFmtBitBlt, 1-bit slice end for FP regs.
    int start;  // start for kFmtBitBlt, 4-bit slice end for FP regs.
  } field_loc[4];
  ArmOpcode opcode;
  uint64_t flags;
  const char* name;
  const char* fmt;
  int size;   // Note: size is in bytes.
};

}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_ARM_ARM_LIR_H_
