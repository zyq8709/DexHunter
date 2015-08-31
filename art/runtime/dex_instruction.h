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

#ifndef ART_RUNTIME_DEX_INSTRUCTION_H_
#define ART_RUNTIME_DEX_INSTRUCTION_H_

#include "base/logging.h"
#include "base/macros.h"
#include "globals.h"

typedef uint8_t uint4_t;
typedef int8_t int4_t;

namespace art {

class DexFile;

enum {
  kNumPackedOpcodes = 0x100
};

class Instruction {
 public:
  // NOP-encoded switch-statement signatures.
  enum {
    kPackedSwitchSignature = 0x0100,
    kSparseSwitchSignature = 0x0200,
    kArrayDataSignature = 0x0300,
  };

  struct PACKED(4) PackedSwitchPayload {
    const uint16_t ident;
    const uint16_t case_count;
    const int32_t first_key;
    const int32_t targets[];

   private:
    DISALLOW_COPY_AND_ASSIGN(PackedSwitchPayload);
  };

  struct PACKED(4) SparseSwitchPayload {
    const uint16_t ident;
    const uint16_t case_count;
    const int32_t keys_and_targets[];

   public:
    const int32_t* GetKeys() const {
      return keys_and_targets;
    }

    const int32_t* GetTargets() const {
      return keys_and_targets + case_count;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(SparseSwitchPayload);
  };

  struct PACKED(4) ArrayDataPayload {
    const uint16_t ident;
    const uint16_t element_width;
    const uint32_t element_count;
    const uint8_t data[];

   private:
    DISALLOW_COPY_AND_ASSIGN(ArrayDataPayload);
  };

  // TODO: the code layout below is deliberate to avoid this enum being picked up by
  //       generate-operator-out.py.
  enum Code
  {  // NOLINT(whitespace/braces)
#define INSTRUCTION_ENUM(opcode, cname, p, f, r, i, a, v) cname = opcode,
#include "dex_instruction_list.h"
    DEX_INSTRUCTION_LIST(INSTRUCTION_ENUM)
#undef DEX_INSTRUCTION_LIST
#undef INSTRUCTION_ENUM
  };

  enum Format {
    k10x,  // op
    k12x,  // op vA, vB
    k11n,  // op vA, #+B
    k11x,  // op vAA
    k10t,  // op +AA
    k20t,  // op +AAAA
    k22x,  // op vAA, vBBBB
    k21t,  // op vAA, +BBBB
    k21s,  // op vAA, #+BBBB
    k21h,  // op vAA, #+BBBB00000[00000000]
    k21c,  // op vAA, thing@BBBB
    k23x,  // op vAA, vBB, vCC
    k22b,  // op vAA, vBB, #+CC
    k22t,  // op vA, vB, +CCCC
    k22s,  // op vA, vB, #+CCCC
    k22c,  // op vA, vB, thing@CCCC
    k32x,  // op vAAAA, vBBBB
    k30t,  // op +AAAAAAAA
    k31t,  // op vAA, +BBBBBBBB
    k31i,  // op vAA, #+BBBBBBBB
    k31c,  // op vAA, thing@BBBBBBBB
    k35c,  // op {vC, vD, vE, vF, vG}, thing@BBBB (B: count, A: vG)
    k3rc,  // op {vCCCC .. v(CCCC+AA-1)}, meth@BBBB
    k51l,  // op vAA, #+BBBBBBBBBBBBBBBB
  };

  enum Flags {
    kBranch   = 0x01,  // conditional or unconditional branch
    kContinue = 0x02,  // flow can continue to next statement
    kSwitch   = 0x04,  // switch statement
    kThrow    = 0x08,  // could cause an exception to be thrown
    kReturn   = 0x10,  // returns, no additional statements
    kInvoke   = 0x20,  // a flavor of invoke
    kUnconditional = 0x40,  // unconditional branch
  };

  enum VerifyFlag {
    kVerifyNone            = 0x00000,
    kVerifyRegA            = 0x00001,
    kVerifyRegAWide        = 0x00002,
    kVerifyRegB            = 0x00004,
    kVerifyRegBField       = 0x00008,
    kVerifyRegBMethod      = 0x00010,
    kVerifyRegBNewInstance = 0x00020,
    kVerifyRegBString      = 0x00040,
    kVerifyRegBType        = 0x00080,
    kVerifyRegBWide        = 0x00100,
    kVerifyRegC            = 0x00200,
    kVerifyRegCField       = 0x00400,
    kVerifyRegCNewArray    = 0x00800,
    kVerifyRegCType        = 0x01000,
    kVerifyRegCWide        = 0x02000,
    kVerifyArrayData       = 0x04000,
    kVerifyBranchTarget    = 0x08000,
    kVerifySwitchTargets   = 0x10000,
    kVerifyVarArg          = 0x20000,
    kVerifyVarArgRange     = 0x40000,
    kVerifyError           = 0x80000,
  };

  // Decodes this instruction, populating its arguments.
  void Decode(uint32_t &vA, uint32_t &vB, uint64_t &vB_wide, uint32_t &vC, uint32_t arg[]) const;

  // Returns the size (in 2 byte code units) of this instruction.
  size_t SizeInCodeUnits() const {
    int result = kInstructionSizeInCodeUnits[Opcode()];
    if (UNLIKELY(result < 0)) {
      return SizeInCodeUnitsComplexOpcode();
    } else {
      return static_cast<size_t>(result);
    }
  }

  // Reads an instruction out of the stream at the specified address.
  static const Instruction* At(const uint16_t* code) {
    DCHECK(code != NULL);
    return reinterpret_cast<const Instruction*>(code);
  }

  // Reads an instruction out of the stream from the current address plus an offset.
  const Instruction* RelativeAt(int32_t offset) const {
    return At(reinterpret_cast<const uint16_t*>(this) + offset);
  }

  // Returns a pointer to the next instruction in the stream.
  const Instruction* Next() const {
    return RelativeAt(SizeInCodeUnits());
  }

  // Returns a pointer to the instruction after this 1xx instruction in the stream.
  const Instruction* Next_1xx() const {
    DCHECK(FormatOf(Opcode()) >= k10x && FormatOf(Opcode()) <= k10t);
    return RelativeAt(1);
  }

  // Returns a pointer to the instruction after this 2xx instruction in the stream.
  const Instruction* Next_2xx() const {
    DCHECK(FormatOf(Opcode()) >= k20t && FormatOf(Opcode()) <= k22c);
    return RelativeAt(2);
  }

  // Returns a pointer to the instruction after this 3xx instruction in the stream.
  const Instruction* Next_3xx() const {
    DCHECK(FormatOf(Opcode()) >= k32x && FormatOf(Opcode()) <= k3rc);
    return RelativeAt(3);
  }

  // Returns a pointer to the instruction after this 51l instruction in the stream.
  const Instruction* Next_51l() const {
    DCHECK(FormatOf(Opcode()) == k51l);
    return RelativeAt(5);
  }

  // Returns the name of this instruction's opcode.
  const char* Name() const {
    return Instruction::Name(Opcode());
  }

  // Returns the name of the given opcode.
  static const char* Name(Code opcode) {
    return kInstructionNames[opcode];
  }

  // VRegA
  bool HasVRegA() const;
  int32_t VRegA() const;
  int8_t VRegA_10t() const;
  uint8_t VRegA_10x() const;
  uint4_t VRegA_11n() const;
  uint8_t VRegA_11x() const;
  uint4_t VRegA_12x() const;
  int16_t VRegA_20t() const;
  uint8_t VRegA_21c() const;
  uint8_t VRegA_21h() const;
  uint8_t VRegA_21s() const;
  uint8_t VRegA_21t() const;
  uint8_t VRegA_22b() const;
  uint4_t VRegA_22c() const;
  uint4_t VRegA_22s() const;
  uint4_t VRegA_22t() const;
  uint8_t VRegA_22x() const;
  uint8_t VRegA_23x() const;
  int32_t VRegA_30t() const;
  uint8_t VRegA_31c() const;
  uint8_t VRegA_31i() const;
  uint8_t VRegA_31t() const;
  uint16_t VRegA_32x() const;
  uint4_t VRegA_35c() const;
  uint8_t VRegA_3rc() const;
  uint8_t VRegA_51l() const;

  // VRegB
  bool HasVRegB() const;
  int32_t VRegB() const;
  int4_t VRegB_11n() const;
  uint4_t VRegB_12x() const;
  uint16_t VRegB_21c() const;
  uint16_t VRegB_21h() const;
  int16_t VRegB_21s() const;
  int16_t VRegB_21t() const;
  uint8_t VRegB_22b() const;
  uint4_t VRegB_22c() const;
  uint4_t VRegB_22s() const;
  uint4_t VRegB_22t() const;
  uint16_t VRegB_22x() const;
  uint8_t VRegB_23x() const;
  uint32_t VRegB_31c() const;
  int32_t VRegB_31i() const;
  int32_t VRegB_31t() const;
  uint16_t VRegB_32x() const;
  uint16_t VRegB_35c() const;
  uint16_t VRegB_3rc() const;
  uint64_t VRegB_51l() const;  // vB_wide

  // VRegC
  bool HasVRegC() const;
  int32_t VRegC() const;
  int8_t VRegC_22b() const;
  uint16_t VRegC_22c() const;
  int16_t VRegC_22s() const;
  int16_t VRegC_22t() const;
  uint8_t VRegC_23x() const;
  uint4_t VRegC_35c() const;
  uint16_t VRegC_3rc() const;

  // Fills the given array with the 'arg' array of the instruction.
  void GetArgs(uint32_t args[5]) const;

  // Returns the opcode field of the instruction.
  Code Opcode() const {
    return static_cast<Code>(Fetch16(0) & 0xFF);
  }

  void SetOpcode(Code opcode) {
    DCHECK_LT(static_cast<uint16_t>(opcode), 256u);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[0] = (insns[0] & 0xff00) | static_cast<uint16_t>(opcode);
  }

  void SetVRegA_10x(uint8_t val) {
    DCHECK(FormatOf(Opcode()) == k10x);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[0] = (val << 8) | (insns[0] & 0x00ff);
  }

  void SetVRegB_3rc(uint16_t val) {
    DCHECK(FormatOf(Opcode()) == k3rc);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[1] = val;
  }

  void SetVRegB_35c(uint16_t val) {
    DCHECK(FormatOf(Opcode()) == k35c);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[1] = val;
  }

  void SetVRegC_22c(uint16_t val) {
    DCHECK(FormatOf(Opcode()) == k22c);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[1] = val;
  }

  // Returns the format of the given opcode.
  static Format FormatOf(Code opcode) {
    return kInstructionFormats[opcode];
  }

  // Returns the flags for the given opcode.
  static int FlagsOf(Code opcode) {
    return kInstructionFlags[opcode];
  }

  // Returns true if this instruction is a branch.
  bool IsBranch() const {
    return (kInstructionFlags[Opcode()] & kBranch) != 0;
  }

  // Returns true if this instruction is a unconditional branch.
  bool IsUnconditional() const {
    return (kInstructionFlags[Opcode()] & kUnconditional) != 0;
  }

  // Returns the branch offset if this instruction is a branch.
  int32_t GetTargetOffset() const;

  // Returns true if the instruction allows control flow to go to the following instruction.
  bool CanFlowThrough() const;

  // Returns true if this instruction is a switch.
  bool IsSwitch() const {
    return (kInstructionFlags[Opcode()] & kSwitch) != 0;
  }

  // Returns true if this instruction can throw.
  bool IsThrow() const {
    return (kInstructionFlags[Opcode()] & kThrow) != 0;
  }

  // Determine if the instruction is any of 'return' instructions.
  bool IsReturn() const {
    return (kInstructionFlags[Opcode()] & kReturn) != 0;
  }

  // Determine if this instruction ends execution of its basic block.
  bool IsBasicBlockEnd() const {
    return IsBranch() || IsReturn() || Opcode() == THROW;
  }

  // Determine if this instruction is an invoke.
  bool IsInvoke() const {
    return (kInstructionFlags[Opcode()] & kInvoke) != 0;
  }

  int GetVerifyTypeArgumentA() const {
    return (kInstructionVerifyFlags[Opcode()] & (kVerifyRegA | kVerifyRegAWide));
  }

  int GetVerifyTypeArgumentB() const {
    return (kInstructionVerifyFlags[Opcode()] & (kVerifyRegB | kVerifyRegBField | kVerifyRegBMethod |
             kVerifyRegBNewInstance | kVerifyRegBString | kVerifyRegBType | kVerifyRegBWide));
  }

  int GetVerifyTypeArgumentC() const {
    return (kInstructionVerifyFlags[Opcode()] & (kVerifyRegC | kVerifyRegCField |
             kVerifyRegCNewArray | kVerifyRegCType | kVerifyRegCWide));
  }

  int GetVerifyExtraFlags() const {
    return (kInstructionVerifyFlags[Opcode()] & (kVerifyArrayData | kVerifyBranchTarget |
             kVerifySwitchTargets | kVerifyVarArg | kVerifyVarArgRange | kVerifyError));
  }

  // Get the dex PC of this instruction as a offset in code units from the beginning of insns.
  uint32_t GetDexPc(const uint16_t* insns) const {
    return (reinterpret_cast<const uint16_t*>(this) - insns);
  }

  // Dump decoded version of instruction
  std::string DumpString(const DexFile*) const;

  // Dump code_units worth of this instruction, padding to code_units for shorter instructions
  std::string DumpHex(size_t code_units) const;

 private:
  size_t SizeInCodeUnitsComplexOpcode() const;

  uint16_t Fetch16(size_t offset) const {
    const uint16_t* insns = reinterpret_cast<const uint16_t*>(this);
    return insns[offset];
  }

  uint32_t Fetch32(size_t offset) const {
    return (Fetch16(offset) | ((uint32_t) Fetch16(offset + 1) << 16));
  }

  uint4_t InstA() const {
    return static_cast<uint4_t>((Fetch16(0) >> 8) & 0x0f);
  }

  uint4_t InstB() const {
    return static_cast<uint4_t>(Fetch16(0) >> 12);
  }

  uint8_t InstAA() const {
    return static_cast<uint8_t>(Fetch16(0) >> 8);
  }

  static const char* const kInstructionNames[];
  static Format const kInstructionFormats[];
  static int const kInstructionFlags[];
  static int const kInstructionVerifyFlags[];
  static int const kInstructionSizeInCodeUnits[];
  DISALLOW_IMPLICIT_CONSTRUCTORS(Instruction);
};
std::ostream& operator<<(std::ostream& os, const Instruction::Code& code);
std::ostream& operator<<(std::ostream& os, const Instruction::Format& format);
std::ostream& operator<<(std::ostream& os, const Instruction::Flags& flags);
std::ostream& operator<<(std::ostream& os, const Instruction::VerifyFlag& vflags);

/*
 * Holds the contents of a decoded instruction.
 */
struct DecodedInstruction {
  uint32_t vA;
  uint32_t vB;
  uint64_t vB_wide;        /* for k51l */
  uint32_t vC;
  uint32_t arg[5];         /* vC/D/E/F/G in invoke or filled-new-array */
  Instruction::Code opcode;

  explicit DecodedInstruction(const Instruction* inst) {
    inst->Decode(vA, vB, vB_wide, vC, arg);
    opcode = inst->Opcode();
  }
};

}  // namespace art

#endif  // ART_RUNTIME_DEX_INSTRUCTION_H_
