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

#include "codegen_mips.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "mips_lir.h"

namespace art {

#define MAX_ASSEMBLER_RETRIES 50

/*
 * opcode: MipsOpCode enum
 * skeleton: pre-designated bit-pattern for this opcode
 * k0: key to applying ds/de
 * ds: dest start bit position
 * de: dest end bit position
 * k1: key to applying s1s/s1e
 * s1s: src1 start bit position
 * s1e: src1 end bit position
 * k2: key to applying s2s/s2e
 * s2s: src2 start bit position
 * s2e: src2 end bit position
 * operands: number of operands (for sanity check purposes)
 * name: mnemonic name
 * fmt: for pretty-printing
 */
#define ENCODING_MAP(opcode, skeleton, k0, ds, de, k1, s1s, s1e, k2, s2s, s2e, \
                     k3, k3s, k3e, flags, name, fmt, size) \
        {skeleton, {{k0, ds, de}, {k1, s1s, s1e}, {k2, s2s, s2e}, \
                    {k3, k3s, k3e}}, opcode, flags, name, fmt, size}

/* Instruction dump string format keys: !pf, where "!" is the start
 * of the key, "p" is which numeric operand to use and "f" is the
 * print format.
 *
 * [p]ositions:
 *     0 -> operands[0] (dest)
 *     1 -> operands[1] (src1)
 *     2 -> operands[2] (src2)
 *     3 -> operands[3] (extra)
 *
 * [f]ormats:
 *     h -> 4-digit hex
 *     d -> decimal
 *     E -> decimal*4
 *     F -> decimal*2
 *     c -> branch condition (beq, bne, etc.)
 *     t -> pc-relative target
 *     T -> pc-region target
 *     u -> 1st half of bl[x] target
 *     v -> 2nd half ob bl[x] target
 *     R -> register list
 *     s -> single precision floating point register
 *     S -> double precision floating point register
 *     m -> Thumb2 modified immediate
 *     n -> complimented Thumb2 modified immediate
 *     M -> Thumb2 16-bit zero-extended immediate
 *     b -> 4-digit binary
 *     N -> append a NOP
 *
 *  [!] escape.  To insert "!", use "!!"
 */
/* NOTE: must be kept in sync with enum MipsOpcode from LIR.h */
/*
 * TUNING: We're currently punting on the branch delay slots.  All branch
 * instructions in this map are given a size of 8, which during assembly
 * is expanded to include a nop.  This scheme should be replaced with
 * an assembler pass to fill those slots when possible.
 */
const MipsEncodingMap MipsMir2Lir::EncodingMap[kMipsLast] = {
    ENCODING_MAP(kMips32BitData, 0x00000000,
                 kFmtBitBlt, 31, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP,
                 "data", "0x!0h(!0d)", 4),
    ENCODING_MAP(kMipsAddiu, 0x24000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "addiu", "!0r,!1r,0x!2h(!2d)", 4),
    ENCODING_MAP(kMipsAddu, 0x00000021,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "addu", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsAnd, 0x00000024,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "and", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsAndi, 0x30000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "andi", "!0r,!1r,0x!2h(!2d)", 4),
    ENCODING_MAP(kMipsB, 0x10000000,
                 kFmtBitBlt, 15, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | NEEDS_FIXUP,
                 "b", "!0t!0N", 8),
    ENCODING_MAP(kMipsBal, 0x04110000,
                 kFmtBitBlt, 15, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_DEF_LR |
                 NEEDS_FIXUP, "bal", "!0t!0N", 8),
    ENCODING_MAP(kMipsBeq, 0x10000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | REG_USE01 |
                 NEEDS_FIXUP, "beq", "!0r,!1r,!2t!0N", 8),
    ENCODING_MAP(kMipsBeqz, 0x10000000, /* same as beq above with t = $zero */
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0 |
                 NEEDS_FIXUP, "beqz", "!0r,!1t!0N", 8),
    ENCODING_MAP(kMipsBgez, 0x04010000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0 |
                 NEEDS_FIXUP, "bgez", "!0r,!1t!0N", 8),
    ENCODING_MAP(kMipsBgtz, 0x1C000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0 |
                 NEEDS_FIXUP, "bgtz", "!0r,!1t!0N", 8),
    ENCODING_MAP(kMipsBlez, 0x18000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0 |
                 NEEDS_FIXUP, "blez", "!0r,!1t!0N", 8),
    ENCODING_MAP(kMipsBltz, 0x04000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0 |
                 NEEDS_FIXUP, "bltz", "!0r,!1t!0N", 8),
    ENCODING_MAP(kMipsBnez, 0x14000000, /* same as bne below with t = $zero */
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0 |
                 NEEDS_FIXUP, "bnez", "!0r,!1t!0N", 8),
    ENCODING_MAP(kMipsBne, 0x14000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | REG_USE01 |
                 NEEDS_FIXUP, "bne", "!0r,!1r,!2t!0N", 8),
    ENCODING_MAP(kMipsDiv, 0x0000001a,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtBitBlt, 25, 21,
                 kFmtBitBlt, 20, 16, IS_QUAD_OP | REG_DEF01 | REG_USE23,
                 "div", "!2r,!3r", 4),
#if __mips_isa_rev >= 2
    ENCODING_MAP(kMipsExt, 0x7c000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 10, 6,
                 kFmtBitBlt, 15, 11, IS_QUAD_OP | REG_DEF0 | REG_USE1,
                 "ext", "!0r,!1r,!2d,!3D", 4),
#endif
    ENCODING_MAP(kMipsJal, 0x0c000000,
                 kFmtBitBlt, 25, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_DEF_LR,
                 "jal", "!0T(!0E)!0N", 8),
    ENCODING_MAP(kMipsJalr, 0x00000009,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | REG_DEF0_USE1,
                 "jalr", "!0r,!1r!0N", 8),
    ENCODING_MAP(kMipsJr, 0x00000008,
                 kFmtBitBlt, 25, 21, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0 |
                 NEEDS_FIXUP, "jr", "!0r!0N", 8),
    ENCODING_MAP(kMipsLahi, 0x3C000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "lahi/lui", "!0r,0x!1h(!1d)", 4),
    ENCODING_MAP(kMipsLalo, 0x34000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "lalo/ori", "!0r,!1r,0x!2h(!2d)", 4),
    ENCODING_MAP(kMipsLui, 0x3C000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "lui", "!0r,0x!1h(!1d)", 4),
    ENCODING_MAP(kMipsLb, 0x80000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lb", "!0r,!1d(!2r)", 4),
    ENCODING_MAP(kMipsLbu, 0x90000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lbu", "!0r,!1d(!2r)", 4),
    ENCODING_MAP(kMipsLh, 0x84000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lh", "!0r,!1d(!2r)", 4),
    ENCODING_MAP(kMipsLhu, 0x94000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lhu", "!0r,!1d(!2r)", 4),
    ENCODING_MAP(kMipsLw, 0x8C000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lw", "!0r,!1d(!2r)", 4),
    ENCODING_MAP(kMipsMfhi, 0x00000010,
                 kFmtBitBlt, 15, 11, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mfhi", "!0r", 4),
    ENCODING_MAP(kMipsMflo, 0x00000012,
                 kFmtBitBlt, 15, 11, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mflo", "!0r", 4),
    ENCODING_MAP(kMipsMove, 0x00000025, /* or using zero reg */
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "move", "!0r,!1r", 4),
    ENCODING_MAP(kMipsMovz, 0x0000000a,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "movz", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsMul, 0x70000002,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "mul", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsNop, 0x00000000,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND,
                 "nop", ";", 4),
    ENCODING_MAP(kMipsNor, 0x00000027, /* used for "not" too */
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "nor", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsOr, 0x00000025,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "or", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsOri, 0x34000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "ori", "!0r,!1r,0x!2h(!2d)", 4),
    ENCODING_MAP(kMipsPref, 0xCC000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE2,
                 "pref", "!0d,!1d(!2r)", 4),
    ENCODING_MAP(kMipsSb, 0xA0000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "sb", "!0r,!1d(!2r)", 4),
#if __mips_isa_rev >= 2
    ENCODING_MAP(kMipsSeb, 0x7c000420,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "seb", "!0r,!1r", 4),
    ENCODING_MAP(kMipsSeh, 0x7c000620,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "seh", "!0r,!1r", 4),
#endif
    ENCODING_MAP(kMipsSh, 0xA4000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "sh", "!0r,!1d(!2r)", 4),
    ENCODING_MAP(kMipsSll, 0x00000000,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "sll", "!0r,!1r,0x!2h(!2d)", 4),
    ENCODING_MAP(kMipsSllv, 0x00000004,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sllv", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsSlt, 0x0000002a,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "slt", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsSlti, 0x28000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "slti", "!0r,!1r,0x!2h(!2d)", 4),
    ENCODING_MAP(kMipsSltu, 0x0000002b,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sltu", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsSra, 0x00000003,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "sra", "!0r,!1r,0x!2h(!2d)", 4),
    ENCODING_MAP(kMipsSrav, 0x00000007,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "srav", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsSrl, 0x00000002,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "srl", "!0r,!1r,0x!2h(!2d)", 4),
    ENCODING_MAP(kMipsSrlv, 0x00000006,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "srlv", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsSubu, 0x00000023, /* used for "neg" too */
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "subu", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsSw, 0xAC000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "sw", "!0r,!1d(!2r)", 4),
    ENCODING_MAP(kMipsXor, 0x00000026,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "xor", "!0r,!1r,!2r", 4),
    ENCODING_MAP(kMipsXori, 0x38000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "xori", "!0r,!1r,0x!2h(!2d)", 4),
    ENCODING_MAP(kMipsFadds, 0x46000000,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtSfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "add.s", "!0s,!1s,!2s", 4),
    ENCODING_MAP(kMipsFsubs, 0x46000001,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtSfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sub.s", "!0s,!1s,!2s", 4),
    ENCODING_MAP(kMipsFmuls, 0x46000002,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtSfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "mul.s", "!0s,!1s,!2s", 4),
    ENCODING_MAP(kMipsFdivs, 0x46000003,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtSfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "div.s", "!0s,!1s,!2s", 4),
    ENCODING_MAP(kMipsFaddd, 0x46200000,
                 kFmtDfp, 10, 6, kFmtDfp, 15, 11, kFmtDfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "add.d", "!0S,!1S,!2S", 4),
    ENCODING_MAP(kMipsFsubd, 0x46200001,
                 kFmtDfp, 10, 6, kFmtDfp, 15, 11, kFmtDfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sub.d", "!0S,!1S,!2S", 4),
    ENCODING_MAP(kMipsFmuld, 0x46200002,
                 kFmtDfp, 10, 6, kFmtDfp, 15, 11, kFmtDfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "mul.d", "!0S,!1S,!2S", 4),
    ENCODING_MAP(kMipsFdivd, 0x46200003,
                 kFmtDfp, 10, 6, kFmtDfp, 15, 11, kFmtDfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "div.d", "!0S,!1S,!2S", 4),
    ENCODING_MAP(kMipsFcvtsd, 0x46200020,
                 kFmtSfp, 10, 6, kFmtDfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.s.d", "!0s,!1S", 4),
    ENCODING_MAP(kMipsFcvtsw, 0x46800020,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.s.w", "!0s,!1s", 4),
    ENCODING_MAP(kMipsFcvtds, 0x46000021,
                 kFmtDfp, 10, 6, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.d.s", "!0S,!1s", 4),
    ENCODING_MAP(kMipsFcvtdw, 0x46800021,
                 kFmtDfp, 10, 6, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.d.w", "!0S,!1s", 4),
    ENCODING_MAP(kMipsFcvtws, 0x46000024,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.w.s", "!0s,!1s", 4),
    ENCODING_MAP(kMipsFcvtwd, 0x46200024,
                 kFmtSfp, 10, 6, kFmtDfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.w.d", "!0s,!1S", 4),
    ENCODING_MAP(kMipsFmovs, 0x46000006,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mov.s", "!0s,!1s", 4),
    ENCODING_MAP(kMipsFmovd, 0x46200006,
                 kFmtDfp, 10, 6, kFmtDfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mov.d", "!0S,!1S", 4),
    ENCODING_MAP(kMipsFlwc1, 0xC4000000,
                 kFmtSfp, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lwc1", "!0s,!1d(!2r)", 4),
    ENCODING_MAP(kMipsFldc1, 0xD4000000,
                 kFmtDfp, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "ldc1", "!0S,!1d(!2r)", 4),
    ENCODING_MAP(kMipsFswc1, 0xE4000000,
                 kFmtSfp, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "swc1", "!0s,!1d(!2r)", 4),
    ENCODING_MAP(kMipsFsdc1, 0xF4000000,
                 kFmtDfp, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "sdc1", "!0S,!1d(!2r)", 4),
    ENCODING_MAP(kMipsMfc1, 0x44000000,
                 kFmtBitBlt, 20, 16, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mfc1", "!0r,!1s", 4),
    ENCODING_MAP(kMipsMtc1, 0x44800000,
                 kFmtBitBlt, 20, 16, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE0 | REG_DEF1,
                 "mtc1", "!0r,!1s", 4),
    ENCODING_MAP(kMipsDelta, 0x27e00000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtUnused, 15, 0,
                 kFmtUnused, -1, -1, IS_QUAD_OP | REG_DEF0 | REG_USE_LR |
                 NEEDS_FIXUP, "addiu", "!0r,ra,0x!1h(!1d)", 4),
    ENCODING_MAP(kMipsDeltaHi, 0x3C000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_QUAD_OP | REG_DEF0 | NEEDS_FIXUP,
                 "lui", "!0r,0x!1h(!1d)", 4),
    ENCODING_MAP(kMipsDeltaLo, 0x34000000,
                 kFmtBlt5_2, 16, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_QUAD_OP | REG_DEF0_USE0 | NEEDS_FIXUP,
                 "ori", "!0r,!0r,0x!1h(!1d)", 4),
    ENCODING_MAP(kMipsCurrPC, 0x04110001,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND | IS_BRANCH | REG_DEF_LR,
                 "addiu", "ra,pc,8", 4),
    ENCODING_MAP(kMipsSync, 0x0000000f,
                 kFmtBitBlt, 10, 6, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP,
                 "sync", ";", 4),
    ENCODING_MAP(kMipsUndefined, 0x64000000,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND,
                 "undefined", "", 4),
};


/*
 * Convert a short-form branch to long form.  Hopefully, this won't happen
 * very often because the PIC sequence is especially unfortunate.
 *
 * Orig conditional branch
 * -----------------------
 *      beq  rs,rt,target
 *
 * Long conditional branch
 * -----------------------
 *      bne  rs,rt,hop
 *      bal  .+8   ; r_RA <- anchor
 *      lui  r_AT, ((target-anchor) >> 16)
 * anchor:
 *      ori  r_AT, r_AT, ((target-anchor) & 0xffff)
 *      addu r_AT, r_AT, r_RA
 *      jr   r_AT
 * hop:
 *
 * Orig unconditional branch
 * -------------------------
 *      b target
 *
 * Long unconditional branch
 * -----------------------
 *      bal  .+8   ; r_RA <- anchor
 *      lui  r_AT, ((target-anchor) >> 16)
 * anchor:
 *      ori  r_AT, r_AT, ((target-anchor) & 0xffff)
 *      addu r_AT, r_AT, r_RA
 *      jr   r_AT
 *
 *
 * NOTE: An out-of-range bal isn't supported because it should
 * never happen with the current PIC model.
 */
void MipsMir2Lir::ConvertShortToLongBranch(LIR* lir) {
  // For conditional branches we'll need to reverse the sense
  bool unconditional = false;
  int opcode = lir->opcode;
  int dalvik_offset = lir->dalvik_offset;
  switch (opcode) {
    case kMipsBal:
      LOG(FATAL) << "long branch and link unsupported";
    case kMipsB:
      unconditional = true;
      break;
    case kMipsBeq:  opcode = kMipsBne; break;
    case kMipsBne:  opcode = kMipsBeq; break;
    case kMipsBeqz: opcode = kMipsBnez; break;
    case kMipsBgez: opcode = kMipsBltz; break;
    case kMipsBgtz: opcode = kMipsBlez; break;
    case kMipsBlez: opcode = kMipsBgtz; break;
    case kMipsBltz: opcode = kMipsBgez; break;
    case kMipsBnez: opcode = kMipsBeqz; break;
    default:
      LOG(FATAL) << "Unexpected branch kind " << opcode;
  }
  LIR* hop_target = NULL;
  if (!unconditional) {
    hop_target = RawLIR(dalvik_offset, kPseudoTargetLabel);
    LIR* hop_branch = RawLIR(dalvik_offset, opcode, lir->operands[0],
                            lir->operands[1], 0, 0, 0, hop_target);
    InsertLIRBefore(lir, hop_branch);
  }
  LIR* curr_pc = RawLIR(dalvik_offset, kMipsCurrPC);
  InsertLIRBefore(lir, curr_pc);
  LIR* anchor = RawLIR(dalvik_offset, kPseudoTargetLabel);
  LIR* delta_hi = RawLIR(dalvik_offset, kMipsDeltaHi, r_AT, 0,
                        reinterpret_cast<uintptr_t>(anchor), 0, 0, lir->target);
  InsertLIRBefore(lir, delta_hi);
  InsertLIRBefore(lir, anchor);
  LIR* delta_lo = RawLIR(dalvik_offset, kMipsDeltaLo, r_AT, 0,
                        reinterpret_cast<uintptr_t>(anchor), 0, 0, lir->target);
  InsertLIRBefore(lir, delta_lo);
  LIR* addu = RawLIR(dalvik_offset, kMipsAddu, r_AT, r_AT, r_RA);
  InsertLIRBefore(lir, addu);
  LIR* jr = RawLIR(dalvik_offset, kMipsJr, r_AT);
  InsertLIRBefore(lir, jr);
  if (!unconditional) {
    InsertLIRBefore(lir, hop_target);
  }
  lir->flags.is_nop = true;
}

/*
 * Assemble the LIR into binary instruction format.  Note that we may
 * discover that pc-relative displacements may not fit the selected
 * instruction.  In those cases we will try to substitute a new code
 * sequence or request that the trace be shortened and retried.
 */
AssemblerStatus MipsMir2Lir::AssembleInstructions(uintptr_t start_addr) {
  LIR *lir;
  AssemblerStatus res = kSuccess;  // Assume success

  for (lir = first_lir_insn_; lir != NULL; lir = NEXT_LIR(lir)) {
    if (lir->opcode < 0) {
      continue;
    }


    if (lir->flags.is_nop) {
      continue;
    }

    if (lir->flags.pcRelFixup) {
      if (lir->opcode == kMipsDelta) {
        /*
         * The "Delta" pseudo-ops load the difference between
         * two pc-relative locations into a the target register
         * found in operands[0].  The delta is determined by
         * (label2 - label1), where label1 is a standard
         * kPseudoTargetLabel and is stored in operands[2].
         * If operands[3] is null, then label2 is a kPseudoTargetLabel
         * and is found in lir->target.  If operands[3] is non-NULL,
         * then it is a Switch/Data table.
         */
        int offset1 = (reinterpret_cast<LIR*>(lir->operands[2]))->offset;
        SwitchTable *tab_rec = reinterpret_cast<SwitchTable*>(lir->operands[3]);
        int offset2 = tab_rec ? tab_rec->offset : lir->target->offset;
        int delta = offset2 - offset1;
        if ((delta & 0xffff) == delta && ((delta & 0x8000) == 0)) {
          // Fits
          lir->operands[1] = delta;
        } else {
          // Doesn't fit - must expand to kMipsDelta[Hi|Lo] pair
          LIR *new_delta_hi =
              RawLIR(lir->dalvik_offset, kMipsDeltaHi,
                     lir->operands[0], 0, lir->operands[2],
                     lir->operands[3], 0, lir->target);
          InsertLIRBefore(lir, new_delta_hi);
          LIR *new_delta_lo =
              RawLIR(lir->dalvik_offset, kMipsDeltaLo,
                     lir->operands[0], 0, lir->operands[2],
                     lir->operands[3], 0, lir->target);
          InsertLIRBefore(lir, new_delta_lo);
          LIR *new_addu =
              RawLIR(lir->dalvik_offset, kMipsAddu,
                     lir->operands[0], lir->operands[0], r_RA);
          InsertLIRBefore(lir, new_addu);
          lir->flags.is_nop = true;
          res = kRetryAll;
        }
      } else if (lir->opcode == kMipsDeltaLo) {
        int offset1 = (reinterpret_cast<LIR*>(lir->operands[2]))->offset;
        SwitchTable *tab_rec = reinterpret_cast<SwitchTable*>(lir->operands[3]);
        int offset2 = tab_rec ? tab_rec->offset : lir->target->offset;
        int delta = offset2 - offset1;
        lir->operands[1] = delta & 0xffff;
      } else if (lir->opcode == kMipsDeltaHi) {
        int offset1 = (reinterpret_cast<LIR*>(lir->operands[2]))->offset;
        SwitchTable *tab_rec = reinterpret_cast<SwitchTable*>(lir->operands[3]);
        int offset2 = tab_rec ? tab_rec->offset : lir->target->offset;
        int delta = offset2 - offset1;
        lir->operands[1] = (delta >> 16) & 0xffff;
      } else if (lir->opcode == kMipsB || lir->opcode == kMipsBal) {
        LIR *target_lir = lir->target;
        uintptr_t pc = lir->offset + 4;
        uintptr_t target = target_lir->offset;
        int delta = target - pc;
        if (delta & 0x3) {
          LOG(FATAL) << "PC-rel offset not multiple of 4: " << delta;
        }
        if (delta > 131068 || delta < -131069) {
          res = kRetryAll;
          ConvertShortToLongBranch(lir);
        } else {
          lir->operands[0] = delta >> 2;
        }
      } else if (lir->opcode >= kMipsBeqz && lir->opcode <= kMipsBnez) {
        LIR *target_lir = lir->target;
        uintptr_t pc = lir->offset + 4;
        uintptr_t target = target_lir->offset;
        int delta = target - pc;
        if (delta & 0x3) {
          LOG(FATAL) << "PC-rel offset not multiple of 4: " << delta;
        }
        if (delta > 131068 || delta < -131069) {
          res = kRetryAll;
          ConvertShortToLongBranch(lir);
        } else {
          lir->operands[1] = delta >> 2;
        }
      } else if (lir->opcode == kMipsBeq || lir->opcode == kMipsBne) {
        LIR *target_lir = lir->target;
        uintptr_t pc = lir->offset + 4;
        uintptr_t target = target_lir->offset;
        int delta = target - pc;
        if (delta & 0x3) {
          LOG(FATAL) << "PC-rel offset not multiple of 4: " << delta;
        }
        if (delta > 131068 || delta < -131069) {
          res = kRetryAll;
          ConvertShortToLongBranch(lir);
        } else {
          lir->operands[2] = delta >> 2;
        }
      } else if (lir->opcode == kMipsJal) {
        uintptr_t cur_pc = (start_addr + lir->offset + 4) & ~3;
        uintptr_t target = lir->operands[0];
        /* ensure PC-region branch can be used */
        DCHECK_EQ((cur_pc & 0xF0000000), (target & 0xF0000000));
        if (target & 0x3) {
          LOG(FATAL) << "Jump target not multiple of 4: " << target;
        }
        lir->operands[0] =  target >> 2;
      } else if (lir->opcode == kMipsLahi) { /* ld address hi (via lui) */
        LIR *target_lir = lir->target;
        uintptr_t target = start_addr + target_lir->offset;
        lir->operands[1] = target >> 16;
      } else if (lir->opcode == kMipsLalo) { /* ld address lo (via ori) */
        LIR *target_lir = lir->target;
        uintptr_t target = start_addr + target_lir->offset;
        lir->operands[2] = lir->operands[2] + target;
      }
    }

    /*
     * If one of the pc-relative instructions expanded we'll have
     * to make another pass.  Don't bother to fully assemble the
     * instruction.
     */
    if (res != kSuccess) {
      continue;
    }
    const MipsEncodingMap *encoder = &EncodingMap[lir->opcode];
    uint32_t bits = encoder->skeleton;
    int i;
    for (i = 0; i < 4; i++) {
      uint32_t operand;
      uint32_t value;
      operand = lir->operands[i];
      switch (encoder->field_loc[i].kind) {
        case kFmtUnused:
          break;
        case kFmtBitBlt:
          if (encoder->field_loc[i].start == 0 && encoder->field_loc[i].end == 31) {
            value = operand;
          } else {
            value = (operand << encoder->field_loc[i].start) &
                ((1 << (encoder->field_loc[i].end + 1)) - 1);
          }
          bits |= value;
          break;
        case kFmtBlt5_2:
          value = (operand & 0x1f);
          bits |= (value << encoder->field_loc[i].start);
          bits |= (value << encoder->field_loc[i].end);
          break;
        case kFmtDfp: {
          DCHECK(MIPS_DOUBLEREG(operand));
          DCHECK_EQ((operand & 0x1), 0U);
          value = ((operand & MIPS_FP_REG_MASK) << encoder->field_loc[i].start) &
              ((1 << (encoder->field_loc[i].end + 1)) - 1);
          bits |= value;
          break;
        }
        case kFmtSfp:
          DCHECK(MIPS_SINGLEREG(operand));
          value = ((operand & MIPS_FP_REG_MASK) << encoder->field_loc[i].start) &
              ((1 << (encoder->field_loc[i].end + 1)) - 1);
          bits |= value;
          break;
        default:
          LOG(FATAL) << "Bad encoder format: " << encoder->field_loc[i].kind;
      }
    }
    // We only support little-endian MIPS.
    code_buffer_.push_back(bits & 0xff);
    code_buffer_.push_back((bits >> 8) & 0xff);
    code_buffer_.push_back((bits >> 16) & 0xff);
    code_buffer_.push_back((bits >> 24) & 0xff);
    // TUNING: replace with proper delay slot handling
    if (encoder->size == 8) {
      const MipsEncodingMap *encoder = &EncodingMap[kMipsNop];
      uint32_t bits = encoder->skeleton;
      code_buffer_.push_back(bits & 0xff);
      code_buffer_.push_back((bits >> 8) & 0xff);
      code_buffer_.push_back((bits >> 16) & 0xff);
      code_buffer_.push_back((bits >> 24) & 0xff);
    }
  }
  return res;
}

int MipsMir2Lir::GetInsnSize(LIR* lir) {
  return EncodingMap[lir->opcode].size;
}

}  // namespace art
