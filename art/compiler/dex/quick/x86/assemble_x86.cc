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

#include "codegen_x86.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "x86_lir.h"

namespace art {

#define MAX_ASSEMBLER_RETRIES 50

const X86EncodingMap X86Mir2Lir::EncodingMap[kX86Last] = {
  { kX8632BitData, kData,    IS_UNARY_OP,            { 0, 0, 0x00, 0, 0, 0, 0, 4 }, "data",  "0x!0d" },
  { kX86Bkpt,      kNullary, NO_OPERAND | IS_BRANCH, { 0, 0, 0xCC, 0, 0, 0, 0, 0 }, "int 3", "" },
  { kX86Nop,       kNop,     IS_UNARY_OP,            { 0, 0, 0x90, 0, 0, 0, 0, 0 }, "nop",   "" },

#define ENCODING_MAP(opname, mem_use, reg_def, uses_ccodes, \
                     rm8_r8, rm32_r32, \
                     r8_rm8, r32_rm32, \
                     ax8_i8, ax32_i32, \
                     rm8_i8, rm8_i8_modrm, \
                     rm32_i32, rm32_i32_modrm, \
                     rm32_i8, rm32_i8_modrm) \
{ kX86 ## opname ## 8MR, kMemReg,    mem_use | IS_TERTIARY_OP |           REG_USE02  | SETS_CCODES | uses_ccodes, { 0,             0, rm8_r8, 0, 0, 0,            0,      0 }, #opname "8MR", "[!0r+!1d],!2r" }, \
{ kX86 ## opname ## 8AR, kArrayReg,  mem_use | IS_QUIN_OP     |           REG_USE014 | SETS_CCODES | uses_ccodes, { 0,             0, rm8_r8, 0, 0, 0,            0,      0 }, #opname "8AR", "[!0r+!1r<<!2d+!3d],!4r" }, \
{ kX86 ## opname ## 8TR, kThreadReg, mem_use | IS_BINARY_OP   |           REG_USE1   | SETS_CCODES | uses_ccodes, { THREAD_PREFIX, 0, rm8_r8, 0, 0, 0,            0,      0 }, #opname "8TR", "fs:[!0d],!1r" }, \
{ kX86 ## opname ## 8RR, kRegReg,              IS_BINARY_OP   | reg_def | REG_USE01  | SETS_CCODES | uses_ccodes, { 0,             0, r8_rm8, 0, 0, 0,            0,      0 }, #opname "8RR", "!0r,!1r" }, \
{ kX86 ## opname ## 8RM, kRegMem,    IS_LOAD | IS_TERTIARY_OP | reg_def | REG_USE01  | SETS_CCODES | uses_ccodes, { 0,             0, r8_rm8, 0, 0, 0,            0,      0 }, #opname "8RM", "!0r,[!1r+!2d]" }, \
{ kX86 ## opname ## 8RA, kRegArray,  IS_LOAD | IS_QUIN_OP     | reg_def | REG_USE012 | SETS_CCODES | uses_ccodes, { 0,             0, r8_rm8, 0, 0, 0,            0,      0 }, #opname "8RA", "!0r,[!1r+!2r<<!3d+!4d]" }, \
{ kX86 ## opname ## 8RT, kRegThread, IS_LOAD | IS_BINARY_OP   | reg_def | REG_USE0   | SETS_CCODES | uses_ccodes, { THREAD_PREFIX, 0, r8_rm8, 0, 0, 0,            0,      0 }, #opname "8RT", "!0r,fs:[!1d]" }, \
{ kX86 ## opname ## 8RI, kRegImm,              IS_BINARY_OP   | reg_def | REG_USE0   | SETS_CCODES | uses_ccodes, { 0,             0, rm8_i8, 0, 0, rm8_i8_modrm, ax8_i8, 1 }, #opname "8RI", "!0r,!1d" }, \
{ kX86 ## opname ## 8MI, kMemImm,    mem_use | IS_TERTIARY_OP |           REG_USE0   | SETS_CCODES | uses_ccodes, { 0,             0, rm8_i8, 0, 0, rm8_i8_modrm, 0,      1 }, #opname "8MI", "[!0r+!1d],!2d" }, \
{ kX86 ## opname ## 8AI, kArrayImm,  mem_use | IS_QUIN_OP     |           REG_USE01  | SETS_CCODES | uses_ccodes, { 0,             0, rm8_i8, 0, 0, rm8_i8_modrm, 0,      1 }, #opname "8AI", "[!0r+!1r<<!2d+!3d],!4d" }, \
{ kX86 ## opname ## 8TI, kThreadImm, mem_use | IS_BINARY_OP   |                        SETS_CCODES | uses_ccodes, { THREAD_PREFIX, 0, rm8_i8, 0, 0, rm8_i8_modrm, 0,      1 }, #opname "8TI", "fs:[!0d],!1d" }, \
  \
{ kX86 ## opname ## 16MR,  kMemReg,    mem_use | IS_TERTIARY_OP |           REG_USE02  | SETS_CCODES | uses_ccodes, { 0x66,          0,    rm32_r32, 0, 0, 0,              0,        0 }, #opname "16MR", "[!0r+!1d],!2r" }, \
{ kX86 ## opname ## 16AR,  kArrayReg,  mem_use | IS_QUIN_OP     |           REG_USE014 | SETS_CCODES | uses_ccodes, { 0x66,          0,    rm32_r32, 0, 0, 0,              0,        0 }, #opname "16AR", "[!0r+!1r<<!2d+!3d],!4r" }, \
{ kX86 ## opname ## 16TR,  kThreadReg, mem_use | IS_BINARY_OP   |           REG_USE1   | SETS_CCODES | uses_ccodes, { THREAD_PREFIX, 0x66, rm32_r32, 0, 0, 0,              0,        0 }, #opname "16TR", "fs:[!0d],!1r" }, \
{ kX86 ## opname ## 16RR,  kRegReg,              IS_BINARY_OP   | reg_def | REG_USE01  | SETS_CCODES | uses_ccodes, { 0x66,          0,    r32_rm32, 0, 0, 0,              0,        0 }, #opname "16RR", "!0r,!1r" }, \
{ kX86 ## opname ## 16RM,  kRegMem,    IS_LOAD | IS_TERTIARY_OP | reg_def | REG_USE01  | SETS_CCODES | uses_ccodes, { 0x66,          0,    r32_rm32, 0, 0, 0,              0,        0 }, #opname "16RM", "!0r,[!1r+!2d]" }, \
{ kX86 ## opname ## 16RA,  kRegArray,  IS_LOAD | IS_QUIN_OP     | reg_def | REG_USE012 | SETS_CCODES | uses_ccodes, { 0x66,          0,    r32_rm32, 0, 0, 0,              0,        0 }, #opname "16RA", "!0r,[!1r+!2r<<!3d+!4d]" }, \
{ kX86 ## opname ## 16RT,  kRegThread, IS_LOAD | IS_BINARY_OP   | reg_def | REG_USE0   | SETS_CCODES | uses_ccodes, { THREAD_PREFIX, 0x66, r32_rm32, 0, 0, 0,              0,        0 }, #opname "16RT", "!0r,fs:[!1d]" }, \
{ kX86 ## opname ## 16RI,  kRegImm,              IS_BINARY_OP   | reg_def | REG_USE0   | SETS_CCODES | uses_ccodes, { 0x66,          0,    rm32_i32, 0, 0, rm32_i32_modrm, ax32_i32, 2 }, #opname "16RI", "!0r,!1d" }, \
{ kX86 ## opname ## 16MI,  kMemImm,    mem_use | IS_TERTIARY_OP |           REG_USE0   | SETS_CCODES | uses_ccodes, { 0x66,          0,    rm32_i32, 0, 0, rm32_i32_modrm, 0,        2 }, #opname "16MI", "[!0r+!1d],!2d" }, \
{ kX86 ## opname ## 16AI,  kArrayImm,  mem_use | IS_QUIN_OP     |           REG_USE01  | SETS_CCODES | uses_ccodes, { 0x66,          0,    rm32_i32, 0, 0, rm32_i32_modrm, 0,        2 }, #opname "16AI", "[!0r+!1r<<!2d+!3d],!4d" }, \
{ kX86 ## opname ## 16TI,  kThreadImm, mem_use | IS_BINARY_OP   |                        SETS_CCODES | uses_ccodes, { THREAD_PREFIX, 0x66, rm32_i32, 0, 0, rm32_i32_modrm, 0,        2 }, #opname "16TI", "fs:[!0d],!1d" }, \
{ kX86 ## opname ## 16RI8, kRegImm,              IS_BINARY_OP   | reg_def | REG_USE0   | SETS_CCODES | uses_ccodes, { 0x66,          0,    rm32_i8,  0, 0, rm32_i8_modrm,  0,        1 }, #opname "16RI8", "!0r,!1d" }, \
{ kX86 ## opname ## 16MI8, kMemImm,    mem_use | IS_TERTIARY_OP |           REG_USE0   | SETS_CCODES | uses_ccodes, { 0x66,          0,    rm32_i8,  0, 0, rm32_i8_modrm,  0,        1 }, #opname "16MI8", "[!0r+!1d],!2d" }, \
{ kX86 ## opname ## 16AI8, kArrayImm,  mem_use | IS_QUIN_OP     |           REG_USE01  | SETS_CCODES | uses_ccodes, { 0x66,          0,    rm32_i8,  0, 0, rm32_i8_modrm,  0,        1 }, #opname "16AI8", "[!0r+!1r<<!2d+!3d],!4d" }, \
{ kX86 ## opname ## 16TI8, kThreadImm, mem_use | IS_BINARY_OP   |                        SETS_CCODES | uses_ccodes, { THREAD_PREFIX, 0x66, rm32_i8,  0, 0, rm32_i8_modrm,  0,        1 }, #opname "16TI8", "fs:[!0d],!1d" }, \
  \
{ kX86 ## opname ## 32MR,  kMemReg,    mem_use | IS_TERTIARY_OP |           REG_USE02  | SETS_CCODES | uses_ccodes, { 0,             0, rm32_r32, 0, 0, 0,              0,        0 }, #opname "32MR", "[!0r+!1d],!2r" }, \
{ kX86 ## opname ## 32AR,  kArrayReg,  mem_use | IS_QUIN_OP     |           REG_USE014 | SETS_CCODES | uses_ccodes, { 0,             0, rm32_r32, 0, 0, 0,              0,        0 }, #opname "32AR", "[!0r+!1r<<!2d+!3d],!4r" }, \
{ kX86 ## opname ## 32TR,  kThreadReg, mem_use | IS_BINARY_OP   |           REG_USE1   | SETS_CCODES | uses_ccodes, { THREAD_PREFIX, 0, rm32_r32, 0, 0, 0,              0,        0 }, #opname "32TR", "fs:[!0d],!1r" }, \
{ kX86 ## opname ## 32RR,  kRegReg,              IS_BINARY_OP   | reg_def | REG_USE01  | SETS_CCODES | uses_ccodes, { 0,             0, r32_rm32, 0, 0, 0,              0,        0 }, #opname "32RR", "!0r,!1r" }, \
{ kX86 ## opname ## 32RM,  kRegMem,    IS_LOAD | IS_TERTIARY_OP | reg_def | REG_USE01  | SETS_CCODES | uses_ccodes, { 0,             0, r32_rm32, 0, 0, 0,              0,        0 }, #opname "32RM", "!0r,[!1r+!2d]" }, \
{ kX86 ## opname ## 32RA,  kRegArray,  IS_LOAD | IS_QUIN_OP     | reg_def | REG_USE012 | SETS_CCODES | uses_ccodes, { 0,             0, r32_rm32, 0, 0, 0,              0,        0 }, #opname "32RA", "!0r,[!1r+!2r<<!3d+!4d]" }, \
{ kX86 ## opname ## 32RT,  kRegThread, IS_LOAD | IS_BINARY_OP   | reg_def | REG_USE0   | SETS_CCODES | uses_ccodes, { THREAD_PREFIX, 0, r32_rm32, 0, 0, 0,              0,        0 }, #opname "32RT", "!0r,fs:[!1d]" }, \
{ kX86 ## opname ## 32RI,  kRegImm,              IS_BINARY_OP   | reg_def | REG_USE0   | SETS_CCODES | uses_ccodes, { 0,             0, rm32_i32, 0, 0, rm32_i32_modrm, ax32_i32, 4 }, #opname "32RI", "!0r,!1d" }, \
{ kX86 ## opname ## 32MI,  kMemImm,    mem_use | IS_TERTIARY_OP |           REG_USE0   | SETS_CCODES | uses_ccodes, { 0,             0, rm32_i32, 0, 0, rm32_i32_modrm, 0,        4 }, #opname "32MI", "[!0r+!1d],!2d" }, \
{ kX86 ## opname ## 32AI,  kArrayImm,  mem_use | IS_QUIN_OP     |           REG_USE01  | SETS_CCODES | uses_ccodes, { 0,             0, rm32_i32, 0, 0, rm32_i32_modrm, 0,        4 }, #opname "32AI", "[!0r+!1r<<!2d+!3d],!4d" }, \
{ kX86 ## opname ## 32TI,  kThreadImm, mem_use | IS_BINARY_OP   |                        SETS_CCODES | uses_ccodes, { THREAD_PREFIX, 0, rm32_i32, 0, 0, rm32_i32_modrm, 0,        4 }, #opname "32TI", "fs:[!0d],!1d" }, \
{ kX86 ## opname ## 32RI8, kRegImm,              IS_BINARY_OP   | reg_def | REG_USE0   | SETS_CCODES | uses_ccodes, { 0,             0, rm32_i8,  0, 0, rm32_i8_modrm,  0,        1 }, #opname "32RI8", "!0r,!1d" }, \
{ kX86 ## opname ## 32MI8, kMemImm,    mem_use | IS_TERTIARY_OP |           REG_USE0   | SETS_CCODES | uses_ccodes, { 0,             0, rm32_i8,  0, 0, rm32_i8_modrm,  0,        1 }, #opname "32MI8", "[!0r+!1d],!2d" }, \
{ kX86 ## opname ## 32AI8, kArrayImm,  mem_use | IS_QUIN_OP     |           REG_USE01  | SETS_CCODES | uses_ccodes, { 0,             0, rm32_i8,  0, 0, rm32_i8_modrm,  0,        1 }, #opname "32AI8", "[!0r+!1r<<!2d+!3d],!4d" }, \
{ kX86 ## opname ## 32TI8, kThreadImm, mem_use | IS_BINARY_OP   |                        SETS_CCODES | uses_ccodes, { THREAD_PREFIX, 0, rm32_i8,  0, 0, rm32_i8_modrm,  0,        1 }, #opname "32TI8", "fs:[!0d],!1d" }

ENCODING_MAP(Add, IS_LOAD | IS_STORE, REG_DEF0, 0,
  0x00 /* RegMem8/Reg8 */,     0x01 /* RegMem32/Reg32 */,
  0x02 /* Reg8/RegMem8 */,     0x03 /* Reg32/RegMem32 */,
  0x04 /* Rax8/imm8 opcode */, 0x05 /* Rax32/imm32 */,
  0x80, 0x0 /* RegMem8/imm8 */,
  0x81, 0x0 /* RegMem32/imm32 */, 0x83, 0x0 /* RegMem32/imm8 */),
ENCODING_MAP(Or, IS_LOAD | IS_STORE, REG_DEF0, 0,
  0x08 /* RegMem8/Reg8 */,     0x09 /* RegMem32/Reg32 */,
  0x0A /* Reg8/RegMem8 */,     0x0B /* Reg32/RegMem32 */,
  0x0C /* Rax8/imm8 opcode */, 0x0D /* Rax32/imm32 */,
  0x80, 0x1 /* RegMem8/imm8 */,
  0x81, 0x1 /* RegMem32/imm32 */, 0x83, 0x1 /* RegMem32/imm8 */),
ENCODING_MAP(Adc, IS_LOAD | IS_STORE, REG_DEF0, USES_CCODES,
  0x10 /* RegMem8/Reg8 */,     0x11 /* RegMem32/Reg32 */,
  0x12 /* Reg8/RegMem8 */,     0x13 /* Reg32/RegMem32 */,
  0x14 /* Rax8/imm8 opcode */, 0x15 /* Rax32/imm32 */,
  0x80, 0x2 /* RegMem8/imm8 */,
  0x81, 0x2 /* RegMem32/imm32 */, 0x83, 0x2 /* RegMem32/imm8 */),
ENCODING_MAP(Sbb, IS_LOAD | IS_STORE, REG_DEF0, USES_CCODES,
  0x18 /* RegMem8/Reg8 */,     0x19 /* RegMem32/Reg32 */,
  0x1A /* Reg8/RegMem8 */,     0x1B /* Reg32/RegMem32 */,
  0x1C /* Rax8/imm8 opcode */, 0x1D /* Rax32/imm32 */,
  0x80, 0x3 /* RegMem8/imm8 */,
  0x81, 0x3 /* RegMem32/imm32 */, 0x83, 0x3 /* RegMem32/imm8 */),
ENCODING_MAP(And, IS_LOAD | IS_STORE, REG_DEF0, 0,
  0x20 /* RegMem8/Reg8 */,     0x21 /* RegMem32/Reg32 */,
  0x22 /* Reg8/RegMem8 */,     0x23 /* Reg32/RegMem32 */,
  0x24 /* Rax8/imm8 opcode */, 0x25 /* Rax32/imm32 */,
  0x80, 0x4 /* RegMem8/imm8 */,
  0x81, 0x4 /* RegMem32/imm32 */, 0x83, 0x4 /* RegMem32/imm8 */),
ENCODING_MAP(Sub, IS_LOAD | IS_STORE, REG_DEF0, 0,
  0x28 /* RegMem8/Reg8 */,     0x29 /* RegMem32/Reg32 */,
  0x2A /* Reg8/RegMem8 */,     0x2B /* Reg32/RegMem32 */,
  0x2C /* Rax8/imm8 opcode */, 0x2D /* Rax32/imm32 */,
  0x80, 0x5 /* RegMem8/imm8 */,
  0x81, 0x5 /* RegMem32/imm32 */, 0x83, 0x5 /* RegMem32/imm8 */),
ENCODING_MAP(Xor, IS_LOAD | IS_STORE, REG_DEF0, 0,
  0x30 /* RegMem8/Reg8 */,     0x31 /* RegMem32/Reg32 */,
  0x32 /* Reg8/RegMem8 */,     0x33 /* Reg32/RegMem32 */,
  0x34 /* Rax8/imm8 opcode */, 0x35 /* Rax32/imm32 */,
  0x80, 0x6 /* RegMem8/imm8 */,
  0x81, 0x6 /* RegMem32/imm32 */, 0x83, 0x6 /* RegMem32/imm8 */),
ENCODING_MAP(Cmp, IS_LOAD, 0, 0,
  0x38 /* RegMem8/Reg8 */,     0x39 /* RegMem32/Reg32 */,
  0x3A /* Reg8/RegMem8 */,     0x3B /* Reg32/RegMem32 */,
  0x3C /* Rax8/imm8 opcode */, 0x3D /* Rax32/imm32 */,
  0x80, 0x7 /* RegMem8/imm8 */,
  0x81, 0x7 /* RegMem32/imm32 */, 0x83, 0x7 /* RegMem32/imm8 */),
#undef ENCODING_MAP

  { kX86Imul16RRI,   kRegRegImm,             IS_TERTIARY_OP | REG_DEF0_USE1  | SETS_CCODES, { 0x66, 0, 0x69, 0, 0, 0, 0, 2 }, "Imul16RRI", "!0r,!1r,!2d" },
  { kX86Imul16RMI,   kRegMemImm,   IS_LOAD | IS_QUAD_OP     | REG_DEF0_USE1  | SETS_CCODES, { 0x66, 0, 0x69, 0, 0, 0, 0, 2 }, "Imul16RMI", "!0r,[!1r+!2d],!3d" },
  { kX86Imul16RAI,   kRegArrayImm, IS_LOAD | IS_SEXTUPLE_OP | REG_DEF0_USE12 | SETS_CCODES, { 0x66, 0, 0x69, 0, 0, 0, 0, 2 }, "Imul16RAI", "!0r,[!1r+!2r<<!3d+!4d],!5d" },

  { kX86Imul32RRI,   kRegRegImm,             IS_TERTIARY_OP | REG_DEF0_USE1  | SETS_CCODES, { 0, 0, 0x69, 0, 0, 0, 0, 4 }, "Imul32RRI", "!0r,!1r,!2d" },
  { kX86Imul32RMI,   kRegMemImm,   IS_LOAD | IS_QUAD_OP     | REG_DEF0_USE1  | SETS_CCODES, { 0, 0, 0x69, 0, 0, 0, 0, 4 }, "Imul32RMI", "!0r,[!1r+!2d],!3d" },
  { kX86Imul32RAI,   kRegArrayImm, IS_LOAD | IS_SEXTUPLE_OP | REG_DEF0_USE12 | SETS_CCODES, { 0, 0, 0x69, 0, 0, 0, 0, 4 }, "Imul32RAI", "!0r,[!1r+!2r<<!3d+!4d],!5d" },
  { kX86Imul32RRI8,  kRegRegImm,             IS_TERTIARY_OP | REG_DEF0_USE1  | SETS_CCODES, { 0, 0, 0x6B, 0, 0, 0, 0, 1 }, "Imul32RRI8", "!0r,!1r,!2d" },
  { kX86Imul32RMI8,  kRegMemImm,   IS_LOAD | IS_QUAD_OP     | REG_DEF0_USE1  | SETS_CCODES, { 0, 0, 0x6B, 0, 0, 0, 0, 1 }, "Imul32RMI8", "!0r,[!1r+!2d],!3d" },
  { kX86Imul32RAI8,  kRegArrayImm, IS_LOAD | IS_SEXTUPLE_OP | REG_DEF0_USE12 | SETS_CCODES, { 0, 0, 0x6B, 0, 0, 0, 0, 1 }, "Imul32RAI8", "!0r,[!1r+!2r<<!3d+!4d],!5d" },

  { kX86Mov8MR, kMemReg,    IS_STORE | IS_TERTIARY_OP | REG_USE02,      { 0,             0, 0x88, 0, 0, 0, 0, 0 }, "Mov8MR", "[!0r+!1d],!2r" },
  { kX86Mov8AR, kArrayReg,  IS_STORE | IS_QUIN_OP     | REG_USE014,     { 0,             0, 0x88, 0, 0, 0, 0, 0 }, "Mov8AR", "[!0r+!1r<<!2d+!3d],!4r" },
  { kX86Mov8TR, kThreadReg, IS_STORE | IS_BINARY_OP   | REG_USE1,       { THREAD_PREFIX, 0, 0x88, 0, 0, 0, 0, 0 }, "Mov8TR", "fs:[!0d],!1r" },
  { kX86Mov8RR, kRegReg,               IS_BINARY_OP   | REG_DEF0_USE1,  { 0,             0, 0x8A, 0, 0, 0, 0, 0 }, "Mov8RR", "!0r,!1r" },
  { kX86Mov8RM, kRegMem,    IS_LOAD  | IS_TERTIARY_OP | REG_DEF0_USE1,  { 0,             0, 0x8A, 0, 0, 0, 0, 0 }, "Mov8RM", "!0r,[!1r+!2d]" },
  { kX86Mov8RA, kRegArray,  IS_LOAD  | IS_QUIN_OP     | REG_DEF0_USE12, { 0,             0, 0x8A, 0, 0, 0, 0, 0 }, "Mov8RA", "!0r,[!1r+!2r<<!3d+!4d]" },
  { kX86Mov8RT, kRegThread, IS_LOAD  | IS_BINARY_OP   | REG_DEF0,       { THREAD_PREFIX, 0, 0x8A, 0, 0, 0, 0, 0 }, "Mov8RT", "!0r,fs:[!1d]" },
  { kX86Mov8RI, kMovRegImm,            IS_BINARY_OP   | REG_DEF0,       { 0,             0, 0xB0, 0, 0, 0, 0, 1 }, "Mov8RI", "!0r,!1d" },
  { kX86Mov8MI, kMemImm,    IS_STORE | IS_TERTIARY_OP | REG_USE0,       { 0,             0, 0xC6, 0, 0, 0, 0, 1 }, "Mov8MI", "[!0r+!1d],!2d" },
  { kX86Mov8AI, kArrayImm,  IS_STORE | IS_QUIN_OP     | REG_USE01,      { 0,             0, 0xC6, 0, 0, 0, 0, 1 }, "Mov8AI", "[!0r+!1r<<!2d+!3d],!4d" },
  { kX86Mov8TI, kThreadImm, IS_STORE | IS_BINARY_OP,                    { THREAD_PREFIX, 0, 0xC6, 0, 0, 0, 0, 1 }, "Mov8TI", "fs:[!0d],!1d" },

  { kX86Mov16MR, kMemReg,    IS_STORE | IS_TERTIARY_OP | REG_USE02,      { 0x66,          0,    0x89, 0, 0, 0, 0, 0 }, "Mov16MR", "[!0r+!1d],!2r" },
  { kX86Mov16AR, kArrayReg,  IS_STORE | IS_QUIN_OP     | REG_USE014,     { 0x66,          0,    0x89, 0, 0, 0, 0, 0 }, "Mov16AR", "[!0r+!1r<<!2d+!3d],!4r" },
  { kX86Mov16TR, kThreadReg, IS_STORE | IS_BINARY_OP   | REG_USE1,       { THREAD_PREFIX, 0x66, 0x89, 0, 0, 0, 0, 0 }, "Mov16TR", "fs:[!0d],!1r" },
  { kX86Mov16RR, kRegReg,               IS_BINARY_OP   | REG_DEF0_USE1,  { 0x66,          0,    0x8B, 0, 0, 0, 0, 0 }, "Mov16RR", "!0r,!1r" },
  { kX86Mov16RM, kRegMem,    IS_LOAD  | IS_TERTIARY_OP | REG_DEF0_USE1,  { 0x66,          0,    0x8B, 0, 0, 0, 0, 0 }, "Mov16RM", "!0r,[!1r+!2d]" },
  { kX86Mov16RA, kRegArray,  IS_LOAD  | IS_QUIN_OP     | REG_DEF0_USE12, { 0x66,          0,    0x8B, 0, 0, 0, 0, 0 }, "Mov16RA", "!0r,[!1r+!2r<<!3d+!4d]" },
  { kX86Mov16RT, kRegThread, IS_LOAD  | IS_BINARY_OP   | REG_DEF0,       { THREAD_PREFIX, 0x66, 0x8B, 0, 0, 0, 0, 0 }, "Mov16RT", "!0r,fs:[!1d]" },
  { kX86Mov16RI, kMovRegImm,            IS_BINARY_OP   | REG_DEF0,       { 0x66,          0,    0xB8, 0, 0, 0, 0, 2 }, "Mov16RI", "!0r,!1d" },
  { kX86Mov16MI, kMemImm,    IS_STORE | IS_TERTIARY_OP | REG_USE0,       { 0x66,          0,    0xC7, 0, 0, 0, 0, 2 }, "Mov16MI", "[!0r+!1d],!2d" },
  { kX86Mov16AI, kArrayImm,  IS_STORE | IS_QUIN_OP     | REG_USE01,      { 0x66,          0,    0xC7, 0, 0, 0, 0, 2 }, "Mov16AI", "[!0r+!1r<<!2d+!3d],!4d" },
  { kX86Mov16TI, kThreadImm, IS_STORE | IS_BINARY_OP,                    { THREAD_PREFIX, 0x66, 0xC7, 0, 0, 0, 0, 2 }, "Mov16TI", "fs:[!0d],!1d" },

  { kX86Mov32MR, kMemReg,    IS_STORE | IS_TERTIARY_OP | REG_USE02,      { 0,             0, 0x89, 0, 0, 0, 0, 0 }, "Mov32MR", "[!0r+!1d],!2r" },
  { kX86Mov32AR, kArrayReg,  IS_STORE | IS_QUIN_OP     | REG_USE014,     { 0,             0, 0x89, 0, 0, 0, 0, 0 }, "Mov32AR", "[!0r+!1r<<!2d+!3d],!4r" },
  { kX86Mov32TR, kThreadReg, IS_STORE | IS_BINARY_OP   | REG_USE1,       { THREAD_PREFIX, 0, 0x89, 0, 0, 0, 0, 0 }, "Mov32TR", "fs:[!0d],!1r" },
  { kX86Mov32RR, kRegReg,               IS_BINARY_OP   | REG_DEF0_USE1,  { 0,             0, 0x8B, 0, 0, 0, 0, 0 }, "Mov32RR", "!0r,!1r" },
  { kX86Mov32RM, kRegMem,    IS_LOAD  | IS_TERTIARY_OP | REG_DEF0_USE1,  { 0,             0, 0x8B, 0, 0, 0, 0, 0 }, "Mov32RM", "!0r,[!1r+!2d]" },
  { kX86Mov32RA, kRegArray,  IS_LOAD  | IS_QUIN_OP     | REG_DEF0_USE12, { 0,             0, 0x8B, 0, 0, 0, 0, 0 }, "Mov32RA", "!0r,[!1r+!2r<<!3d+!4d]" },
  { kX86Mov32RT, kRegThread, IS_LOAD  | IS_BINARY_OP   | REG_DEF0,       { THREAD_PREFIX, 0, 0x8B, 0, 0, 0, 0, 0 }, "Mov32RT", "!0r,fs:[!1d]" },
  { kX86Mov32RI, kMovRegImm,            IS_BINARY_OP   | REG_DEF0,       { 0,             0, 0xB8, 0, 0, 0, 0, 4 }, "Mov32RI", "!0r,!1d" },
  { kX86Mov32MI, kMemImm,    IS_STORE | IS_TERTIARY_OP | REG_USE0,       { 0,             0, 0xC7, 0, 0, 0, 0, 4 }, "Mov32MI", "[!0r+!1d],!2d" },
  { kX86Mov32AI, kArrayImm,  IS_STORE | IS_QUIN_OP     | REG_USE01,      { 0,             0, 0xC7, 0, 0, 0, 0, 4 }, "Mov32AI", "[!0r+!1r<<!2d+!3d],!4d" },
  { kX86Mov32TI, kThreadImm, IS_STORE | IS_BINARY_OP,                    { THREAD_PREFIX, 0, 0xC7, 0, 0, 0, 0, 4 }, "Mov32TI", "fs:[!0d],!1d" },

  { kX86Lea32RA, kRegArray, IS_QUIN_OP | REG_DEF0_USE12, { 0, 0, 0x8D, 0, 0, 0, 0, 0 }, "Lea32RA", "!0r,[!1r+!2r<<!3d+!4d]" },

#define SHIFT_ENCODING_MAP(opname, modrm_opcode) \
{ kX86 ## opname ## 8RI, kShiftRegImm,                        IS_BINARY_OP   | REG_DEF0_USE0 |            SETS_CCODES, { 0,    0, 0xC0, 0, 0, modrm_opcode, 0xD1, 1 }, #opname "8RI", "!0r,!1d" }, \
{ kX86 ## opname ## 8MI, kShiftMemImm,   IS_LOAD | IS_STORE | IS_TERTIARY_OP | REG_USE0      |            SETS_CCODES, { 0,    0, 0xC0, 0, 0, modrm_opcode, 0xD1, 1 }, #opname "8MI", "[!0r+!1d],!2d" }, \
{ kX86 ## opname ## 8AI, kShiftArrayImm, IS_LOAD | IS_STORE | IS_QUIN_OP     | REG_USE01     |            SETS_CCODES, { 0,    0, 0xC0, 0, 0, modrm_opcode, 0xD1, 1 }, #opname "8AI", "[!0r+!1r<<!2d+!3d],!4d" }, \
{ kX86 ## opname ## 8RC, kShiftRegCl,                         IS_BINARY_OP   | REG_DEF0_USE0 | REG_USEC | SETS_CCODES, { 0,    0, 0xD2, 0, 0, modrm_opcode, 0,    1 }, #opname "8RC", "!0r,cl" }, \
{ kX86 ## opname ## 8MC, kShiftMemCl,    IS_LOAD | IS_STORE | IS_TERTIARY_OP | REG_USE0      | REG_USEC | SETS_CCODES, { 0,    0, 0xD2, 0, 0, modrm_opcode, 0,    1 }, #opname "8MC", "[!0r+!1d],cl" }, \
{ kX86 ## opname ## 8AC, kShiftArrayCl,  IS_LOAD | IS_STORE | IS_QUIN_OP     | REG_USE01     | REG_USEC | SETS_CCODES, { 0,    0, 0xD2, 0, 0, modrm_opcode, 0,    1 }, #opname "8AC", "[!0r+!1r<<!2d+!3d],cl" }, \
  \
{ kX86 ## opname ## 16RI, kShiftRegImm,                        IS_BINARY_OP   | REG_DEF0_USE0 |            SETS_CCODES, { 0x66, 0, 0xC1, 0, 0, modrm_opcode, 0xD1, 1 }, #opname "16RI", "!0r,!1d" }, \
{ kX86 ## opname ## 16MI, kShiftMemImm,   IS_LOAD | IS_STORE | IS_TERTIARY_OP | REG_USE0      |            SETS_CCODES, { 0x66, 0, 0xC1, 0, 0, modrm_opcode, 0xD1, 1 }, #opname "16MI", "[!0r+!1d],!2d" }, \
{ kX86 ## opname ## 16AI, kShiftArrayImm, IS_LOAD | IS_STORE | IS_QUIN_OP     | REG_USE01     |            SETS_CCODES, { 0x66, 0, 0xC1, 0, 0, modrm_opcode, 0xD1, 1 }, #opname "16AI", "[!0r+!1r<<!2d+!3d],!4d" }, \
{ kX86 ## opname ## 16RC, kShiftRegCl,                         IS_BINARY_OP   | REG_DEF0_USE0 | REG_USEC | SETS_CCODES, { 0x66, 0, 0xD3, 0, 0, modrm_opcode, 0,    1 }, #opname "16RC", "!0r,cl" }, \
{ kX86 ## opname ## 16MC, kShiftMemCl,    IS_LOAD | IS_STORE | IS_TERTIARY_OP | REG_USE0      | REG_USEC | SETS_CCODES, { 0x66, 0, 0xD3, 0, 0, modrm_opcode, 0,    1 }, #opname "16MC", "[!0r+!1d],cl" }, \
{ kX86 ## opname ## 16AC, kShiftArrayCl,  IS_LOAD | IS_STORE | IS_QUIN_OP     | REG_USE01     | REG_USEC | SETS_CCODES, { 0x66, 0, 0xD3, 0, 0, modrm_opcode, 0,    1 }, #opname "16AC", "[!0r+!1r<<!2d+!3d],cl" }, \
  \
{ kX86 ## opname ## 32RI, kShiftRegImm,                        IS_BINARY_OP   | REG_DEF0_USE0 |            SETS_CCODES, { 0,    0, 0xC1, 0, 0, modrm_opcode, 0xD1, 1 }, #opname "32RI", "!0r,!1d" }, \
{ kX86 ## opname ## 32MI, kShiftMemImm,   IS_LOAD | IS_STORE | IS_TERTIARY_OP | REG_USE0      |            SETS_CCODES, { 0,    0, 0xC1, 0, 0, modrm_opcode, 0xD1, 1 }, #opname "32MI", "[!0r+!1d],!2d" }, \
{ kX86 ## opname ## 32AI, kShiftArrayImm, IS_LOAD | IS_STORE | IS_QUIN_OP     | REG_USE01     |            SETS_CCODES, { 0,    0, 0xC1, 0, 0, modrm_opcode, 0xD1, 1 }, #opname "32AI", "[!0r+!1r<<!2d+!3d],!4d" }, \
{ kX86 ## opname ## 32RC, kShiftRegCl,                         IS_BINARY_OP   | REG_DEF0_USE0 | REG_USEC | SETS_CCODES, { 0,    0, 0xD3, 0, 0, modrm_opcode, 0,    0 }, #opname "32RC", "!0r,cl" }, \
{ kX86 ## opname ## 32MC, kShiftMemCl,    IS_LOAD | IS_STORE | IS_TERTIARY_OP | REG_USE0      | REG_USEC | SETS_CCODES, { 0,    0, 0xD3, 0, 0, modrm_opcode, 0,    0 }, #opname "32MC", "[!0r+!1d],cl" }, \
{ kX86 ## opname ## 32AC, kShiftArrayCl,  IS_LOAD | IS_STORE | IS_QUIN_OP     | REG_USE01     | REG_USEC | SETS_CCODES, { 0,    0, 0xD3, 0, 0, modrm_opcode, 0,    0 }, #opname "32AC", "[!0r+!1r<<!2d+!3d],cl" }

  SHIFT_ENCODING_MAP(Rol, 0x0),
  SHIFT_ENCODING_MAP(Ror, 0x1),
  SHIFT_ENCODING_MAP(Rcl, 0x2),
  SHIFT_ENCODING_MAP(Rcr, 0x3),
  SHIFT_ENCODING_MAP(Sal, 0x4),
  SHIFT_ENCODING_MAP(Shr, 0x5),
  SHIFT_ENCODING_MAP(Sar, 0x7),
#undef SHIFT_ENCODING_MAP

  { kX86Cmc, kNullary, NO_OPERAND, { 0, 0, 0xF5, 0, 0, 0, 0, 0}, "Cmc", "" },

  { kX86Test8RI,  kRegImm,             IS_BINARY_OP   | REG_USE0  | SETS_CCODES, { 0,    0, 0xF6, 0, 0, 0, 0, 1}, "Test8RI", "!0r,!1d" },
  { kX86Test8MI,  kMemImm,   IS_LOAD | IS_TERTIARY_OP | REG_USE0  | SETS_CCODES, { 0,    0, 0xF6, 0, 0, 0, 0, 1}, "Test8MI", "[!0r+!1d],!2d" },
  { kX86Test8AI,  kArrayImm, IS_LOAD | IS_QUIN_OP     | REG_USE01 | SETS_CCODES, { 0,    0, 0xF6, 0, 0, 0, 0, 1}, "Test8AI", "[!0r+!1r<<!2d+!3d],!4d" },
  { kX86Test16RI, kRegImm,             IS_BINARY_OP   | REG_USE0  | SETS_CCODES, { 0x66, 0, 0xF7, 0, 0, 0, 0, 2}, "Test16RI", "!0r,!1d" },
  { kX86Test16MI, kMemImm,   IS_LOAD | IS_TERTIARY_OP | REG_USE0  | SETS_CCODES, { 0x66, 0, 0xF7, 0, 0, 0, 0, 2}, "Test16MI", "[!0r+!1d],!2d" },
  { kX86Test16AI, kArrayImm, IS_LOAD | IS_QUIN_OP     | REG_USE01 | SETS_CCODES, { 0x66, 0, 0xF7, 0, 0, 0, 0, 2}, "Test16AI", "[!0r+!1r<<!2d+!3d],!4d" },
  { kX86Test32RI, kRegImm,             IS_BINARY_OP   | REG_USE0  | SETS_CCODES, { 0,    0, 0xF7, 0, 0, 0, 0, 4}, "Test32RI", "!0r,!1d" },
  { kX86Test32MI, kMemImm,   IS_LOAD | IS_TERTIARY_OP | REG_USE0  | SETS_CCODES, { 0,    0, 0xF7, 0, 0, 0, 0, 4}, "Test32MI", "[!0r+!1d],!2d" },
  { kX86Test32AI, kArrayImm, IS_LOAD | IS_QUIN_OP     | REG_USE01 | SETS_CCODES, { 0,    0, 0xF7, 0, 0, 0, 0, 4}, "Test32AI", "[!0r+!1r<<!2d+!3d],!4d" },
  { kX86Test32RR, kRegReg,             IS_BINARY_OP   | REG_USE01 | SETS_CCODES, { 0,    0, 0x85, 0, 0, 0, 0, 0}, "Test32RR", "!0r,!1r" },

#define UNARY_ENCODING_MAP(opname, modrm, is_store, sets_ccodes, \
                           reg, reg_kind, reg_flags, \
                           mem, mem_kind, mem_flags, \
                           arr, arr_kind, arr_flags, imm, \
                           b_flags, hw_flags, w_flags, \
                           b_format, hw_format, w_format) \
{ kX86 ## opname ## 8 ## reg,  reg_kind,                      reg_flags | b_flags  | sets_ccodes, { 0,    0, 0xF6, 0, 0, modrm, 0, imm << 0}, #opname "8" #reg, #b_format "!0r" }, \
{ kX86 ## opname ## 8 ## mem,  mem_kind, IS_LOAD | is_store | mem_flags | b_flags  | sets_ccodes, { 0,    0, 0xF6, 0, 0, modrm, 0, imm << 0}, #opname "8" #mem, #b_format "[!0r+!1d]" }, \
{ kX86 ## opname ## 8 ## arr,  arr_kind, IS_LOAD | is_store | arr_flags | b_flags  | sets_ccodes, { 0,    0, 0xF6, 0, 0, modrm, 0, imm << 0}, #opname "8" #arr, #b_format "[!0r+!1r<<!2d+!3d]" }, \
{ kX86 ## opname ## 16 ## reg, reg_kind,                      reg_flags | hw_flags | sets_ccodes, { 0x66, 0, 0xF7, 0, 0, modrm, 0, imm << 1}, #opname "16" #reg, #hw_format "!0r" }, \
{ kX86 ## opname ## 16 ## mem, mem_kind, IS_LOAD | is_store | mem_flags | hw_flags | sets_ccodes, { 0x66, 0, 0xF7, 0, 0, modrm, 0, imm << 1}, #opname "16" #mem, #hw_format "[!0r+!1d]" }, \
{ kX86 ## opname ## 16 ## arr, arr_kind, IS_LOAD | is_store | arr_flags | hw_flags | sets_ccodes, { 0x66, 0, 0xF7, 0, 0, modrm, 0, imm << 1}, #opname "16" #arr, #hw_format "[!0r+!1r<<!2d+!3d]" }, \
{ kX86 ## opname ## 32 ## reg, reg_kind,                      reg_flags | w_flags  | sets_ccodes, { 0,    0, 0xF7, 0, 0, modrm, 0, imm << 2}, #opname "32" #reg, #w_format "!0r" }, \
{ kX86 ## opname ## 32 ## mem, mem_kind, IS_LOAD | is_store | mem_flags | w_flags  | sets_ccodes, { 0,    0, 0xF7, 0, 0, modrm, 0, imm << 2}, #opname "32" #mem, #w_format "[!0r+!1d]" }, \
{ kX86 ## opname ## 32 ## arr, arr_kind, IS_LOAD | is_store | arr_flags | w_flags  | sets_ccodes, { 0,    0, 0xF7, 0, 0, modrm, 0, imm << 2}, #opname "32" #arr, #w_format "[!0r+!1r<<!2d+!3d]" }

  UNARY_ENCODING_MAP(Not, 0x2, IS_STORE, 0,           R, kReg, IS_UNARY_OP | REG_DEF0_USE0, M, kMem, IS_BINARY_OP | REG_USE0, A, kArray, IS_QUAD_OP | REG_USE01, 0, 0, 0, 0, "", "", ""),
  UNARY_ENCODING_MAP(Neg, 0x3, IS_STORE, SETS_CCODES, R, kReg, IS_UNARY_OP | REG_DEF0_USE0, M, kMem, IS_BINARY_OP | REG_USE0, A, kArray, IS_QUAD_OP | REG_USE01, 0, 0, 0, 0, "", "", ""),

  UNARY_ENCODING_MAP(Mul,     0x4, 0, SETS_CCODES, DaR, kRegRegReg, IS_UNARY_OP | REG_USE0, DaM, kRegRegMem, IS_BINARY_OP | REG_USE0, DaA, kRegRegArray, IS_QUAD_OP | REG_USE01, 0, REG_DEFA_USEA, REG_DEFAD_USEA,  REG_DEFAD_USEA,  "ax,al,", "dx:ax,ax,", "edx:eax,eax,"),
  UNARY_ENCODING_MAP(Imul,    0x5, 0, SETS_CCODES, DaR, kRegRegReg, IS_UNARY_OP | REG_USE0, DaM, kRegRegMem, IS_BINARY_OP | REG_USE0, DaA, kRegRegArray, IS_QUAD_OP | REG_USE01, 0, REG_DEFA_USEA, REG_DEFAD_USEA,  REG_DEFAD_USEA,  "ax,al,", "dx:ax,ax,", "edx:eax,eax,"),
  UNARY_ENCODING_MAP(Divmod,  0x6, 0, SETS_CCODES, DaR, kRegRegReg, IS_UNARY_OP | REG_USE0, DaM, kRegRegMem, IS_BINARY_OP | REG_USE0, DaA, kRegRegArray, IS_QUAD_OP | REG_USE01, 0, REG_DEFA_USEA, REG_DEFAD_USEAD, REG_DEFAD_USEAD, "ah:al,ax,", "dx:ax,dx:ax,", "edx:eax,edx:eax,"),
  UNARY_ENCODING_MAP(Idivmod, 0x7, 0, SETS_CCODES, DaR, kRegRegReg, IS_UNARY_OP | REG_USE0, DaM, kRegRegMem, IS_BINARY_OP | REG_USE0, DaA, kRegRegArray, IS_QUAD_OP | REG_USE01, 0, REG_DEFA_USEA, REG_DEFAD_USEAD, REG_DEFAD_USEAD, "ah:al,ax,", "dx:ax,dx:ax,", "edx:eax,edx:eax,"),
#undef UNARY_ENCODING_MAP

#define EXT_0F_ENCODING_MAP(opname, prefix, opcode, reg_def) \
{ kX86 ## opname ## RR, kRegReg,             IS_BINARY_OP   | reg_def | REG_USE01,  { prefix, 0, 0x0F, opcode, 0, 0, 0, 0 }, #opname "RR", "!0r,!1r" }, \
{ kX86 ## opname ## RM, kRegMem,   IS_LOAD | IS_TERTIARY_OP | reg_def | REG_USE01,  { prefix, 0, 0x0F, opcode, 0, 0, 0, 0 }, #opname "RM", "!0r,[!1r+!2d]" }, \
{ kX86 ## opname ## RA, kRegArray, IS_LOAD | IS_QUIN_OP     | reg_def | REG_USE012, { prefix, 0, 0x0F, opcode, 0, 0, 0, 0 }, #opname "RA", "!0r,[!1r+!2r<<!3d+!4d]" }

  EXT_0F_ENCODING_MAP(Movsd, 0xF2, 0x10, REG_DEF0),
  { kX86MovsdMR, kMemReg,   IS_STORE | IS_TERTIARY_OP | REG_USE02,  { 0xF2, 0, 0x0F, 0x11, 0, 0, 0, 0 }, "MovsdMR", "[!0r+!1d],!2r" },
  { kX86MovsdAR, kArrayReg, IS_STORE | IS_QUIN_OP     | REG_USE014, { 0xF2, 0, 0x0F, 0x11, 0, 0, 0, 0 }, "MovsdAR", "[!0r+!1r<<!2d+!3d],!4r" },

  EXT_0F_ENCODING_MAP(Movss, 0xF3, 0x10, REG_DEF0),
  { kX86MovssMR, kMemReg,   IS_STORE | IS_TERTIARY_OP | REG_USE02,  { 0xF3, 0, 0x0F, 0x11, 0, 0, 0, 0 }, "MovssMR", "[!0r+!1d],!2r" },
  { kX86MovssAR, kArrayReg, IS_STORE | IS_QUIN_OP     | REG_USE014, { 0xF3, 0, 0x0F, 0x11, 0, 0, 0, 0 }, "MovssAR", "[!0r+!1r<<!2d+!3d],!4r" },

  EXT_0F_ENCODING_MAP(Cvtsi2sd,  0xF2, 0x2A, REG_DEF0),
  EXT_0F_ENCODING_MAP(Cvtsi2ss,  0xF3, 0x2A, REG_DEF0),
  EXT_0F_ENCODING_MAP(Cvttsd2si, 0xF2, 0x2C, REG_DEF0),
  EXT_0F_ENCODING_MAP(Cvttss2si, 0xF3, 0x2C, REG_DEF0),
  EXT_0F_ENCODING_MAP(Cvtsd2si,  0xF2, 0x2D, REG_DEF0),
  EXT_0F_ENCODING_MAP(Cvtss2si,  0xF3, 0x2D, REG_DEF0),
  EXT_0F_ENCODING_MAP(Ucomisd,   0x66, 0x2E, SETS_CCODES),
  EXT_0F_ENCODING_MAP(Ucomiss,   0x00, 0x2E, SETS_CCODES),
  EXT_0F_ENCODING_MAP(Comisd,    0x66, 0x2F, SETS_CCODES),
  EXT_0F_ENCODING_MAP(Comiss,    0x00, 0x2F, SETS_CCODES),
  EXT_0F_ENCODING_MAP(Orps,      0x00, 0x56, REG_DEF0),
  EXT_0F_ENCODING_MAP(Xorps,     0x00, 0x57, REG_DEF0),
  EXT_0F_ENCODING_MAP(Addsd,     0xF2, 0x58, REG_DEF0),
  EXT_0F_ENCODING_MAP(Addss,     0xF3, 0x58, REG_DEF0),
  EXT_0F_ENCODING_MAP(Mulsd,     0xF2, 0x59, REG_DEF0),
  EXT_0F_ENCODING_MAP(Mulss,     0xF3, 0x59, REG_DEF0),
  EXT_0F_ENCODING_MAP(Cvtsd2ss,  0xF2, 0x5A, REG_DEF0),
  EXT_0F_ENCODING_MAP(Cvtss2sd,  0xF3, 0x5A, REG_DEF0),
  EXT_0F_ENCODING_MAP(Subsd,     0xF2, 0x5C, REG_DEF0),
  EXT_0F_ENCODING_MAP(Subss,     0xF3, 0x5C, REG_DEF0),
  EXT_0F_ENCODING_MAP(Divsd,     0xF2, 0x5E, REG_DEF0),
  EXT_0F_ENCODING_MAP(Divss,     0xF3, 0x5E, REG_DEF0),

  { kX86PsrlqRI, kRegImm, IS_BINARY_OP | REG_DEF0_USE0, { 0x66, 0, 0x0F, 0x73, 0, 2, 0, 1 }, "PsrlqRI", "!0r,!1d" },
  { kX86PsllqRI, kRegImm, IS_BINARY_OP | REG_DEF0_USE0, { 0x66, 0, 0x0F, 0x73, 0, 6, 0, 1 }, "PsllqRI", "!0r,!1d" },

  EXT_0F_ENCODING_MAP(Movdxr,    0x66, 0x6E, REG_DEF0),
  { kX86MovdrxRR, kRegRegStore, IS_BINARY_OP | REG_DEF0   | REG_USE01,  { 0x66, 0, 0x0F, 0x7E, 0, 0, 0, 0 }, "MovdrxRR", "!0r,!1r" },
  { kX86MovdrxMR, kMemReg,      IS_STORE | IS_TERTIARY_OP | REG_USE02,  { 0x66, 0, 0x0F, 0x7E, 0, 0, 0, 0 }, "MovdrxMR", "[!0r+!1d],!2r" },
  { kX86MovdrxAR, kArrayReg,    IS_STORE | IS_QUIN_OP     | REG_USE014, { 0x66, 0, 0x0F, 0x7E, 0, 0, 0, 0 }, "MovdrxAR", "[!0r+!1r<<!2d+!3d],!4r" },

  { kX86Set8R, kRegCond,              IS_BINARY_OP   | REG_DEF0  | USES_CCODES, { 0, 0, 0x0F, 0x90, 0, 0, 0, 0 }, "Set8R", "!1c !0r" },
  { kX86Set8M, kMemCond,   IS_STORE | IS_TERTIARY_OP | REG_USE0  | USES_CCODES, { 0, 0, 0x0F, 0x90, 0, 0, 0, 0 }, "Set8M", "!2c [!0r+!1d]" },
  { kX86Set8A, kArrayCond, IS_STORE | IS_QUIN_OP     | REG_USE01 | USES_CCODES, { 0, 0, 0x0F, 0x90, 0, 0, 0, 0 }, "Set8A", "!4c [!0r+!1r<<!2d+!3d]" },

  // TODO: load/store?
  // Encode the modrm opcode as an extra opcode byte to avoid computation during assembly.
  { kX86Mfence, kReg,                 NO_OPERAND,     { 0, 0, 0x0F, 0xAE, 0, 6, 0, 0 }, "Mfence", "" },

  EXT_0F_ENCODING_MAP(Imul16,  0x66, 0xAF, REG_DEF0 | SETS_CCODES),
  EXT_0F_ENCODING_MAP(Imul32,  0x00, 0xAF, REG_DEF0 | SETS_CCODES),

  { kX86CmpxchgRR, kRegRegStore, IS_BINARY_OP | REG_DEF0 | REG_USE01 | REG_DEFA_USEA | SETS_CCODES, { 0, 0, 0x0F, 0xB1, 0, 0, 0, 0 }, "Cmpxchg", "!0r,!1r" },
  { kX86CmpxchgMR, kMemReg,   IS_STORE | IS_TERTIARY_OP | REG_USE02 | REG_DEFA_USEA | SETS_CCODES, { 0, 0, 0x0F, 0xB1, 0, 0, 0, 0 }, "Cmpxchg", "[!0r+!1d],!2r" },
  { kX86CmpxchgAR, kArrayReg, IS_STORE | IS_QUIN_OP | REG_USE014 | REG_DEFA_USEA | SETS_CCODES, { 0, 0, 0x0F, 0xB1, 0, 0, 0, 0 }, "Cmpxchg", "[!0r+!1r<<!2d+!3d],!4r" },
  { kX86LockCmpxchgRR, kRegRegStore, IS_BINARY_OP | REG_DEF0 | REG_USE01 | REG_DEFA_USEA | SETS_CCODES, { 0xF0, 0, 0x0F, 0xB1, 0, 0, 0, 0 }, "Lock Cmpxchg", "!0r,!1r" },
  { kX86LockCmpxchgMR, kMemReg,   IS_STORE | IS_TERTIARY_OP | REG_USE02 | REG_DEFA_USEA | SETS_CCODES, { 0xF0, 0, 0x0F, 0xB1, 0, 0, 0, 0 }, "Lock Cmpxchg", "[!0r+!1d],!2r" },
  { kX86LockCmpxchgAR, kArrayReg, IS_STORE | IS_QUIN_OP | REG_USE014 | REG_DEFA_USEA | SETS_CCODES, { 0xF0, 0, 0x0F, 0xB1, 0, 0, 0, 0 }, "Lock Cmpxchg", "[!0r+!1r<<!2d+!3d],!4r" },

  EXT_0F_ENCODING_MAP(Movzx8,  0x00, 0xB6, REG_DEF0),
  EXT_0F_ENCODING_MAP(Movzx16, 0x00, 0xB7, REG_DEF0),
  EXT_0F_ENCODING_MAP(Movsx8,  0x00, 0xBE, REG_DEF0),
  EXT_0F_ENCODING_MAP(Movsx16, 0x00, 0xBF, REG_DEF0),
#undef EXT_0F_ENCODING_MAP

  { kX86Jcc8,  kJcc,  IS_BINARY_OP | IS_BRANCH | NEEDS_FIXUP | USES_CCODES, { 0,             0, 0x70, 0,    0, 0, 0, 0 }, "Jcc8",  "!1c !0t" },
  { kX86Jcc32, kJcc,  IS_BINARY_OP | IS_BRANCH | NEEDS_FIXUP | USES_CCODES, { 0,             0, 0x0F, 0x80, 0, 0, 0, 0 }, "Jcc32", "!1c !0t" },
  { kX86Jmp8,  kJmp,  IS_UNARY_OP  | IS_BRANCH | NEEDS_FIXUP,               { 0,             0, 0xEB, 0,    0, 0, 0, 0 }, "Jmp8",  "!0t" },
  { kX86Jmp32, kJmp,  IS_UNARY_OP  | IS_BRANCH | NEEDS_FIXUP,               { 0,             0, 0xE9, 0,    0, 0, 0, 0 }, "Jmp32", "!0t" },
  { kX86JmpR,  kJmp,  IS_UNARY_OP  | IS_BRANCH | REG_USE0,                  { 0,             0, 0xFF, 0,    0, 4, 0, 0 }, "JmpR",  "!0r" },
  { kX86JmpT,  kJmp,  IS_UNARY_OP  | IS_BRANCH | IS_LOAD,                   { THREAD_PREFIX, 0, 0xFF, 0,    0, 4, 0, 0 }, "JmpT",  "fs:[!0d]" },
  { kX86CallR, kCall, IS_UNARY_OP  | IS_BRANCH | REG_USE0,                  { 0,             0, 0xE8, 0,    0, 0, 0, 0 }, "CallR", "!0r" },
  { kX86CallM, kCall, IS_BINARY_OP | IS_BRANCH | IS_LOAD | REG_USE0,        { 0,             0, 0xFF, 0,    0, 2, 0, 0 }, "CallM", "[!0r+!1d]" },
  { kX86CallA, kCall, IS_QUAD_OP   | IS_BRANCH | IS_LOAD | REG_USE01,       { 0,             0, 0xFF, 0,    0, 2, 0, 0 }, "CallA", "[!0r+!1r<<!2d+!3d]" },
  { kX86CallT, kCall, IS_UNARY_OP  | IS_BRANCH | IS_LOAD,                   { THREAD_PREFIX, 0, 0xFF, 0,    0, 2, 0, 0 }, "CallT", "fs:[!0d]" },
  { kX86Ret,   kNullary, NO_OPERAND | IS_BRANCH,                            { 0,             0, 0xC3, 0,    0, 0, 0, 0 }, "Ret", "" },

  { kX86StartOfMethod, kMacro,  IS_UNARY_OP | SETS_CCODES,             { 0, 0, 0,    0, 0, 0, 0, 0 }, "StartOfMethod", "!0r" },
  { kX86PcRelLoadRA,   kPcRel,  IS_LOAD | IS_QUIN_OP | REG_DEF0_USE12, { 0, 0, 0x8B, 0, 0, 0, 0, 0 }, "PcRelLoadRA",   "!0r,[!1r+!2r<<!3d+!4p]" },
  { kX86PcRelAdr,      kPcRel,  IS_LOAD | IS_BINARY_OP | REG_DEF0,     { 0, 0, 0xB8, 0, 0, 0, 0, 4 }, "PcRelAdr",      "!0r,!1d" },
};

static size_t ComputeSize(const X86EncodingMap* entry, int base, int displacement, bool has_sib) {
  size_t size = 0;
  if (entry->skeleton.prefix1 > 0) {
    ++size;
    if (entry->skeleton.prefix2 > 0) {
      ++size;
    }
  }
  ++size;  // opcode
  if (entry->skeleton.opcode == 0x0F) {
    ++size;
    if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode1 == 0x3A) {
      ++size;
    }
  }
  ++size;  // modrm
  if (has_sib || base == rX86_SP) {
    // SP requires a SIB byte.
    ++size;
  }
  if (displacement != 0 || base == rBP) {
    // BP requires an explicit displacement, even when it's 0.
    if (entry->opcode != kX86Lea32RA) {
      DCHECK_NE(entry->flags & (IS_LOAD | IS_STORE), 0ULL) << entry->name;
    }
    size += IS_SIMM8(displacement) ? 1 : 4;
  }
  size += entry->skeleton.immediate_bytes;
  return size;
}

int X86Mir2Lir::GetInsnSize(LIR* lir) {
  const X86EncodingMap* entry = &X86Mir2Lir::EncodingMap[lir->opcode];
  switch (entry->kind) {
    case kData:
      return 4;  // 4 bytes of data
    case kNop:
      return lir->operands[0];  // length of nop is sole operand
    case kNullary:
      return 1;  // 1 byte of opcode
    case kReg:  // lir operands - 0: reg
      return ComputeSize(entry, 0, 0, false);
    case kMem:  // lir operands - 0: base, 1: disp
      return ComputeSize(entry, lir->operands[0], lir->operands[1], false);
    case kArray:  // lir operands - 0: base, 1: index, 2: scale, 3: disp
      return ComputeSize(entry, lir->operands[0], lir->operands[3], true);
    case kMemReg:  // lir operands - 0: base, 1: disp, 2: reg
      return ComputeSize(entry, lir->operands[0], lir->operands[1], false);
    case kArrayReg:  // lir operands - 0: base, 1: index, 2: scale, 3: disp, 4: reg
      return ComputeSize(entry, lir->operands[0], lir->operands[3], true);
    case kThreadReg:  // lir operands - 0: disp, 1: reg
      return ComputeSize(entry, 0, lir->operands[0], false);
    case kRegReg:
      return ComputeSize(entry, 0, 0, false);
    case kRegRegStore:
      return ComputeSize(entry, 0, 0, false);
    case kRegMem:  // lir operands - 0: reg, 1: base, 2: disp
      return ComputeSize(entry, lir->operands[1], lir->operands[2], false);
    case kRegArray:   // lir operands - 0: reg, 1: base, 2: index, 3: scale, 4: disp
      return ComputeSize(entry, lir->operands[1], lir->operands[4], true);
    case kRegThread:  // lir operands - 0: reg, 1: disp
      return ComputeSize(entry, 0, 0x12345678, false);  // displacement size is always 32bit
    case kRegImm: {  // lir operands - 0: reg, 1: immediate
      size_t size = ComputeSize(entry, 0, 0, false);
      if (entry->skeleton.ax_opcode == 0) {
        return size;
      } else {
        // AX opcodes don't require the modrm byte.
        int reg = lir->operands[0];
        return size - (reg == rAX ? 1 : 0);
      }
    }
    case kMemImm:  // lir operands - 0: base, 1: disp, 2: immediate
      return ComputeSize(entry, lir->operands[0], lir->operands[1], false);
    case kArrayImm:  // lir operands - 0: base, 1: index, 2: scale, 3: disp 4: immediate
      return ComputeSize(entry, lir->operands[0], lir->operands[3], true);
    case kThreadImm:  // lir operands - 0: disp, 1: imm
      return ComputeSize(entry, 0, 0x12345678, false);  // displacement size is always 32bit
    case kRegRegImm:  // lir operands - 0: reg, 1: reg, 2: imm
      return ComputeSize(entry, 0, 0, false);
    case kRegMemImm:  // lir operands - 0: reg, 1: base, 2: disp, 3: imm
      return ComputeSize(entry, lir->operands[1], lir->operands[2], false);
    case kRegArrayImm:  // lir operands - 0: reg, 1: base, 2: index, 3: scale, 4: disp, 5: imm
      return ComputeSize(entry, lir->operands[1], lir->operands[4], true);
    case kMovRegImm:  // lir operands - 0: reg, 1: immediate
      return 1 + entry->skeleton.immediate_bytes;
    case kShiftRegImm:  // lir operands - 0: reg, 1: immediate
      // Shift by immediate one has a shorter opcode.
      return ComputeSize(entry, 0, 0, false) - (lir->operands[1] == 1 ? 1 : 0);
    case kShiftMemImm:  // lir operands - 0: base, 1: disp, 2: immediate
      // Shift by immediate one has a shorter opcode.
      return ComputeSize(entry, lir->operands[0], lir->operands[1], false) -
             (lir->operands[2] == 1 ? 1 : 0);
    case kShiftArrayImm:  // lir operands - 0: base, 1: index, 2: scale, 3: disp 4: immediate
      // Shift by immediate one has a shorter opcode.
      return ComputeSize(entry, lir->operands[0], lir->operands[3], true) -
             (lir->operands[4] == 1 ? 1 : 0);
    case kShiftRegCl:
      return ComputeSize(entry, 0, 0, false);
    case kShiftMemCl:  // lir operands - 0: base, 1: disp, 2: cl
      return ComputeSize(entry, lir->operands[0], lir->operands[1], false);
    case kShiftArrayCl:  // lir operands - 0: base, 1: index, 2: scale, 3: disp, 4: reg
      return ComputeSize(entry, lir->operands[0], lir->operands[3], true);
    case kRegCond:  // lir operands - 0: reg, 1: cond
      return ComputeSize(entry, 0, 0, false);
    case kMemCond:  // lir operands - 0: base, 1: disp, 2: cond
      return ComputeSize(entry, lir->operands[0], lir->operands[1], false);
    case kArrayCond:  // lir operands - 0: base, 1: index, 2: scale, 3: disp, 4: cond
      return ComputeSize(entry, lir->operands[0], lir->operands[3], true);
    case kJcc:
      if (lir->opcode == kX86Jcc8) {
        return 2;  // opcode + rel8
      } else {
        DCHECK(lir->opcode == kX86Jcc32);
        return 6;  // 2 byte opcode + rel32
      }
    case kJmp:
      if (lir->opcode == kX86Jmp8) {
        return 2;  // opcode + rel8
      } else if (lir->opcode == kX86Jmp32) {
        return 5;  // opcode + rel32
      } else if (lir->opcode == kX86JmpT) {
        return ComputeSize(entry, 0, 0x12345678, false);  // displacement size is always 32bit
      } else {
        DCHECK(lir->opcode == kX86JmpR);
        return 2;  // opcode + modrm
      }
    case kCall:
      switch (lir->opcode) {
        case kX86CallR: return 2;  // opcode modrm
        case kX86CallM:  // lir operands - 0: base, 1: disp
          return ComputeSize(entry, lir->operands[0], lir->operands[1], false);
        case kX86CallA:  // lir operands - 0: base, 1: index, 2: scale, 3: disp
          return ComputeSize(entry, lir->operands[0], lir->operands[3], true);
        case kX86CallT:  // lir operands - 0: disp
          return ComputeSize(entry, 0, 0x12345678, false);  // displacement size is always 32bit
        default:
          break;
      }
      break;
    case kPcRel:
      if (entry->opcode == kX86PcRelLoadRA) {
        // lir operands - 0: reg, 1: base, 2: index, 3: scale, 4: table
        return ComputeSize(entry, lir->operands[1], 0x12345678, true);
      } else {
        DCHECK(entry->opcode == kX86PcRelAdr);
        return 5;  // opcode with reg + 4 byte immediate
      }
    case kMacro:
      DCHECK_EQ(lir->opcode, static_cast<int>(kX86StartOfMethod));
      return 5 /* call opcode + 4 byte displacement */ + 1 /* pop reg */ +
          ComputeSize(&X86Mir2Lir::EncodingMap[kX86Sub32RI], 0, 0, false) -
          (lir->operands[0] == rAX  ? 1 : 0);  // shorter ax encoding
    default:
      break;
  }
  UNIMPLEMENTED(FATAL) << "Unimplemented size encoding for: " << entry->name;
  return 0;
}

static uint8_t ModrmForDisp(int base, int disp) {
  // BP requires an explicit disp, so do not omit it in the 0 case
  if (disp == 0 && base != rBP) {
    return 0;
  } else if (IS_SIMM8(disp)) {
    return 1;
  } else {
    return 2;
  }
}

void X86Mir2Lir::EmitDisp(int base, int disp) {
  // BP requires an explicit disp, so do not omit it in the 0 case
  if (disp == 0 && base != rBP) {
    return;
  } else if (IS_SIMM8(disp)) {
    code_buffer_.push_back(disp & 0xFF);
  } else {
    code_buffer_.push_back(disp & 0xFF);
    code_buffer_.push_back((disp >> 8) & 0xFF);
    code_buffer_.push_back((disp >> 16) & 0xFF);
    code_buffer_.push_back((disp >> 24) & 0xFF);
  }
}

void X86Mir2Lir::EmitOpReg(const X86EncodingMap* entry, uint8_t reg) {
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  code_buffer_.push_back(entry->skeleton.opcode);
  if (entry->skeleton.opcode == 0x0F) {
    code_buffer_.push_back(entry->skeleton.extra_opcode1);
    if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode2 == 0x3A) {
      code_buffer_.push_back(entry->skeleton.extra_opcode2);
    } else {
      DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.extra_opcode1);
    DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  }
  if (X86_FPREG(reg)) {
    reg = reg & X86_FP_REG_MASK;
  }
  if (reg >= 4) {
    DCHECK(strchr(entry->name, '8') == NULL) << entry->name << " " << static_cast<int>(reg)
        << " in " << PrettyMethod(cu_->method_idx, *cu_->dex_file);
  }
  DCHECK_LT(reg, 8);
  uint8_t modrm = (3 << 6) | (entry->skeleton.modrm_opcode << 3) | reg;
  code_buffer_.push_back(modrm);
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
  DCHECK_EQ(0, entry->skeleton.immediate_bytes);
}

void X86Mir2Lir::EmitOpMem(const X86EncodingMap* entry, uint8_t base, int disp) {
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  code_buffer_.push_back(entry->skeleton.opcode);
  DCHECK_EQ(0, entry->skeleton.extra_opcode1);
  DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  DCHECK_LT(entry->skeleton.modrm_opcode, 8);
  DCHECK_LT(base, 8);
  uint8_t modrm = (ModrmForDisp(base, disp) << 6) | (entry->skeleton.modrm_opcode << 3) | base;
  code_buffer_.push_back(modrm);
  EmitDisp(base, disp);
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
  DCHECK_EQ(0, entry->skeleton.immediate_bytes);
}

void X86Mir2Lir::EmitMemReg(const X86EncodingMap* entry,
                       uint8_t base, int disp, uint8_t reg) {
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  code_buffer_.push_back(entry->skeleton.opcode);
  if (entry->skeleton.opcode == 0x0F) {
    code_buffer_.push_back(entry->skeleton.extra_opcode1);
    if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode2 == 0x3A) {
      code_buffer_.push_back(entry->skeleton.extra_opcode2);
    } else {
      DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.extra_opcode1);
    DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  }
  if (X86_FPREG(reg)) {
    reg = reg & X86_FP_REG_MASK;
  }
  if (reg >= 4) {
    DCHECK(strchr(entry->name, '8') == NULL) << entry->name << " " << static_cast<int>(reg)
        << " in " << PrettyMethod(cu_->method_idx, *cu_->dex_file);
  }
  DCHECK_LT(reg, 8);
  DCHECK_LT(base, 8);
  uint8_t modrm = (ModrmForDisp(base, disp) << 6) | (reg << 3) | base;
  code_buffer_.push_back(modrm);
  if (base == rX86_SP) {
    // Special SIB for SP base
    code_buffer_.push_back(0 << 6 | (rX86_SP << 3) | rX86_SP);
  }
  EmitDisp(base, disp);
  DCHECK_EQ(0, entry->skeleton.modrm_opcode);
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
  DCHECK_EQ(0, entry->skeleton.immediate_bytes);
}

void X86Mir2Lir::EmitRegMem(const X86EncodingMap* entry,
                       uint8_t reg, uint8_t base, int disp) {
  // Opcode will flip operands.
  EmitMemReg(entry, base, disp, reg);
}

void X86Mir2Lir::EmitRegArray(const X86EncodingMap* entry, uint8_t reg, uint8_t base, uint8_t index,
                  int scale, int disp) {
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  code_buffer_.push_back(entry->skeleton.opcode);
  if (entry->skeleton.opcode == 0x0F) {
    code_buffer_.push_back(entry->skeleton.extra_opcode1);
    if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode2 == 0x3A) {
      code_buffer_.push_back(entry->skeleton.extra_opcode2);
    } else {
      DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.extra_opcode1);
    DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  }
  if (X86_FPREG(reg)) {
    reg = reg & X86_FP_REG_MASK;
  }
  DCHECK_LT(reg, 8);
  uint8_t modrm = (ModrmForDisp(base, disp) << 6) | (reg << 3) | rX86_SP;
  code_buffer_.push_back(modrm);
  DCHECK_LT(scale, 4);
  DCHECK_LT(index, 8);
  DCHECK_LT(base, 8);
  uint8_t sib = (scale << 6) | (index << 3) | base;
  code_buffer_.push_back(sib);
  EmitDisp(base, disp);
  DCHECK_EQ(0, entry->skeleton.modrm_opcode);
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
  DCHECK_EQ(0, entry->skeleton.immediate_bytes);
}

void X86Mir2Lir::EmitArrayReg(const X86EncodingMap* entry, uint8_t base, uint8_t index, int scale, int disp,
                  uint8_t reg) {
  // Opcode will flip operands.
  EmitRegArray(entry, reg, base, index, scale, disp);
}

void X86Mir2Lir::EmitRegThread(const X86EncodingMap* entry, uint8_t reg, int disp) {
  DCHECK_NE(entry->skeleton.prefix1, 0);
  code_buffer_.push_back(entry->skeleton.prefix1);
  if (entry->skeleton.prefix2 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix2);
  }
  code_buffer_.push_back(entry->skeleton.opcode);
  if (entry->skeleton.opcode == 0x0F) {
    code_buffer_.push_back(entry->skeleton.extra_opcode1);
    if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode2 == 0x3A) {
      code_buffer_.push_back(entry->skeleton.extra_opcode2);
    } else {
      DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.extra_opcode1);
    DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  }
  if (X86_FPREG(reg)) {
    reg = reg & X86_FP_REG_MASK;
  }
  if (reg >= 4) {
    DCHECK(strchr(entry->name, '8') == NULL) << entry->name << " " << static_cast<int>(reg)
        << " in " << PrettyMethod(cu_->method_idx, *cu_->dex_file);
  }
  DCHECK_LT(reg, 8);
  uint8_t modrm = (0 << 6) | (reg << 3) | rBP;
  code_buffer_.push_back(modrm);
  code_buffer_.push_back(disp & 0xFF);
  code_buffer_.push_back((disp >> 8) & 0xFF);
  code_buffer_.push_back((disp >> 16) & 0xFF);
  code_buffer_.push_back((disp >> 24) & 0xFF);
  DCHECK_EQ(0, entry->skeleton.modrm_opcode);
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
  DCHECK_EQ(0, entry->skeleton.immediate_bytes);
}

void X86Mir2Lir::EmitRegReg(const X86EncodingMap* entry, uint8_t reg1, uint8_t reg2) {
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  code_buffer_.push_back(entry->skeleton.opcode);
  if (entry->skeleton.opcode == 0x0F) {
    code_buffer_.push_back(entry->skeleton.extra_opcode1);
    if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode2 == 0x3A) {
      code_buffer_.push_back(entry->skeleton.extra_opcode2);
    } else {
      DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.extra_opcode1);
    DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  }
  if (X86_FPREG(reg1)) {
    reg1 = reg1 & X86_FP_REG_MASK;
  }
  if (X86_FPREG(reg2)) {
    reg2 = reg2 & X86_FP_REG_MASK;
  }
  DCHECK_LT(reg1, 8);
  DCHECK_LT(reg2, 8);
  uint8_t modrm = (3 << 6) | (reg1 << 3) | reg2;
  code_buffer_.push_back(modrm);
  DCHECK_EQ(0, entry->skeleton.modrm_opcode);
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
  DCHECK_EQ(0, entry->skeleton.immediate_bytes);
}

void X86Mir2Lir::EmitRegRegImm(const X86EncodingMap* entry,
                          uint8_t reg1, uint8_t reg2, int32_t imm) {
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  code_buffer_.push_back(entry->skeleton.opcode);
  if (entry->skeleton.opcode == 0x0F) {
    code_buffer_.push_back(entry->skeleton.extra_opcode1);
    if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode2 == 0x3A) {
      code_buffer_.push_back(entry->skeleton.extra_opcode2);
    } else {
      DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.extra_opcode1);
    DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  }
  if (X86_FPREG(reg1)) {
    reg1 = reg1 & X86_FP_REG_MASK;
  }
  if (X86_FPREG(reg2)) {
    reg2 = reg2 & X86_FP_REG_MASK;
  }
  DCHECK_LT(reg1, 8);
  DCHECK_LT(reg2, 8);
  uint8_t modrm = (3 << 6) | (reg1 << 3) | reg2;
  code_buffer_.push_back(modrm);
  DCHECK_EQ(0, entry->skeleton.modrm_opcode);
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
  switch (entry->skeleton.immediate_bytes) {
    case 1:
      DCHECK(IS_SIMM8(imm));
      code_buffer_.push_back(imm & 0xFF);
      break;
    case 2:
      DCHECK(IS_SIMM16(imm));
      code_buffer_.push_back(imm & 0xFF);
      code_buffer_.push_back((imm >> 8) & 0xFF);
      break;
    case 4:
      code_buffer_.push_back(imm & 0xFF);
      code_buffer_.push_back((imm >> 8) & 0xFF);
      code_buffer_.push_back((imm >> 16) & 0xFF);
      code_buffer_.push_back((imm >> 24) & 0xFF);
      break;
    default:
      LOG(FATAL) << "Unexpected immediate bytes (" << entry->skeleton.immediate_bytes
                 << ") for instruction: " << entry->name;
      break;
  }
}

void X86Mir2Lir::EmitRegImm(const X86EncodingMap* entry, uint8_t reg, int imm) {
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  if (reg == rAX && entry->skeleton.ax_opcode != 0) {
    code_buffer_.push_back(entry->skeleton.ax_opcode);
  } else {
    code_buffer_.push_back(entry->skeleton.opcode);
    if (entry->skeleton.opcode == 0x0F) {
      code_buffer_.push_back(entry->skeleton.extra_opcode1);
      if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode2 == 0x3A) {
        code_buffer_.push_back(entry->skeleton.extra_opcode2);
      } else {
        DCHECK_EQ(0, entry->skeleton.extra_opcode2);
      }
    } else {
      DCHECK_EQ(0, entry->skeleton.extra_opcode1);
      DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    }
    if (X86_FPREG(reg)) {
      reg = reg & X86_FP_REG_MASK;
    }
    uint8_t modrm = (3 << 6) | (entry->skeleton.modrm_opcode << 3) | reg;
    code_buffer_.push_back(modrm);
  }
  switch (entry->skeleton.immediate_bytes) {
    case 1:
      DCHECK(IS_SIMM8(imm));
      code_buffer_.push_back(imm & 0xFF);
      break;
    case 2:
      DCHECK(IS_SIMM16(imm));
      code_buffer_.push_back(imm & 0xFF);
      code_buffer_.push_back((imm >> 8) & 0xFF);
      break;
    case 4:
      code_buffer_.push_back(imm & 0xFF);
      code_buffer_.push_back((imm >> 8) & 0xFF);
      code_buffer_.push_back((imm >> 16) & 0xFF);
      code_buffer_.push_back((imm >> 24) & 0xFF);
      break;
    default:
      LOG(FATAL) << "Unexpected immediate bytes (" << entry->skeleton.immediate_bytes
          << ") for instruction: " << entry->name;
      break;
  }
}

void X86Mir2Lir::EmitThreadImm(const X86EncodingMap* entry, int disp, int imm) {
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  code_buffer_.push_back(entry->skeleton.opcode);
  if (entry->skeleton.opcode == 0x0F) {
    code_buffer_.push_back(entry->skeleton.extra_opcode1);
    if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode2 == 0x3A) {
      code_buffer_.push_back(entry->skeleton.extra_opcode2);
    } else {
      DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.extra_opcode1);
    DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  }
  uint8_t modrm = (0 << 6) | (entry->skeleton.modrm_opcode << 3) | rBP;
  code_buffer_.push_back(modrm);
  code_buffer_.push_back(disp & 0xFF);
  code_buffer_.push_back((disp >> 8) & 0xFF);
  code_buffer_.push_back((disp >> 16) & 0xFF);
  code_buffer_.push_back((disp >> 24) & 0xFF);
  switch (entry->skeleton.immediate_bytes) {
    case 1:
      DCHECK(IS_SIMM8(imm));
      code_buffer_.push_back(imm & 0xFF);
      break;
    case 2:
      DCHECK(IS_SIMM16(imm));
      code_buffer_.push_back(imm & 0xFF);
      code_buffer_.push_back((imm >> 8) & 0xFF);
      break;
    case 4:
      code_buffer_.push_back(imm & 0xFF);
      code_buffer_.push_back((imm >> 8) & 0xFF);
      code_buffer_.push_back((imm >> 16) & 0xFF);
      code_buffer_.push_back((imm >> 24) & 0xFF);
      break;
    default:
      LOG(FATAL) << "Unexpected immediate bytes (" << entry->skeleton.immediate_bytes
          << ") for instruction: " << entry->name;
      break;
  }
  DCHECK_EQ(entry->skeleton.ax_opcode, 0);
}

void X86Mir2Lir::EmitMovRegImm(const X86EncodingMap* entry, uint8_t reg, int imm) {
  DCHECK_LT(reg, 8);
  code_buffer_.push_back(0xB8 + reg);
  code_buffer_.push_back(imm & 0xFF);
  code_buffer_.push_back((imm >> 8) & 0xFF);
  code_buffer_.push_back((imm >> 16) & 0xFF);
  code_buffer_.push_back((imm >> 24) & 0xFF);
}

void X86Mir2Lir::EmitShiftRegImm(const X86EncodingMap* entry, uint8_t reg, int imm) {
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  if (imm != 1) {
    code_buffer_.push_back(entry->skeleton.opcode);
  } else {
    // Shorter encoding for 1 bit shift
    code_buffer_.push_back(entry->skeleton.ax_opcode);
  }
  if (entry->skeleton.opcode == 0x0F) {
    code_buffer_.push_back(entry->skeleton.extra_opcode1);
    if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode2 == 0x3A) {
      code_buffer_.push_back(entry->skeleton.extra_opcode2);
    } else {
      DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.extra_opcode1);
    DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  }
  if (reg >= 4) {
    DCHECK(strchr(entry->name, '8') == NULL) << entry->name << " " << static_cast<int>(reg)
        << " in " << PrettyMethod(cu_->method_idx, *cu_->dex_file);
  }
  DCHECK_LT(reg, 8);
  uint8_t modrm = (3 << 6) | (entry->skeleton.modrm_opcode << 3) | reg;
  code_buffer_.push_back(modrm);
  if (imm != 1) {
    DCHECK_EQ(entry->skeleton.immediate_bytes, 1);
    DCHECK(IS_SIMM8(imm));
    code_buffer_.push_back(imm & 0xFF);
  }
}

void X86Mir2Lir::EmitShiftRegCl(const X86EncodingMap* entry, uint8_t reg, uint8_t cl) {
  DCHECK_EQ(cl, static_cast<uint8_t>(rCX));
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  code_buffer_.push_back(entry->skeleton.opcode);
  DCHECK_EQ(0, entry->skeleton.extra_opcode1);
  DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  DCHECK_LT(reg, 8);
  uint8_t modrm = (3 << 6) | (entry->skeleton.modrm_opcode << 3) | reg;
  code_buffer_.push_back(modrm);
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
  DCHECK_EQ(0, entry->skeleton.immediate_bytes);
}

void X86Mir2Lir::EmitRegCond(const X86EncodingMap* entry, uint8_t reg, uint8_t condition) {
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
  DCHECK_EQ(0x0F, entry->skeleton.opcode);
  code_buffer_.push_back(0x0F);
  DCHECK_EQ(0x90, entry->skeleton.extra_opcode1);
  code_buffer_.push_back(0x90 | condition);
  DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  DCHECK_LT(reg, 8);
  uint8_t modrm = (3 << 6) | (entry->skeleton.modrm_opcode << 3) | reg;
  code_buffer_.push_back(modrm);
  DCHECK_EQ(entry->skeleton.immediate_bytes, 0);
}

void X86Mir2Lir::EmitJmp(const X86EncodingMap* entry, int rel) {
  if (entry->opcode == kX86Jmp8) {
    DCHECK(IS_SIMM8(rel));
    code_buffer_.push_back(0xEB);
    code_buffer_.push_back(rel & 0xFF);
  } else if (entry->opcode == kX86Jmp32) {
    code_buffer_.push_back(0xE9);
    code_buffer_.push_back(rel & 0xFF);
    code_buffer_.push_back((rel >> 8) & 0xFF);
    code_buffer_.push_back((rel >> 16) & 0xFF);
    code_buffer_.push_back((rel >> 24) & 0xFF);
  } else {
    DCHECK(entry->opcode == kX86JmpR);
    code_buffer_.push_back(entry->skeleton.opcode);
    uint8_t reg = static_cast<uint8_t>(rel);
    DCHECK_LT(reg, 8);
    uint8_t modrm = (3 << 6) | (entry->skeleton.modrm_opcode << 3) | reg;
    code_buffer_.push_back(modrm);
  }
}

void X86Mir2Lir::EmitJcc(const X86EncodingMap* entry, int rel, uint8_t cc) {
  DCHECK_LT(cc, 16);
  if (entry->opcode == kX86Jcc8) {
    DCHECK(IS_SIMM8(rel));
    code_buffer_.push_back(0x70 | cc);
    code_buffer_.push_back(rel & 0xFF);
  } else {
    DCHECK(entry->opcode == kX86Jcc32);
    code_buffer_.push_back(0x0F);
    code_buffer_.push_back(0x80 | cc);
    code_buffer_.push_back(rel & 0xFF);
    code_buffer_.push_back((rel >> 8) & 0xFF);
    code_buffer_.push_back((rel >> 16) & 0xFF);
    code_buffer_.push_back((rel >> 24) & 0xFF);
  }
}

void X86Mir2Lir::EmitCallMem(const X86EncodingMap* entry, uint8_t base, int disp) {
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  code_buffer_.push_back(entry->skeleton.opcode);
  if (entry->skeleton.opcode == 0x0F) {
    code_buffer_.push_back(entry->skeleton.extra_opcode1);
    if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode2 == 0x3A) {
      code_buffer_.push_back(entry->skeleton.extra_opcode2);
    } else {
      DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.extra_opcode1);
    DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  }
  uint8_t modrm = (ModrmForDisp(base, disp) << 6) | (entry->skeleton.modrm_opcode << 3) | base;
  code_buffer_.push_back(modrm);
  if (base == rX86_SP) {
    // Special SIB for SP base
    code_buffer_.push_back(0 << 6 | (rX86_SP << 3) | rX86_SP);
  }
  EmitDisp(base, disp);
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
  DCHECK_EQ(0, entry->skeleton.immediate_bytes);
}

void X86Mir2Lir::EmitCallThread(const X86EncodingMap* entry, int disp) {
  DCHECK_NE(entry->skeleton.prefix1, 0);
  code_buffer_.push_back(entry->skeleton.prefix1);
  if (entry->skeleton.prefix2 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix2);
  }
  code_buffer_.push_back(entry->skeleton.opcode);
  if (entry->skeleton.opcode == 0x0F) {
    code_buffer_.push_back(entry->skeleton.extra_opcode1);
    if (entry->skeleton.extra_opcode1 == 0x38 || entry->skeleton.extra_opcode2 == 0x3A) {
      code_buffer_.push_back(entry->skeleton.extra_opcode2);
    } else {
      DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.extra_opcode1);
    DCHECK_EQ(0, entry->skeleton.extra_opcode2);
  }
  uint8_t modrm = (0 << 6) | (entry->skeleton.modrm_opcode << 3) | rBP;
  code_buffer_.push_back(modrm);
  code_buffer_.push_back(disp & 0xFF);
  code_buffer_.push_back((disp >> 8) & 0xFF);
  code_buffer_.push_back((disp >> 16) & 0xFF);
  code_buffer_.push_back((disp >> 24) & 0xFF);
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
  DCHECK_EQ(0, entry->skeleton.immediate_bytes);
}

void X86Mir2Lir::EmitPcRel(const X86EncodingMap* entry, uint8_t reg,
                      int base_or_table, uint8_t index, int scale, int table_or_disp) {
  int disp;
  if (entry->opcode == kX86PcRelLoadRA) {
    Mir2Lir::SwitchTable *tab_rec = reinterpret_cast<Mir2Lir::SwitchTable*>(table_or_disp);
    disp = tab_rec->offset;
  } else {
    DCHECK(entry->opcode == kX86PcRelAdr);
    Mir2Lir::FillArrayData *tab_rec = reinterpret_cast<Mir2Lir::FillArrayData*>(base_or_table);
    disp = tab_rec->offset;
  }
  if (entry->skeleton.prefix1 != 0) {
    code_buffer_.push_back(entry->skeleton.prefix1);
    if (entry->skeleton.prefix2 != 0) {
      code_buffer_.push_back(entry->skeleton.prefix2);
    }
  } else {
    DCHECK_EQ(0, entry->skeleton.prefix2);
  }
  if (X86_FPREG(reg)) {
    reg = reg & X86_FP_REG_MASK;
  }
  DCHECK_LT(reg, 8);
  if (entry->opcode == kX86PcRelLoadRA) {
    code_buffer_.push_back(entry->skeleton.opcode);
    DCHECK_EQ(0, entry->skeleton.extra_opcode1);
    DCHECK_EQ(0, entry->skeleton.extra_opcode2);
    uint8_t modrm = (2 << 6) | (reg << 3) | rX86_SP;
    code_buffer_.push_back(modrm);
    DCHECK_LT(scale, 4);
    DCHECK_LT(index, 8);
    DCHECK_LT(base_or_table, 8);
    uint8_t base = static_cast<uint8_t>(base_or_table);
    uint8_t sib = (scale << 6) | (index << 3) | base;
    code_buffer_.push_back(sib);
    DCHECK_EQ(0, entry->skeleton.immediate_bytes);
  } else {
    code_buffer_.push_back(entry->skeleton.opcode + reg);
  }
  code_buffer_.push_back(disp & 0xFF);
  code_buffer_.push_back((disp >> 8) & 0xFF);
  code_buffer_.push_back((disp >> 16) & 0xFF);
  code_buffer_.push_back((disp >> 24) & 0xFF);
  DCHECK_EQ(0, entry->skeleton.modrm_opcode);
  DCHECK_EQ(0, entry->skeleton.ax_opcode);
}

void X86Mir2Lir::EmitMacro(const X86EncodingMap* entry, uint8_t reg, int offset) {
  DCHECK(entry->opcode == kX86StartOfMethod) << entry->name;
  code_buffer_.push_back(0xE8);  // call +0
  code_buffer_.push_back(0);
  code_buffer_.push_back(0);
  code_buffer_.push_back(0);
  code_buffer_.push_back(0);

  DCHECK_LT(reg, 8);
  code_buffer_.push_back(0x58 + reg);  // pop reg

  EmitRegImm(&X86Mir2Lir::EncodingMap[kX86Sub32RI], reg, offset + 5 /* size of call +0 */);
}

void X86Mir2Lir::EmitUnimplemented(const X86EncodingMap* entry, LIR* lir) {
  UNIMPLEMENTED(WARNING) << "encoding kind for " << entry->name << " "
                         << BuildInsnString(entry->fmt, lir, 0);
  for (int i = 0; i < GetInsnSize(lir); ++i) {
    code_buffer_.push_back(0xCC);  // push breakpoint instruction - int 3
  }
}

/*
 * Assemble the LIR into binary instruction format.  Note that we may
 * discover that pc-relative displacements may not fit the selected
 * instruction.  In those cases we will try to substitute a new code
 * sequence or request that the trace be shortened and retried.
 */
AssemblerStatus X86Mir2Lir::AssembleInstructions(uintptr_t start_addr) {
  LIR *lir;
  AssemblerStatus res = kSuccess;  // Assume success

  const bool kVerbosePcFixup = false;
  for (lir = first_lir_insn_; lir != NULL; lir = NEXT_LIR(lir)) {
    if (lir->opcode < 0) {
      continue;
    }

    if (lir->flags.is_nop) {
      continue;
    }

    if (lir->flags.pcRelFixup) {
      switch (lir->opcode) {
        case kX86Jcc8: {
          LIR *target_lir = lir->target;
          DCHECK(target_lir != NULL);
          int delta = 0;
          uintptr_t pc;
          if (IS_SIMM8(lir->operands[0])) {
            pc = lir->offset + 2 /* opcode + rel8 */;
          } else {
            pc = lir->offset + 6 /* 2 byte opcode + rel32 */;
          }
          uintptr_t target = target_lir->offset;
          delta = target - pc;
          if (IS_SIMM8(delta) != IS_SIMM8(lir->operands[0])) {
            if (kVerbosePcFixup) {
              LOG(INFO) << "Retry for JCC growth at " << lir->offset
                  << " delta: " << delta << " old delta: " << lir->operands[0];
            }
            lir->opcode = kX86Jcc32;
            SetupResourceMasks(lir);
            res = kRetryAll;
          }
          if (kVerbosePcFixup) {
            LOG(INFO) << "Source:";
            DumpLIRInsn(lir, 0);
            LOG(INFO) << "Target:";
            DumpLIRInsn(target_lir, 0);
            LOG(INFO) << "Delta " << delta;
          }
          lir->operands[0] = delta;
          break;
        }
        case kX86Jcc32: {
          LIR *target_lir = lir->target;
          DCHECK(target_lir != NULL);
          uintptr_t pc = lir->offset + 6 /* 2 byte opcode + rel32 */;
          uintptr_t target = target_lir->offset;
          int delta = target - pc;
          if (kVerbosePcFixup) {
            LOG(INFO) << "Source:";
            DumpLIRInsn(lir, 0);
            LOG(INFO) << "Target:";
            DumpLIRInsn(target_lir, 0);
            LOG(INFO) << "Delta " << delta;
          }
          lir->operands[0] = delta;
          break;
        }
        case kX86Jmp8: {
          LIR *target_lir = lir->target;
          DCHECK(target_lir != NULL);
          int delta = 0;
          uintptr_t pc;
          if (IS_SIMM8(lir->operands[0])) {
            pc = lir->offset + 2 /* opcode + rel8 */;
          } else {
            pc = lir->offset + 5 /* opcode + rel32 */;
          }
          uintptr_t target = target_lir->offset;
          delta = target - pc;
          if (!(cu_->disable_opt & (1 << kSafeOptimizations)) && delta == 0) {
            // Useless branch
            lir->flags.is_nop = true;
            if (kVerbosePcFixup) {
              LOG(INFO) << "Retry for useless branch at " << lir->offset;
            }
            res = kRetryAll;
          } else if (IS_SIMM8(delta) != IS_SIMM8(lir->operands[0])) {
            if (kVerbosePcFixup) {
              LOG(INFO) << "Retry for JMP growth at " << lir->offset;
            }
            lir->opcode = kX86Jmp32;
            SetupResourceMasks(lir);
            res = kRetryAll;
          }
          lir->operands[0] = delta;
          break;
        }
        case kX86Jmp32: {
          LIR *target_lir = lir->target;
          DCHECK(target_lir != NULL);
          uintptr_t pc = lir->offset + 5 /* opcode + rel32 */;
          uintptr_t target = target_lir->offset;
          int delta = target - pc;
          lir->operands[0] = delta;
          break;
        }
        default:
          break;
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
    CHECK_EQ(static_cast<size_t>(lir->offset), code_buffer_.size());
    const X86EncodingMap *entry = &X86Mir2Lir::EncodingMap[lir->opcode];
    size_t starting_cbuf_size = code_buffer_.size();
    switch (entry->kind) {
      case kData:  // 4 bytes of data
        code_buffer_.push_back(lir->operands[0]);
        break;
      case kNullary:  // 1 byte of opcode
        DCHECK_EQ(0, entry->skeleton.prefix1);
        DCHECK_EQ(0, entry->skeleton.prefix2);
        code_buffer_.push_back(entry->skeleton.opcode);
        if (entry->skeleton.extra_opcode1 != 0) {
          code_buffer_.push_back(entry->skeleton.extra_opcode1);
          if (entry->skeleton.extra_opcode2 != 0) {
            code_buffer_.push_back(entry->skeleton.extra_opcode2);
          }
        } else {
          DCHECK_EQ(0, entry->skeleton.extra_opcode2);
        }
        DCHECK_EQ(0, entry->skeleton.modrm_opcode);
        DCHECK_EQ(0, entry->skeleton.ax_opcode);
        DCHECK_EQ(0, entry->skeleton.immediate_bytes);
        break;
      case kReg:  // lir operands - 0: reg
        EmitOpReg(entry, lir->operands[0]);
        break;
      case kMem:  // lir operands - 0: base, 1: disp
        EmitOpMem(entry, lir->operands[0], lir->operands[1]);
        break;
      case kMemReg:  // lir operands - 0: base, 1: disp, 2: reg
        EmitMemReg(entry, lir->operands[0], lir->operands[1], lir->operands[2]);
        break;
      case kArrayReg:  // lir operands - 0: base, 1: index, 2: scale, 3: disp, 4: reg
        EmitArrayReg(entry, lir->operands[0], lir->operands[1], lir->operands[2],
                     lir->operands[3], lir->operands[4]);
        break;
      case kRegMem:  // lir operands - 0: reg, 1: base, 2: disp
        EmitRegMem(entry, lir->operands[0], lir->operands[1], lir->operands[2]);
        break;
      case kRegArray:  // lir operands - 0: reg, 1: base, 2: index, 3: scale, 4: disp
        EmitRegArray(entry, lir->operands[0], lir->operands[1], lir->operands[2],
                     lir->operands[3], lir->operands[4]);
        break;
      case kRegThread:  // lir operands - 0: reg, 1: disp
        EmitRegThread(entry, lir->operands[0], lir->operands[1]);
        break;
      case kRegReg:  // lir operands - 0: reg1, 1: reg2
        EmitRegReg(entry, lir->operands[0], lir->operands[1]);
        break;
      case kRegRegStore:  // lir operands - 0: reg2, 1: reg1
        EmitRegReg(entry, lir->operands[1], lir->operands[0]);
        break;
      case kRegRegImm:
        EmitRegRegImm(entry, lir->operands[0], lir->operands[1], lir->operands[2]);
        break;
      case kRegImm:  // lir operands - 0: reg, 1: immediate
        EmitRegImm(entry, lir->operands[0], lir->operands[1]);
        break;
      case kThreadImm:  // lir operands - 0: disp, 1: immediate
        EmitThreadImm(entry, lir->operands[0], lir->operands[1]);
        break;
      case kMovRegImm:  // lir operands - 0: reg, 1: immediate
        EmitMovRegImm(entry, lir->operands[0], lir->operands[1]);
        break;
      case kShiftRegImm:  // lir operands - 0: reg, 1: immediate
        EmitShiftRegImm(entry, lir->operands[0], lir->operands[1]);
        break;
      case kShiftRegCl:  // lir operands - 0: reg, 1: cl
        EmitShiftRegCl(entry, lir->operands[0], lir->operands[1]);
        break;
      case kRegCond:  // lir operands - 0: reg, 1: condition
        EmitRegCond(entry, lir->operands[0], lir->operands[1]);
        break;
      case kJmp:  // lir operands - 0: rel
        if (entry->opcode == kX86JmpT) {
          // This works since the instruction format for jmp and call is basically the same and
          // EmitCallThread loads opcode info.
          EmitCallThread(entry, lir->operands[0]);
        } else {
          EmitJmp(entry, lir->operands[0]);
        }
        break;
      case kJcc:  // lir operands - 0: rel, 1: CC, target assigned
        EmitJcc(entry, lir->operands[0], lir->operands[1]);
        break;
      case kCall:
        switch (entry->opcode) {
          case kX86CallM:  // lir operands - 0: base, 1: disp
            EmitCallMem(entry, lir->operands[0], lir->operands[1]);
            break;
          case kX86CallT:  // lir operands - 0: disp
            EmitCallThread(entry, lir->operands[0]);
            break;
          default:
            EmitUnimplemented(entry, lir);
            break;
        }
        break;
      case kPcRel:  // lir operands - 0: reg, 1: base, 2: index, 3: scale, 4: table
        EmitPcRel(entry, lir->operands[0], lir->operands[1], lir->operands[2],
                  lir->operands[3], lir->operands[4]);
        break;
      case kMacro:
        EmitMacro(entry, lir->operands[0], lir->offset);
        break;
      default:
        EmitUnimplemented(entry, lir);
        break;
    }
    CHECK_EQ(static_cast<size_t>(GetInsnSize(lir)),
             code_buffer_.size() - starting_cbuf_size)
        << "Instruction size mismatch for entry: " << X86Mir2Lir::EncodingMap[lir->opcode].name;
  }
  return res;
}

}  // namespace art
