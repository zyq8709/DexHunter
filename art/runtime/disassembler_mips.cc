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

#include "disassembler_mips.h"

#include <iostream>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "thread.h"

namespace art {
namespace mips {

struct MipsInstruction {
  uint32_t mask;
  uint32_t value;
  const char* name;
  const char* args_fmt;

  bool Matches(uint32_t instruction) const {
    return (instruction & mask) == value;
  }
};

static const uint32_t kOpcodeShift = 26;

static const uint32_t kCop1 = (17 << kOpcodeShift);

static const uint32_t kITypeMask = (0x3f << kOpcodeShift);
static const uint32_t kJTypeMask = (0x3f << kOpcodeShift);
static const uint32_t kRTypeMask = ((0x3f << kOpcodeShift) | (0x3f));
static const uint32_t kSpecial2Mask = (0x3f << kOpcodeShift);
static const uint32_t kFpMask = kRTypeMask;

static const MipsInstruction gMipsInstructions[] = {
  // "sll r0, r0, 0" is the canonical "nop", used in delay slots.
  { 0xffffffff, 0, "nop", "" },

  // R-type instructions.
  { kRTypeMask, 0, "sll", "DTA", },
  // 0, 1, movci
  { kRTypeMask, 2, "srl", "DTA", },
  { kRTypeMask, 3, "sra", "DTA", },
  { kRTypeMask, 4, "sllv", "DTS", },
  { kRTypeMask, 6, "srlv", "DTS", },
  { kRTypeMask, 7, "srav", "DTS", },
  { kRTypeMask, 8, "jr", "S", },
  { kRTypeMask | (0x1f << 11), 9 | (31 << 11), "jalr", "S", },  // rd = 31 is implicit.
  { kRTypeMask, 9, "jalr", "DS", },  // General case.
  { kRTypeMask | (0x1f << 6), 10, "movz", "DST", },
  { kRTypeMask | (0x1f << 6), 11, "movn", "DST", },
  { kRTypeMask, 12, "syscall", "", },  // TODO: code
  { kRTypeMask, 13, "break", "", },  // TODO: code
  { kRTypeMask, 15, "sync", "", },  // TODO: type
  { kRTypeMask, 16, "mfhi", "D", },
  { kRTypeMask, 17, "mthi", "S", },
  { kRTypeMask, 18, "mflo", "D", },
  { kRTypeMask, 19, "mtlo", "S", },
  { kRTypeMask, 24, "mult", "ST", },
  { kRTypeMask, 25, "multu", "ST", },
  { kRTypeMask, 26, "div", "ST", },
  { kRTypeMask, 27, "divu", "ST", },
  { kRTypeMask, 32, "add", "DST", },
  { kRTypeMask, 33, "addu", "DST", },
  { kRTypeMask, 34, "sub", "DST", },
  { kRTypeMask, 35, "subu", "DST", },
  { kRTypeMask, 36, "and", "DST", },
  { kRTypeMask, 37, "or", "DST", },
  { kRTypeMask, 38, "xor", "DST", },
  { kRTypeMask, 39, "nor", "DST", },
  { kRTypeMask, 42, "slt", "DST", },
  { kRTypeMask, 43, "sltu", "DST", },
  // 0, 48, tge
  // 0, 49, tgeu
  // 0, 50, tlt
  // 0, 51, tltu
  // 0, 52, teq
  // 0, 54, tne

  // SPECIAL2
  { kSpecial2Mask | 0x7ff, (28 << kOpcodeShift) | 2, "mul", "DST" },
  { kSpecial2Mask | 0x7ff, (28 << kOpcodeShift) | 32, "clz", "DS" },
  { kSpecial2Mask | 0xffff, (28 << kOpcodeShift) | 0, "madd", "ST" },
  { kSpecial2Mask | 0xffff, (28 << kOpcodeShift) | 1, "maddu", "ST" },
  { kSpecial2Mask | 0xffff, (28 << kOpcodeShift) | 2, "mul", "DST" },
  { kSpecial2Mask | 0xffff, (28 << kOpcodeShift) | 4, "msub", "ST" },
  { kSpecial2Mask | 0xffff, (28 << kOpcodeShift) | 5, "msubu", "ST" },
  { kSpecial2Mask | 0x3f, (28 << kOpcodeShift) | 0x3f, "sdbbp", "" },  // TODO: code

  // J-type instructions.
  { kJTypeMask, 2 << kOpcodeShift, "j", "L" },
  { kJTypeMask, 3 << kOpcodeShift, "jal", "L" },

  // I-type instructions.
  { kITypeMask, 4 << kOpcodeShift, "beq", "STB" },
  { kITypeMask, 5 << kOpcodeShift, "bne", "STB" },
  { kITypeMask | (0x1f << 16), 1 << kOpcodeShift | (1 << 16), "bgez", "SB" },
  { kITypeMask | (0x1f << 16), 1 << kOpcodeShift | (0 << 16), "bltz", "SB" },
  { kITypeMask | (0x1f << 16), 1 << kOpcodeShift | (2 << 16), "bltzl", "SB" },
  { kITypeMask | (0x1f << 16), 1 << kOpcodeShift | (16 << 16), "bltzal", "SB" },
  { kITypeMask | (0x1f << 16), 1 << kOpcodeShift | (18 << 16), "bltzall", "SB" },
  { kITypeMask | (0x1f << 16), 6 << kOpcodeShift | (0 << 16), "blez", "SB" },
  { kITypeMask | (0x1f << 16), 7 << kOpcodeShift | (0 << 16), "bgtz", "SB" },

  { 0xffff0000, (4 << kOpcodeShift), "b", "B" },
  { 0xffff0000, (1 << kOpcodeShift) | (17 << 16), "bal", "B" },

  { kITypeMask, 8 << kOpcodeShift, "addi", "TSi", },
  { kITypeMask, 9 << kOpcodeShift, "addiu", "TSi", },
  { kITypeMask, 10 << kOpcodeShift, "slti", "TSi", },
  { kITypeMask, 11 << kOpcodeShift, "sltiu", "TSi", },
  { kITypeMask, 12 << kOpcodeShift, "andi", "TSi", },
  { kITypeMask, 13 << kOpcodeShift, "ori", "TSi", },
  { kITypeMask, 14 << kOpcodeShift, "ori", "TSi", },
  { kITypeMask, 15 << kOpcodeShift, "lui", "TI", },

  { kITypeMask, 32u << kOpcodeShift, "lb", "TO", },
  { kITypeMask, 33u << kOpcodeShift, "lh", "TO", },
  { kITypeMask, 35u << kOpcodeShift, "lw", "TO", },
  { kITypeMask, 36u << kOpcodeShift, "lbu", "TO", },
  { kITypeMask, 37u << kOpcodeShift, "lhu", "TO", },
  { kITypeMask, 40u << kOpcodeShift, "sb", "TO", },
  { kITypeMask, 41u << kOpcodeShift, "sh", "TO", },
  { kITypeMask, 43u << kOpcodeShift, "sw", "TO", },
  { kITypeMask, 49u << kOpcodeShift, "lwc1", "tO", },
  { kITypeMask, 57u << kOpcodeShift, "swc1", "tO", },

  // Floating point.
  { kFpMask,                kCop1 | 0, "add", "fdst" },
  { kFpMask,                kCop1 | 1, "sub", "fdst" },
  { kFpMask,                kCop1 | 2, "mul", "fdst" },
  { kFpMask,                kCop1 | 3, "div", "fdst" },
  { kFpMask | (0x1f << 16), kCop1 | 4, "sqrt", "fdst" },
  { kFpMask | (0x1f << 16), kCop1 | 5, "abs", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 6, "mov", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 7, "neg", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 8, "round.l", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 9, "trunc.l", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 10, "ceil.l", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 11, "floor.l", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 12, "round.w", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 13, "trunc.w", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 14, "ceil.w", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 15, "floor.w", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 32, "cvt.s", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 33, "cvt.d", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 36, "cvt.w", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 37, "cvt.l", "fds" },
  { kFpMask | (0x1f << 16), kCop1 | 38, "cvt.ps", "fds" },
};

static uint32_t ReadU32(const uint8_t* ptr) {
  // We only support little-endian MIPS.
  return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
}

static void DumpMips(std::ostream& os, const uint8_t* instr_ptr) {
  uint32_t instruction = ReadU32(instr_ptr);

  uint32_t rs = (instruction >> 21) & 0x1f;  // I-type, R-type.
  uint32_t rt = (instruction >> 16) & 0x1f;  // I-type, R-type.
  uint32_t rd = (instruction >> 11) & 0x1f;  // R-type.
  uint32_t sa = (instruction >>  6) & 0x1f;  // R-type.

  std::string opcode;
  std::ostringstream args;

  // TODO: remove this!
  uint32_t op = (instruction >> 26) & 0x3f;
  uint32_t function = (instruction & 0x3f);  // R-type.
  opcode = StringPrintf("op=%d fn=%d", op, function);

  for (size_t i = 0; i < arraysize(gMipsInstructions); ++i) {
    if (gMipsInstructions[i].Matches(instruction)) {
      opcode = gMipsInstructions[i].name;
      for (const char* args_fmt = gMipsInstructions[i].args_fmt; *args_fmt; ++args_fmt) {
        switch (*args_fmt) {
          case 'A':  // sa (shift amount).
            args << sa;
            break;
          case 'B':  // Branch offset.
            {
              int32_t offset = static_cast<int16_t>(instruction & 0xffff);
              offset <<= 2;
              offset += 4;  // Delay slot.
              args << StringPrintf("%p  ; %+d", instr_ptr + offset, offset);
            }
            break;
          case 'D': args << 'r' << rd; break;
          case 'd': args << 'f' << rd; break;
          case 'f':  // Floating point "fmt".
            {
              size_t fmt = (instruction >> 21) & 0x7;  // TODO: other fmts?
              switch (fmt) {
                case 0: opcode += ".s"; break;
                case 1: opcode += ".d"; break;
                case 4: opcode += ".w"; break;
                case 5: opcode += ".l"; break;
                case 6: opcode += ".ps"; break;
                default: opcode += ".?"; break;
              }
              continue;  // No ", ".
            }
            break;
          case 'I':  // Upper 16-bit immediate.
            args << reinterpret_cast<void*>((instruction & 0xffff) << 16);
            break;
          case 'i':  // Sign-extended lower 16-bit immediate.
            args << static_cast<int16_t>(instruction & 0xffff);
            break;
          case 'L':  // Jump label.
            {
              // TODO: is this right?
              uint32_t instr_index = (instruction & 0x1ffffff);
              uint32_t target = (instr_index << 2);
              target |= (reinterpret_cast<uintptr_t>(instr_ptr + 4) & 0xf0000000);
              args << reinterpret_cast<void*>(target);
            }
            break;
          case 'O':  // +x(rs)
            {
              int32_t offset = static_cast<int16_t>(instruction & 0xffff);
              args << StringPrintf("%+d(r%d)", offset, rs);
              if (rs == 17) {
                args << "  ; ";
                Thread::DumpThreadOffset(args, offset, 4);
              }
            }
            break;
          case 'S': args << 'r' << rs; break;
          case 's': args << 'f' << rs; break;
          case 'T': args << 'r' << rt; break;
          case 't': args << 'f' << rt; break;
        }
        if (*(args_fmt + 1)) {
          args << ", ";
        }
      }
      break;
    }
  }

  os << StringPrintf("%p: %08x\t%-7s ", instr_ptr, instruction, opcode.c_str()) << args.str() << '\n';
}

DisassemblerMips::DisassemblerMips() {
}

size_t DisassemblerMips::Dump(std::ostream& os, const uint8_t* begin) {
  DumpMips(os, begin);
  return 4;
}

void DisassemblerMips::Dump(std::ostream& os, const uint8_t* begin, const uint8_t* end) {
  for (const uint8_t* cur = begin; cur < end; cur += 4) {
    DumpMips(os, cur);
  }
}

}  // namespace mips
}  // namespace art
