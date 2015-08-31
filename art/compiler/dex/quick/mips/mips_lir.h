/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_COMPILER_DEX_QUICK_MIPS_MIPS_LIR_H_
#define ART_COMPILER_DEX_QUICK_MIPS_MIPS_LIR_H_

#include "dex/compiler_internals.h"

namespace art {

/*
 * Runtime register conventions.
 *
 * zero is always the value 0
 * at is scratch (normally used as temp reg by assembler)
 * v0, v1 are scratch (normally hold subroutine return values)
 * a0-a3 are scratch (normally hold subroutine arguments)
 * t0-t8 are scratch
 * t9 is scratch (normally used for function calls)
 * s0 (rMIPS_SUSPEND) is reserved [holds suspend-check counter]
 * s1 (rMIPS_SELF) is reserved [holds current &Thread]
 * s2-s7 are callee save (promotion target)
 * k0, k1 are reserved for use by interrupt handlers
 * gp is reserved for global pointer
 * sp is reserved
 * s8 is callee save (promotion target)
 * ra is scratch (normally holds the return addr)
 *
 * Preserved across C calls: s0-s8
 * Trashed across C calls: at, v0-v1, a0-a3, t0-t9, gp, ra
 *
 * Floating pointer registers
 * NOTE: there are 32 fp registers (16 df pairs), but currently
 *       only support 16 fp registers (8 df pairs).
 * f0-f15
 * df0-df7, where df0={f0,f1}, df1={f2,f3}, ... , df7={f14,f15}
 *
 * f0-f15 (df0-df7) trashed across C calls
 *
 * For mips32 code use:
 *      a0-a3 to hold operands
 *      v0-v1 to hold results
 *      t0-t9 for temps
 *
 * All jump/branch instructions have a delay slot after it.
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
#define MIPS_FP_REG_OFFSET 32
// Offset to distinguish DP FP regs.
#define MIPS_FP_DOUBLE 64
// Offset to distingish the extra regs.
#define MIPS_EXTRA_REG_OFFSET 128
// Reg types.
#define MIPS_REGTYPE(x) (x & (MIPS_FP_REG_OFFSET | MIPS_FP_DOUBLE))
#define MIPS_FPREG(x) ((x & MIPS_FP_REG_OFFSET) == MIPS_FP_REG_OFFSET)
#define MIPS_EXTRAREG(x) ((x & MIPS_EXTRA_REG_OFFSET) == MIPS_EXTRA_REG_OFFSET)
#define MIPS_DOUBLEREG(x) ((x & MIPS_FP_DOUBLE) == MIPS_FP_DOUBLE)
#define MIPS_SINGLEREG(x) (MIPS_FPREG(x) && !MIPS_DOUBLEREG(x))
/*
 * Note: the low register of a floating point pair is sufficient to
 * create the name of a double, but require both names to be passed to
 * allow for asserts to verify that the pair is consecutive if significant
 * rework is done in this area.  Also, it is a good reminder in the calling
 * code that reg locations always describe doubles as a pair of singles.
 */
#define MIPS_S2D(x, y) ((x) | MIPS_FP_DOUBLE)
// Mask to strip off fp flags.
#define MIPS_FP_REG_MASK (MIPS_FP_REG_OFFSET-1)

#ifdef HAVE_LITTLE_ENDIAN
#define LOWORD_OFFSET 0
#define HIWORD_OFFSET 4
#define r_ARG0 r_A0
#define r_ARG1 r_A1
#define r_ARG2 r_A2
#define r_ARG3 r_A3
#define r_RESULT0 r_V0
#define r_RESULT1 r_V1
#else
#define LOWORD_OFFSET 4
#define HIWORD_OFFSET 0
#define r_ARG0 r_A1
#define r_ARG1 r_A0
#define r_ARG2 r_A3
#define r_ARG3 r_A2
#define r_RESULT0 r_V1
#define r_RESULT1 r_V0
#endif

// These are the same for both big and little endian.
#define r_FARG0 r_F12
#define r_FARG1 r_F13
#define r_FARG2 r_F14
#define r_FARG3 r_F15
#define r_FRESULT0 r_F0
#define r_FRESULT1 r_F1

// Regs not used for Mips.
#define rMIPS_PC INVALID_REG

// RegisterLocation templates return values (r_V0, or r_V0/r_V1).
#define MIPS_LOC_C_RETURN {kLocPhysReg, 0, 0, 0, 0, 0, 0, 0, 1, r_V0, INVALID_REG, \
                           INVALID_SREG, INVALID_SREG}
#define MIPS_LOC_C_RETURN_FLOAT {kLocPhysReg, 0, 0, 0, 0, 0, 0, 0, 1, r_FRESULT0, \
                                 INVALID_REG, INVALID_SREG, INVALID_SREG}
#define MIPS_LOC_C_RETURN_WIDE {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, r_RESULT0, \
                                r_RESULT1, INVALID_SREG, INVALID_SREG}
#define MIPS_LOC_C_RETURN_DOUBLE {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, r_FRESULT0, \
                                  r_FRESULT1, INVALID_SREG, INVALID_SREG}

enum MipsResourceEncodingPos {
  kMipsGPReg0   = 0,
  kMipsRegSP    = 29,
  kMipsRegLR    = 31,
  kMipsFPReg0   = 32,  // only 16 fp regs supported currently.
  kMipsFPRegEnd   = 48,
  kMipsRegHI    = kMipsFPRegEnd,
  kMipsRegLO,
  kMipsRegPC,
  kMipsRegEnd   = 51,
};

#define ENCODE_MIPS_REG_LIST(N)      (static_cast<uint64_t>(N))
#define ENCODE_MIPS_REG_SP           (1ULL << kMipsRegSP)
#define ENCODE_MIPS_REG_LR           (1ULL << kMipsRegLR)
#define ENCODE_MIPS_REG_PC           (1ULL << kMipsRegPC)

enum MipsNativeRegisterPool {
  r_ZERO = 0,
  r_AT = 1,
  r_V0 = 2,
  r_V1 = 3,
  r_A0 = 4,
  r_A1 = 5,
  r_A2 = 6,
  r_A3 = 7,
  r_T0 = 8,
  r_T1 = 9,
  r_T2 = 10,
  r_T3 = 11,
  r_T4 = 12,
  r_T5 = 13,
  r_T6 = 14,
  r_T7 = 15,
  r_S0 = 16,
  r_S1 = 17,
  r_S2 = 18,
  r_S3 = 19,
  r_S4 = 20,
  r_S5 = 21,
  r_S6 = 22,
  r_S7 = 23,
  r_T8 = 24,
  r_T9 = 25,
  r_K0 = 26,
  r_K1 = 27,
  r_GP = 28,
  r_SP = 29,
  r_FP = 30,
  r_RA = 31,

  r_F0 = 0 + MIPS_FP_REG_OFFSET,
  r_F1,
  r_F2,
  r_F3,
  r_F4,
  r_F5,
  r_F6,
  r_F7,
  r_F8,
  r_F9,
  r_F10,
  r_F11,
  r_F12,
  r_F13,
  r_F14,
  r_F15,
#if 0
  /*
   * TODO: The shared resource mask doesn't have enough bit positions to describe all
   * MIPS registers.  Expand it and enable use of fp registers 16 through 31.
   */
  r_F16,
  r_F17,
  r_F18,
  r_F19,
  r_F20,
  r_F21,
  r_F22,
  r_F23,
  r_F24,
  r_F25,
  r_F26,
  r_F27,
  r_F28,
  r_F29,
  r_F30,
  r_F31,
#endif
  r_DF0 = r_F0 + MIPS_FP_DOUBLE,
  r_DF1 = r_F2 + MIPS_FP_DOUBLE,
  r_DF2 = r_F4 + MIPS_FP_DOUBLE,
  r_DF3 = r_F6 + MIPS_FP_DOUBLE,
  r_DF4 = r_F8 + MIPS_FP_DOUBLE,
  r_DF5 = r_F10 + MIPS_FP_DOUBLE,
  r_DF6 = r_F12 + MIPS_FP_DOUBLE,
  r_DF7 = r_F14 + MIPS_FP_DOUBLE,
#if 0  // TODO: expand resource mask to enable use of all MIPS fp registers.
  r_DF8 = r_F16 + MIPS_FP_DOUBLE,
  r_DF9 = r_F18 + MIPS_FP_DOUBLE,
  r_DF10 = r_F20 + MIPS_FP_DOUBLE,
  r_DF11 = r_F22 + MIPS_FP_DOUBLE,
  r_DF12 = r_F24 + MIPS_FP_DOUBLE,
  r_DF13 = r_F26 + MIPS_FP_DOUBLE,
  r_DF14 = r_F28 + MIPS_FP_DOUBLE,
  r_DF15 = r_F30 + MIPS_FP_DOUBLE,
#endif
  r_HI = MIPS_EXTRA_REG_OFFSET,
  r_LO,
  r_PC,
};

#define rMIPS_SUSPEND r_S0
#define rMIPS_SELF r_S1
#define rMIPS_SP r_SP
#define rMIPS_ARG0 r_ARG0
#define rMIPS_ARG1 r_ARG1
#define rMIPS_ARG2 r_ARG2
#define rMIPS_ARG3 r_ARG3
#define rMIPS_FARG0 r_FARG0
#define rMIPS_FARG1 r_FARG1
#define rMIPS_FARG2 r_FARG2
#define rMIPS_FARG3 r_FARG3
#define rMIPS_RET0 r_RESULT0
#define rMIPS_RET1 r_RESULT1
#define rMIPS_INVOKE_TGT r_T9
#define rMIPS_COUNT INVALID_REG
#define rMIPS_LR r_RA

enum MipsShiftEncodings {
  kMipsLsl = 0x0,
  kMipsLsr = 0x1,
  kMipsAsr = 0x2,
  kMipsRor = 0x3
};

// MIPS sync kinds (Note: support for kinds other than kSYNC0 may not exist).
#define kSYNC0        0x00
#define kSYNC_WMB     0x04
#define kSYNC_MB      0x01
#define kSYNC_ACQUIRE 0x11
#define kSYNC_RELEASE 0x12
#define kSYNC_RMB     0x13

// TODO: Use smaller hammer when appropriate for target CPU.
#define kST kSYNC0
#define kSY kSYNC0

/*
 * The following enum defines the list of supported Thumb instructions by the
 * assembler. Their corresponding EncodingMap positions will be defined in
 * Assemble.cc.
 */
enum MipsOpCode {
  kMipsFirst = 0,
  kMips32BitData = kMipsFirst,  // data [31..0].
  kMipsAddiu,  // addiu t,s,imm16 [001001] s[25..21] t[20..16] imm16[15..0].
  kMipsAddu,  // add d,s,t [000000] s[25..21] t[20..16] d[15..11] [00000100001].
  kMipsAnd,   // and d,s,t [000000] s[25..21] t[20..16] d[15..11] [00000100100].
  kMipsAndi,  // andi t,s,imm16 [001100] s[25..21] t[20..16] imm16[15..0].
  kMipsB,     // b o   [0001000000000000] o[15..0].
  kMipsBal,   // bal o [0000010000010001] o[15..0].
  // NOTE: the code tests the range kMipsBeq thru kMipsBne, so adding an instruction in this
  //       range may require updates.
  kMipsBeq,   // beq s,t,o [000100] s[25..21] t[20..16] o[15..0].
  kMipsBeqz,  // beqz s,o [000100] s[25..21] [00000] o[15..0].
  kMipsBgez,  // bgez s,o [000001] s[25..21] [00001] o[15..0].
  kMipsBgtz,  // bgtz s,o [000111] s[25..21] [00000] o[15..0].
  kMipsBlez,  // blez s,o [000110] s[25..21] [00000] o[15..0].
  kMipsBltz,  // bltz s,o [000001] s[25..21] [00000] o[15..0].
  kMipsBnez,  // bnez s,o [000101] s[25..21] [00000] o[15..0].
  kMipsBne,   // bne s,t,o [000101] s[25..21] t[20..16] o[15..0].
  kMipsDiv,   // div s,t [000000] s[25..21] t[20..16] [0000000000011010].
#if __mips_isa_rev >= 2
  kMipsExt,   // ext t,s,p,z [011111] s[25..21] t[20..16] z[15..11] p[10..6] [000000].
#endif
  kMipsJal,   // jal t [000011] t[25..0].
  kMipsJalr,  // jalr d,s [000000] s[25..21] [00000] d[15..11] hint[10..6] [001001].
  kMipsJr,    // jr s [000000] s[25..21] [0000000000] hint[10..6] [001000].
  kMipsLahi,  // lui t,imm16 [00111100000] t[20..16] imm16[15..0] load addr hi.
  kMipsLalo,  // ori t,s,imm16 [001001] s[25..21] t[20..16] imm16[15..0] load addr lo.
  kMipsLui,   // lui t,imm16 [00111100000] t[20..16] imm16[15..0].
  kMipsLb,    // lb t,o(b) [100000] b[25..21] t[20..16] o[15..0].
  kMipsLbu,   // lbu t,o(b) [100100] b[25..21] t[20..16] o[15..0].
  kMipsLh,    // lh t,o(b) [100001] b[25..21] t[20..16] o[15..0].
  kMipsLhu,   // lhu t,o(b) [100101] b[25..21] t[20..16] o[15..0].
  kMipsLw,    // lw t,o(b) [100011] b[25..21] t[20..16] o[15..0].
  kMipsMfhi,  // mfhi d [0000000000000000] d[15..11] [00000010000].
  kMipsMflo,  // mflo d [0000000000000000] d[15..11] [00000010010].
  kMipsMove,  // move d,s [000000] s[25..21] [00000] d[15..11] [00000100101].
  kMipsMovz,  // movz d,s,t [000000] s[25..21] t[20..16] d[15..11] [00000001010].
  kMipsMul,   // mul d,s,t [011100] s[25..21] t[20..16] d[15..11] [00000000010].
  kMipsNop,   // nop [00000000000000000000000000000000].
  kMipsNor,   // nor d,s,t [000000] s[25..21] t[20..16] d[15..11] [00000100111].
  kMipsOr,    // or d,s,t [000000] s[25..21] t[20..16] d[15..11] [00000100101].
  kMipsOri,   // ori t,s,imm16 [001001] s[25..21] t[20..16] imm16[15..0].
  kMipsPref,  // pref h,o(b) [101011] b[25..21] h[20..16] o[15..0].
  kMipsSb,    // sb t,o(b) [101000] b[25..21] t[20..16] o[15..0].
#if __mips_isa_rev >= 2
  kMipsSeb,   // seb d,t [01111100000] t[20..16] d[15..11] [10000100000].
  kMipsSeh,   // seh d,t [01111100000] t[20..16] d[15..11] [11000100000].
#endif
  kMipsSh,    // sh t,o(b) [101001] b[25..21] t[20..16] o[15..0].
  kMipsSll,   // sll d,t,a [00000000000] t[20..16] d[15..11] a[10..6] [000000].
  kMipsSllv,  // sllv d,t,s [000000] s[25..21] t[20..16] d[15..11] [00000000100].
  kMipsSlt,   // slt d,s,t [000000] s[25..21] t[20..16] d[15..11] [00000101010].
  kMipsSlti,  // slti t,s,imm16 [001010] s[25..21] t[20..16] imm16[15..0].
  kMipsSltu,  // sltu d,s,t [000000] s[25..21] t[20..16] d[15..11] [00000101011].
  kMipsSra,   // sra d,s,imm5 [00000000000] t[20..16] d[15..11] imm5[10..6] [000011].
  kMipsSrav,  // srav d,t,s [000000] s[25..21] t[20..16] d[15..11] [00000000111].
  kMipsSrl,   // srl d,t,a [00000000000] t[20..16] d[20..16] a[10..6] [000010].
  kMipsSrlv,  // srlv d,t,s [000000] s[25..21] t[20..16] d[15..11] [00000000110].
  kMipsSubu,  // subu d,s,t [000000] s[25..21] t[20..16] d[15..11] [00000100011].
  kMipsSw,    // sw t,o(b) [101011] b[25..21] t[20..16] o[15..0].
  kMipsXor,   // xor d,s,t [000000] s[25..21] t[20..16] d[15..11] [00000100110].
  kMipsXori,  // xori t,s,imm16 [001110] s[25..21] t[20..16] imm16[15..0].
  kMipsFadds,  // add.s d,s,t [01000110000] t[20..16] s[15..11] d[10..6] [000000].
  kMipsFsubs,  // sub.s d,s,t [01000110000] t[20..16] s[15..11] d[10..6] [000001].
  kMipsFmuls,  // mul.s d,s,t [01000110000] t[20..16] s[15..11] d[10..6] [000010].
  kMipsFdivs,  // div.s d,s,t [01000110000] t[20..16] s[15..11] d[10..6] [000011].
  kMipsFaddd,  // add.d d,s,t [01000110001] t[20..16] s[15..11] d[10..6] [000000].
  kMipsFsubd,  // sub.d d,s,t [01000110001] t[20..16] s[15..11] d[10..6] [000001].
  kMipsFmuld,  // mul.d d,s,t [01000110001] t[20..16] s[15..11] d[10..6] [000010].
  kMipsFdivd,  // div.d d,s,t [01000110001] t[20..16] s[15..11] d[10..6] [000011].
  kMipsFcvtsd,  // cvt.s.d d,s [01000110001] [00000] s[15..11] d[10..6] [100000].
  kMipsFcvtsw,  // cvt.s.w d,s [01000110100] [00000] s[15..11] d[10..6] [100000].
  kMipsFcvtds,  // cvt.d.s d,s [01000110000] [00000] s[15..11] d[10..6] [100001].
  kMipsFcvtdw,  // cvt.d.w d,s [01000110100] [00000] s[15..11] d[10..6] [100001].
  kMipsFcvtws,  // cvt.w.d d,s [01000110000] [00000] s[15..11] d[10..6] [100100].
  kMipsFcvtwd,  // cvt.w.d d,s [01000110001] [00000] s[15..11] d[10..6] [100100].
  kMipsFmovs,  // mov.s d,s [01000110000] [00000] s[15..11] d[10..6] [000110].
  kMipsFmovd,  // mov.d d,s [01000110001] [00000] s[15..11] d[10..6] [000110].
  kMipsFlwc1,  // lwc1 t,o(b) [110001] b[25..21] t[20..16] o[15..0].
  kMipsFldc1,  // ldc1 t,o(b) [110101] b[25..21] t[20..16] o[15..0].
  kMipsFswc1,  // swc1 t,o(b) [111001] b[25..21] t[20..16] o[15..0].
  kMipsFsdc1,  // sdc1 t,o(b) [111101] b[25..21] t[20..16] o[15..0].
  kMipsMfc1,  // mfc1 t,s [01000100000] t[20..16] s[15..11] [00000000000].
  kMipsMtc1,  // mtc1 t,s [01000100100] t[20..16] s[15..11] [00000000000].
  kMipsDelta,  // Psuedo for ori t, s, <label>-<label>.
  kMipsDeltaHi,  // Pseudo for lui t, high16(<label>-<label>).
  kMipsDeltaLo,  // Pseudo for ori t, s, low16(<label>-<label>).
  kMipsCurrPC,  // jal to .+8 to materialize pc.
  kMipsSync,    // sync kind [000000] [0000000000000000] s[10..6] [001111].
  kMipsUndefined,  // undefined [011001xxxxxxxxxxxxxxxx].
  kMipsLast
};

// Instruction assembly field_loc kind.
enum MipsEncodingKind {
  kFmtUnused,
  kFmtBitBlt,    /* Bit string using end/start */
  kFmtDfp,       /* Double FP reg */
  kFmtSfp,       /* Single FP reg */
  kFmtBlt5_2,    /* Same 5-bit field to 2 locations */
};

// Struct used to define the snippet positions for each MIPS opcode.
struct MipsEncodingMap {
  uint32_t skeleton;
  struct {
    MipsEncodingKind kind;
    int end;   // end for kFmtBitBlt, 1-bit slice end for FP regs.
    int start;  // start for kFmtBitBlt, 4-bit slice end for FP regs.
  } field_loc[4];
  MipsOpCode opcode;
  uint64_t flags;
  const char *name;
  const char* fmt;
  int size;   // Note: size is in bytes.
};

extern MipsEncodingMap EncodingMap[kMipsLast];

#define IS_UIMM16(v) ((0 <= (v)) && ((v) <= 65535))
#define IS_SIMM16(v) ((-32768 <= (v)) && ((v) <= 32766))
#define IS_SIMM16_2WORD(v) ((-32764 <= (v)) && ((v) <= 32763))  // 2 offsets must fit.

}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_MIPS_MIPS_LIR_H_
