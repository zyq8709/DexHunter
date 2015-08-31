/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "sea.h"
#include "dex_instruction.h"

#ifndef ART_COMPILER_SEA_IR_IR_INSTRUCTION_TOOLS_H_
#define ART_COMPILER_SEA_IR_IR_INSTRUCTION_TOOLS_H_


// Note: This file has content cannibalized for SEA_IR from the MIR implementation,
//       to avoid having a dependence on MIR.
namespace sea_ir {

#define DF_NOP                  0
#define DF_UA                   (1 << kUA)
#define DF_UB                   (1 << kUB)
#define DF_UC                   (1 << kUC)
#define DF_A_WIDE               (1 << kAWide)
#define DF_B_WIDE               (1 << kBWide)
#define DF_C_WIDE               (1 << kCWide)
#define DF_DA                   (1 << kDA)
#define DF_IS_MOVE              (1 << kIsMove)
#define DF_SETS_CONST           (1 << kSetsConst)
#define DF_FORMAT_35C           (1 << kFormat35c)
#define DF_FORMAT_3RC           (1 << kFormat3rc)
#define DF_NULL_CHK_0           (1 << kNullCheckSrc0)
#define DF_NULL_CHK_1           (1 << kNullCheckSrc1)
#define DF_NULL_CHK_2           (1 << kNullCheckSrc2)
#define DF_NULL_CHK_OUT0        (1 << kNullCheckOut0)
#define DF_NON_NULL_DST         (1 << kDstNonNull)
#define DF_NON_NULL_RET         (1 << kRetNonNull)
#define DF_NULL_TRANSFER_0      (1 << kNullTransferSrc0)
#define DF_NULL_TRANSFER_N      (1 << kNullTransferSrcN)
#define DF_RANGE_CHK_1          (1 << kRangeCheckSrc1)
#define DF_RANGE_CHK_2          (1 << kRangeCheckSrc2)
#define DF_RANGE_CHK_3          (1 << kRangeCheckSrc3)
#define DF_FP_A                 (1 << kFPA)
#define DF_FP_B                 (1 << kFPB)
#define DF_FP_C                 (1 << kFPC)
#define DF_CORE_A               (1 << kCoreA)
#define DF_CORE_B               (1 << kCoreB)
#define DF_CORE_C               (1 << kCoreC)
#define DF_REF_A                (1 << kRefA)
#define DF_REF_B                (1 << kRefB)
#define DF_REF_C                (1 << kRefC)
#define DF_UMS                  (1 << kUsesMethodStar)

#define DF_HAS_USES             (DF_UA | DF_UB | DF_UC)

#define DF_HAS_DEFS             (DF_DA)

#define DF_HAS_NULL_CHKS        (DF_NULL_CHK_0 | \
                                 DF_NULL_CHK_1 | \
                                 DF_NULL_CHK_2 | \
                                 DF_NULL_CHK_OUT0)

#define DF_HAS_RANGE_CHKS       (DF_RANGE_CHK_1 | \
                                 DF_RANGE_CHK_2 | \
                                 DF_RANGE_CHK_3)

#define DF_HAS_NR_CHKS          (DF_HAS_NULL_CHKS | \
                                 DF_HAS_RANGE_CHKS)

#define DF_A_IS_REG             (DF_UA | DF_DA)
#define DF_B_IS_REG             (DF_UB)
#define DF_C_IS_REG             (DF_UC)
#define DF_IS_GETTER_OR_SETTER  (DF_IS_GETTER | DF_IS_SETTER)
#define DF_USES_FP              (DF_FP_A | DF_FP_B | DF_FP_C)

enum DataFlowAttributePos {
  kUA = 0,
  kUB,
  kUC,
  kAWide,
  kBWide,
  kCWide,
  kDA,
  kIsMove,
  kSetsConst,
  kFormat35c,
  kFormat3rc,
  kNullCheckSrc0,        // Null check of uses[0].
  kNullCheckSrc1,        // Null check of uses[1].
  kNullCheckSrc2,        // Null check of uses[2].
  kNullCheckOut0,        // Null check out outgoing arg0.
  kDstNonNull,           // May assume dst is non-null.
  kRetNonNull,           // May assume retval is non-null.
  kNullTransferSrc0,     // Object copy src[0] -> dst.
  kNullTransferSrcN,     // Phi null check state transfer.
  kRangeCheckSrc1,       // Range check of uses[1].
  kRangeCheckSrc2,       // Range check of uses[2].
  kRangeCheckSrc3,       // Range check of uses[3].
  kFPA,
  kFPB,
  kFPC,
  kCoreA,
  kCoreB,
  kCoreC,
  kRefA,
  kRefB,
  kRefC,
  kUsesMethodStar,       // Implicit use of Method*.
};

class InstructionTools {
 public:
  static bool IsDefinition(const art::Instruction* instruction);
  static const int instruction_attributes_[];
};
}  // namespace sea_ir
#endif  // ART_COMPILER_SEA_IR_IR_INSTRUCTION_TOOLS_H_
