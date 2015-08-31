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

#include "dex_instruction-inl.h"

#include "dex_file-inl.h"
#include "utils.h"
#include <iomanip>

namespace art {

const char* const Instruction::kInstructionNames[] = {
#define INSTRUCTION_NAME(o, c, pname, f, r, i, a, v) pname,
#include "dex_instruction_list.h"
  DEX_INSTRUCTION_LIST(INSTRUCTION_NAME)
#undef DEX_INSTRUCTION_LIST
#undef INSTRUCTION_NAME
};

Instruction::Format const Instruction::kInstructionFormats[] = {
#define INSTRUCTION_FORMAT(o, c, p, format, r, i, a, v) format,
#include "dex_instruction_list.h"
  DEX_INSTRUCTION_LIST(INSTRUCTION_FORMAT)
#undef DEX_INSTRUCTION_LIST
#undef INSTRUCTION_FORMAT
};

int const Instruction::kInstructionFlags[] = {
#define INSTRUCTION_FLAGS(o, c, p, f, r, i, flags, v) flags,
#include "dex_instruction_list.h"
  DEX_INSTRUCTION_LIST(INSTRUCTION_FLAGS)
#undef DEX_INSTRUCTION_LIST
#undef INSTRUCTION_FLAGS
};

int const Instruction::kInstructionVerifyFlags[] = {
#define INSTRUCTION_VERIFY_FLAGS(o, c, p, f, r, i, a, vflags) vflags,
#include "dex_instruction_list.h"
  DEX_INSTRUCTION_LIST(INSTRUCTION_VERIFY_FLAGS)
#undef DEX_INSTRUCTION_LIST
#undef INSTRUCTION_VERIFY_FLAGS
};

int const Instruction::kInstructionSizeInCodeUnits[] = {
#define INSTRUCTION_SIZE(opcode, c, p, format, r, i, a, v) \
    ((opcode == NOP)                        ? -1 : \
     ((format >= k10x) && (format <= k10t)) ?  1 : \
     ((format >= k20t) && (format <= k22c)) ?  2 : \
     ((format >= k32x) && (format <= k3rc)) ?  3 : \
      (format == k51l)                      ?  5 : -1),
#include "dex_instruction_list.h"
  DEX_INSTRUCTION_LIST(INSTRUCTION_SIZE)
#undef DEX_INSTRUCTION_LIST
#undef INSTRUCTION_SIZE
};

/*
 * Handy macros for helping decode instructions.
 */
#define FETCH(_offset)      (insns[(_offset)])
#define FETCH_uint32(_offset)   (fetch_uint32_impl((_offset), insns))
#define INST_A(_insn)       (((uint16_t)(_insn) >> 8) & 0x0f)
#define INST_B(_insn)       ((uint16_t)(_insn) >> 12)
#define INST_AA(_insn)      ((_insn) >> 8)

/* Helper for FETCH_uint32, above. */
static inline uint32_t fetch_uint32_impl(uint32_t offset, const uint16_t* insns) {
  return insns[offset] | ((uint32_t) insns[offset+1] << 16);
}


bool Instruction::HasVRegC() const {
  switch (FormatOf(Opcode())) {
    case k23x: return true;
    case k35c: return true;
    case k3rc: return true;
    default: return false;
  }
}

bool Instruction::HasVRegB() const {
  switch (FormatOf(Opcode())) {
    case k12x: return true;
    case k22b: return true;
    case k22c: return true;
    case k22s: return true;
    case k22t: return true;
    case k22x: return true;
    case k23x: return true;
    case k32x: return true;
    default: return false;
  }
}

bool Instruction::HasVRegA() const {
  switch (FormatOf(Opcode())) {
    case k11n: return true;
    case k11x: return true;
    case k12x: return true;
    case k21c: return true;
    case k21h: return true;
    case k21s: return true;
    case k21t: return true;
    case k22b: return true;
    case k22c: return true;
    case k22s: return true;
    case k22t: return true;
    case k22x: return true;
    case k23x: return true;
    case k31c: return true;
    case k31i: return true;
    case k31t: return true;
    case k32x: return true;
    case k51l: return true;
    default: return false;
  }
}

int32_t Instruction::VRegC() const {
  switch (FormatOf(Opcode())) {
    case k23x: return VRegC_23x();
    case k35c: return VRegC_35c();
    case k3rc: return VRegC_3rc();
    default: LOG(FATAL) << "Tried to access vC of instruction " << Name() <<
        " which has no C operand.";
  }
  return -1;
}

int32_t Instruction::VRegB() const {
  switch (FormatOf(Opcode())) {
    case k12x: return VRegB_12x();
    case k22b: return VRegB_22b();
    case k22c: return VRegB_22c();
    case k22s: return VRegB_22s();
    case k22t: return VRegB_22t();
    case k22x: return VRegB_22x();
    case k23x: return VRegB_23x();
    case k32x: return VRegB_32x();
    default: LOG(FATAL) << "Tried to access vB of instruction " << Name() <<
        " which has no B operand.";
  }
  return -1;
}

int32_t Instruction::VRegA() const {
  switch (FormatOf(Opcode())) {
    case k11n: return VRegA_11n();
    case k11x: return VRegA_11x();
    case k12x: return VRegA_12x();
    case k21c: return VRegA_21c();
    case k21h: return VRegA_21h();
    case k21s: return VRegA_21s();
    case k21t: return VRegA_21t();
    case k22b: return VRegA_22b();
    case k22c: return VRegA_22c();
    case k22s: return VRegA_22s();
    case k22t: return VRegA_22t();
    case k22x: return VRegA_22x();
    case k23x: return VRegA_23x();
    case k31c: return VRegA_31c();
    case k31i: return VRegA_31i();
    case k31t: return VRegA_31t();
    case k32x: return VRegA_32x();
    case k51l: return VRegA_51l();
    default: LOG(FATAL) << "Tried to access vA of instruction " << Name() <<
        " which has no A operand.";
  }
  return -1;
}

int32_t Instruction::GetTargetOffset() const {
  switch (FormatOf(Opcode())) {
    // Cases for conditional branches follow.
    case k22t: return VRegC_22t();
    case k21t: return VRegB_21t();
    // Cases for unconditional branches follow.
    case k10t: return VRegA_10t();
    case k20t: return VRegA_20t();
    case k30t: return VRegA_30t();
    default: LOG(FATAL) << "Tried to access the branch offset of an instruction " << Name() <<
        " which does not have a target operand.";
  }
  return 0;
}

bool Instruction::CanFlowThrough() const {
  const uint16_t* insns = reinterpret_cast<const uint16_t*>(this);
  uint16_t insn = *insns;
  Code opcode = static_cast<Code>(insn & 0xFF);
  return  FlagsOf(opcode) & Instruction::kContinue;
}

void Instruction::Decode(uint32_t &vA, uint32_t &vB, uint64_t &vB_wide, uint32_t &vC, uint32_t arg[]) const {
  const uint16_t* insns = reinterpret_cast<const uint16_t*>(this);
  uint16_t insn = *insns;
  Code opcode = static_cast<Code>(insn & 0xFF);

  switch (FormatOf(opcode)) {
    case k10x:       // op
      /* nothing to do; copy the AA bits out for the verifier */
      vA = INST_AA(insn);
      break;
    case k12x:       // op vA, vB
      vA = INST_A(insn);
      vB = INST_B(insn);
      break;
    case k11n:       // op vA, #+B
      vA = INST_A(insn);
      vB = (int32_t) (INST_B(insn) << 28) >> 28;  // sign extend 4-bit value
      break;
    case k11x:       // op vAA
      vA = INST_AA(insn);
      break;
    case k10t:       // op +AA
      vA = (int8_t) INST_AA(insn);              // sign-extend 8-bit value
      break;
    case k20t:       // op +AAAA
      vA = (int16_t) FETCH(1);                   // sign-extend 16-bit value
      break;
    case k21c:       // op vAA, thing@BBBB
    case k22x:       // op vAA, vBBBB
      vA = INST_AA(insn);
      vB = FETCH(1);
      break;
    case k21s:       // op vAA, #+BBBB
    case k21t:       // op vAA, +BBBB
      vA = INST_AA(insn);
      vB = (int16_t) FETCH(1);                   // sign-extend 16-bit value
      break;
    case k21h:       // op vAA, #+BBBB0000[00000000]
      vA = INST_AA(insn);
      /*
       * The value should be treated as right-zero-extended, but we don't
       * actually do that here. Among other things, we don't know if it's
       * the top bits of a 32- or 64-bit value.
       */
      vB = FETCH(1);
      break;
    case k23x:       // op vAA, vBB, vCC
      vA = INST_AA(insn);
      vB = FETCH(1) & 0xff;
      vC = FETCH(1) >> 8;
      break;
    case k22b:       // op vAA, vBB, #+CC
      vA = INST_AA(insn);
      vB = FETCH(1) & 0xff;
      vC = (int8_t) (FETCH(1) >> 8);            // sign-extend 8-bit value
      break;
    case k22s:       // op vA, vB, #+CCCC
    case k22t:       // op vA, vB, +CCCC
      vA = INST_A(insn);
      vB = INST_B(insn);
      vC = (int16_t) FETCH(1);                   // sign-extend 16-bit value
      break;
    case k22c:       // op vA, vB, thing@CCCC
      vA = INST_A(insn);
      vB = INST_B(insn);
      vC = FETCH(1);
      break;
    case k30t:       // op +AAAAAAAA
      vA = FETCH_uint32(1);                     // signed 32-bit value
      break;
    case k31t:       // op vAA, +BBBBBBBB
    case k31c:       // op vAA, string@BBBBBBBB
      vA = INST_AA(insn);
      vB = FETCH_uint32(1);                     // 32-bit value
      break;
    case k32x:       // op vAAAA, vBBBB
      vA = FETCH(1);
      vB = FETCH(2);
      break;
    case k31i:       // op vAA, #+BBBBBBBB
      vA = INST_AA(insn);
      vB = FETCH_uint32(1);                     // signed 32-bit value
      break;
    case k35c:       // op {vC, vD, vE, vF, vG}, thing@BBBB
      {
        /*
         * Note that the fields mentioned in the spec don't appear in
         * their "usual" positions here compared to most formats. This
         * was done so that the field names for the argument count and
         * reference index match between this format and the corresponding
         * range formats (3rc and friends).
         *
         * Bottom line: The argument count is always in vA, and the
         * method constant (or equivalent) is always in vB.
         */
        uint16_t regList;
        int count;

        vA = INST_B(insn);  // This is labeled A in the spec.
        vB = FETCH(1);
        regList = FETCH(2);

        count = vA;

        /*
         * Copy the argument registers into the arg[] array, and
         * also copy the first argument (if any) into vC. (The
         * DecodedInstruction structure doesn't have separate
         * fields for {vD, vE, vF, vG}, so there's no need to make
         * copies of those.) Note that cases 5..2 fall through.
         */
        switch (count) {
        case 5: arg[4] = INST_A(insn);
        case 4: arg[3] = (regList >> 12) & 0x0f;
        case 3: arg[2] = (regList >> 8) & 0x0f;
        case 2: arg[1] = (regList >> 4) & 0x0f;
        case 1: vC = arg[0] = regList & 0x0f; break;
        case 0: break;  // Valid, but no need to do anything.
        default:
          LOG(ERROR) << "Invalid arg count in 35c (" << count << ")";
          return;
        }
      }
      break;
    case k3rc:       // op {vCCCC .. v(CCCC+AA-1)}, meth@BBBB
      vA = INST_AA(insn);
      vB = FETCH(1);
      vC = FETCH(2);
        break;
    case k51l:       // op vAA, #+BBBBBBBBBBBBBBBB
      vA = INST_AA(insn);
      vB_wide = FETCH_uint32(1) | ((uint64_t) FETCH_uint32(3) << 32);
      break;
    default:
      LOG(ERROR) << "Can't decode unexpected format " << FormatOf(opcode) << " (op=" << opcode << ")";
      return;
  }
}

size_t Instruction::SizeInCodeUnitsComplexOpcode() const {
  const uint16_t* insns = reinterpret_cast<const uint16_t*>(this);
  // Handle special NOP encoded variable length sequences.
  switch (*insns) {
    case kPackedSwitchSignature:
      return (4 + insns[1] * 2);
    case kSparseSwitchSignature:
      return (2 + insns[1] * 4);
    case kArrayDataSignature: {
      uint16_t element_size = insns[1];
      uint32_t length = insns[2] | (((uint32_t)insns[3]) << 16);
      // The plus 1 is to round up for odd size and width.
      return (4 + (element_size * length + 1) / 2);
    }
    default:
      if ((*insns & 0xFF) == 0) {
        return 1;  // NOP.
      } else {
        LOG(FATAL) << "Unreachable: " << DumpString(NULL);
        return 0;
      }
  }
}

std::string Instruction::DumpHex(size_t code_units) const {
  size_t inst_length = SizeInCodeUnits();
  if (inst_length > code_units) {
    inst_length = code_units;
  }
  std::ostringstream os;
  const uint16_t* insn = reinterpret_cast<const uint16_t*>(this);
  for (size_t i = 0; i < inst_length; i++) {
    os << StringPrintf("0x%04x", insn[i]) << " ";
  }
  for (size_t i = inst_length; i < code_units; i++) {
    os << "       ";
  }
  return os.str();
}

std::string Instruction::DumpString(const DexFile* file) const {
  std::ostringstream os;
  const char* opcode = kInstructionNames[Opcode()];
  switch (FormatOf(Opcode())) {
    case k10x:  os << opcode; break;
    case k12x:  os << StringPrintf("%s v%d, v%d", opcode, VRegA_12x(), VRegB_12x()); break;
    case k11n:  os << StringPrintf("%s v%d, #%+d", opcode, VRegA_11n(), VRegB_11n()); break;
    case k11x:  os << StringPrintf("%s v%d", opcode, VRegA_11x()); break;
    case k10t:  os << StringPrintf("%s %+d", opcode, VRegA_10t()); break;
    case k20t:  os << StringPrintf("%s %+d", opcode, VRegA_20t()); break;
    case k22x:  os << StringPrintf("%s v%d, v%d", opcode, VRegA_22x(), VRegB_22x()); break;
    case k21t:  os << StringPrintf("%s v%d, %+d", opcode, VRegA_21t(), VRegB_21t()); break;
    case k21s:  os << StringPrintf("%s v%d, #%+d", opcode, VRegA_21s(), VRegB_21s()); break;
    case k21h: {
        // op vAA, #+BBBB0000[00000000]
        if (Opcode() == CONST_HIGH16) {
          uint32_t value = VRegB_21h() << 16;
          os << StringPrintf("%s v%d, #int %+d // 0x%x", opcode, VRegA_21h(), value, value);
        } else {
          uint64_t value = static_cast<uint64_t>(VRegB_21h()) << 48;
          os << StringPrintf("%s v%d, #long %+lld // 0x%llx", opcode, VRegA_21h(), value, value);
        }
      }
      break;
    case k21c: {
      switch (Opcode()) {
        case CONST_STRING:
          if (file != NULL) {
            uint32_t string_idx = VRegB_21c();
            os << StringPrintf("const-string v%d, %s // string@%d", VRegA_21c(),
                               PrintableString(file->StringDataByIdx(string_idx)).c_str(), string_idx);
            break;
          }  // else fall-through
        case CHECK_CAST:
        case CONST_CLASS:
        case NEW_INSTANCE:
          if (file != NULL) {
            uint32_t type_idx = VRegB_21c();
            os << opcode << " v" << static_cast<int>(VRegA_21c()) << ", " << PrettyType(type_idx, *file)
               << " // type@" << type_idx;
            break;
          }  // else fall-through
        case SGET:
        case SGET_WIDE:
        case SGET_OBJECT:
        case SGET_BOOLEAN:
        case SGET_BYTE:
        case SGET_CHAR:
        case SGET_SHORT:
          if (file != NULL) {
            uint32_t field_idx = VRegB_21c();
            os << opcode << "  v" << static_cast<int>(VRegA_21c()) << ", " << PrettyField(field_idx, *file, true)
               << " // field@" << field_idx;
            break;
          }  // else fall-through
        case SPUT:
        case SPUT_WIDE:
        case SPUT_OBJECT:
        case SPUT_BOOLEAN:
        case SPUT_BYTE:
        case SPUT_CHAR:
        case SPUT_SHORT:
          if (file != NULL) {
            uint32_t field_idx = VRegB_21c();
            os << opcode << " v" << static_cast<int>(VRegA_21c()) << ", " << PrettyField(field_idx, *file, true)
               << " // field@" << field_idx;
            break;
          }  // else fall-through
        default:
          os << StringPrintf("%s v%d, thing@%d", opcode, VRegA_21c(), VRegB_21c());
          break;
      }
      break;
    }
    case k23x:  os << StringPrintf("%s v%d, v%d, v%d", opcode, VRegA_23x(), VRegB_23x(), VRegC_23x()); break;
    case k22b:  os << StringPrintf("%s v%d, v%d, #%+d", opcode, VRegA_22b(), VRegB_22b(), VRegC_22b()); break;
    case k22t:  os << StringPrintf("%s v%d, v%d, %+d", opcode, VRegA_22t(), VRegB_22t(), VRegC_22t()); break;
    case k22s:  os << StringPrintf("%s v%d, v%d, #%+d", opcode, VRegA_22s(), VRegB_22s(), VRegC_22s()); break;
    case k22c: {
      switch (Opcode()) {
        case IGET:
        case IGET_WIDE:
        case IGET_OBJECT:
        case IGET_BOOLEAN:
        case IGET_BYTE:
        case IGET_CHAR:
        case IGET_SHORT:
          if (file != NULL) {
            uint32_t field_idx = VRegC_22c();
            os << opcode << " v" << static_cast<int>(VRegA_22c()) << ", v" << static_cast<int>(VRegB_22c()) << ", "
               << PrettyField(field_idx, *file, true) << " // field@" << field_idx;
            break;
          }  // else fall-through
        case IGET_QUICK:
        case IGET_OBJECT_QUICK:
          if (file != NULL) {
            uint32_t field_idx = VRegC_22c();
            os << opcode << " v" << static_cast<int>(VRegA_22c()) << ", v" << static_cast<int>(VRegB_22c()) << ", "
               << "// offset@" << field_idx;
            break;
          }  // else fall-through
        case IPUT:
        case IPUT_WIDE:
        case IPUT_OBJECT:
        case IPUT_BOOLEAN:
        case IPUT_BYTE:
        case IPUT_CHAR:
        case IPUT_SHORT:
          if (file != NULL) {
            uint32_t field_idx = VRegC_22c();
            os << opcode << " v" << static_cast<int>(VRegA_22c()) << ", v" << static_cast<int>(VRegB_22c()) << ", "
               << PrettyField(field_idx, *file, true) << " // field@" << field_idx;
            break;
          }  // else fall-through
        case IPUT_QUICK:
        case IPUT_OBJECT_QUICK:
          if (file != NULL) {
            uint32_t field_idx = VRegC_22c();
            os << opcode << " v" << static_cast<int>(VRegA_22c()) << ", v" << static_cast<int>(VRegB_22c()) << ", "
               << "// offset@" << field_idx;
            break;
          }  // else fall-through
        case INSTANCE_OF:
          if (file != NULL) {
            uint32_t type_idx = VRegC_22c();
            os << opcode << " v" << static_cast<int>(VRegA_22c()) << ", v" << static_cast<int>(VRegB_22c()) << ", "
               << PrettyType(type_idx, *file) << " // type@" << type_idx;
            break;
          }
        case NEW_ARRAY:
          if (file != NULL) {
            uint32_t type_idx = VRegC_22c();
            os << opcode << " v" << static_cast<int>(VRegA_22c()) << ", v" << static_cast<int>(VRegB_22c()) << ", "
               << PrettyType(type_idx, *file) << " // type@" << type_idx;
            break;
          }  // else fall-through
        default:
          os << StringPrintf("%s v%d, v%d, thing@%d", opcode, VRegA_22c(), VRegB_22c(), VRegC_22c());
          break;
      }
      break;
    }
    case k32x:  os << StringPrintf("%s v%d, v%d", opcode, VRegA_32x(), VRegB_32x()); break;
    case k30t:  os << StringPrintf("%s %+d", opcode, VRegA_30t()); break;
    case k31t:  os << StringPrintf("%s v%d, %+d", opcode, VRegA_31t(), VRegB_31t()); break;
    case k31i:  os << StringPrintf("%s v%d, #%+d", opcode, VRegA_31i(), VRegB_31i()); break;
    case k31c:
      if (Opcode() == CONST_STRING_JUMBO) {
        uint32_t string_idx = VRegB_31c();
        if (file != NULL) {
          os << StringPrintf("%s v%d, %s // string@%d", opcode, VRegA_31c(),
                             PrintableString(file->StringDataByIdx(string_idx)).c_str(),
                             string_idx);
        } else {
          os << StringPrintf("%s v%d, string@%d", opcode, VRegA_31c(), string_idx);
        }
      } else {
        os << StringPrintf("%s v%d, thing@%d", opcode, VRegA_31c(), VRegB_31c()); break;
      }
      break;
    case k35c: {
      uint32_t arg[5];
      GetArgs(arg);
      switch (Opcode()) {
        case INVOKE_VIRTUAL:
        case INVOKE_SUPER:
        case INVOKE_DIRECT:
        case INVOKE_STATIC:
        case INVOKE_INTERFACE:
          if (file != NULL) {
            os << opcode << " {";
            uint32_t method_idx = VRegB_35c();
            for (size_t i = 0; i < VRegA_35c(); ++i) {
              if (i != 0) {
                os << ", ";
              }
              os << "v" << arg[i];
            }
            os << "}, " << PrettyMethod(method_idx, *file) << " // method@" << method_idx;
            break;
          }  // else fall-through
        case INVOKE_VIRTUAL_QUICK:
          if (file != NULL) {
            os << opcode << " {";
            uint32_t method_idx = VRegB_35c();
            for (size_t i = 0; i < VRegA_35c(); ++i) {
              if (i != 0) {
                os << ", ";
              }
              os << "v" << arg[i];
            }
            os << "},  // vtable@" << method_idx;
            break;
          }  // else fall-through
        default:
          os << opcode << " {v" << arg[0] << ", v" << arg[1] << ", v" << arg[2]
                       << ", v" << arg[3] << ", v" << arg[4] << "}, thing@" << VRegB_35c();
          break;
      }
      break;
    }
    case k3rc: {
      switch (Opcode()) {
        case INVOKE_VIRTUAL_RANGE:
        case INVOKE_SUPER_RANGE:
        case INVOKE_DIRECT_RANGE:
        case INVOKE_STATIC_RANGE:
        case INVOKE_INTERFACE_RANGE:
          if (file != NULL) {
            uint32_t method_idx = VRegB_3rc();
            os << StringPrintf("%s, {v%d .. v%d}, ", opcode, VRegC_3rc(), (VRegC_3rc() + VRegA_3rc() - 1))
               << PrettyMethod(method_idx, *file) << " // method@" << method_idx;
            break;
          }  // else fall-through
        case INVOKE_VIRTUAL_RANGE_QUICK:
          if (file != NULL) {
            uint32_t method_idx = VRegB_3rc();
            os << StringPrintf("%s, {v%d .. v%d}, ", opcode, VRegC_3rc(), (VRegC_3rc() + VRegA_3rc() - 1))
               << "// vtable@" << method_idx;
            break;
          }  // else fall-through
        default:
          os << StringPrintf("%s, {v%d .. v%d}, thing@%d", opcode, VRegC_3rc(),
                             (VRegC_3rc() + VRegA_3rc() - 1), VRegB_3rc());
          break;
      }
      break;
    }
    case k51l: os << StringPrintf("%s v%d, #%+lld", opcode, VRegA_51l(), VRegB_51l()); break;
    default: os << " unknown format (" << DumpHex(5) << ")"; break;
  }
  return os.str();
}

std::ostream& operator<<(std::ostream& os, const Instruction::Code& code) {
  return os << Instruction::Name(code);
}

}  // namespace art
