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

#include "disassembler_x86.h"

#include <iostream>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "thread.h"

namespace art {
namespace x86 {

DisassemblerX86::DisassemblerX86() {}

size_t DisassemblerX86::Dump(std::ostream& os, const uint8_t* begin) {
  return DumpInstruction(os, begin);
}

void DisassemblerX86::Dump(std::ostream& os, const uint8_t* begin, const uint8_t* end) {
  size_t length = 0;
  for (const uint8_t* cur = begin; cur < end; cur += length) {
    length = DumpInstruction(os, cur);
  }
}

static const char* gReg8Names[]  = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" };
static const char* gReg16Names[] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" };
static const char* gReg32Names[] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi" };

static void DumpReg0(std::ostream& os, uint8_t /*rex*/, size_t reg,
                     bool byte_operand, uint8_t size_override) {
  DCHECK_LT(reg, 8u);
  // TODO: combine rex into size
  size_t size = byte_operand ? 1 : (size_override == 0x66 ? 2 : 4);
  switch (size) {
    case 1: os << gReg8Names[reg]; break;
    case 2: os << gReg16Names[reg]; break;
    case 4: os << gReg32Names[reg]; break;
    default: LOG(FATAL) << "unexpected size " << size;
  }
}

enum RegFile { GPR, MMX, SSE };

static void DumpReg(std::ostream& os, uint8_t rex, uint8_t reg,
                    bool byte_operand, uint8_t size_override, RegFile reg_file) {
  size_t reg_num = reg;  // TODO: combine with REX.R on 64bit
  if (reg_file == GPR) {
    DumpReg0(os, rex, reg_num, byte_operand, size_override);
  } else if (reg_file == SSE) {
    os << "xmm" << reg_num;
  } else {
    os << "mm" << reg_num;
  }
}

static void DumpBaseReg(std::ostream& os, uint8_t rex, uint8_t reg) {
  size_t reg_num = reg;  // TODO: combine with REX.B on 64bit
  DumpReg0(os, rex, reg_num, false, 0);
}

static void DumpIndexReg(std::ostream& os, uint8_t rex, uint8_t reg) {
  int reg_num = reg;  // TODO: combine with REX.X on 64bit
  DumpReg0(os, rex, reg_num, false, 0);
}

enum SegmentPrefix {
  kCs = 0x2e,
  kSs = 0x36,
  kDs = 0x3e,
  kEs = 0x26,
  kFs = 0x64,
  kGs = 0x65,
};

static void DumpSegmentOverride(std::ostream& os, uint8_t segment_prefix) {
  switch (segment_prefix) {
    case kCs: os << "cs:"; break;
    case kSs: os << "ss:"; break;
    case kDs: os << "ds:"; break;
    case kEs: os << "es:"; break;
    case kFs: os << "fs:"; break;
    case kGs: os << "gs:"; break;
    default: break;
  }
}

size_t DisassemblerX86::DumpInstruction(std::ostream& os, const uint8_t* instr) {
  const uint8_t* begin_instr = instr;
  bool have_prefixes = true;
  uint8_t prefix[4] = {0, 0, 0, 0};
  const char** modrm_opcodes = NULL;
  do {
    switch (*instr) {
        // Group 1 - lock and repeat prefixes:
      case 0xF0:
      case 0xF2:
      case 0xF3:
        prefix[0] = *instr;
        break;
        // Group 2 - segment override prefixes:
      case kCs:
      case kSs:
      case kDs:
      case kEs:
      case kFs:
      case kGs:
        prefix[1] = *instr;
        break;
        // Group 3 - operand size override:
      case 0x66:
        prefix[2] = *instr;
        break;
        // Group 4 - address size override:
      case 0x67:
        prefix[3] = *instr;
        break;
      default:
        have_prefixes = false;
        break;
    }
    if (have_prefixes) {
      instr++;
    }
  } while (have_prefixes);
  uint8_t rex = (*instr >= 0x40 && *instr <= 0x4F) ? *instr : 0;
  bool has_modrm = false;
  bool reg_is_opcode = false;
  size_t immediate_bytes = 0;
  size_t branch_bytes = 0;
  std::ostringstream opcode;
  bool store = false;  // stores to memory (ie rm is on the left)
  bool load = false;  // loads from memory (ie rm is on the right)
  bool byte_operand = false;
  bool ax = false;  // implicit use of ax
  bool cx = false;  // implicit use of cx
  bool reg_in_opcode = false;  // low 3-bits of opcode encode register parameter
  bool no_ops = false;
  RegFile src_reg_file = GPR;
  RegFile dst_reg_file = GPR;
  switch (*instr) {
#define DISASSEMBLER_ENTRY(opname, \
                     rm8_r8, rm32_r32, \
                     r8_rm8, r32_rm32, \
                     ax8_i8, ax32_i32) \
  case rm8_r8:   opcode << #opname; store = true; has_modrm = true; byte_operand = true; break; \
  case rm32_r32: opcode << #opname; store = true; has_modrm = true; break; \
  case r8_rm8:   opcode << #opname; load = true; has_modrm = true; byte_operand = true; break; \
  case r32_rm32: opcode << #opname; load = true; has_modrm = true; break; \
  case ax8_i8:   opcode << #opname; ax = true; immediate_bytes = 1; byte_operand = true; break; \
  case ax32_i32: opcode << #opname; ax = true; immediate_bytes = 4; break;

DISASSEMBLER_ENTRY(add,
  0x00 /* RegMem8/Reg8 */,     0x01 /* RegMem32/Reg32 */,
  0x02 /* Reg8/RegMem8 */,     0x03 /* Reg32/RegMem32 */,
  0x04 /* Rax8/imm8 opcode */, 0x05 /* Rax32/imm32 */)
DISASSEMBLER_ENTRY(or,
  0x08 /* RegMem8/Reg8 */,     0x09 /* RegMem32/Reg32 */,
  0x0A /* Reg8/RegMem8 */,     0x0B /* Reg32/RegMem32 */,
  0x0C /* Rax8/imm8 opcode */, 0x0D /* Rax32/imm32 */)
DISASSEMBLER_ENTRY(adc,
  0x10 /* RegMem8/Reg8 */,     0x11 /* RegMem32/Reg32 */,
  0x12 /* Reg8/RegMem8 */,     0x13 /* Reg32/RegMem32 */,
  0x14 /* Rax8/imm8 opcode */, 0x15 /* Rax32/imm32 */)
DISASSEMBLER_ENTRY(sbb,
  0x18 /* RegMem8/Reg8 */,     0x19 /* RegMem32/Reg32 */,
  0x1A /* Reg8/RegMem8 */,     0x1B /* Reg32/RegMem32 */,
  0x1C /* Rax8/imm8 opcode */, 0x1D /* Rax32/imm32 */)
DISASSEMBLER_ENTRY(and,
  0x20 /* RegMem8/Reg8 */,     0x21 /* RegMem32/Reg32 */,
  0x22 /* Reg8/RegMem8 */,     0x23 /* Reg32/RegMem32 */,
  0x24 /* Rax8/imm8 opcode */, 0x25 /* Rax32/imm32 */)
DISASSEMBLER_ENTRY(sub,
  0x28 /* RegMem8/Reg8 */,     0x29 /* RegMem32/Reg32 */,
  0x2A /* Reg8/RegMem8 */,     0x2B /* Reg32/RegMem32 */,
  0x2C /* Rax8/imm8 opcode */, 0x2D /* Rax32/imm32 */)
DISASSEMBLER_ENTRY(xor,
  0x30 /* RegMem8/Reg8 */,     0x31 /* RegMem32/Reg32 */,
  0x32 /* Reg8/RegMem8 */,     0x33 /* Reg32/RegMem32 */,
  0x34 /* Rax8/imm8 opcode */, 0x35 /* Rax32/imm32 */)
DISASSEMBLER_ENTRY(cmp,
  0x38 /* RegMem8/Reg8 */,     0x39 /* RegMem32/Reg32 */,
  0x3A /* Reg8/RegMem8 */,     0x3B /* Reg32/RegMem32 */,
  0x3C /* Rax8/imm8 opcode */, 0x3D /* Rax32/imm32 */)

#undef DISASSEMBLER_ENTRY
  case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
    opcode << "push";
    reg_in_opcode = true;
    break;
  case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
    opcode << "pop";
    reg_in_opcode = true;
    break;
  case 0x68: opcode << "push"; immediate_bytes = 4; break;
  case 0x6A: opcode << "push"; immediate_bytes = 1; break;
  case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
  case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
    static const char* condition_codes[] =
    {"o", "no", "b/nae/c", "nb/ae/nc", "z/eq",  "nz/ne", "be/na", "nbe/a",
     "s", "ns", "p/pe",    "np/po",    "l/nge", "nl/ge", "le/ng", "nle/g"
    };
    opcode << "j" << condition_codes[*instr & 0xF];
    branch_bytes = 1;
    break;
  case 0x88: opcode << "mov"; store = true; has_modrm = true; byte_operand = true; break;
  case 0x89: opcode << "mov"; store = true; has_modrm = true; break;
  case 0x8A: opcode << "mov"; load = true; has_modrm = true; byte_operand = true; break;
  case 0x8B: opcode << "mov"; load = true; has_modrm = true; break;

  case 0x0F:  // 2 byte extended opcode
    instr++;
    switch (*instr) {
      case 0x10: case 0x11:
        if (prefix[0] == 0xF2) {
          opcode << "movsd";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF3) {
          opcode << "movss";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[2] == 0x66) {
          opcode << "movupd";
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          opcode << "movups";
        }
        has_modrm = true;
        src_reg_file = dst_reg_file = SSE;
        load = *instr == 0x10;
        store = !load;
        break;
      case 0x2A:
        if (prefix[2] == 0x66) {
          opcode << "cvtpi2pd";
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF2) {
          opcode << "cvtsi2sd";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF3) {
          opcode << "cvtsi2ss";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          opcode << "cvtpi2ps";
        }
        load = true;
        has_modrm = true;
        dst_reg_file = SSE;
        break;
      case 0x2C:
        if (prefix[2] == 0x66) {
          opcode << "cvttpd2pi";
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF2) {
          opcode << "cvttsd2si";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF3) {
          opcode << "cvttss2si";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          opcode << "cvttps2pi";
        }
        load = true;
        has_modrm = true;
        src_reg_file = SSE;
        break;
      case 0x2D:
        if (prefix[2] == 0x66) {
          opcode << "cvtpd2pi";
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF2) {
          opcode << "cvtsd2si";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF3) {
          opcode << "cvtss2si";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          opcode << "cvtps2pi";
        }
        load = true;
        has_modrm = true;
        src_reg_file = SSE;
        break;
      case 0x2E:
        opcode << "u";
        // FALLTHROUGH
      case 0x2F:
        if (prefix[2] == 0x66) {
          opcode << "comisd";
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          opcode << "comiss";
        }
        has_modrm = true;
        load = true;
        src_reg_file = dst_reg_file = SSE;
        break;
      case 0x38:  // 3 byte extended opcode
        opcode << StringPrintf("unknown opcode '0F 38 %02X'", *instr);
        break;
      case 0x3A:  // 3 byte extended opcode
        opcode << StringPrintf("unknown opcode '0F 3A %02X'", *instr);
        break;
      case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
      case 0x58: case 0x59: case 0x5C: case 0x5D: case 0x5E: case 0x5F: {
        switch (*instr) {
          case 0x50: opcode << "movmsk"; break;
          case 0x51: opcode << "sqrt"; break;
          case 0x52: opcode << "rsqrt"; break;
          case 0x53: opcode << "rcp"; break;
          case 0x54: opcode << "and"; break;
          case 0x55: opcode << "andn"; break;
          case 0x56: opcode << "or"; break;
          case 0x57: opcode << "xor"; break;
          case 0x58: opcode << "add"; break;
          case 0x59: opcode << "mul"; break;
          case 0x5C: opcode << "sub"; break;
          case 0x5D: opcode << "min"; break;
          case 0x5E: opcode << "div"; break;
          case 0x5F: opcode << "max"; break;
          default: LOG(FATAL) << "Unreachable";
        }
        if (prefix[2] == 0x66) {
          opcode << "pd";
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF2) {
          opcode << "sd";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF3) {
          opcode << "ss";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          opcode << "ps";
        }
        load = true;
        has_modrm = true;
        src_reg_file = dst_reg_file = SSE;
        break;
      }
      case 0x5A:
        if (prefix[2] == 0x66) {
          opcode << "cvtpd2ps";
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF2) {
          opcode << "cvtsd2ss";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF3) {
          opcode << "cvtss2sd";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          opcode << "cvtps2pd";
        }
        load = true;
        has_modrm = true;
        src_reg_file = dst_reg_file = SSE;
        break;
      case 0x5B:
        if (prefix[2] == 0x66) {
          opcode << "cvtps2dq";
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF2) {
          opcode << "bad opcode F2 0F 5B";
        } else if (prefix[0] == 0xF3) {
          opcode << "cvttps2dq";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          opcode << "cvtdq2ps";
        }
        load = true;
        has_modrm = true;
        src_reg_file = dst_reg_file = SSE;
        break;
      case 0x6E:
        if (prefix[2] == 0x66) {
          dst_reg_file = SSE;
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          dst_reg_file = MMX;
        }
        opcode << "movd";
        load = true;
        has_modrm = true;
        break;
      case 0x6F:
        if (prefix[2] == 0x66) {
          dst_reg_file = SSE;
          opcode << "movdqa";
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else if (prefix[0] == 0xF3) {
          dst_reg_file = SSE;
          opcode << "movdqu";
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          dst_reg_file = MMX;
          opcode << "movq";
        }
        load = true;
        has_modrm = true;
        break;
      case 0x71:
        if (prefix[2] == 0x66) {
          dst_reg_file = SSE;
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          dst_reg_file = MMX;
        }
        static const char* x71_opcodes[] = {"unknown-71", "unknown-71", "psrlw", "unknown-71", "psraw", "unknown-71", "psllw", "unknown-71"};
        modrm_opcodes = x71_opcodes;
        reg_is_opcode = true;
        has_modrm = true;
        store = true;
        immediate_bytes = 1;
        break;
      case 0x72:
        if (prefix[2] == 0x66) {
          dst_reg_file = SSE;
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          dst_reg_file = MMX;
        }
        static const char* x72_opcodes[] = {"unknown-72", "unknown-72", "psrld", "unknown-72", "psrad", "unknown-72", "pslld", "unknown-72"};
        modrm_opcodes = x72_opcodes;
        reg_is_opcode = true;
        has_modrm = true;
        store = true;
        immediate_bytes = 1;
        break;
      case 0x73:
        if (prefix[2] == 0x66) {
          dst_reg_file = SSE;
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          dst_reg_file = MMX;
        }
        static const char* x73_opcodes[] = {"unknown-73", "unknown-73", "psrlq", "unknown-73", "unknown-73", "unknown-73", "psllq", "unknown-73"};
        modrm_opcodes = x73_opcodes;
        reg_is_opcode = true;
        has_modrm = true;
        store = true;
        immediate_bytes = 1;
        break;
      case 0x7E:
        if (prefix[2] == 0x66) {
          src_reg_file = SSE;
          prefix[2] = 0;  // clear prefix now it's served its purpose as part of the opcode
        } else {
          src_reg_file = MMX;
        }
        opcode << "movd";
        has_modrm = true;
        store = true;
        break;
      case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
      case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F:
        opcode << "j" << condition_codes[*instr & 0xF];
        branch_bytes = 4;
        break;
      case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
      case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E: case 0x9F:
        opcode << "set" << condition_codes[*instr & 0xF];
        modrm_opcodes = NULL;
        reg_is_opcode = true;
        has_modrm = true;
        store = true;
        break;
      case 0xAE:
        if (prefix[0] == 0xF3) {
          prefix[0] = 0;  // clear prefix now it's served its purpose as part of the opcode
          static const char* xAE_opcodes[] = {"rdfsbase", "rdgsbase", "wrfsbase", "wrgsbase", "unknown-AE", "unknown-AE", "unknown-AE", "unknown-AE"};
          modrm_opcodes = xAE_opcodes;
          reg_is_opcode = true;
          has_modrm = true;
          uint8_t reg_or_opcode = (instr[1] >> 3) & 7;
          switch (reg_or_opcode) {
            case 0:
              prefix[1] = kFs;
              load = true;
              break;
            case 1:
              prefix[1] = kGs;
              load = true;
              break;
            case 2:
              prefix[1] = kFs;
              store = true;
              break;
            case 3:
              prefix[1] = kGs;
              store = true;
              break;
            default:
              load = true;
              break;
          }
        } else {
          static const char* xAE_opcodes[] = {"unknown-AE", "unknown-AE", "unknown-AE", "unknown-AE", "unknown-AE", "lfence", "mfence", "sfence"};
          modrm_opcodes = xAE_opcodes;
          reg_is_opcode = true;
          has_modrm = true;
          load = true;
          no_ops = true;
        }
        break;
      case 0xB1: opcode << "cmpxchg"; has_modrm = true; store = true; break;
      case 0xB6: opcode << "movzxb"; has_modrm = true; load = true; break;
      case 0xB7: opcode << "movzxw"; has_modrm = true; load = true; break;
      case 0xBE: opcode << "movsxb"; has_modrm = true; load = true; break;
      case 0xBF: opcode << "movsxw"; has_modrm = true; load = true; break;
      default:
        opcode << StringPrintf("unknown opcode '0F %02X'", *instr);
        break;
    }
    break;
  case 0x80: case 0x81: case 0x82: case 0x83:
    static const char* x80_opcodes[] = {"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"};
    modrm_opcodes = x80_opcodes;
    has_modrm = true;
    reg_is_opcode = true;
    store = true;
    byte_operand = (*instr & 1) == 0;
    immediate_bytes = *instr == 0x81 ? 4 : 1;
    break;
  case 0x84: case 0x85:
    opcode << "test";
    has_modrm = true;
    load = true;
    byte_operand = (*instr & 1) == 0;
    break;
  case 0x8D:
    opcode << "lea";
    has_modrm = true;
    load = true;
    break;
  case 0x8F:
    opcode << "pop";
    has_modrm = true;
    reg_is_opcode = true;
    store = true;
    break;
  case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
    opcode << "mov";
    immediate_bytes = 1;
    reg_in_opcode = true;
    break;
  case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
    opcode << "mov";
    immediate_bytes = 4;
    reg_in_opcode = true;
    break;
  case 0xC0: case 0xC1:
  case 0xD0: case 0xD1: case 0xD2: case 0xD3:
    static const char* shift_opcodes[] =
        {"rol", "ror", "rcl", "rcr", "shl", "shr", "unknown-shift", "sar"};
    modrm_opcodes = shift_opcodes;
    has_modrm = true;
    reg_is_opcode = true;
    store = true;
    immediate_bytes = ((*instr & 0xf0) == 0xc0) ? 1 : 0;
    cx = (*instr == 0xD2) || (*instr == 0xD3);
    byte_operand = (*instr == 0xC0);
    break;
  case 0xC3: opcode << "ret"; break;
  case 0xC7:
    static const char* c7_opcodes[] = {"mov", "unknown-c7", "unknown-c7", "unknown-c7", "unknown-c7", "unknown-c7", "unknown-c7", "unknown-c7"};
    modrm_opcodes = c7_opcodes;
    store = true;
    immediate_bytes = 4;
    has_modrm = true;
    reg_is_opcode = true;
    break;
  case 0xCC: opcode << "int 3"; break;
  case 0xE8: opcode << "call"; branch_bytes = 4; break;
  case 0xE9: opcode << "jmp"; branch_bytes = 4; break;
  case 0xEB: opcode << "jmp"; branch_bytes = 1; break;
  case 0xF5: opcode << "cmc"; break;
  case 0xF6: case 0xF7:
    static const char* f7_opcodes[] = {"test", "unknown-f7", "not", "neg", "mul edx:eax, eax *", "imul edx:eax, eax *", "div edx:eax, edx:eax /", "idiv edx:eax, edx:eax /"};
    modrm_opcodes = f7_opcodes;
    has_modrm = true;
    reg_is_opcode = true;
    store = true;
    immediate_bytes = ((instr[1] & 0x38) == 0) ? 1 : 0;
    break;
  case 0xFF:
    static const char* ff_opcodes[] = {"inc", "dec", "call", "call", "jmp", "jmp", "push", "unknown-ff"};
    modrm_opcodes = ff_opcodes;
    has_modrm = true;
    reg_is_opcode = true;
    load = true;
    break;
  default:
    opcode << StringPrintf("unknown opcode '%02X'", *instr);
    break;
  }
  std::ostringstream args;
  if (reg_in_opcode) {
    DCHECK(!has_modrm);
    DumpReg(args, rex, *instr & 0x7, false, prefix[2], GPR);
  }
  instr++;
  uint32_t address_bits = 0;
  if (has_modrm) {
    uint8_t modrm = *instr;
    instr++;
    uint8_t mod = modrm >> 6;
    uint8_t reg_or_opcode = (modrm >> 3) & 7;
    uint8_t rm = modrm & 7;
    std::ostringstream address;
    if (mod == 0 && rm == 5) {  // fixed address
      address_bits = *reinterpret_cast<const uint32_t*>(instr);
      address << StringPrintf("[0x%x]", address_bits);
      instr += 4;
    } else if (rm == 4 && mod != 3) {  // SIB
      uint8_t sib = *instr;
      instr++;
      uint8_t ss = (sib >> 6) & 3;
      uint8_t index = (sib >> 3) & 7;
      uint8_t base = sib & 7;
      address << "[";
      if (base != 5 || mod != 0) {
        DumpBaseReg(address, rex, base);
        if (index != 4) {
          address << " + ";
        }
      }
      if (index != 4) {
        DumpIndexReg(address, rex, index);
        if (ss != 0) {
          address << StringPrintf(" * %d", 1 << ss);
        }
      }
      if (mod == 1) {
        address << StringPrintf(" + %d", *reinterpret_cast<const int8_t*>(instr));
        instr++;
      } else if (mod == 2) {
        address << StringPrintf(" + %d", *reinterpret_cast<const int32_t*>(instr));
        instr += 4;
      }
      address << "]";
    } else {
      if (mod == 3) {
        if (!no_ops) {
          DumpReg(address, rex, rm, byte_operand, prefix[2], load ? src_reg_file : dst_reg_file);
        }
      } else {
        address << "[";
        DumpBaseReg(address, rex, rm);
        if (mod == 1) {
          address << StringPrintf(" + %d", *reinterpret_cast<const int8_t*>(instr));
          instr++;
        } else if (mod == 2) {
          address << StringPrintf(" + %d", *reinterpret_cast<const int32_t*>(instr));
          instr += 4;
        }
        address << "]";
      }
    }

    if (reg_is_opcode && modrm_opcodes != NULL) {
      opcode << modrm_opcodes[reg_or_opcode];
    }
    if (load) {
      if (!reg_is_opcode) {
        DumpReg(args, rex, reg_or_opcode, byte_operand, prefix[2], dst_reg_file);
        args << ", ";
      }
      DumpSegmentOverride(args, prefix[1]);
      args << address.str();
    } else {
      DCHECK(store);
      DumpSegmentOverride(args, prefix[1]);
      args << address.str();
      if (!reg_is_opcode) {
        args << ", ";
        DumpReg(args, rex, reg_or_opcode, byte_operand, prefix[2], src_reg_file);
      }
    }
  }
  if (ax) {
    // If this opcode implicitly uses ax, ax is always the first arg.
    DumpReg(args, rex, 0 /* EAX */, byte_operand, prefix[2], GPR);
  }
  if (cx) {
    args << ", ";
    DumpReg(args, rex, 1 /* ECX */, true, prefix[2], GPR);
  }
  if (immediate_bytes > 0) {
    if (has_modrm || reg_in_opcode || ax || cx) {
      args << ", ";
    }
    if (immediate_bytes == 1) {
      args << StringPrintf("%d", *reinterpret_cast<const int8_t*>(instr));
      instr++;
    } else {
      CHECK_EQ(immediate_bytes, 4u);
      args << StringPrintf("%d", *reinterpret_cast<const int32_t*>(instr));
      instr += 4;
    }
  } else if (branch_bytes > 0) {
    DCHECK(!has_modrm);
    int32_t displacement;
    if (branch_bytes == 1) {
      displacement = *reinterpret_cast<const int8_t*>(instr);
      instr++;
    } else {
      CHECK_EQ(branch_bytes, 4u);
      displacement = *reinterpret_cast<const int32_t*>(instr);
      instr += 4;
    }
    args << StringPrintf("%+d (%p)", displacement, instr + displacement);
  }
  if (prefix[1] == kFs) {
    args << "  ; ";
    Thread::DumpThreadOffset(args, address_bits, 4);
  }
  std::stringstream hex;
  for (size_t i = 0; begin_instr + i < instr; ++i) {
    hex << StringPrintf("%02X", begin_instr[i]);
  }
  std::stringstream prefixed_opcode;
  switch (prefix[0]) {
    case 0xF0: prefixed_opcode << "lock "; break;
    case 0xF2: prefixed_opcode << "repne "; break;
    case 0xF3: prefixed_opcode << "repe "; break;
    case 0: break;
    default: LOG(FATAL) << "Unreachable";
  }
  prefixed_opcode << opcode.str();
  os << StringPrintf("%p: %22s    \t%-7s ", begin_instr, hex.str().c_str(),
                     prefixed_opcode.str().c_str())
     << args.str() << '\n';
  return instr - begin_instr;
}  // NOLINT(readability/fn_size)

}  // namespace x86
}  // namespace art
