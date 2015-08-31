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

#include "Dalvik.h"
#include "libdex/DexOpcodes.h"

#include "../../CompilerInternals.h"
#include "MipsLIR.h"
#include "Codegen.h"
#include <unistd.h>             /* for cacheflush */
#include <sys/mman.h>           /* for protection change */

#define MAX_ASSEMBLER_RETRIES 10

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
 *
 *  [!] escape.  To insert "!", use "!!"
 */
/* NOTE: must be kept in sync with enum MipsOpcode from MipsLIR.h */
MipsEncodingMap EncodingMap[kMipsLast] = {
    ENCODING_MAP(kMips32BitData, 0x00000000,
                 kFmtBitBlt, 31, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP,
                 "data", "0x!0h(!0d)", 2),
    ENCODING_MAP(kMipsAddiu, 0x24000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "addiu", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsAddu, 0x00000021,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "addu", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsAnd, 0x00000024,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "and", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsAndi, 0x30000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "andi", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsB, 0x10000000,
                 kFmtBitBlt, 15, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND | IS_BRANCH,
                 "b", "!0t", 2),
    ENCODING_MAP(kMipsBal, 0x04110000,
                 kFmtBitBlt, 15, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND | IS_BRANCH | REG_DEF_LR,
                 "bal", "!0t", 2),
    ENCODING_MAP(kMipsBeq, 0x10000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | REG_USE01,
                 "beq", "!0r,!1r,!2t", 2),
    ENCODING_MAP(kMipsBeqz, 0x10000000, /* same as beq above with t = $zero */
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "beqz", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBgez, 0x04010000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "bgez", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBgtz, 0x1C000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "bgtz", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBlez, 0x18000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "blez", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBltz, 0x04000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "bltz", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBnez, 0x14000000, /* same as bne below with t = $zero */
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "bnez", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBne, 0x14000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | REG_USE01,
                 "bne", "!0r,!1r,!2t", 2),
    ENCODING_MAP(kMipsDiv, 0x0000001a,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtBitBlt, 25, 21,
                 kFmtBitBlt, 20, 16, IS_QUAD_OP | REG_DEF01 | REG_USE23,
                 "div", "!2r,!3r", 2),
#if __mips_isa_rev>=2
    ENCODING_MAP(kMipsExt, 0x7c000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 10, 6,
                 kFmtBitBlt, 15, 11, IS_QUAD_OP | REG_DEF0 | REG_USE1,
                 "ext", "!0r,!1r,!2d,!3D", 2),
#endif
    ENCODING_MAP(kMipsJal, 0x0c000000,
                 kFmtBitBlt, 25, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_DEF_LR,
                 "jal", "!0T(!0E)", 2),
    ENCODING_MAP(kMipsJalr, 0x00000009,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | REG_DEF0_USE1,
                 "jalr", "!0r,!1r", 2),
    ENCODING_MAP(kMipsJr, 0x00000008,
                 kFmtBitBlt, 25, 21, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "jr", "!0r", 2),
    ENCODING_MAP(kMipsLahi, 0x3C000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "lahi/lui", "!0r,0x!1h(!1d)", 2),
    ENCODING_MAP(kMipsLalo, 0x34000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "lalo/ori", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsLui, 0x3C000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "lui", "!0r,0x!1h(!1d)", 2),
    ENCODING_MAP(kMipsLb, 0x80000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lb", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsLbu, 0x90000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lbu", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsLh, 0x84000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lh", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsLhu, 0x94000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lhu", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsLw, 0x8C000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lw", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsMfhi, 0x00000010,
                 kFmtBitBlt, 15, 11, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mfhi", "!0r", 2),
    ENCODING_MAP(kMipsMflo, 0x00000012,
                 kFmtBitBlt, 15, 11, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mflo", "!0r", 2),
    ENCODING_MAP(kMipsMove, 0x00000025, /* or using zero reg */
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "move", "!0r,!1r", 2),
    ENCODING_MAP(kMipsMovz, 0x0000000a,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "movz", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsMul, 0x70000002,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "mul", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsNop, 0x00000000,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND,
                 "nop", "", 2),
    ENCODING_MAP(kMipsNor, 0x00000027, /* used for "not" too */
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "nor", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsOr, 0x00000025,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "or", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsOri, 0x34000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "ori", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsPref, 0xCC000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE2,
                 "pref", "!0d,!1d(!2r)", 2),
    ENCODING_MAP(kMipsSb, 0xA0000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "sb", "!0r,!1d(!2r)", 2),
#if __mips_isa_rev>=2
    ENCODING_MAP(kMipsSeb, 0x7c000420,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "seb", "!0r,!1r", 2),
    ENCODING_MAP(kMipsSeh, 0x7c000620,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "seh", "!0r,!1r", 2),
#endif
    ENCODING_MAP(kMipsSh, 0xA4000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "sh", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsSll, 0x00000000,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "sll", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsSllv, 0x00000004,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sllv", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSlt, 0x0000002a,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "slt", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSlti, 0x28000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "slti", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsSltu, 0x0000002b,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sltu", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSra, 0x00000003,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "sra", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsSrav, 0x00000007,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "srav", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSrl, 0x00000002,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "srl", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsSrlv, 0x00000006,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "srlv", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSubu, 0x00000023, /* used for "neg" too */
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "subu", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSw, 0xAC000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "sw", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsXor, 0x00000026,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "xor", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsXori, 0x38000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "xori", "!0r,!1r,0x!2h(!2d)", 2),
#ifdef __mips_hard_float
    ENCODING_MAP(kMipsFadds, 0x46000000,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtSfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "add.s", "!0s,!1s,!2s", 2),
    ENCODING_MAP(kMipsFsubs, 0x46000001,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtSfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sub.s", "!0s,!1s,!2s", 2),
    ENCODING_MAP(kMipsFmuls, 0x46000002,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtSfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "mul.s", "!0s,!1s,!2s", 2),
    ENCODING_MAP(kMipsFdivs, 0x46000003,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtSfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "div.s", "!0s,!1s,!2s", 2),
    ENCODING_MAP(kMipsFaddd, 0x46200000,
                 kFmtDfp, 10, 6, kFmtDfp, 15, 11, kFmtDfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "add.d", "!0S,!1S,!2S", 2),
    ENCODING_MAP(kMipsFsubd, 0x46200001,
                 kFmtDfp, 10, 6, kFmtDfp, 15, 11, kFmtDfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sub.d", "!0S,!1S,!2S", 2),
    ENCODING_MAP(kMipsFmuld, 0x46200002,
                 kFmtDfp, 10, 6, kFmtDfp, 15, 11, kFmtDfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "mul.d", "!0S,!1S,!2S", 2),
    ENCODING_MAP(kMipsFdivd, 0x46200003,
                 kFmtDfp, 10, 6, kFmtDfp, 15, 11, kFmtDfp, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "div.d", "!0S,!1S,!2S", 2),
    ENCODING_MAP(kMipsFcvtsd, 0x46200020,
                 kFmtSfp, 10, 6, kFmtDfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.s.d", "!0s,!1S", 2),
    ENCODING_MAP(kMipsFcvtsw, 0x46800020,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.s.w", "!0s,!1s", 2),
    ENCODING_MAP(kMipsFcvtds, 0x46000021,
                 kFmtDfp, 10, 6, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.d.s", "!0S,!1s", 2),
    ENCODING_MAP(kMipsFcvtdw, 0x46800021,
                 kFmtDfp, 10, 6, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.d.w", "!0S,!1s", 2),
    ENCODING_MAP(kMipsFcvtws, 0x46000024,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.w.s", "!0s,!1s", 2),
    ENCODING_MAP(kMipsFcvtwd, 0x46200024,
                 kFmtSfp, 10, 6, kFmtDfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "cvt.w.d", "!0s,!1S", 2),
    ENCODING_MAP(kMipsFmovs, 0x46000006,
                 kFmtSfp, 10, 6, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mov.s", "!0s,!1s", 2),
    ENCODING_MAP(kMipsFmovd, 0x46200006,
                 kFmtDfp, 10, 6, kFmtDfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mov.d", "!0S,!1S", 2),
    ENCODING_MAP(kMipsFlwc1, 0xC4000000,
                 kFmtSfp, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lwc1", "!0s,!1d(!2r)", 2),
    ENCODING_MAP(kMipsFldc1, 0xD4000000,
                 kFmtDfp, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "ldc1", "!0S,!1d(!2r)", 2),
    ENCODING_MAP(kMipsFswc1, 0xE4000000,
                 kFmtSfp, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "swc1", "!0s,!1d(!2r)", 2),
    ENCODING_MAP(kMipsFsdc1, 0xF4000000,
                 kFmtDfp, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "sdc1", "!0S,!1d(!2r)", 2),
    ENCODING_MAP(kMipsMfc1, 0x44000000,
                 kFmtBitBlt, 20, 16, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mfc1", "!0r,!1s", 2),
    ENCODING_MAP(kMipsMtc1, 0x44800000,
                 kFmtBitBlt, 20, 16, kFmtSfp, 15, 11, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE0 | REG_DEF1,
                 "mtc1", "!0r,!1s", 2),
#endif
    ENCODING_MAP(kMipsUndefined, 0x64000000,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND,
                 "undefined", "", 2),
};

/* Track the number of times that the code cache is patched */
#if defined(WITH_JIT_TUNING)
#define UPDATE_CODE_CACHE_PATCHES()    (gDvmJit.codeCachePatches++)
#else
#define UPDATE_CODE_CACHE_PATCHES()
#endif

/* Write the numbers in the constant and class pool to the output stream */
static void installLiteralPools(CompilationUnit *cUnit)
{
    int *dataPtr = (int *) ((char *) cUnit->baseAddr + cUnit->dataOffset);
    /* Install number of class pointer literals */
    *dataPtr++ = cUnit->numClassPointers;
    MipsLIR *dataLIR = (MipsLIR *) cUnit->classPointerList;
    while (dataLIR) {
        /*
         * Install the callsiteinfo pointers into the cells for now. They will
         * be converted into real pointers in dvmJitInstallClassObjectPointers.
         */
        *dataPtr++ = dataLIR->operands[0];
        dataLIR = NEXT_LIR(dataLIR);
    }
    dataLIR = (MipsLIR *) cUnit->literalList;
    while (dataLIR) {
        *dataPtr++ = dataLIR->operands[0];
        dataLIR = NEXT_LIR(dataLIR);
    }
}

/*
 * Assemble the LIR into binary instruction format.  Note that we may
 * discover that pc-relative displacements may not fit the selected
 * instruction.  In those cases we will try to substitute a new code
 * sequence or request that the trace be shortened and retried.
 */
static AssemblerStatus assembleInstructions(CompilationUnit *cUnit,
                                            intptr_t startAddr)
{
    int *bufferAddr = (int *) cUnit->codeBuffer;
    MipsLIR *lir;

    for (lir = (MipsLIR *) cUnit->firstLIRInsn; lir; lir = NEXT_LIR(lir)) {
        if (lir->opcode < 0) {
            continue;
        }


        if (lir->flags.isNop) {
            continue;
        }

        if (lir->opcode == kMipsB || lir->opcode == kMipsBal) {
            MipsLIR *targetLIR = (MipsLIR *) lir->generic.target;
            intptr_t pc = lir->generic.offset + 4;
            intptr_t target = targetLIR->generic.offset;
            int delta = target - pc;
            if (delta & 0x3) {
                ALOGE("PC-rel distance is not multiple of 4: %d", delta);
                dvmAbort();
            }
            if (delta > 131068 || delta < -131069) {
                ALOGE("Unconditional branch distance out of range: %d", delta);
                dvmAbort();
            }
            lir->operands[0] = delta >> 2;
        } else if (lir->opcode >= kMipsBeqz && lir->opcode <= kMipsBnez) {
            MipsLIR *targetLIR = (MipsLIR *) lir->generic.target;
            intptr_t pc = lir->generic.offset + 4;
            intptr_t target = targetLIR->generic.offset;
            int delta = target - pc;
            if (delta & 0x3) {
                ALOGE("PC-rel distance is not multiple of 4: %d", delta);
                dvmAbort();
            }
            if (delta > 131068 || delta < -131069) {
                ALOGE("Conditional branch distance out of range: %d", delta);
                dvmAbort();
            }
            lir->operands[1] = delta >> 2;
        } else if (lir->opcode == kMipsBeq || lir->opcode == kMipsBne) {
            MipsLIR *targetLIR = (MipsLIR *) lir->generic.target;
            intptr_t pc = lir->generic.offset + 4;
            intptr_t target = targetLIR->generic.offset;
            int delta = target - pc;
            if (delta & 0x3) {
                ALOGE("PC-rel distance is not multiple of 4: %d", delta);
                dvmAbort();
            }
            if (delta > 131068 || delta < -131069) {
                ALOGE("Conditional branch distance out of range: %d", delta);
                dvmAbort();
            }
            lir->operands[2] = delta >> 2;
        } else if (lir->opcode == kMipsJal) {
            intptr_t curPC = (startAddr + lir->generic.offset + 4) & ~3;
            intptr_t target = lir->operands[0];
            /* ensure PC-region branch can be used */
            assert((curPC & 0xF0000000) == (target & 0xF0000000));
            if (target & 0x3) {
                ALOGE("Jump target is not multiple of 4: %d", target);
                dvmAbort();
            }
            lir->operands[0] =  target >> 2;
        } else if (lir->opcode == kMipsLahi) { /* load address hi (via lui) */
            MipsLIR *targetLIR = (MipsLIR *) lir->generic.target;
            intptr_t target = startAddr + targetLIR->generic.offset;
            lir->operands[1] = target >> 16;
        } else if (lir->opcode == kMipsLalo) { /* load address lo (via ori) */
            MipsLIR *targetLIR = (MipsLIR *) lir->generic.target;
            intptr_t target = startAddr + targetLIR->generic.offset;
            lir->operands[2] = lir->operands[2] + target;
        }


        MipsEncodingMap *encoder = &EncodingMap[lir->opcode];
        u4 bits = encoder->skeleton;
        int i;
        for (i = 0; i < 4; i++) {
            u4 operand;
            u4 value;
            operand = lir->operands[i];
            switch(encoder->fieldLoc[i].kind) {
                case kFmtUnused:
                    break;
                case kFmtBitBlt:
                    if (encoder->fieldLoc[i].start == 0 && encoder->fieldLoc[i].end == 31) {
                        value = operand;
                    } else {
                        value = (operand << encoder->fieldLoc[i].start) &
                                ((1 << (encoder->fieldLoc[i].end + 1)) - 1);
                    }
                    bits |= value;
                    break;
                case kFmtDfp: {
                    assert(DOUBLEREG(operand));
                    assert((operand & 0x1) == 0);
                    value = ((operand & FP_REG_MASK) << encoder->fieldLoc[i].start) &
                            ((1 << (encoder->fieldLoc[i].end + 1)) - 1);
                    bits |= value;
                    break;
                }
                case kFmtSfp:
                    assert(SINGLEREG(operand));
                    value = ((operand & FP_REG_MASK) << encoder->fieldLoc[i].start) &
                            ((1 << (encoder->fieldLoc[i].end + 1)) - 1);
                    bits |= value;
                    break;
                default:
                    assert(0);
            }
        }
        assert(encoder->size == 2);
        *bufferAddr++ = bits;
    }
    return kSuccess;
}

static int assignLiteralOffsetCommon(LIR *lir, int offset)
{
    for (;lir != NULL; lir = lir->next) {
        lir->offset = offset;
        offset += 4;
    }
    return offset;
}

/* Determine the offset of each literal field */
static int assignLiteralOffset(CompilationUnit *cUnit, int offset)
{
    /* Reserved for the size field of class pointer pool */
    offset += 4;
    offset = assignLiteralOffsetCommon(cUnit->classPointerList, offset);
    offset = assignLiteralOffsetCommon(cUnit->literalList, offset);
    return offset;
}

/*
 * Translation layout in the code cache.  Note that the codeAddress pointer
 * in JitTable will point directly to the code body (field codeAddress).  The
 * chain cell offset codeAddress - 4, and the address of the trace profile
 * counter is at codeAddress - 8.
 *
 *      +----------------------------+
 *      | Trace Profile Counter addr |  -> 4 bytes (PROF_COUNTER_ADDR_SIZE)
 *      +----------------------------+
 *   +--| Offset to chain cell counts|  -> 4 bytes (CHAIN_CELL_OFFSET_SIZE)
 *   |  +----------------------------+
 *   |  | Trace profile code         |  <- entry point when profiling
 *   |  .  -   -   -   -   -   -   - .
 *   |  | Code body                  |  <- entry point when not profiling
 *   |  .                            .
 *   |  |                            |
 *   |  +----------------------------+
 *   |  | Chaining Cells             |  -> 16/20 bytes, 4 byte aligned
 *   |  .                            .
 *   |  .                            .
 *   |  |                            |
 *   |  +----------------------------+
 *   |  | Gap for large switch stmt  |  -> # cases >= MAX_CHAINED_SWITCH_CASES
 *   |  +----------------------------+
 *   +->| Chaining cell counts       |  -> 8 bytes, chain cell counts by type
 *      +----------------------------+
 *      | Trace description          |  -> variable sized
 *      .                            .
 *      |                            |
 *      +----------------------------+
 *      | # Class pointer pool size  |  -> 4 bytes
 *      +----------------------------+
 *      | Class pointer pool         |  -> 4-byte aligned, variable size
 *      .                            .
 *      .                            .
 *      |                            |
 *      +----------------------------+
 *      | Literal pool               |  -> 4-byte aligned, variable size
 *      .                            .
 *      .                            .
 *      |                            |
 *      +----------------------------+
 *
 */

#define PROF_COUNTER_ADDR_SIZE 4
#define CHAIN_CELL_OFFSET_SIZE 4

/*
 * Utility functions to navigate various parts in a trace. If we change the
 * layout/offset in the future, we just modify these functions and we don't need
 * to propagate the changes to all the use cases.
 */
static inline char *getTraceBase(const JitEntry *p)
{
    return (char*)p->codeAddress -
        (PROF_COUNTER_ADDR_SIZE + CHAIN_CELL_OFFSET_SIZE);
}

/* Handy function to retrieve the profile count */
static inline JitTraceCounter_t getProfileCount(const JitEntry *entry)
{
    if (entry->dPC == 0 || entry->codeAddress == 0 ||
        entry->codeAddress == dvmCompilerGetInterpretTemplate())
        return 0;

    JitTraceCounter_t **p = (JitTraceCounter_t **) getTraceBase(entry);

    return **p;
}

/* Handy function to reset the profile count */
static inline void resetProfileCount(const JitEntry *entry)
{
    if (entry->dPC == 0 || entry->codeAddress == 0 ||
        entry->codeAddress == dvmCompilerGetInterpretTemplate())
        return;

    JitTraceCounter_t **p = (JitTraceCounter_t **) getTraceBase(entry);

    **p = 0;
}

/* Get the pointer of the chain cell count */
static inline ChainCellCounts* getChainCellCountsPointer(const char *base)
{
    /* 4 is the size of the profile count */
    u4 *chainCellOffsetP = (u4 *) (base + PROF_COUNTER_ADDR_SIZE);
    u4 chainCellOffset = *chainCellOffsetP;
    return (ChainCellCounts *) ((char *) chainCellOffsetP + chainCellOffset);
}

/* Get the size of all chaining cells */
static inline u4 getChainCellSize(const ChainCellCounts* pChainCellCounts)
{
    int cellSize = 0;
    int i;

    /* Get total count of chain cells */
    for (i = 0; i < kChainingCellGap; i++) {
        if (i != kChainingCellInvokePredicted) {
            cellSize += pChainCellCounts->u.count[i] *
                        (CHAIN_CELL_NORMAL_SIZE >> 2);
        } else {
            cellSize += pChainCellCounts->u.count[i] *
                (CHAIN_CELL_PREDICTED_SIZE >> 2);
        }
    }
    return cellSize;
}

/* Get the starting pointer of the trace description section */
static JitTraceDescription* getTraceDescriptionPointer(const char *base)
{
    ChainCellCounts* pCellCounts = getChainCellCountsPointer(base);
    return (JitTraceDescription*) ((char*)pCellCounts + sizeof(*pCellCounts));
}

/* Get the size of a trace description */
static int getTraceDescriptionSize(const JitTraceDescription *desc)
{
    int runCount;
    /* Trace end is always of non-meta type (ie isCode == true) */
    for (runCount = 0; ; runCount++) {
        if (desc->trace[runCount].isCode &&
            desc->trace[runCount].info.frag.runEnd)
           break;
    }
    return sizeof(JitTraceDescription) + ((runCount+1) * sizeof(JitTraceRun));
}

#if defined(SIGNATURE_BREAKPOINT)
/* Inspect the assembled instruction stream to find potential matches */
static void matchSignatureBreakpoint(const CompilationUnit *cUnit,
                                     unsigned int size)
{
    unsigned int i, j;
    u4 *ptr = (u4 *) cUnit->codeBuffer;

    for (i = 0; i < size - gDvmJit.signatureBreakpointSize + 1; i++) {
        if (ptr[i] == gDvmJit.signatureBreakpoint[0]) {
            for (j = 1; j < gDvmJit.signatureBreakpointSize; j++) {
                if (ptr[i+j] != gDvmJit.signatureBreakpoint[j]) {
                    break;
                }
            }
            if (j == gDvmJit.signatureBreakpointSize) {
                ALOGD("Signature match starting from offset %#x (%d words)",
                     i*4, gDvmJit.signatureBreakpointSize);
                int descSize = getTraceDescriptionSize(cUnit->traceDesc);
                JitTraceDescription *newCopy =
                    (JitTraceDescription *) malloc(descSize);
                memcpy(newCopy, cUnit->traceDesc, descSize);
                dvmCompilerWorkEnqueue(NULL, kWorkOrderTraceDebug, newCopy);
                break;
            }
        }
    }
}
#endif

/*
 * Go over each instruction in the list and calculate the offset from the top
 * before sending them off to the assembler. If out-of-range branch distance is
 * seen rearrange the instructions a bit to correct it.
 */
void dvmCompilerAssembleLIR(CompilationUnit *cUnit, JitTranslationInfo *info)
{
    MipsLIR *mipsLIR;
    int offset = 0;
    int i;
    ChainCellCounts chainCellCounts;
    int descSize = (cUnit->jitMode == kJitMethod) ?
        0 : getTraceDescriptionSize(cUnit->traceDesc);
    int chainingCellGap = 0;

    info->instructionSet = cUnit->instructionSet;

    /* Beginning offset needs to allow space for chain cell offset */
    for (mipsLIR = (MipsLIR *) cUnit->firstLIRInsn;
         mipsLIR;
         mipsLIR = NEXT_LIR(mipsLIR)) {
        mipsLIR->generic.offset = offset;
        if (mipsLIR->opcode >= 0 && !mipsLIR->flags.isNop) {
            mipsLIR->flags.size = EncodingMap[mipsLIR->opcode].size * 2;
            offset += mipsLIR->flags.size;
        }
        /* Pseudo opcodes don't consume space */
    }

    /* Const values have to be word aligned */
    offset = (offset + 3) & ~3;

    u4 chainCellOffset = offset;
    MipsLIR *chainCellOffsetLIR = NULL;

    if (cUnit->jitMode != kJitMethod) {
        /*
         * Get the gap (# of u4) between the offset of chaining cell count and
         * the bottom of real chaining cells. If the translation has chaining
         * cells, the gap is guaranteed to be multiples of 4.
         */
        chainingCellGap = (offset - cUnit->chainingCellBottom->offset) >> 2;

        /* Add space for chain cell counts & trace description */
        chainCellOffsetLIR = (MipsLIR *) cUnit->chainCellOffsetLIR;
        assert(chainCellOffsetLIR);
        assert(chainCellOffset < 0x10000);
        assert(chainCellOffsetLIR->opcode == kMips32BitData &&
               chainCellOffsetLIR->operands[0] == CHAIN_CELL_OFFSET_TAG);

        /*
         * Adjust the CHAIN_CELL_OFFSET_TAG LIR's offset to remove the
         * space occupied by the pointer to the trace profiling counter.
         */
        chainCellOffsetLIR->operands[0] = chainCellOffset - 4;

        offset += sizeof(chainCellCounts) + descSize;

        assert((offset & 0x3) == 0);  /* Should still be word aligned */
    }

    /* Set up offsets for literals */
    cUnit->dataOffset = offset;

    /*
     * Assign each class pointer/constant an offset from the beginning of the
     * compilation unit.
     */
    offset = assignLiteralOffset(cUnit, offset);

    cUnit->totalSize = offset;

    if (gDvmJit.codeCacheByteUsed + cUnit->totalSize > gDvmJit.codeCacheSize) {
        gDvmJit.codeCacheFull = true;
        info->discardResult = true;
        return;
    }

    /* Allocate enough space for the code block */
    cUnit->codeBuffer = (unsigned char *)dvmCompilerNew(chainCellOffset, true);
    if (cUnit->codeBuffer == NULL) {
        ALOGE("Code buffer allocation failure");
        info->discardResult = true;
        return;
    }

    /*
     * Attempt to assemble the trace.  Note that assembleInstructions
     * may rewrite the code sequence and request a retry.
     */
    cUnit->assemblerStatus = assembleInstructions(cUnit,
          (intptr_t) gDvmJit.codeCache + gDvmJit.codeCacheByteUsed);

    switch(cUnit->assemblerStatus) {
        case kSuccess:
            break;
        case kRetryAll:
            if (cUnit->assemblerRetries < MAX_ASSEMBLER_RETRIES) {
                if (cUnit->jitMode != kJitMethod) {
                    /* Restore pristine chain cell marker on retry */
                    chainCellOffsetLIR->operands[0] = CHAIN_CELL_OFFSET_TAG;
                }
                return;
            }
            /* Too many retries - reset and try cutting the trace in half */
            cUnit->assemblerRetries = 0;
            cUnit->assemblerStatus = kRetryHalve;
            return;
        case kRetryHalve:
            return;
        default:
             ALOGE("Unexpected assembler status: %d", cUnit->assemblerStatus);
             dvmAbort();
    }

#if defined(SIGNATURE_BREAKPOINT)
    if (info->discardResult == false && gDvmJit.signatureBreakpoint != NULL &&
        chainCellOffset/4 >= gDvmJit.signatureBreakpointSize) {
        matchSignatureBreakpoint(cUnit, chainCellOffset/4);
    }
#endif

    /* Don't go all the way if the goal is just to get the verbose output */
    if (info->discardResult) return;

    /*
     * The cache might disappear - acquire lock and check version
     * Continue holding lock until translation cache update is complete.
     * These actions are required here in the compiler thread because
     * it is unaffected by suspend requests and doesn't know if a
     * translation cache flush is in progress.
     */
    dvmLockMutex(&gDvmJit.compilerLock);
    if (info->cacheVersion != gDvmJit.cacheVersion) {
        /* Cache changed - discard current translation */
        info->discardResult = true;
        info->codeAddress = NULL;
        dvmUnlockMutex(&gDvmJit.compilerLock);
        return;
    }

    cUnit->baseAddr = (char *) gDvmJit.codeCache + gDvmJit.codeCacheByteUsed;
    gDvmJit.codeCacheByteUsed += offset;

    UNPROTECT_CODE_CACHE(cUnit->baseAddr, offset);

    /* Install the code block */
    memcpy((char*)cUnit->baseAddr, cUnit->codeBuffer, chainCellOffset);
    gDvmJit.numCompilations++;

    if (cUnit->jitMode != kJitMethod) {
        /* Install the chaining cell counts */
        for (i=0; i< kChainingCellGap; i++) {
            chainCellCounts.u.count[i] = cUnit->numChainingCells[i];
        }

        /* Set the gap number in the chaining cell count structure */
        chainCellCounts.u.count[kChainingCellGap] = chainingCellGap;

        memcpy((char*)cUnit->baseAddr + chainCellOffset, &chainCellCounts,
               sizeof(chainCellCounts));

        /* Install the trace description */
        memcpy((char*) cUnit->baseAddr + chainCellOffset +
                       sizeof(chainCellCounts),
               cUnit->traceDesc, descSize);
    }

    /* Write the literals directly into the code cache */
    installLiteralPools(cUnit);

    /* Flush dcache and invalidate the icache to maintain coherence */
    dvmCompilerCacheFlush((long)cUnit->baseAddr,
                          (long)((char *) cUnit->baseAddr + offset), 0);

    UPDATE_CODE_CACHE_PATCHES();

    PROTECT_CODE_CACHE(cUnit->baseAddr, offset);

    /* Translation cache update complete - release lock */
    dvmUnlockMutex(&gDvmJit.compilerLock);

    /* Record code entry point and instruction set */
    info->codeAddress = (char*)cUnit->baseAddr + cUnit->headerSize;
    /* transfer the size of the profiling code */
    info->profileCodeSize = cUnit->profileCodeSize;
}

/*
 * Returns the skeleton bit pattern associated with an opcode.  All
 * variable fields are zeroed.
 */
static u4 getSkeleton(MipsOpCode op)
{
    return EncodingMap[op].skeleton;
}

static u4 assembleChainingBranch(int branchOffset, bool thumbTarget)
{
    return getSkeleton(kMipsJal) | ((branchOffset & 0x0FFFFFFF) >> 2);
}

/*
 * Perform translation chain operation.
 * For MIPS, we'll use a JAL instruction to generate an
 * unconditional chaining branch of up to 256M. The JAL
 * instruction also has a restriction that the jump target
 * must be in the same 256M page as the JAL instruction's
 * delay slot address.
 * If the target is out of JAL's range, don't chain.
 * If one or more threads is suspended, don't chain.
 */
void* dvmJitChain(void* tgtAddr, u4* branchAddr)
{
    u4 newInst;

    /*
     * Only chain translations when there is no urge to ask all threads to
     * suspend themselves via the interpreter.
     */
    if ((gDvmJit.pProfTable != NULL) && (gDvm.sumThreadSuspendCount == 0) &&
        (gDvmJit.codeCacheFull == false) &&
        ((((int) tgtAddr) & 0xF0000000) == (((int) branchAddr+4) & 0xF0000000))) {
        gDvmJit.translationChains++;

        COMPILER_TRACE_CHAINING(
            ALOGD("Jit Runtime: chaining 0x%x to 0x%x",
                 (int) branchAddr, (int) tgtAddr & -2));

        newInst = assembleChainingBranch((int) tgtAddr & -2, 0);

        UNPROTECT_CODE_CACHE(branchAddr, sizeof(*branchAddr));

        *branchAddr = newInst;
        dvmCompilerCacheFlush((long)branchAddr, (long)branchAddr + 4, 0);
        UPDATE_CODE_CACHE_PATCHES();

        PROTECT_CODE_CACHE(branchAddr, sizeof(*branchAddr));

        gDvmJit.hasNewChain = true;
    }

    return tgtAddr;
}

#if !defined(WITH_SELF_VERIFICATION)
/*
 * Attempt to enqueue a work order to patch an inline cache for a predicted
 * chaining cell for virtual/interface calls.
 */
static void inlineCachePatchEnqueue(PredictedChainingCell *cellAddr,
                                    PredictedChainingCell *newContent)
{
    /*
     * Make sure only one thread gets here since updating the cell (ie fast
     * path and queueing the request (ie the queued path) have to be done
     * in an atomic fashion.
     */
    dvmLockMutex(&gDvmJit.compilerICPatchLock);

    /* Fast path for uninitialized chaining cell */
    if (cellAddr->clazz == NULL &&
        cellAddr->branch == PREDICTED_CHAIN_BX_PAIR_INIT) {

        UNPROTECT_CODE_CACHE(cellAddr, sizeof(*cellAddr));

        cellAddr->method = newContent->method;
        cellAddr->branch = newContent->branch;

        /*
         * The update order matters - make sure clazz is updated last since it
         * will bring the uninitialized chaining cell to life.
         */
        android_atomic_release_store((int32_t)newContent->clazz,
            (volatile int32_t *)(void*) &cellAddr->clazz);
        dvmCompilerCacheFlush((long) cellAddr, (long) (cellAddr+1), 0);
        UPDATE_CODE_CACHE_PATCHES();

        PROTECT_CODE_CACHE(cellAddr, sizeof(*cellAddr));

#if defined(WITH_JIT_TUNING)
        gDvmJit.icPatchInit++;
#endif
    /* Check if this is a frequently missed clazz */
    } else if (cellAddr->stagedClazz != newContent->clazz) {
        /* Not proven to be frequent yet - build up the filter cache */
        UNPROTECT_CODE_CACHE(cellAddr, sizeof(*cellAddr));

        cellAddr->stagedClazz = newContent->clazz;

        UPDATE_CODE_CACHE_PATCHES();
        PROTECT_CODE_CACHE(cellAddr, sizeof(*cellAddr));

#if defined(WITH_JIT_TUNING)
        gDvmJit.icPatchRejected++;
#endif
    /*
     * Different classes but same method implementation - it is safe to just
     * patch the class value without the need to stop the world.
     */
    } else if (cellAddr->method == newContent->method) {
        UNPROTECT_CODE_CACHE(cellAddr, sizeof(*cellAddr));

        cellAddr->clazz = newContent->clazz;
        /* No need to flush the cache here since the branch is not patched */
        UPDATE_CODE_CACHE_PATCHES();

        PROTECT_CODE_CACHE(cellAddr, sizeof(*cellAddr));

#if defined(WITH_JIT_TUNING)
        gDvmJit.icPatchLockFree++;
#endif
    /*
     * Cannot patch the chaining cell inline - queue it until the next safe
     * point.
     */
    } else if (gDvmJit.compilerICPatchIndex < COMPILER_IC_PATCH_QUEUE_SIZE) {
        int index = gDvmJit.compilerICPatchIndex++;
        const ClassObject *clazz = newContent->clazz;

        gDvmJit.compilerICPatchQueue[index].cellAddr = cellAddr;
        gDvmJit.compilerICPatchQueue[index].cellContent = *newContent;
        gDvmJit.compilerICPatchQueue[index].classDescriptor = clazz->descriptor;
        gDvmJit.compilerICPatchQueue[index].classLoader = clazz->classLoader;
        /* For verification purpose only */
        gDvmJit.compilerICPatchQueue[index].serialNumber = clazz->serialNumber;
#if defined(WITH_JIT_TUNING)
        gDvmJit.icPatchQueued++;
#endif
    } else {
    /* Queue is full - just drop this patch request */
#if defined(WITH_JIT_TUNING)
        gDvmJit.icPatchDropped++;
#endif
    }

    dvmUnlockMutex(&gDvmJit.compilerICPatchLock);
}
#endif

/*
 * This method is called from the invoke templates for virtual and interface
 * methods to speculatively setup a chain to the callee. The templates are
 * written in assembly and have setup method, cell, and clazz at r0, r2, and
 * r3 respectively, so there is a unused argument in the list. Upon return one
 * of the following three results may happen:
 *   1) Chain is not setup because the callee is native. Reset the rechain
 *      count to a big number so that it will take a long time before the next
 *      rechain attempt to happen.
 *   2) Chain is not setup because the callee has not been created yet. Reset
 *      the rechain count to a small number and retry in the near future.
 *   3) Ask all other threads to stop before patching this chaining cell.
 *      This is required because another thread may have passed the class check
 *      but hasn't reached the chaining cell yet to follow the chain. If we
 *      patch the content before halting the other thread, there could be a
 *      small window for race conditions to happen that it may follow the new
 *      but wrong chain to invoke a different method.
 */
const Method *dvmJitToPatchPredictedChain(const Method *method,
                                          Thread *self,
                                          PredictedChainingCell *cell,
                                          const ClassObject *clazz)
{
    int newRechainCount = PREDICTED_CHAIN_COUNTER_RECHAIN;
#if defined(WITH_SELF_VERIFICATION)
    newRechainCount = PREDICTED_CHAIN_COUNTER_AVOID;
    goto done;
#else
    PredictedChainingCell newCell;
    int baseAddr, tgtAddr;
    if (dvmIsNativeMethod(method)) {
        UNPROTECT_CODE_CACHE(cell, sizeof(*cell));

        /*
         * Put a non-zero/bogus value in the clazz field so that it won't
         * trigger immediate patching and will continue to fail to match with
         * a real clazz pointer.
         */
        cell->clazz = (ClassObject *) PREDICTED_CHAIN_FAKE_CLAZZ;

        UPDATE_CODE_CACHE_PATCHES();
        PROTECT_CODE_CACHE(cell, sizeof(*cell));
        goto done;
    }

    tgtAddr = (int) dvmJitGetTraceAddr(method->insns);
    baseAddr = (int) cell + 4;   // PC is cur_addr + 4

    if ((baseAddr & 0xF0000000) != (tgtAddr & 0xF0000000)) {
        COMPILER_TRACE_CHAINING(
            ALOGD("Jit Runtime: predicted chain %p to distant target %s ignored",
                 cell, method->name));
        goto done;
    }

    /*
     * Compilation not made yet for the callee. Reset the counter to a small
     * value and come back to check soon.
     */
    if ((tgtAddr == 0) ||
        ((void*)tgtAddr == dvmCompilerGetInterpretTemplate())) {
        COMPILER_TRACE_CHAINING(
            ALOGD("Jit Runtime: predicted chain %p to method %s%s delayed",
                 cell, method->clazz->descriptor, method->name));
        goto done;
    }

    if (cell->clazz == NULL) {
        newRechainCount = self->icRechainCount;
    }

    newCell.branch = assembleChainingBranch(tgtAddr, true);
    newCell.delay_slot = getSkeleton(kMipsNop);
    newCell.clazz = clazz;
    newCell.method = method;
    newCell.stagedClazz = NULL;

    /*
     * Enter the work order to the queue and the chaining cell will be patched
     * the next time a safe point is entered.
     *
     * If the enqueuing fails reset the rechain count to a normal value so that
     * it won't get indefinitely delayed.
     */
    inlineCachePatchEnqueue(cell, &newCell);
#endif
done:
    self->icRechainCount = newRechainCount;
    return method;
}

/*
 * Patch the inline cache content based on the content passed from the work
 * order.
 */
void dvmCompilerPatchInlineCache(void)
{
    int i;
    PredictedChainingCell *minAddr, *maxAddr;

    /* Nothing to be done */
    if (gDvmJit.compilerICPatchIndex == 0) return;

    /*
     * Since all threads are already stopped we don't really need to acquire
     * the lock. But race condition can be easily introduced in the future w/o
     * paying attention so we still acquire the lock here.
     */
    dvmLockMutex(&gDvmJit.compilerICPatchLock);

    UNPROTECT_CODE_CACHE(gDvmJit.codeCache, gDvmJit.codeCacheByteUsed);

    //ALOGD("Number of IC patch work orders: %d", gDvmJit.compilerICPatchIndex);

    /* Initialize the min/max address range */
    minAddr = (PredictedChainingCell *)
        ((char *) gDvmJit.codeCache + gDvmJit.codeCacheSize);
    maxAddr = (PredictedChainingCell *) gDvmJit.codeCache;

    for (i = 0; i < gDvmJit.compilerICPatchIndex; i++) {
        ICPatchWorkOrder *workOrder = &gDvmJit.compilerICPatchQueue[i];
        PredictedChainingCell *cellAddr = workOrder->cellAddr;
        PredictedChainingCell *cellContent = &workOrder->cellContent;
        ClassObject *clazz = dvmFindClassNoInit(workOrder->classDescriptor,
                                                workOrder->classLoader);

        assert(clazz->serialNumber == workOrder->serialNumber);

        /* Use the newly resolved clazz pointer */
        cellContent->clazz = clazz;

        COMPILER_TRACE_CHAINING(
            ALOGD("Jit Runtime: predicted chain %p from %s to %s (%s) "
                 "patched",
                 cellAddr,
                 cellAddr->clazz->descriptor,
                 cellContent->clazz->descriptor,
                 cellContent->method->name));

        /* Patch the chaining cell */
        *cellAddr = *cellContent;
        minAddr = (cellAddr < minAddr) ? cellAddr : minAddr;
        maxAddr = (cellAddr > maxAddr) ? cellAddr : maxAddr;
    }

    /* Then synchronize the I/D cache */
    dvmCompilerCacheFlush((long) minAddr, (long) (maxAddr+1), 0);
    UPDATE_CODE_CACHE_PATCHES();

    PROTECT_CODE_CACHE(gDvmJit.codeCache, gDvmJit.codeCacheByteUsed);

    gDvmJit.compilerICPatchIndex = 0;
    dvmUnlockMutex(&gDvmJit.compilerICPatchLock);
}

/*
 * Unchain a trace given the starting address of the translation
 * in the code cache.  Refer to the diagram in dvmCompilerAssembleLIR.
 * Returns the address following the last cell unchained.  Note that
 * the incoming codeAddr is a thumb code address, and therefore has
 * the low bit set.
 */
static u4* unchainSingle(JitEntry *trace)
{
    const char *base = getTraceBase(trace);
    ChainCellCounts *pChainCellCounts = getChainCellCountsPointer(base);
    int cellSize = getChainCellSize(pChainCellCounts);
    u4* pChainCells;
    int i,j;
    PredictedChainingCell *predChainCell;

    if (cellSize == 0)
        return (u4 *) pChainCellCounts;

    /* Locate the beginning of the chain cell region */
    pChainCells = ((u4 *) pChainCellCounts) - cellSize -
                  pChainCellCounts->u.count[kChainingCellGap];

    /* The cells are sorted in order - walk through them and reset */
    for (i = 0; i < kChainingCellGap; i++) {
        int elemSize = CHAIN_CELL_NORMAL_SIZE >> 2;  /* In 32-bit words */
        if (i == kChainingCellInvokePredicted) {
            elemSize = CHAIN_CELL_PREDICTED_SIZE >> 2;
        }

        for (j = 0; j < pChainCellCounts->u.count[i]; j++) {
            int targetOffset;
            switch(i) {
                case kChainingCellNormal:
                    targetOffset = offsetof(Thread,
                          jitToInterpEntries.dvmJitToInterpNormal);
                    break;
                case kChainingCellHot:
                case kChainingCellInvokeSingleton:
                    targetOffset = offsetof(Thread,
                          jitToInterpEntries.dvmJitToInterpTraceSelect);
                    break;
                case kChainingCellInvokePredicted:
                    targetOffset = 0;
                    predChainCell = (PredictedChainingCell *) pChainCells;
                    /*
                     * There could be a race on another mutator thread to use
                     * this particular predicted cell and the check has passed
                     * the clazz comparison. So we cannot safely wipe the
                     * method and branch but it is safe to clear the clazz,
                     * which serves as the key.
                     */
                    predChainCell->clazz = PREDICTED_CHAIN_CLAZZ_INIT;
                    break;
#if defined(WITH_SELF_VERIFICATION)
                case kChainingCellBackwardBranch:
                    targetOffset = offsetof(Thread,
                          jitToInterpEntries.dvmJitToInterpBackwardBranch);
                    break;
#else
                case kChainingCellBackwardBranch:
                    targetOffset = offsetof(Thread,
                          jitToInterpEntries.dvmJitToInterpNormal);
                    break;
#endif
                default:
                    targetOffset = 0; // make gcc happy
                    ALOGE("Unexpected chaining type: %d", i);
                    dvmAbort();  // dvmAbort OK here - can't safely recover
            }
            COMPILER_TRACE_CHAINING(
                ALOGD("Jit Runtime: unchaining %#x", (int)pChainCells));
            /*
             * Code sequence for a chaining cell is:
             *     lw   a0, offset(rSELF)
             *     jalr ra, a0
             */
            if (i != kChainingCellInvokePredicted) {
                *pChainCells = getSkeleton(kMipsLw) | (r_A0 << 16) |
                               targetOffset | (rSELF << 21);
                *(pChainCells+1) = getSkeleton(kMipsJalr) | (r_RA << 11) |
                                   (r_A0 << 21);
            }
            pChainCells += elemSize;  /* Advance by a fixed number of words */
        }
    }
    return pChainCells;
}

/* Unchain all translation in the cache. */
void dvmJitUnchainAll()
{
    u4* lowAddress = NULL;
    u4* highAddress = NULL;
    unsigned int i;
    if (gDvmJit.pJitEntryTable != NULL) {
        COMPILER_TRACE_CHAINING(ALOGD("Jit Runtime: unchaining all"));
        dvmLockMutex(&gDvmJit.tableLock);

        UNPROTECT_CODE_CACHE(gDvmJit.codeCache, gDvmJit.codeCacheByteUsed);

        for (i = 0; i < gDvmJit.jitTableSize; i++) {
            if (gDvmJit.pJitEntryTable[i].dPC &&
                !gDvmJit.pJitEntryTable[i].u.info.isMethodEntry &&
                gDvmJit.pJitEntryTable[i].codeAddress &&
                (gDvmJit.pJitEntryTable[i].codeAddress !=
                 dvmCompilerGetInterpretTemplate())) {
                u4* lastAddress;
                lastAddress = unchainSingle(&gDvmJit.pJitEntryTable[i]);
                if (lowAddress == NULL ||
                      (u4*)gDvmJit.pJitEntryTable[i].codeAddress < lowAddress)
                    lowAddress = (u4*)gDvmJit.pJitEntryTable[i].codeAddress;
                if (lastAddress > highAddress)
                    highAddress = lastAddress;
            }
        }

        if (lowAddress && highAddress)
            dvmCompilerCacheFlush((long)lowAddress, (long)highAddress, 0);

        UPDATE_CODE_CACHE_PATCHES();

        PROTECT_CODE_CACHE(gDvmJit.codeCache, gDvmJit.codeCacheByteUsed);

        dvmUnlockMutex(&gDvmJit.tableLock);
        gDvmJit.translationChains = 0;
    }
    gDvmJit.hasNewChain = false;
}

typedef struct jitProfileAddrToLine {
    u4 lineNum;
    u4 bytecodeOffset;
} jitProfileAddrToLine;


/* Callback function to track the bytecode offset/line number relationiship */
static int addrToLineCb (void *cnxt, u4 bytecodeOffset, u4 lineNum)
{
    jitProfileAddrToLine *addrToLine = (jitProfileAddrToLine *) cnxt;

    /* Best match so far for this offset */
    if (addrToLine->bytecodeOffset >= bytecodeOffset) {
        addrToLine->lineNum = lineNum;
    }
    return 0;
}

/* Dumps profile info for a single trace */
static int dumpTraceProfile(JitEntry *p, bool silent, bool reset,
                            unsigned long sum)
{
    int idx;

    if (p->codeAddress == NULL) {
        if (!silent)
            ALOGD("TRACEPROFILE NULL");
        return 0;
    }
    if (p->codeAddress == dvmCompilerGetInterpretTemplate()) {
        if (!silent)
            ALOGD("TRACEPROFILE INTERPRET_ONLY");
        return 0;
    }

    JitTraceCounter_t count = getProfileCount(p);
    if (reset) {
        resetProfileCount(p);
    }
    if (silent) {
        return count;
    }
    JitTraceDescription *desc = getTraceDescriptionPointer(getTraceBase(p));
    const Method *method = desc->method;
    char *methodDesc = dexProtoCopyMethodDescriptor(&method->prototype);
    jitProfileAddrToLine addrToLine = {0, desc->trace[0].info.frag.startOffset};

    /*
     * We may end up decoding the debug information for the same method
     * multiple times, but the tradeoff is we don't need to allocate extra
     * space to store the addr/line mapping. Since this is a debugging feature
     * and done infrequently so the slower but simpler mechanism should work
     * just fine.
     */
    dexDecodeDebugInfo(method->clazz->pDvmDex->pDexFile,
                       dvmGetMethodCode(method),
                       method->clazz->descriptor,
                       method->prototype.protoIdx,
                       method->accessFlags,
                       addrToLineCb, NULL, &addrToLine);

    ALOGD("TRACEPROFILE 0x%08x % 10d %5.2f%% [%#x(+%d), %d] %s%s;%s",
         (int) getTraceBase(p),
         count,
         ((float ) count) / sum * 100.0,
         desc->trace[0].info.frag.startOffset,
         desc->trace[0].info.frag.numInsts,
         addrToLine.lineNum,
         method->clazz->descriptor, method->name, methodDesc);
    free(methodDesc);

    /* Find the last fragment (ie runEnd is set) */
    for (idx = 0;
         desc->trace[idx].isCode && !desc->trace[idx].info.frag.runEnd;
         idx++) {
    }

    /*
     * runEnd must comes with a JitCodeDesc frag. If isCode is false it must
     * be a meta info field (only used by callsite info for now).
     */
    if (!desc->trace[idx].isCode) {
        const Method *method = (const Method *)
            desc->trace[idx+JIT_TRACE_CUR_METHOD-1].info.meta;
        char *methodDesc = dexProtoCopyMethodDescriptor(&method->prototype);
        /* Print the callee info in the trace */
        ALOGD("    -> %s%s;%s", method->clazz->descriptor, method->name,
             methodDesc);
    }

    return count;
}

/* Create a copy of the trace descriptor of an existing compilation */
JitTraceDescription *dvmCopyTraceDescriptor(const u2 *pc,
                                            const JitEntry *knownEntry)
{
    const JitEntry *jitEntry = knownEntry ? knownEntry
                                          : dvmJitFindEntry(pc, false);
    if ((jitEntry == NULL) || (jitEntry->codeAddress == 0))
        return NULL;

    JitTraceDescription *desc =
        getTraceDescriptionPointer(getTraceBase(jitEntry));

    /* Now make a copy and return */
    int descSize = getTraceDescriptionSize(desc);
    JitTraceDescription *newCopy = (JitTraceDescription *) malloc(descSize);
    memcpy(newCopy, desc, descSize);
    return newCopy;
}

/* qsort callback function */
static int sortTraceProfileCount(const void *entry1, const void *entry2)
{
    const JitEntry *jitEntry1 = (const JitEntry *)entry1;
    const JitEntry *jitEntry2 = (const JitEntry *)entry2;

    JitTraceCounter_t count1 = getProfileCount(jitEntry1);
    JitTraceCounter_t count2 = getProfileCount(jitEntry2);
    return (count1 == count2) ? 0 : ((count1 > count2) ? -1 : 1);
}

/* Sort the trace profile counts and dump them */
void dvmCompilerSortAndPrintTraceProfiles()
{
    JitEntry *sortedEntries;
    int numTraces = 0;
    unsigned long sum = 0;
    unsigned int i;

    /* Make sure that the table is not changing */
    dvmLockMutex(&gDvmJit.tableLock);

    /* Sort the entries by descending order */
    sortedEntries = (JitEntry *)malloc(sizeof(JitEntry) * gDvmJit.jitTableSize);
    if (sortedEntries == NULL)
        goto done;
    memcpy(sortedEntries, gDvmJit.pJitEntryTable,
           sizeof(JitEntry) * gDvmJit.jitTableSize);
    qsort(sortedEntries, gDvmJit.jitTableSize, sizeof(JitEntry),
          sortTraceProfileCount);

    /* Analyze the sorted entries */
    for (i=0; i < gDvmJit.jitTableSize; i++) {
        if (sortedEntries[i].dPC != 0) {
            sum += dumpTraceProfile(&sortedEntries[i],
                                       true /* silent */,
                                       false /* reset */,
                                       0);
            numTraces++;
        }
    }
    if (numTraces == 0)
        numTraces = 1;
    if (sum == 0) {
        sum = 1;
    }

    ALOGD("JIT: Average execution count -> %d",(int)(sum / numTraces));

    /* Dump the sorted entries. The count of each trace will be reset to 0. */
    for (i=0; i < gDvmJit.jitTableSize; i++) {
        if (sortedEntries[i].dPC != 0) {
            dumpTraceProfile(&sortedEntries[i],
                             false /* silent */,
                             true /* reset */,
                             sum);
        }
    }

    for (i=0; i < gDvmJit.jitTableSize && i < 10; i++) {
        /* Stip interpreter stubs */
        if (sortedEntries[i].codeAddress == dvmCompilerGetInterpretTemplate()) {
            continue;
        }
        JitTraceDescription* desc =
            dvmCopyTraceDescriptor(NULL, &sortedEntries[i]);
        if (desc) {
            dvmCompilerWorkEnqueue(sortedEntries[i].dPC,
                                   kWorkOrderTraceDebug, desc);
        }
    }

    free(sortedEntries);
done:
    dvmUnlockMutex(&gDvmJit.tableLock);
    return;
}

static void findClassPointersSingleTrace(char *base, void (*callback)(void *))
{
    unsigned int chainTypeIdx, chainIdx;
    ChainCellCounts *pChainCellCounts = getChainCellCountsPointer(base);
    int cellSize = getChainCellSize(pChainCellCounts);
    /* Scan the chaining cells */
    if (cellSize) {
        /* Locate the beginning of the chain cell region */
        u4 *pChainCells = ((u4 *) pChainCellCounts) - cellSize -
            pChainCellCounts->u.count[kChainingCellGap];
        /* The cells are sorted in order - walk through them */
        for (chainTypeIdx = 0; chainTypeIdx < kChainingCellGap;
             chainTypeIdx++) {
            if (chainTypeIdx != kChainingCellInvokePredicted) {
                /* In 32-bit words */
                pChainCells += (CHAIN_CELL_NORMAL_SIZE >> 2) *
                    pChainCellCounts->u.count[chainTypeIdx];
                continue;
            }
            for (chainIdx = 0;
                 chainIdx < pChainCellCounts->u.count[chainTypeIdx];
                 chainIdx++) {
                PredictedChainingCell *cell =
                    (PredictedChainingCell *) pChainCells;
                /*
                 * Report the cell if it contains a sane class
                 * pointer.
                 */
                if (cell->clazz != NULL &&
                    cell->clazz !=
                      (ClassObject *) PREDICTED_CHAIN_FAKE_CLAZZ) {
                    callback(&cell->clazz);
                }
                pChainCells += CHAIN_CELL_PREDICTED_SIZE >> 2;
            }
        }
    }

    /* Scan the class pointer pool */
    JitTraceDescription *desc = getTraceDescriptionPointer(base);
    int descSize = getTraceDescriptionSize(desc);
    int *classPointerP = (int *) ((char *) desc + descSize);
    int numClassPointers = *classPointerP++;
    for (; numClassPointers; numClassPointers--, classPointerP++) {
        callback(classPointerP);
    }
}

/*
 * Scan class pointers in each translation and pass its address to the callback
 * function. Currently such a pointers can be found in the pointer pool and the
 * clazz field in the predicted chaining cells.
 */
void dvmJitScanAllClassPointers(void (*callback)(void *))
{
    UNPROTECT_CODE_CACHE(gDvmJit.codeCache, gDvmJit.codeCacheByteUsed);

    /* Handle the inflight compilation first */
    if (gDvmJit.inflightBaseAddr)
        findClassPointersSingleTrace((char *) gDvmJit.inflightBaseAddr,
                                     callback);

    if (gDvmJit.pJitEntryTable != NULL) {
        unsigned int traceIdx;
        dvmLockMutex(&gDvmJit.tableLock);
        for (traceIdx = 0; traceIdx < gDvmJit.jitTableSize; traceIdx++) {
            const JitEntry *entry = &gDvmJit.pJitEntryTable[traceIdx];
            if (entry->dPC &&
                !entry->u.info.isMethodEntry &&
                entry->codeAddress &&
                (entry->codeAddress != dvmCompilerGetInterpretTemplate())) {
                char *base = getTraceBase(entry);
                findClassPointersSingleTrace(base, callback);
            }
        }
        dvmUnlockMutex(&gDvmJit.tableLock);
    }
    UPDATE_CODE_CACHE_PATCHES();

    PROTECT_CODE_CACHE(gDvmJit.codeCache, gDvmJit.codeCacheByteUsed);
}

/*
 * Provide the final touch on the class object pointer pool to install the
 * actual pointers. The thread has to be in the running state.
 */
void dvmJitInstallClassObjectPointers(CompilationUnit *cUnit, char *codeAddress)
{
    char *base = codeAddress - cUnit->headerSize;

    /* Scan the class pointer pool */
    JitTraceDescription *desc = getTraceDescriptionPointer(base);
    int descSize = getTraceDescriptionSize(desc);
    intptr_t *classPointerP = (int *) ((char *) desc + descSize);
    int numClassPointers = *(int *)classPointerP++;
    intptr_t *startClassPointerP = classPointerP;

    /*
     * Change the thread state to VM_RUNNING so that GC won't be happening
     * when the assembler looks up the class pointers. May suspend the current
     * thread if there is a pending request before the state is actually
     * changed to RUNNING.
     */
    dvmChangeStatus(gDvmJit.compilerThread, THREAD_RUNNING);

    /*
     * Unprotecting the code cache will need to acquire the code cache
     * protection lock first. Doing so after the state change may increase the
     * time spent in the RUNNING state (which may delay the next GC request
     * should there be contention on codeCacheProtectionLock). In practice
     * this is probably not going to happen often since a GC is just served.
     * More importantly, acquiring the lock before the state change will
     * cause deadlock (b/4192964).
     */
    UNPROTECT_CODE_CACHE(startClassPointerP,
                         numClassPointers * sizeof(intptr_t));
#if defined(WITH_JIT_TUNING)
    u8 startTime = dvmGetRelativeTimeUsec();
#endif
    for (;numClassPointers; numClassPointers--) {
        CallsiteInfo *callsiteInfo = (CallsiteInfo *) *classPointerP;
        ClassObject *clazz = dvmFindClassNoInit(
            callsiteInfo->classDescriptor, callsiteInfo->classLoader);
        assert(!strcmp(clazz->descriptor, callsiteInfo->classDescriptor));
        *classPointerP++ = (intptr_t) clazz;
    }

    /*
     * Register the base address so that if GC kicks in after the thread state
     * has been changed to VMWAIT and before the compiled code is registered
     * in the JIT table, its content can be patched if class objects are
     * moved.
     */
    gDvmJit.inflightBaseAddr = base;

#if defined(WITH_JIT_TUNING)
    u8 blockTime = dvmGetRelativeTimeUsec() - startTime;
    gDvmJit.compilerThreadBlockGCTime += blockTime;
    if (blockTime > gDvmJit.maxCompilerThreadBlockGCTime)
        gDvmJit.maxCompilerThreadBlockGCTime = blockTime;
    gDvmJit.numCompilerThreadBlockGC++;
#endif
    UPDATE_CODE_CACHE_PATCHES();

    PROTECT_CODE_CACHE(startClassPointerP, numClassPointers * sizeof(intptr_t));

    /* Change the thread state back to VMWAIT */
    dvmChangeStatus(gDvmJit.compilerThread, THREAD_VMWAIT);
}

#if defined(WITH_SELF_VERIFICATION)
/*
 * The following are used to keep compiled loads and stores from modifying
 * memory during self verification mode.
 *
 * Stores do not modify memory. Instead, the address and value pair are stored
 * into heapSpace. Addresses within heapSpace are unique. For accesses smaller
 * than a word, the word containing the address is loaded first before being
 * updated.
 *
 * Loads check heapSpace first and return data from there if an entry exists.
 * Otherwise, data is loaded from memory as usual.
 */

/* Used to specify sizes of memory operations */
enum {
    kSVByte,
    kSVSignedByte,
    kSVHalfword,
    kSVSignedHalfword,
    kSVWord,
    kSVDoubleword,
    kSVVariable,
};

/* Load the value of a decoded register from the stack */
static int selfVerificationMemRegLoad(int* sp, int reg)
{
assert(0); /* MIPSTODO retarg func */
    return *(sp + reg);
}

/* Load the value of a decoded doubleword register from the stack */
static s8 selfVerificationMemRegLoadDouble(int* sp, int reg)
{
assert(0); /* MIPSTODO retarg func */
    return *((s8*)(sp + reg));
}

/* Store the value of a decoded register out to the stack */
static void selfVerificationMemRegStore(int* sp, int data, int reg)
{
assert(0); /* MIPSTODO retarg func */
    *(sp + reg) = data;
}

/* Store the value of a decoded doubleword register out to the stack */
static void selfVerificationMemRegStoreDouble(int* sp, s8 data, int reg)
{
assert(0); /* MIPSTODO retarg func */
    *((s8*)(sp + reg)) = data;
}

/*
 * Load the specified size of data from the specified address, checking
 * heapSpace first if Self Verification mode wrote to it previously, and
 * falling back to actual memory otherwise.
 */
static int selfVerificationLoad(int addr, int size)
{
assert(0); /* MIPSTODO retarg func */
    Thread *self = dvmThreadSelf();
    ShadowSpace *shadowSpace = self->shadowSpace;
    ShadowHeap *heapSpacePtr;

    int data;
    int maskedAddr = addr & 0xFFFFFFFC;
    int alignment = addr & 0x3;

    for (heapSpacePtr = shadowSpace->heapSpace;
         heapSpacePtr != shadowSpace->heapSpaceTail; heapSpacePtr++) {
        if (heapSpacePtr->addr == maskedAddr) {
            addr = ((unsigned int) &(heapSpacePtr->data)) | alignment;
            break;
        }
    }

    switch (size) {
        case kSVByte:
            data = *((u1*) addr);
            break;
        case kSVSignedByte:
            data = *((s1*) addr);
            break;
        case kSVHalfword:
            data = *((u2*) addr);
            break;
        case kSVSignedHalfword:
            data = *((s2*) addr);
            break;
        case kSVWord:
            data = *((u4*) addr);
            break;
        default:
            ALOGE("*** ERROR: BAD SIZE IN selfVerificationLoad: %d", size);
            data = 0;
            dvmAbort();
    }

    //ALOGD("*** HEAP LOAD: Addr: %#x Data: %#x Size: %d", addr, data, size);
    return data;
}

/* Like selfVerificationLoad, but specifically for doublewords */
static s8 selfVerificationLoadDoubleword(int addr)
{
assert(0); /* MIPSTODO retarg func */
    Thread *self = dvmThreadSelf();
    ShadowSpace* shadowSpace = self->shadowSpace;
    ShadowHeap* heapSpacePtr;

    int addr2 = addr+4;
    unsigned int data = *((unsigned int*) addr);
    unsigned int data2 = *((unsigned int*) addr2);

    for (heapSpacePtr = shadowSpace->heapSpace;
         heapSpacePtr != shadowSpace->heapSpaceTail; heapSpacePtr++) {
        if (heapSpacePtr->addr == addr) {
            data = heapSpacePtr->data;
        } else if (heapSpacePtr->addr == addr2) {
            data2 = heapSpacePtr->data;
        }
    }

    //ALOGD("*** HEAP LOAD DOUBLEWORD: Addr: %#x Data: %#x Data2: %#x",
    //    addr, data, data2);
    return (((s8) data2) << 32) | data;
}

/*
 * Handles a store of a specified size of data to a specified address.
 * This gets logged as an addr/data pair in heapSpace instead of modifying
 * memory.  Addresses in heapSpace are unique, and accesses smaller than a
 * word pull the entire word from memory first before updating.
 */
static void selfVerificationStore(int addr, int data, int size)
{
assert(0); /* MIPSTODO retarg func */
    Thread *self = dvmThreadSelf();
    ShadowSpace *shadowSpace = self->shadowSpace;
    ShadowHeap *heapSpacePtr;

    int maskedAddr = addr & 0xFFFFFFFC;
    int alignment = addr & 0x3;

    //ALOGD("*** HEAP STORE: Addr: %#x Data: %#x Size: %d", addr, data, size);

    for (heapSpacePtr = shadowSpace->heapSpace;
         heapSpacePtr != shadowSpace->heapSpaceTail; heapSpacePtr++) {
        if (heapSpacePtr->addr == maskedAddr) break;
    }

    if (heapSpacePtr == shadowSpace->heapSpaceTail) {
        heapSpacePtr->addr = maskedAddr;
        heapSpacePtr->data = *((unsigned int*) maskedAddr);
        shadowSpace->heapSpaceTail++;
    }

    addr = ((unsigned int) &(heapSpacePtr->data)) | alignment;
    switch (size) {
        case kSVByte:
            *((u1*) addr) = data;
            break;
        case kSVSignedByte:
            *((s1*) addr) = data;
            break;
        case kSVHalfword:
            *((u2*) addr) = data;
            break;
        case kSVSignedHalfword:
            *((s2*) addr) = data;
            break;
        case kSVWord:
            *((u4*) addr) = data;
            break;
        default:
            ALOGE("*** ERROR: BAD SIZE IN selfVerificationSave: %d", size);
            dvmAbort();
    }
}

/* Like selfVerificationStore, but specifically for doublewords */
static void selfVerificationStoreDoubleword(int addr, s8 double_data)
{
assert(0); /* MIPSTODO retarg func */
    Thread *self = dvmThreadSelf();
    ShadowSpace *shadowSpace = self->shadowSpace;
    ShadowHeap *heapSpacePtr;

    int addr2 = addr+4;
    int data = double_data;
    int data2 = double_data >> 32;
    bool store1 = false, store2 = false;

    //ALOGD("*** HEAP STORE DOUBLEWORD: Addr: %#x Data: %#x, Data2: %#x",
    //    addr, data, data2);

    for (heapSpacePtr = shadowSpace->heapSpace;
         heapSpacePtr != shadowSpace->heapSpaceTail; heapSpacePtr++) {
        if (heapSpacePtr->addr == addr) {
            heapSpacePtr->data = data;
            store1 = true;
        } else if (heapSpacePtr->addr == addr2) {
            heapSpacePtr->data = data2;
            store2 = true;
        }
    }

    if (!store1) {
        shadowSpace->heapSpaceTail->addr = addr;
        shadowSpace->heapSpaceTail->data = data;
        shadowSpace->heapSpaceTail++;
    }
    if (!store2) {
        shadowSpace->heapSpaceTail->addr = addr2;
        shadowSpace->heapSpaceTail->data = data2;
        shadowSpace->heapSpaceTail++;
    }
}

/*
 * Decodes the memory instruction at the address specified in the link
 * register. All registers (r0-r12,lr) and fp registers (d0-d15) are stored
 * consecutively on the stack beginning at the specified stack pointer.
 * Calls the proper Self Verification handler for the memory instruction and
 * updates the link register to point past the decoded memory instruction.
 */
void dvmSelfVerificationMemOpDecode(int lr, int* sp)
{
assert(0); /* MIPSTODO retarg func */
    enum {
        kMemOpLdrPcRel = 0x09, // ldr(3)  [01001] rd[10..8] imm_8[7..0]
        kMemOpRRR      = 0x0A, // Full opcode is 7 bits
        kMemOp2Single  = 0x0A, // Used for Vstrs and Vldrs
        kMemOpRRR2     = 0x0B, // Full opcode is 7 bits
        kMemOp2Double  = 0x0B, // Used for Vstrd and Vldrd
        kMemOpStrRRI5  = 0x0C, // str(1)  [01100] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpLdrRRI5  = 0x0D, // ldr(1)  [01101] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpStrbRRI5 = 0x0E, // strb(1) [01110] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpLdrbRRI5 = 0x0F, // ldrb(1) [01111] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpStrhRRI5 = 0x10, // strh(1) [10000] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpLdrhRRI5 = 0x11, // ldrh(1) [10001] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpLdrSpRel = 0x13, // ldr(4)  [10011] rd[10..8] imm_8[7..0]
        kMemOpStmia    = 0x18, // stmia   [11000] rn[10..8] reglist [7..0]
        kMemOpLdmia    = 0x19, // ldmia   [11001] rn[10..8] reglist [7..0]
        kMemOpStrRRR   = 0x28, // str(2)  [0101000] rm[8..6] rn[5..3] rd[2..0]
        kMemOpStrhRRR  = 0x29, // strh(2) [0101001] rm[8..6] rn[5..3] rd[2..0]
        kMemOpStrbRRR  = 0x2A, // strb(2) [0101010] rm[8..6] rn[5..3] rd[2..0]
        kMemOpLdrsbRRR = 0x2B, // ldrsb   [0101011] rm[8..6] rn[5..3] rd[2..0]
        kMemOpLdrRRR   = 0x2C, // ldr(2)  [0101100] rm[8..6] rn[5..3] rd[2..0]
        kMemOpLdrhRRR  = 0x2D, // ldrh(2) [0101101] rm[8..6] rn[5..3] rd[2..0]
        kMemOpLdrbRRR  = 0x2E, // ldrb(2) [0101110] rm[8..6] rn[5..3] rd[2..0]
        kMemOpLdrshRRR = 0x2F, // ldrsh   [0101111] rm[8..6] rn[5..3] rd[2..0]
        kMemOp2Stmia   = 0xE88, // stmia  [111010001000[ rn[19..16] mask[15..0]
        kMemOp2Ldmia   = 0xE89, // ldmia  [111010001001[ rn[19..16] mask[15..0]
        kMemOp2Stmia2  = 0xE8A, // stmia  [111010001010[ rn[19..16] mask[15..0]
        kMemOp2Ldmia2  = 0xE8B, // ldmia  [111010001011[ rn[19..16] mask[15..0]
        kMemOp2Vstr    = 0xED8, // Used for Vstrs and Vstrd
        kMemOp2Vldr    = 0xED9, // Used for Vldrs and Vldrd
        kMemOp2Vstr2   = 0xEDC, // Used for Vstrs and Vstrd
        kMemOp2Vldr2   = 0xEDD, // Used for Vstrs and Vstrd
        kMemOp2StrbRRR = 0xF80, /* str rt,[rn,rm,LSL #imm] [111110000000]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2LdrbRRR = 0xF81, /* ldrb rt,[rn,rm,LSL #imm] [111110000001]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2StrhRRR = 0xF82, /* str rt,[rn,rm,LSL #imm] [111110000010]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2LdrhRRR = 0xF83, /* ldrh rt,[rn,rm,LSL #imm] [111110000011]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2StrRRR  = 0xF84, /* str rt,[rn,rm,LSL #imm] [111110000100]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2LdrRRR  = 0xF85, /* ldr rt,[rn,rm,LSL #imm] [111110000101]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2StrbRRI12 = 0xF88, /* strb rt,[rn,#imm12] [111110001000]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2LdrbRRI12 = 0xF89, /* ldrb rt,[rn,#imm12] [111110001001]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2StrhRRI12 = 0xF8A, /* strh rt,[rn,#imm12] [111110001010]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2LdrhRRI12 = 0xF8B, /* ldrh rt,[rn,#imm12] [111110001011]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2StrRRI12 = 0xF8C, /* str(Imm,T3) rd,[rn,#imm12] [111110001100]
                                       rn[19..16] rt[15..12] imm12[11..0] */
        kMemOp2LdrRRI12 = 0xF8D, /* ldr(Imm,T3) rd,[rn,#imm12] [111110001101]
                                       rn[19..16] rt[15..12] imm12[11..0] */
        kMemOp2LdrsbRRR = 0xF91, /* ldrsb rt,[rn,rm,LSL #imm] [111110010001]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2LdrshRRR = 0xF93, /* ldrsh rt,[rn,rm,LSL #imm] [111110010011]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2LdrsbRRI12 = 0xF99, /* ldrsb rt,[rn,#imm12] [111110011001]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2LdrshRRI12 = 0xF9B, /* ldrsh rt,[rn,#imm12] [111110011011]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2        = 0xE000, // top 3 bits set indicates Thumb2
    };

    int addr, offset, data;
    long long double_data;
    int size = kSVWord;
    bool store = false;
    unsigned int *lr_masked = (unsigned int *) (lr & 0xFFFFFFFE);
    unsigned int insn = *lr_masked;

    int old_lr;
    old_lr = selfVerificationMemRegLoad(sp, 13);

    if ((insn & kMemOp2) == kMemOp2) {
        insn = (insn << 16) | (insn >> 16);
        //ALOGD("*** THUMB2 - Addr: %#x Insn: %#x", lr, insn);

        int opcode12 = (insn >> 20) & 0xFFF;
        int opcode6 = (insn >> 6) & 0x3F;
        int opcode4 = (insn >> 8) & 0xF;
        int imm2 = (insn >> 4) & 0x3;
        int imm8 = insn & 0xFF;
        int imm12 = insn & 0xFFF;
        int rd = (insn >> 12) & 0xF;
        int rm = insn & 0xF;
        int rn = (insn >> 16) & 0xF;
        int rt = (insn >> 12) & 0xF;
        bool wBack = true;

        // Update the link register
        selfVerificationMemRegStore(sp, old_lr+4, 13);

        // Determine whether the mem op is a store or load
        switch (opcode12) {
            case kMemOp2Stmia:
            case kMemOp2Stmia2:
            case kMemOp2Vstr:
            case kMemOp2Vstr2:
            case kMemOp2StrbRRR:
            case kMemOp2StrhRRR:
            case kMemOp2StrRRR:
            case kMemOp2StrbRRI12:
            case kMemOp2StrhRRI12:
            case kMemOp2StrRRI12:
                store = true;
        }

        // Determine the size of the mem access
        switch (opcode12) {
            case kMemOp2StrbRRR:
            case kMemOp2LdrbRRR:
            case kMemOp2StrbRRI12:
            case kMemOp2LdrbRRI12:
                size = kSVByte;
                break;
            case kMemOp2LdrsbRRR:
            case kMemOp2LdrsbRRI12:
                size = kSVSignedByte;
                break;
            case kMemOp2StrhRRR:
            case kMemOp2LdrhRRR:
            case kMemOp2StrhRRI12:
            case kMemOp2LdrhRRI12:
                size = kSVHalfword;
                break;
            case kMemOp2LdrshRRR:
            case kMemOp2LdrshRRI12:
                size = kSVSignedHalfword;
                break;
            case kMemOp2Vstr:
            case kMemOp2Vstr2:
            case kMemOp2Vldr:
            case kMemOp2Vldr2:
                if (opcode4 == kMemOp2Double) size = kSVDoubleword;
                break;
            case kMemOp2Stmia:
            case kMemOp2Ldmia:
            case kMemOp2Stmia2:
            case kMemOp2Ldmia2:
                size = kSVVariable;
                break;
        }

        // Load the value of the address
        addr = selfVerificationMemRegLoad(sp, rn);

        // Figure out the offset
        switch (opcode12) {
            case kMemOp2Vstr:
            case kMemOp2Vstr2:
            case kMemOp2Vldr:
            case kMemOp2Vldr2:
                offset = imm8 << 2;
                if (opcode4 == kMemOp2Single) {
                    rt = rd << 1;
                    if (insn & 0x400000) rt |= 0x1;
                } else if (opcode4 == kMemOp2Double) {
                    if (insn & 0x400000) rt |= 0x10;
                    rt = rt << 1;
                } else {
                    ALOGE("*** ERROR: UNRECOGNIZED VECTOR MEM OP: %x", opcode4);
                    dvmAbort();
                }
                rt += 14;
                break;
            case kMemOp2StrbRRR:
            case kMemOp2LdrbRRR:
            case kMemOp2StrhRRR:
            case kMemOp2LdrhRRR:
            case kMemOp2StrRRR:
            case kMemOp2LdrRRR:
            case kMemOp2LdrsbRRR:
            case kMemOp2LdrshRRR:
                offset = selfVerificationMemRegLoad(sp, rm) << imm2;
                break;
            case kMemOp2StrbRRI12:
            case kMemOp2LdrbRRI12:
            case kMemOp2StrhRRI12:
            case kMemOp2LdrhRRI12:
            case kMemOp2StrRRI12:
            case kMemOp2LdrRRI12:
            case kMemOp2LdrsbRRI12:
            case kMemOp2LdrshRRI12:
                offset = imm12;
                break;
            case kMemOp2Stmia:
            case kMemOp2Ldmia:
                wBack = false;
            case kMemOp2Stmia2:
            case kMemOp2Ldmia2:
                offset = 0;
                break;
            default:
                ALOGE("*** ERROR: UNRECOGNIZED THUMB2 MEM OP: %x", opcode12);
                offset = 0;
                dvmAbort();
        }

        // Handle the decoded mem op accordingly
        if (store) {
            if (size == kSVVariable) {
                ALOGD("*** THUMB2 STMIA CURRENTLY UNUSED (AND UNTESTED)");
                int i;
                int regList = insn & 0xFFFF;
                for (i = 0; i < 16; i++) {
                    if (regList & 0x1) {
                        data = selfVerificationMemRegLoad(sp, i);
                        selfVerificationStore(addr, data, kSVWord);
                        addr += 4;
                    }
                    regList = regList >> 1;
                }
                if (wBack) selfVerificationMemRegStore(sp, addr, rn);
            } else if (size == kSVDoubleword) {
                double_data = selfVerificationMemRegLoadDouble(sp, rt);
                selfVerificationStoreDoubleword(addr+offset, double_data);
            } else {
                data = selfVerificationMemRegLoad(sp, rt);
                selfVerificationStore(addr+offset, data, size);
            }
        } else {
            if (size == kSVVariable) {
                ALOGD("*** THUMB2 LDMIA CURRENTLY UNUSED (AND UNTESTED)");
                int i;
                int regList = insn & 0xFFFF;
                for (i = 0; i < 16; i++) {
                    if (regList & 0x1) {
                        data = selfVerificationLoad(addr, kSVWord);
                        selfVerificationMemRegStore(sp, data, i);
                        addr += 4;
                    }
                    regList = regList >> 1;
                }
                if (wBack) selfVerificationMemRegStore(sp, addr, rn);
            } else if (size == kSVDoubleword) {
                double_data = selfVerificationLoadDoubleword(addr+offset);
                selfVerificationMemRegStoreDouble(sp, double_data, rt);
            } else {
                data = selfVerificationLoad(addr+offset, size);
                selfVerificationMemRegStore(sp, data, rt);
            }
        }
    } else {
        //ALOGD("*** THUMB - Addr: %#x Insn: %#x", lr, insn);

        // Update the link register
        selfVerificationMemRegStore(sp, old_lr+2, 13);

        int opcode5 = (insn >> 11) & 0x1F;
        int opcode7 = (insn >> 9) & 0x7F;
        int imm = (insn >> 6) & 0x1F;
        int rd = (insn >> 8) & 0x7;
        int rm = (insn >> 6) & 0x7;
        int rn = (insn >> 3) & 0x7;
        int rt = insn & 0x7;

        // Determine whether the mem op is a store or load
        switch (opcode5) {
            case kMemOpRRR:
                switch (opcode7) {
                    case kMemOpStrRRR:
                    case kMemOpStrhRRR:
                    case kMemOpStrbRRR:
                        store = true;
                }
                break;
            case kMemOpStrRRI5:
            case kMemOpStrbRRI5:
            case kMemOpStrhRRI5:
            case kMemOpStmia:
                store = true;
        }

        // Determine the size of the mem access
        switch (opcode5) {
            case kMemOpRRR:
            case kMemOpRRR2:
                switch (opcode7) {
                    case kMemOpStrbRRR:
                    case kMemOpLdrbRRR:
                        size = kSVByte;
                        break;
                    case kMemOpLdrsbRRR:
                        size = kSVSignedByte;
                        break;
                    case kMemOpStrhRRR:
                    case kMemOpLdrhRRR:
                        size = kSVHalfword;
                        break;
                    case kMemOpLdrshRRR:
                        size = kSVSignedHalfword;
                        break;
                }
                break;
            case kMemOpStrbRRI5:
            case kMemOpLdrbRRI5:
                size = kSVByte;
                break;
            case kMemOpStrhRRI5:
            case kMemOpLdrhRRI5:
                size = kSVHalfword;
                break;
            case kMemOpStmia:
            case kMemOpLdmia:
                size = kSVVariable;
                break;
        }

        // Load the value of the address
        if (opcode5 == kMemOpLdrPcRel)
            addr = selfVerificationMemRegLoad(sp, 4);
        else if (opcode5 == kMemOpStmia || opcode5 == kMemOpLdmia)
            addr = selfVerificationMemRegLoad(sp, rd);
        else
            addr = selfVerificationMemRegLoad(sp, rn);

        // Figure out the offset
        switch (opcode5) {
            case kMemOpLdrPcRel:
                offset = (insn & 0xFF) << 2;
                rt = rd;
                break;
            case kMemOpRRR:
            case kMemOpRRR2:
                offset = selfVerificationMemRegLoad(sp, rm);
                break;
            case kMemOpStrRRI5:
            case kMemOpLdrRRI5:
                offset = imm << 2;
                break;
            case kMemOpStrhRRI5:
            case kMemOpLdrhRRI5:
                offset = imm << 1;
                break;
            case kMemOpStrbRRI5:
            case kMemOpLdrbRRI5:
                offset = imm;
                break;
            case kMemOpStmia:
            case kMemOpLdmia:
                offset = 0;
                break;
            default:
                ALOGE("*** ERROR: UNRECOGNIZED THUMB MEM OP: %x", opcode5);
                offset = 0;
                dvmAbort();
        }

        // Handle the decoded mem op accordingly
        if (store) {
            if (size == kSVVariable) {
                int i;
                int regList = insn & 0xFF;
                for (i = 0; i < 8; i++) {
                    if (regList & 0x1) {
                        data = selfVerificationMemRegLoad(sp, i);
                        selfVerificationStore(addr, data, kSVWord);
                        addr += 4;
                    }
                    regList = regList >> 1;
                }
                selfVerificationMemRegStore(sp, addr, rd);
            } else {
                data = selfVerificationMemRegLoad(sp, rt);
                selfVerificationStore(addr+offset, data, size);
            }
        } else {
            if (size == kSVVariable) {
                bool wBack = true;
                int i;
                int regList = insn & 0xFF;
                for (i = 0; i < 8; i++) {
                    if (regList & 0x1) {
                        if (i == rd) wBack = false;
                        data = selfVerificationLoad(addr, kSVWord);
                        selfVerificationMemRegStore(sp, data, i);
                        addr += 4;
                    }
                    regList = regList >> 1;
                }
                if (wBack) selfVerificationMemRegStore(sp, addr, rd);
            } else {
                data = selfVerificationLoad(addr+offset, size);
                selfVerificationMemRegStore(sp, data, rt);
            }
        }
    }
}
#endif
