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

#include "register_line.h"

#include "dex_instruction-inl.h"
#include "method_verifier.h"
#include "register_line-inl.h"

namespace art {
namespace verifier {

bool RegisterLine::CheckConstructorReturn() const {
  for (size_t i = 0; i < num_regs_; i++) {
    if (GetRegisterType(i).IsUninitializedThisReference() ||
        GetRegisterType(i).IsUnresolvedAndUninitializedThisReference()) {
      verifier_->Fail(VERIFY_ERROR_BAD_CLASS_SOFT)
          << "Constructor returning without calling superclass constructor";
      return false;
    }
  }
  return true;
}

bool RegisterLine::SetRegisterType(uint32_t vdst, const RegType& new_type) {
  DCHECK_LT(vdst, num_regs_);
  if (new_type.IsLowHalf() || new_type.IsHighHalf()) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Expected category1 register type not '"
        << new_type << "'";
    return false;
  } else if (new_type.IsConflict()) {  // should only be set during a merge
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_SOFT) << "Set register to unknown type " << new_type;
    return false;
  } else {
    line_[vdst] = new_type.GetId();
  }
  // Clear the monitor entry bits for this register.
  ClearAllRegToLockDepths(vdst);
  return true;
}

bool RegisterLine::SetRegisterTypeWide(uint32_t vdst, const RegType& new_type1,
                                       const RegType& new_type2) {
  DCHECK_LT(vdst, num_regs_);
  if (!new_type1.CheckWidePair(new_type2)) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_SOFT) << "Invalid wide pair '"
        << new_type1 << "' '" << new_type2 << "'";
    return false;
  } else {
    line_[vdst] = new_type1.GetId();
    line_[vdst + 1] = new_type2.GetId();
  }
  // Clear the monitor entry bits for this register.
  ClearAllRegToLockDepths(vdst);
  ClearAllRegToLockDepths(vdst + 1);
  return true;
}

void RegisterLine::SetResultTypeToUnknown() {
  result_[0] = verifier_->GetRegTypeCache()->Undefined().GetId();
  result_[1] = result_[0];
}

void RegisterLine::SetResultRegisterType(const RegType& new_type) {
  DCHECK(!new_type.IsLowHalf());
  DCHECK(!new_type.IsHighHalf());
  result_[0] = new_type.GetId();
  result_[1] = verifier_->GetRegTypeCache()->Undefined().GetId();
}

void RegisterLine::SetResultRegisterTypeWide(const RegType& new_type1,
                                             const RegType& new_type2) {
  DCHECK(new_type1.CheckWidePair(new_type2));
  result_[0] = new_type1.GetId();
  result_[1] = new_type2.GetId();
}

const RegType& RegisterLine::GetInvocationThis(const Instruction* inst, bool is_range) {
  const size_t args_count = is_range ? inst->VRegA_3rc() : inst->VRegA_35c();
  if (args_count < 1) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invoke lacks 'this'";
    return verifier_->GetRegTypeCache()->Conflict();
  }
  /* get the element type of the array held in vsrc */
  const uint32_t this_reg = (is_range) ? inst->VRegC_3rc() : inst->VRegC_35c();
  const RegType& this_type = GetRegisterType(this_reg);
  if (!this_type.IsReferenceTypes()) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "tried to get class from non-reference register v"
                                                 << this_reg << " (type=" << this_type << ")";
    return verifier_->GetRegTypeCache()->Conflict();
  }
  return this_type;
}

bool RegisterLine::VerifyRegisterType(uint32_t vsrc,
                                      const RegType& check_type) {
  // Verify the src register type against the check type refining the type of the register
  const RegType& src_type = GetRegisterType(vsrc);
  if (!(check_type.IsAssignableFrom(src_type))) {
    enum VerifyError fail_type;
    if (!check_type.IsNonZeroReferenceTypes() || !src_type.IsNonZeroReferenceTypes()) {
      // Hard fail if one of the types is primitive, since they are concretely known.
      fail_type = VERIFY_ERROR_BAD_CLASS_HARD;
    } else if (check_type.IsUnresolvedTypes() || src_type.IsUnresolvedTypes()) {
      fail_type = VERIFY_ERROR_NO_CLASS;
    } else {
      fail_type = VERIFY_ERROR_BAD_CLASS_SOFT;
    }
    verifier_->Fail(fail_type) << "register v" << vsrc << " has type "
                               << src_type << " but expected " << check_type;
    return false;
  }
  if (check_type.IsLowHalf()) {
    const RegType& src_type_h = GetRegisterType(vsrc + 1);
    if (!src_type.CheckWidePair(src_type_h)) {
      verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "wide register v" << vsrc << " has type "
                                                   << src_type << "/" << src_type_h;
      return false;
    }
  }
  // The register at vsrc has a defined type, we know the lower-upper-bound, but this is less
  // precise than the subtype in vsrc so leave it for reference types. For primitive types
  // if they are a defined type then they are as precise as we can get, however, for constant
  // types we may wish to refine them. Unfortunately constant propagation has rendered this useless.
  return true;
}

bool RegisterLine::VerifyRegisterTypeWide(uint32_t vsrc, const RegType& check_type1,
                                          const RegType& check_type2) {
  DCHECK(check_type1.CheckWidePair(check_type2));
  // Verify the src register type against the check type refining the type of the register
  const RegType& src_type = GetRegisterType(vsrc);
  if (!check_type1.IsAssignableFrom(src_type)) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "register v" << vsrc << " has type " << src_type
                               << " but expected " << check_type1;
    return false;
  }
  const RegType& src_type_h = GetRegisterType(vsrc + 1);
  if (!src_type.CheckWidePair(src_type_h)) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "wide register v" << vsrc << " has type "
        << src_type << "/" << src_type_h;
    return false;
  }
  // The register at vsrc has a defined type, we know the lower-upper-bound, but this is less
  // precise than the subtype in vsrc so leave it for reference types. For primitive types
  // if they are a defined type then they are as precise as we can get, however, for constant
  // types we may wish to refine them. Unfortunately constant propagation has rendered this useless.
  return true;
}

void RegisterLine::MarkRefsAsInitialized(const RegType& uninit_type) {
  DCHECK(uninit_type.IsUninitializedTypes());
  const RegType& init_type = verifier_->GetRegTypeCache()->FromUninitialized(uninit_type);
  size_t changed = 0;
  for (uint32_t i = 0; i < num_regs_; i++) {
    if (GetRegisterType(i).Equals(uninit_type)) {
      line_[i] = init_type.GetId();
      changed++;
    }
  }
  DCHECK_GT(changed, 0u);
}

void RegisterLine::MarkAllRegistersAsConflicts() {
  uint16_t conflict_type_id = verifier_->GetRegTypeCache()->Conflict().GetId();
  for (uint32_t i = 0; i < num_regs_; i++) {
    line_[i] = conflict_type_id;
  }
}

void RegisterLine::MarkAllRegistersAsConflictsExcept(uint32_t vsrc) {
  uint16_t conflict_type_id = verifier_->GetRegTypeCache()->Conflict().GetId();
  for (uint32_t i = 0; i < num_regs_; i++) {
    if (i != vsrc) {
      line_[i] = conflict_type_id;
    }
  }
}

void RegisterLine::MarkAllRegistersAsConflictsExceptWide(uint32_t vsrc) {
  uint16_t conflict_type_id = verifier_->GetRegTypeCache()->Conflict().GetId();
  for (uint32_t i = 0; i < num_regs_; i++) {
    if ((i != vsrc) && (i != (vsrc + 1))) {
      line_[i] = conflict_type_id;
    }
  }
}

std::string RegisterLine::Dump() const {
  std::string result;
  for (size_t i = 0; i < num_regs_; i++) {
    result += StringPrintf("%zd:[", i);
    result += GetRegisterType(i).Dump();
    result += "],";
  }
  for (const auto& monitor : monitors_) {
    result += StringPrintf("{%d},", monitor);
  }
  return result;
}

void RegisterLine::MarkUninitRefsAsInvalid(const RegType& uninit_type) {
  for (size_t i = 0; i < num_regs_; i++) {
    if (GetRegisterType(i).Equals(uninit_type)) {
      line_[i] = verifier_->GetRegTypeCache()->Conflict().GetId();
      ClearAllRegToLockDepths(i);
    }
  }
}

void RegisterLine::CopyRegister1(uint32_t vdst, uint32_t vsrc, TypeCategory cat) {
  DCHECK(cat == kTypeCategory1nr || cat == kTypeCategoryRef);
  const RegType& type = GetRegisterType(vsrc);
  if (!SetRegisterType(vdst, type)) {
    return;
  }
  if ((cat == kTypeCategory1nr && !type.IsCategory1Types()) ||
      (cat == kTypeCategoryRef && !type.IsReferenceTypes())) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "copy1 v" << vdst << "<-v" << vsrc << " type=" << type
                                                 << " cat=" << static_cast<int>(cat);
  } else if (cat == kTypeCategoryRef) {
    CopyRegToLockDepth(vdst, vsrc);
  }
}

void RegisterLine::CopyRegister2(uint32_t vdst, uint32_t vsrc) {
  const RegType& type_l = GetRegisterType(vsrc);
  const RegType& type_h = GetRegisterType(vsrc + 1);

  if (!type_l.CheckWidePair(type_h)) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "copy2 v" << vdst << "<-v" << vsrc
                                                 << " type=" << type_l << "/" << type_h;
  } else {
    SetRegisterTypeWide(vdst, type_l, type_h);
  }
}

void RegisterLine::CopyResultRegister1(uint32_t vdst, bool is_reference) {
  const RegType& type = verifier_->GetRegTypeCache()->GetFromId(result_[0]);
  if ((!is_reference && !type.IsCategory1Types()) ||
      (is_reference && !type.IsReferenceTypes())) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD)
        << "copyRes1 v" << vdst << "<- result0"  << " type=" << type;
  } else {
    DCHECK(verifier_->GetRegTypeCache()->GetFromId(result_[1]).IsUndefined());
    SetRegisterType(vdst, type);
    result_[0] = verifier_->GetRegTypeCache()->Undefined().GetId();
  }
}

/*
 * Implement "move-result-wide". Copy the category-2 value from the result
 * register to another register, and reset the result register.
 */
void RegisterLine::CopyResultRegister2(uint32_t vdst) {
  const RegType& type_l = verifier_->GetRegTypeCache()->GetFromId(result_[0]);
  const RegType& type_h = verifier_->GetRegTypeCache()->GetFromId(result_[1]);
  if (!type_l.IsCategory2Types()) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD)
        << "copyRes2 v" << vdst << "<- result0"  << " type=" << type_l;
  } else {
    DCHECK(type_l.CheckWidePair(type_h));  // Set should never allow this case
    SetRegisterTypeWide(vdst, type_l, type_h);  // also sets the high
    result_[0] = verifier_->GetRegTypeCache()->Undefined().GetId();
    result_[1] = verifier_->GetRegTypeCache()->Undefined().GetId();
  }
}

void RegisterLine::CheckUnaryOp(const Instruction* inst,
                                const RegType& dst_type,
                                const RegType& src_type) {
  if (VerifyRegisterType(inst->VRegB_12x(), src_type)) {
    SetRegisterType(inst->VRegA_12x(), dst_type);
  }
}

void RegisterLine::CheckUnaryOpWide(const Instruction* inst,
                                    const RegType& dst_type1, const RegType& dst_type2,
                                    const RegType& src_type1, const RegType& src_type2) {
  if (VerifyRegisterTypeWide(inst->VRegB_12x(), src_type1, src_type2)) {
    SetRegisterTypeWide(inst->VRegA_12x(), dst_type1, dst_type2);
  }
}

void RegisterLine::CheckUnaryOpToWide(const Instruction* inst,
                                      const RegType& dst_type1, const RegType& dst_type2,
                                      const RegType& src_type) {
  if (VerifyRegisterType(inst->VRegB_12x(), src_type)) {
    SetRegisterTypeWide(inst->VRegA_12x(), dst_type1, dst_type2);
  }
}

void RegisterLine::CheckUnaryOpFromWide(const Instruction* inst,
                                        const RegType& dst_type,
                                        const RegType& src_type1, const RegType& src_type2) {
  if (VerifyRegisterTypeWide(inst->VRegB_12x(), src_type1, src_type2)) {
    SetRegisterType(inst->VRegA_12x(), dst_type);
  }
}

void RegisterLine::CheckBinaryOp(const Instruction* inst,
                                 const RegType& dst_type,
                                 const RegType& src_type1, const RegType& src_type2,
                                 bool check_boolean_op) {
  const uint32_t vregB = inst->VRegB_23x();
  const uint32_t vregC = inst->VRegC_23x();
  if (VerifyRegisterType(vregB, src_type1) &&
      VerifyRegisterType(vregC, src_type2)) {
    if (check_boolean_op) {
      DCHECK(dst_type.IsInteger());
      if (GetRegisterType(vregB).IsBooleanTypes() &&
          GetRegisterType(vregC).IsBooleanTypes()) {
        SetRegisterType(inst->VRegA_23x(), verifier_->GetRegTypeCache()->Boolean());
        return;
      }
    }
    SetRegisterType(inst->VRegA_23x(), dst_type);
  }
}

void RegisterLine::CheckBinaryOpWide(const Instruction* inst,
                                     const RegType& dst_type1, const RegType& dst_type2,
                                     const RegType& src_type1_1, const RegType& src_type1_2,
                                     const RegType& src_type2_1, const RegType& src_type2_2) {
  if (VerifyRegisterTypeWide(inst->VRegB_23x(), src_type1_1, src_type1_2) &&
      VerifyRegisterTypeWide(inst->VRegC_23x(), src_type2_1, src_type2_2)) {
    SetRegisterTypeWide(inst->VRegA_23x(), dst_type1, dst_type2);
  }
}

void RegisterLine::CheckBinaryOpWideShift(const Instruction* inst,
                                          const RegType& long_lo_type, const RegType& long_hi_type,
                                          const RegType& int_type) {
  if (VerifyRegisterTypeWide(inst->VRegB_23x(), long_lo_type, long_hi_type) &&
      VerifyRegisterType(inst->VRegC_23x(), int_type)) {
    SetRegisterTypeWide(inst->VRegA_23x(), long_lo_type, long_hi_type);
  }
}

void RegisterLine::CheckBinaryOp2addr(const Instruction* inst,
                                      const RegType& dst_type, const RegType& src_type1,
                                      const RegType& src_type2, bool check_boolean_op) {
  const uint32_t vregA = inst->VRegA_12x();
  const uint32_t vregB = inst->VRegB_12x();
  if (VerifyRegisterType(vregA, src_type1) &&
      VerifyRegisterType(vregB, src_type2)) {
    if (check_boolean_op) {
      DCHECK(dst_type.IsInteger());
      if (GetRegisterType(vregA).IsBooleanTypes() &&
          GetRegisterType(vregB).IsBooleanTypes()) {
        SetRegisterType(vregA, verifier_->GetRegTypeCache()->Boolean());
        return;
      }
    }
    SetRegisterType(vregA, dst_type);
  }
}

void RegisterLine::CheckBinaryOp2addrWide(const Instruction* inst,
                                          const RegType& dst_type1, const RegType& dst_type2,
                                          const RegType& src_type1_1, const RegType& src_type1_2,
                                          const RegType& src_type2_1, const RegType& src_type2_2) {
  const uint32_t vregA = inst->VRegA_12x();
  const uint32_t vregB = inst->VRegB_12x();
  if (VerifyRegisterTypeWide(vregA, src_type1_1, src_type1_2) &&
      VerifyRegisterTypeWide(vregB, src_type2_1, src_type2_2)) {
    SetRegisterTypeWide(vregA, dst_type1, dst_type2);
  }
}

void RegisterLine::CheckBinaryOp2addrWideShift(const Instruction* inst,
                                               const RegType& long_lo_type, const RegType& long_hi_type,
                                               const RegType& int_type) {
  const uint32_t vregA = inst->VRegA_12x();
  const uint32_t vregB = inst->VRegB_12x();
  if (VerifyRegisterTypeWide(vregA, long_lo_type, long_hi_type) &&
      VerifyRegisterType(vregB, int_type)) {
    SetRegisterTypeWide(vregA, long_lo_type, long_hi_type);
  }
}

void RegisterLine::CheckLiteralOp(const Instruction* inst,
                                  const RegType& dst_type, const RegType& src_type,
                                  bool check_boolean_op, bool is_lit16) {
  const uint32_t vregA = is_lit16 ? inst->VRegA_22s() : inst->VRegA_22b();
  const uint32_t vregB = is_lit16 ? inst->VRegB_22s() : inst->VRegB_22b();
  if (VerifyRegisterType(vregB, src_type)) {
    if (check_boolean_op) {
      DCHECK(dst_type.IsInteger());
      /* check vB with the call, then check the constant manually */
      const uint32_t val = is_lit16 ? inst->VRegC_22s() : inst->VRegC_22b();
      if (GetRegisterType(vregB).IsBooleanTypes() && (val == 0 || val == 1)) {
        SetRegisterType(vregA, verifier_->GetRegTypeCache()->Boolean());
        return;
      }
    }
    SetRegisterType(vregA, dst_type);
  }
}

void RegisterLine::PushMonitor(uint32_t reg_idx, int32_t insn_idx) {
  const RegType& reg_type = GetRegisterType(reg_idx);
  if (!reg_type.IsReferenceTypes()) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "monitor-enter on non-object (" << reg_type << ")";
  } else if (monitors_.size() >= 32) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "monitor-enter stack overflow: " << monitors_.size();
  } else {
    SetRegToLockDepth(reg_idx, monitors_.size());
    monitors_.push_back(insn_idx);
  }
}

void RegisterLine::PopMonitor(uint32_t reg_idx) {
  const RegType& reg_type = GetRegisterType(reg_idx);
  if (!reg_type.IsReferenceTypes()) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "monitor-exit on non-object (" << reg_type << ")";
  } else if (monitors_.empty()) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "monitor-exit stack underflow";
  } else {
    monitors_.pop_back();
    if (!IsSetLockDepth(reg_idx, monitors_.size())) {
      // Bug 3215458: Locks and unlocks are on objects, if that object is a literal then before
      // format "036" the constant collector may create unlocks on the same object but referenced
      // via different registers.
      ((verifier_->DexFileVersion() >= 36) ? verifier_->Fail(VERIFY_ERROR_BAD_CLASS_SOFT)
                                           : verifier_->LogVerifyInfo())
            << "monitor-exit not unlocking the top of the monitor stack";
    } else {
      // Record the register was unlocked
      ClearRegToLockDepth(reg_idx, monitors_.size());
    }
  }
}

bool RegisterLine::VerifyMonitorStackEmpty() const {
  if (MonitorStackDepth() != 0) {
    verifier_->Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "expected empty monitor stack";
    return false;
  } else {
    return true;
  }
}

bool RegisterLine::MergeRegisters(const RegisterLine* incoming_line) {
  bool changed = false;
  CHECK(NULL != incoming_line);
  CHECK(NULL != line_.get());
  for (size_t idx = 0; idx < num_regs_; idx++) {
    if (line_[idx] != incoming_line->line_[idx]) {
      const RegType& incoming_reg_type = incoming_line->GetRegisterType(idx);
      const RegType& cur_type = GetRegisterType(idx);
      const RegType& new_type = cur_type.Merge(incoming_reg_type, verifier_->GetRegTypeCache());
      changed = changed || !cur_type.Equals(new_type);
      line_[idx] = new_type.GetId();
    }
  }
  if (monitors_.size() != incoming_line->monitors_.size()) {
    LOG(WARNING) << "mismatched stack depths (depth=" << MonitorStackDepth()
                 << ", incoming depth=" << incoming_line->MonitorStackDepth() << ")";
  } else if (reg_to_lock_depths_ != incoming_line->reg_to_lock_depths_) {
    for (uint32_t idx = 0; idx < num_regs_; idx++) {
      size_t depths = reg_to_lock_depths_.count(idx);
      size_t incoming_depths = incoming_line->reg_to_lock_depths_.count(idx);
      if (depths != incoming_depths) {
        if (depths == 0 || incoming_depths == 0) {
          reg_to_lock_depths_.erase(idx);
        } else {
          LOG(WARNING) << "mismatched stack depths for register v" << idx
                       << ": " << depths  << " != " << incoming_depths;
          break;
        }
      }
    }
  }
  return changed;
}

void RegisterLine::WriteReferenceBitMap(std::vector<uint8_t>& data, size_t max_bytes) {
  for (size_t i = 0; i < num_regs_; i += 8) {
    uint8_t val = 0;
    for (size_t j = 0; j < 8 && (i + j) < num_regs_; j++) {
      // Note: we write 1 for a Reference but not for Null
      if (GetRegisterType(i + j).IsNonZeroReferenceTypes()) {
        val |= 1 << j;
      }
    }
    if ((i / 8) >= max_bytes) {
      DCHECK_EQ(0, val);
      continue;
    }
    DCHECK_LT(i / 8, max_bytes) << "val=" << static_cast<uint32_t>(val);
    data.push_back(val);
  }
}

std::ostream& operator<<(std::ostream& os, const RegisterLine& rhs)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  os << rhs.Dump();
  return os;
}

}  // namespace verifier
}  // namespace art
