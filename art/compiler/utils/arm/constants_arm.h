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

#ifndef ART_COMPILER_UTILS_ARM_CONSTANTS_ARM_H_
#define ART_COMPILER_UTILS_ARM_CONSTANTS_ARM_H_

#include <stdint.h>

#include <iosfwd>

#include "arch/arm/registers_arm.h"
#include "base/casts.h"
#include "base/logging.h"
#include "globals.h"

namespace art {
namespace arm {

// Defines constants and accessor classes to assemble, disassemble and
// simulate ARM instructions.
//
// Section references in the code refer to the "ARM Architecture Reference
// Manual" from July 2005 (available at http://www.arm.com/miscPDFs/14128.pdf)
//
// Constants for specific fields are defined in their respective named enums.
// General constants are in an anonymous enum in class Instr.


// We support both VFPv3-D16 and VFPv3-D32 profiles, but currently only one at
// a time, so that compile time optimizations can be applied.
// Warning: VFPv3-D32 is untested.
#define VFPv3_D16
#if defined(VFPv3_D16) == defined(VFPv3_D32)
#error "Exactly one of VFPv3_D16 or VFPv3_D32 can be defined at a time."
#endif


enum ScaleFactor {
  TIMES_1 = 0,
  TIMES_2 = 1,
  TIMES_4 = 2,
  TIMES_8 = 3
};

// Values for double-precision floating point registers.
enum DRegister {
  D0  =  0,
  D1  =  1,
  D2  =  2,
  D3  =  3,
  D4  =  4,
  D5  =  5,
  D6  =  6,
  D7  =  7,
  D8  =  8,
  D9  =  9,
  D10 = 10,
  D11 = 11,
  D12 = 12,
  D13 = 13,
  D14 = 14,
  D15 = 15,
#ifdef VFPv3_D16
  kNumberOfDRegisters = 16,
#else
  D16 = 16,
  D17 = 17,
  D18 = 18,
  D19 = 19,
  D20 = 20,
  D21 = 21,
  D22 = 22,
  D23 = 23,
  D24 = 24,
  D25 = 25,
  D26 = 26,
  D27 = 27,
  D28 = 28,
  D29 = 29,
  D30 = 30,
  D31 = 31,
  kNumberOfDRegisters = 32,
#endif
  kNumberOfOverlappingDRegisters = 16,
  kNoDRegister = -1,
};
std::ostream& operator<<(std::ostream& os, const DRegister& rhs);


// Values for the condition field as defined in section A3.2.
enum Condition {
  kNoCondition = -1,
  EQ =  0,  // equal
  NE =  1,  // not equal
  CS =  2,  // carry set/unsigned higher or same
  CC =  3,  // carry clear/unsigned lower
  MI =  4,  // minus/negative
  PL =  5,  // plus/positive or zero
  VS =  6,  // overflow
  VC =  7,  // no overflow
  HI =  8,  // unsigned higher
  LS =  9,  // unsigned lower or same
  GE = 10,  // signed greater than or equal
  LT = 11,  // signed less than
  GT = 12,  // signed greater than
  LE = 13,  // signed less than or equal
  AL = 14,  // always (unconditional)
  kSpecialCondition = 15,  // special condition (refer to section A3.2.1)
  kMaxCondition = 16,
};
std::ostream& operator<<(std::ostream& os, const Condition& rhs);


// Opcodes for Data-processing instructions (instructions with a type 0 and 1)
// as defined in section A3.4
enum Opcode {
  kNoOperand = -1,
  AND =  0,  // Logical AND
  EOR =  1,  // Logical Exclusive OR
  SUB =  2,  // Subtract
  RSB =  3,  // Reverse Subtract
  ADD =  4,  // Add
  ADC =  5,  // Add with Carry
  SBC =  6,  // Subtract with Carry
  RSC =  7,  // Reverse Subtract with Carry
  TST =  8,  // Test
  TEQ =  9,  // Test Equivalence
  CMP = 10,  // Compare
  CMN = 11,  // Compare Negated
  ORR = 12,  // Logical (inclusive) OR
  MOV = 13,  // Move
  BIC = 14,  // Bit Clear
  MVN = 15,  // Move Not
  kMaxOperand = 16
};


// Shifter types for Data-processing operands as defined in section A5.1.2.
enum Shift {
  kNoShift = -1,
  LSL = 0,  // Logical shift left
  LSR = 1,  // Logical shift right
  ASR = 2,  // Arithmetic shift right
  ROR = 3,  // Rotate right
  kMaxShift = 4
};


// Constants used for the decoding or encoding of the individual fields of
// instructions. Based on the "Figure 3-1 ARM instruction set summary".
enum InstructionFields {
  kConditionShift = 28,
  kConditionBits = 4,
  kTypeShift = 25,
  kTypeBits = 3,
  kLinkShift = 24,
  kLinkBits = 1,
  kUShift = 23,
  kUBits = 1,
  kOpcodeShift = 21,
  kOpcodeBits = 4,
  kSShift = 20,
  kSBits = 1,
  kRnShift = 16,
  kRnBits = 4,
  kRdShift = 12,
  kRdBits = 4,
  kRsShift = 8,
  kRsBits = 4,
  kRmShift = 0,
  kRmBits = 4,

  // Immediate instruction fields encoding.
  kRotateShift = 8,
  kRotateBits = 4,
  kImmed8Shift = 0,
  kImmed8Bits = 8,

  // Shift instruction register fields encodings.
  kShiftImmShift = 7,
  kShiftRegisterShift = 8,
  kShiftImmBits = 5,
  kShiftShift = 5,
  kShiftBits = 2,

  // Load/store instruction offset field encoding.
  kOffset12Shift = 0,
  kOffset12Bits = 12,
  kOffset12Mask = 0x00000fff,

  // Mul instruction register fields encodings.
  kMulRdShift = 16,
  kMulRdBits = 4,
  kMulRnShift = 12,
  kMulRnBits = 4,

  kBranchOffsetMask = 0x00ffffff
};


// Size (in bytes) of registers.
const int kRegisterSize = 4;

// List of registers used in load/store multiple.
typedef uint16_t RegList;

// The class Instr enables access to individual fields defined in the ARM
// architecture instruction set encoding as described in figure A3-1.
//
// Example: Test whether the instruction at ptr does set the condition code
// bits.
//
// bool InstructionSetsConditionCodes(byte* ptr) {
//   Instr* instr = Instr::At(ptr);
//   int type = instr->TypeField();
//   return ((type == 0) || (type == 1)) && instr->HasS();
// }
//
class Instr {
 public:
  enum {
    kInstrSize = 4,
    kInstrSizeLog2 = 2,
    kPCReadOffset = 8
  };

  bool IsBreakPoint() {
    return IsBkpt();
  }

  // Get the raw instruction bits.
  inline int32_t InstructionBits() const {
    return *reinterpret_cast<const int32_t*>(this);
  }

  // Set the raw instruction bits to value.
  inline void SetInstructionBits(int32_t value) {
    *reinterpret_cast<int32_t*>(this) = value;
  }

  // Read one particular bit out of the instruction bits.
  inline int Bit(int nr) const {
    return (InstructionBits() >> nr) & 1;
  }

  // Read a bit field out of the instruction bits.
  inline int Bits(int shift, int count) const {
    return (InstructionBits() >> shift) & ((1 << count) - 1);
  }


  // Accessors for the different named fields used in the ARM encoding.
  // The naming of these accessor corresponds to figure A3-1.
  // Generally applicable fields
  inline Condition ConditionField() const {
    return static_cast<Condition>(Bits(kConditionShift, kConditionBits));
  }
  inline int TypeField() const { return Bits(kTypeShift, kTypeBits); }

  inline Register RnField() const { return static_cast<Register>(
                                        Bits(kRnShift, kRnBits)); }
  inline Register RdField() const { return static_cast<Register>(
                                        Bits(kRdShift, kRdBits)); }

  // Fields used in Data processing instructions
  inline Opcode OpcodeField() const {
    return static_cast<Opcode>(Bits(kOpcodeShift, kOpcodeBits));
  }
  inline int SField() const { return Bits(kSShift, kSBits); }
  // with register
  inline Register RmField() const {
    return static_cast<Register>(Bits(kRmShift, kRmBits));
  }
  inline Shift ShiftField() const { return static_cast<Shift>(
                                        Bits(kShiftShift, kShiftBits)); }
  inline int RegShiftField() const { return Bit(4); }
  inline Register RsField() const {
    return static_cast<Register>(Bits(kRsShift, kRsBits));
  }
  inline int ShiftAmountField() const { return Bits(kShiftImmShift,
                                                    kShiftImmBits); }
  // with immediate
  inline int RotateField() const { return Bits(kRotateShift, kRotateBits); }
  inline int Immed8Field() const { return Bits(kImmed8Shift, kImmed8Bits); }

  // Fields used in Load/Store instructions
  inline int PUField() const { return Bits(23, 2); }
  inline int  BField() const { return Bit(22); }
  inline int  WField() const { return Bit(21); }
  inline int  LField() const { return Bit(20); }
  // with register uses same fields as Data processing instructions above
  // with immediate
  inline int Offset12Field() const { return Bits(kOffset12Shift,
                                                 kOffset12Bits); }
  // multiple
  inline int RlistField() const { return Bits(0, 16); }
  // extra loads and stores
  inline int SignField() const { return Bit(6); }
  inline int HField() const { return Bit(5); }
  inline int ImmedHField() const { return Bits(8, 4); }
  inline int ImmedLField() const { return Bits(0, 4); }

  // Fields used in Branch instructions
  inline int LinkField() const { return Bits(kLinkShift, kLinkBits); }
  inline int SImmed24Field() const { return ((InstructionBits() << 8) >> 8); }

  // Fields used in Supervisor Call instructions
  inline uint32_t SvcField() const { return Bits(0, 24); }

  // Field used in Breakpoint instruction
  inline uint16_t BkptField() const {
    return ((Bits(8, 12) << 4) | Bits(0, 4));
  }

  // Field used in 16-bit immediate move instructions
  inline uint16_t MovwField() const {
    return ((Bits(16, 4) << 12) | Bits(0, 12));
  }

  // Field used in VFP float immediate move instruction
  inline float ImmFloatField() const {
    uint32_t imm32 = (Bit(19) << 31) | (((1 << 5) - Bit(18)) << 25) |
                     (Bits(16, 2) << 23) | (Bits(0, 4) << 19);
    return bit_cast<float, uint32_t>(imm32);
  }

  // Field used in VFP double immediate move instruction
  inline double ImmDoubleField() const {
    uint64_t imm64 = (Bit(19)*(1LL << 63)) | (((1LL << 8) - Bit(18)) << 54) |
                     (Bits(16, 2)*(1LL << 52)) | (Bits(0, 4)*(1LL << 48));
    return bit_cast<double, uint64_t>(imm64);
  }

  // Test for data processing instructions of type 0 or 1.
  // See "ARM Architecture Reference Manual ARMv7-A and ARMv7-R edition",
  // section A5.1 "ARM instruction set encoding".
  inline bool IsDataProcessing() const {
    CHECK_NE(ConditionField(), kSpecialCondition);
    CHECK_EQ(Bits(26, 2), 0);  // Type 0 or 1.
    return ((Bits(20, 5) & 0x19) != 0x10) &&
      ((Bit(25) == 1) ||  // Data processing immediate.
       (Bit(4) == 0) ||  // Data processing register.
       (Bit(7) == 0));  // Data processing register-shifted register.
  }

  // Tests for special encodings of type 0 instructions (extra loads and stores,
  // as well as multiplications, synchronization primitives, and miscellaneous).
  // Can only be called for a type 0 or 1 instruction.
  inline bool IsMiscellaneous() const {
    CHECK_EQ(Bits(26, 2), 0);  // Type 0 or 1.
    return ((Bit(25) == 0) && ((Bits(20, 5) & 0x19) == 0x10) && (Bit(7) == 0));
  }
  inline bool IsMultiplyOrSyncPrimitive() const {
    CHECK_EQ(Bits(26, 2), 0);  // Type 0 or 1.
    return ((Bit(25) == 0) && (Bits(4, 4) == 9));
  }

  // Test for Supervisor Call instruction.
  inline bool IsSvc() const {
    return ((InstructionBits() & 0xff000000) == 0xef000000);
  }

  // Test for Breakpoint instruction.
  inline bool IsBkpt() const {
    return ((InstructionBits() & 0xfff000f0) == 0xe1200070);
  }

  // VFP register fields.
  inline SRegister SnField() const {
    return static_cast<SRegister>((Bits(kRnShift, kRnBits) << 1) + Bit(7));
  }
  inline SRegister SdField() const {
    return static_cast<SRegister>((Bits(kRdShift, kRdBits) << 1) + Bit(22));
  }
  inline SRegister SmField() const {
    return static_cast<SRegister>((Bits(kRmShift, kRmBits) << 1) + Bit(5));
  }
  inline DRegister DnField() const {
    return static_cast<DRegister>(Bits(kRnShift, kRnBits) + (Bit(7) << 4));
  }
  inline DRegister DdField() const {
    return static_cast<DRegister>(Bits(kRdShift, kRdBits) + (Bit(22) << 4));
  }
  inline DRegister DmField() const {
    return static_cast<DRegister>(Bits(kRmShift, kRmBits) + (Bit(5) << 4));
  }

  // Test for VFP data processing or single transfer instructions of type 7.
  inline bool IsVFPDataProcessingOrSingleTransfer() const {
    CHECK_NE(ConditionField(), kSpecialCondition);
    CHECK_EQ(TypeField(), 7);
    return ((Bit(24) == 0) && (Bits(9, 3) == 5));
    // Bit(4) == 0: Data Processing
    // Bit(4) == 1: 8, 16, or 32-bit Transfer between ARM Core and VFP
  }

  // Test for VFP 64-bit transfer instructions of type 6.
  inline bool IsVFPDoubleTransfer() const {
    CHECK_NE(ConditionField(), kSpecialCondition);
    CHECK_EQ(TypeField(), 6);
    return ((Bits(21, 4) == 2) && (Bits(9, 3) == 5) &&
            ((Bits(4, 4) & 0xd) == 1));
  }

  // Test for VFP load and store instructions of type 6.
  inline bool IsVFPLoadStore() const {
    CHECK_NE(ConditionField(), kSpecialCondition);
    CHECK_EQ(TypeField(), 6);
    return ((Bits(20, 5) & 0x12) == 0x10) && (Bits(9, 3) == 5);
  }

  // Special accessors that test for existence of a value.
  inline bool HasS() const { return SField() == 1; }
  inline bool HasB() const { return BField() == 1; }
  inline bool HasW() const { return WField() == 1; }
  inline bool HasL() const { return LField() == 1; }
  inline bool HasSign() const { return SignField() == 1; }
  inline bool HasH() const { return HField() == 1; }
  inline bool HasLink() const { return LinkField() == 1; }

  // Instructions are read out of a code stream. The only way to get a
  // reference to an instruction is to convert a pointer. There is no way
  // to allocate or create instances of class Instr.
  // Use the At(pc) function to create references to Instr.
  static Instr* At(uword pc) { return reinterpret_cast<Instr*>(pc); }
  Instr* Next() { return this + kInstrSize; }

 private:
  // We need to prevent the creation of instances of class Instr.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Instr);
};

}  // namespace arm
}  // namespace art

#endif  // ART_COMPILER_UTILS_ARM_CONSTANTS_ARM_H_
