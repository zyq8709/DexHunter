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

#include "method_verifier.h"

#include <iostream>

#include "base/logging.h"
#include "base/mutex-inl.h"
#include "base/stringpiece.h"
#include "class_linker.h"
#include "dex_file-inl.h"
#include "dex_instruction-inl.h"
#include "dex_instruction_visitor.h"
#include "gc/accounting/card_table-inl.h"
#include "indenter.h"
#include "intern_table.h"
#include "leb128.h"
#include "mirror/art_field-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "object_utils.h"
#include "register_line-inl.h"
#include "runtime.h"
#include "verifier/dex_gc_map.h"

namespace art {
namespace verifier {

static const bool gDebugVerify = false;
// TODO: Add a constant to method_verifier to turn on verbose logging?

void PcToRegisterLineTable::Init(RegisterTrackingMode mode, InstructionFlags* flags,
                                 uint32_t insns_size, uint16_t registers_size,
                                 MethodVerifier* verifier) {
  DCHECK_GT(insns_size, 0U);

  for (uint32_t i = 0; i < insns_size; i++) {
    bool interesting = false;
    switch (mode) {
      case kTrackRegsAll:
        interesting = flags[i].IsOpcode();
        break;
      case kTrackCompilerInterestPoints:
        interesting = flags[i].IsCompileTimeInfoPoint() || flags[i].IsBranchTarget();
        break;
      case kTrackRegsBranches:
        interesting = flags[i].IsBranchTarget();
        break;
      default:
        break;
    }
    if (interesting) {
      pc_to_register_line_.Put(i, new RegisterLine(registers_size, verifier));
    }
  }
}

MethodVerifier::FailureKind MethodVerifier::VerifyClass(const mirror::Class* klass,
                                                        bool allow_soft_failures,
                                                        std::string* error) {
  if (klass->IsVerified()) {
    return kNoFailure;
  }
  mirror::Class* super = klass->GetSuperClass();
  if (super == NULL && StringPiece(ClassHelper(klass).GetDescriptor()) != "Ljava/lang/Object;") {
    *error = "Verifier rejected class ";
    *error += PrettyDescriptor(klass);
    *error += " that has no super class";
    return kHardFailure;
  }
  if (super != NULL && super->IsFinal()) {
    *error = "Verifier rejected class ";
    *error += PrettyDescriptor(klass);
    *error += " that attempts to sub-class final class ";
    *error += PrettyDescriptor(super);
    return kHardFailure;
  }
  ClassHelper kh(klass);
  const DexFile& dex_file = kh.GetDexFile();
  const DexFile::ClassDef* class_def = kh.GetClassDef();
  if (class_def == NULL) {
    *error = "Verifier rejected class ";
    *error += PrettyDescriptor(klass);
    *error += " that isn't present in dex file ";
    *error += dex_file.GetLocation();
    return kHardFailure;
  }
  return VerifyClass(&dex_file,
                     kh.GetDexCache(),
                     klass->GetClassLoader(),
                     class_def,
                     allow_soft_failures,
                     error);
}

MethodVerifier::FailureKind MethodVerifier::VerifyClass(const DexFile* dex_file,
                                                        mirror::DexCache* dex_cache,
                                                        mirror::ClassLoader* class_loader,
                                                        const DexFile::ClassDef* class_def,
                                                        bool allow_soft_failures,
                                                        std::string* error) {
  DCHECK(class_def != nullptr);
  const byte* class_data = dex_file->GetClassData(*class_def);
  if (class_data == NULL) {
    // empty class, probably a marker interface
    return kNoFailure;
  }
  ClassDataItemIterator it(*dex_file, class_data);
  while (it.HasNextStaticField() || it.HasNextInstanceField()) {
    it.Next();
  }
  size_t error_count = 0;
  bool hard_fail = false;
  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  int64_t previous_direct_method_idx = -1;
  while (it.HasNextDirectMethod()) {
    uint32_t method_idx = it.GetMemberIndex();
    if (method_idx == previous_direct_method_idx) {
      // smali can create dex files with two encoded_methods sharing the same method_idx
      // http://code.google.com/p/smali/issues/detail?id=119
      it.Next();
      continue;
    }
    previous_direct_method_idx = method_idx;
    InvokeType type = it.GetMethodInvokeType(*class_def);
    mirror::ArtMethod* method =
        linker->ResolveMethod(*dex_file, method_idx, dex_cache, class_loader, NULL, type);
    if (method == NULL) {
      DCHECK(Thread::Current()->IsExceptionPending());
      // We couldn't resolve the method, but continue regardless.
      Thread::Current()->ClearException();
    }
    MethodVerifier::FailureKind result = VerifyMethod(method_idx,
                                                      dex_file,
                                                      dex_cache,
                                                      class_loader,
                                                      class_def,
                                                      it.GetMethodCodeItem(),
                                                      method,
                                                      it.GetMemberAccessFlags(),
                                                      allow_soft_failures);
    if (result != kNoFailure) {
      if (result == kHardFailure) {
        hard_fail = true;
        if (error_count > 0) {
          *error += "\n";
        }
        *error = "Verifier rejected class ";
        *error += PrettyDescriptor(dex_file->GetClassDescriptor(*class_def));
        *error += " due to bad method ";
        *error += PrettyMethod(method_idx, *dex_file);
      }
      ++error_count;
    }
    it.Next();
  }
  int64_t previous_virtual_method_idx = -1;
  while (it.HasNextVirtualMethod()) {
    uint32_t method_idx = it.GetMemberIndex();
    if (method_idx == previous_virtual_method_idx) {
      // smali can create dex files with two encoded_methods sharing the same method_idx
      // http://code.google.com/p/smali/issues/detail?id=119
      it.Next();
      continue;
    }
    previous_virtual_method_idx = method_idx;
    InvokeType type = it.GetMethodInvokeType(*class_def);
    mirror::ArtMethod* method =
        linker->ResolveMethod(*dex_file, method_idx, dex_cache, class_loader, NULL, type);
    if (method == NULL) {
      DCHECK(Thread::Current()->IsExceptionPending());
      // We couldn't resolve the method, but continue regardless.
      Thread::Current()->ClearException();
    }
    MethodVerifier::FailureKind result = VerifyMethod(method_idx,
                                                      dex_file,
                                                      dex_cache,
                                                      class_loader,
                                                      class_def,
                                                      it.GetMethodCodeItem(),
                                                      method,
                                                      it.GetMemberAccessFlags(),
                                                      allow_soft_failures);
    if (result != kNoFailure) {
      if (result == kHardFailure) {
        hard_fail = true;
        if (error_count > 0) {
          *error += "\n";
        }
        *error = "Verifier rejected class ";
        *error += PrettyDescriptor(dex_file->GetClassDescriptor(*class_def));
        *error += " due to bad method ";
        *error += PrettyMethod(method_idx, *dex_file);
      }
      ++error_count;
    }
    it.Next();
  }
  if (error_count == 0) {
    return kNoFailure;
  } else {
    return hard_fail ? kHardFailure : kSoftFailure;
  }
}

MethodVerifier::FailureKind MethodVerifier::VerifyMethod(uint32_t method_idx,
                                                         const DexFile* dex_file,
                                                         mirror::DexCache* dex_cache,
                                                         mirror::ClassLoader* class_loader,
                                                         const DexFile::ClassDef* class_def,
                                                         const DexFile::CodeItem* code_item,
                                                         mirror::ArtMethod* method,
                                                         uint32_t method_access_flags,
                                                         bool allow_soft_failures) {
  MethodVerifier::FailureKind result = kNoFailure;
  uint64_t start_ns = NanoTime();

  MethodVerifier verifier_(dex_file, dex_cache, class_loader, class_def, code_item, method_idx,
                           method, method_access_flags, true, allow_soft_failures);
  if (verifier_.Verify()) {
    // Verification completed, however failures may be pending that didn't cause the verification
    // to hard fail.
    CHECK(!verifier_.have_pending_hard_failure_);
    if (verifier_.failures_.size() != 0) {
      if (VLOG_IS_ON(verifier)) {
          verifier_.DumpFailures(VLOG_STREAM(verifier) << "Soft verification failures in "
                                << PrettyMethod(method_idx, *dex_file) << "\n");
      }
      result = kSoftFailure;
    }
  } else {
    // Bad method data.
    CHECK_NE(verifier_.failures_.size(), 0U);
    CHECK(verifier_.have_pending_hard_failure_);
    verifier_.DumpFailures(LOG(INFO) << "Verification error in "
                                    << PrettyMethod(method_idx, *dex_file) << "\n");
    if (gDebugVerify) {
      std::cout << "\n" << verifier_.info_messages_.str();
      verifier_.Dump(std::cout);
    }
    result = kHardFailure;
  }
  uint64_t duration_ns = NanoTime() - start_ns;
  if (duration_ns > MsToNs(100)) {
    LOG(WARNING) << "Verification of " << PrettyMethod(method_idx, *dex_file)
                 << " took " << PrettyDuration(duration_ns);
  }
  return result;
}

void MethodVerifier::VerifyMethodAndDump(std::ostream& os, uint32_t dex_method_idx,
                                         const DexFile* dex_file, mirror::DexCache* dex_cache,
                                         mirror::ClassLoader* class_loader,
                                         const DexFile::ClassDef* class_def,
                                         const DexFile::CodeItem* code_item,
                                         mirror::ArtMethod* method,
                                         uint32_t method_access_flags) {
  MethodVerifier verifier(dex_file, dex_cache, class_loader, class_def, code_item,
                          dex_method_idx, method, method_access_flags, true, true);
  verifier.Verify();
  verifier.DumpFailures(os);
  os << verifier.info_messages_.str();
  verifier.Dump(os);
}

MethodVerifier::MethodVerifier(const DexFile* dex_file, mirror::DexCache* dex_cache,
                               mirror::ClassLoader* class_loader,
                               const DexFile::ClassDef* class_def,
                               const DexFile::CodeItem* code_item,
                               uint32_t dex_method_idx, mirror::ArtMethod* method,
                               uint32_t method_access_flags, bool can_load_classes,
                               bool allow_soft_failures)
    : reg_types_(can_load_classes),
      work_insn_idx_(-1),
      dex_method_idx_(dex_method_idx),
      mirror_method_(method),
      method_access_flags_(method_access_flags),
      dex_file_(dex_file),
      dex_cache_(dex_cache),
      class_loader_(class_loader),
      class_def_(class_def),
      code_item_(code_item),
      declaring_class_(NULL),
      interesting_dex_pc_(-1),
      monitor_enter_dex_pcs_(NULL),
      have_pending_hard_failure_(false),
      have_pending_runtime_throw_failure_(false),
      new_instance_count_(0),
      monitor_enter_count_(0),
      can_load_classes_(can_load_classes),
      allow_soft_failures_(allow_soft_failures),
      has_check_casts_(false),
      has_virtual_or_interface_invokes_(false) {
  DCHECK(class_def != NULL);
}

void MethodVerifier::FindLocksAtDexPc(mirror::ArtMethod* m, uint32_t dex_pc,
                                      std::vector<uint32_t>& monitor_enter_dex_pcs) {
  MethodHelper mh(m);
  MethodVerifier verifier(&mh.GetDexFile(), mh.GetDexCache(), mh.GetClassLoader(),
                          &mh.GetClassDef(), mh.GetCodeItem(), m->GetDexMethodIndex(),
                          m, m->GetAccessFlags(), false, true);
  verifier.interesting_dex_pc_ = dex_pc;
  verifier.monitor_enter_dex_pcs_ = &monitor_enter_dex_pcs;
  verifier.FindLocksAtDexPc();
}

void MethodVerifier::FindLocksAtDexPc() {
  CHECK(monitor_enter_dex_pcs_ != NULL);
  CHECK(code_item_ != NULL);  // This only makes sense for methods with code.

  // Strictly speaking, we ought to be able to get away with doing a subset of the full method
  // verification. In practice, the phase we want relies on data structures set up by all the
  // earlier passes, so we just run the full method verification and bail out early when we've
  // got what we wanted.
  Verify();
}

mirror::ArtField* MethodVerifier::FindAccessedFieldAtDexPc(mirror::ArtMethod* m,
                                                        uint32_t dex_pc) {
  MethodHelper mh(m);
  MethodVerifier verifier(&mh.GetDexFile(), mh.GetDexCache(), mh.GetClassLoader(),
                          &mh.GetClassDef(), mh.GetCodeItem(), m->GetDexMethodIndex(),
                          m, m->GetAccessFlags(), false, true);
  return verifier.FindAccessedFieldAtDexPc(dex_pc);
}

mirror::ArtField* MethodVerifier::FindAccessedFieldAtDexPc(uint32_t dex_pc) {
  CHECK(code_item_ != NULL);  // This only makes sense for methods with code.

  // Strictly speaking, we ought to be able to get away with doing a subset of the full method
  // verification. In practice, the phase we want relies on data structures set up by all the
  // earlier passes, so we just run the full method verification and bail out early when we've
  // got what we wanted.
  bool success = Verify();
  if (!success) {
    return NULL;
  }
  RegisterLine* register_line = reg_table_.GetLine(dex_pc);
  if (register_line == NULL) {
    return NULL;
  }
  const Instruction* inst = Instruction::At(code_item_->insns_ + dex_pc);
  return GetQuickFieldAccess(inst, register_line);
}

mirror::ArtMethod* MethodVerifier::FindInvokedMethodAtDexPc(mirror::ArtMethod* m,
                                                                 uint32_t dex_pc) {
  MethodHelper mh(m);
  MethodVerifier verifier(&mh.GetDexFile(), mh.GetDexCache(), mh.GetClassLoader(),
                          &mh.GetClassDef(), mh.GetCodeItem(), m->GetDexMethodIndex(),
                          m, m->GetAccessFlags(), false, true);
  return verifier.FindInvokedMethodAtDexPc(dex_pc);
}

mirror::ArtMethod* MethodVerifier::FindInvokedMethodAtDexPc(uint32_t dex_pc) {
  CHECK(code_item_ != NULL);  // This only makes sense for methods with code.

  // Strictly speaking, we ought to be able to get away with doing a subset of the full method
  // verification. In practice, the phase we want relies on data structures set up by all the
  // earlier passes, so we just run the full method verification and bail out early when we've
  // got what we wanted.
  bool success = Verify();
  if (!success) {
    return NULL;
  }
  RegisterLine* register_line = reg_table_.GetLine(dex_pc);
  if (register_line == NULL) {
    return NULL;
  }
  const Instruction* inst = Instruction::At(code_item_->insns_ + dex_pc);
  const bool is_range = (inst->Opcode() == Instruction::INVOKE_VIRTUAL_RANGE_QUICK);
  return GetQuickInvokedMethod(inst, register_line, is_range);
}

bool MethodVerifier::Verify() {
  // If there aren't any instructions, make sure that's expected, then exit successfully.
  if (code_item_ == NULL) {
    if ((method_access_flags_ & (kAccNative | kAccAbstract)) == 0) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "zero-length code in concrete non-native method";
      return false;
    } else {
      return true;
    }
  }
  // Sanity-check the register counts. ins + locals = registers, so make sure that ins <= registers.
  if (code_item_->ins_size_ > code_item_->registers_size_) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "bad register counts (ins=" << code_item_->ins_size_
                                      << " regs=" << code_item_->registers_size_;
    return false;
  }
  // Allocate and initialize an array to hold instruction data.
  insn_flags_.reset(new InstructionFlags[code_item_->insns_size_in_code_units_]());
  // Run through the instructions and see if the width checks out.
  bool result = ComputeWidthsAndCountOps();
  // Flag instructions guarded by a "try" block and check exception handlers.
  result = result && ScanTryCatchBlocks();
  // Perform static instruction verification.
  result = result && VerifyInstructions();
  // Perform code-flow analysis and return.
  return result && VerifyCodeFlow();
}

std::ostream& MethodVerifier::Fail(VerifyError error) {
  switch (error) {
    case VERIFY_ERROR_NO_CLASS:
    case VERIFY_ERROR_NO_FIELD:
    case VERIFY_ERROR_NO_METHOD:
    case VERIFY_ERROR_ACCESS_CLASS:
    case VERIFY_ERROR_ACCESS_FIELD:
    case VERIFY_ERROR_ACCESS_METHOD:
    case VERIFY_ERROR_INSTANTIATION:
    case VERIFY_ERROR_CLASS_CHANGE:
      if (Runtime::Current()->IsCompiler() || !can_load_classes_) {
        // If we're optimistically running verification at compile time, turn NO_xxx, ACCESS_xxx,
        // class change and instantiation errors into soft verification errors so that we re-verify
        // at runtime. We may fail to find or to agree on access because of not yet available class
        // loaders, or class loaders that will differ at runtime. In these cases, we don't want to
        // affect the soundness of the code being compiled. Instead, the generated code runs "slow
        // paths" that dynamically perform the verification and cause the behavior to be that akin
        // to an interpreter.
        error = VERIFY_ERROR_BAD_CLASS_SOFT;
      } else {
        // If we fail again at runtime, mark that this instruction would throw and force this
        // method to be executed using the interpreter with checks.
        have_pending_runtime_throw_failure_ = true;
      }
      break;
      // Indication that verification should be retried at runtime.
    case VERIFY_ERROR_BAD_CLASS_SOFT:
      if (!allow_soft_failures_) {
        have_pending_hard_failure_ = true;
      }
      break;
      // Hard verification failures at compile time will still fail at runtime, so the class is
      // marked as rejected to prevent it from being compiled.
    case VERIFY_ERROR_BAD_CLASS_HARD: {
      if (Runtime::Current()->IsCompiler()) {
        ClassReference ref(dex_file_, dex_file_->GetIndexForClassDef(*class_def_));
        AddRejectedClass(ref);
      }
      have_pending_hard_failure_ = true;
      break;
    }
  }
  failures_.push_back(error);
  std::string location(StringPrintf("%s: [0x%X]", PrettyMethod(dex_method_idx_, *dex_file_).c_str(),
                                    work_insn_idx_));
  std::ostringstream* failure_message = new std::ostringstream(location);
  failure_messages_.push_back(failure_message);
  return *failure_message;
}

void MethodVerifier::PrependToLastFailMessage(std::string prepend) {
  size_t failure_num = failure_messages_.size();
  DCHECK_NE(failure_num, 0U);
  std::ostringstream* last_fail_message = failure_messages_[failure_num - 1];
  prepend += last_fail_message->str();
  failure_messages_[failure_num - 1] = new std::ostringstream(prepend);
  delete last_fail_message;
}

void MethodVerifier::AppendToLastFailMessage(std::string append) {
  size_t failure_num = failure_messages_.size();
  DCHECK_NE(failure_num, 0U);
  std::ostringstream* last_fail_message = failure_messages_[failure_num - 1];
  (*last_fail_message) << append;
}

bool MethodVerifier::ComputeWidthsAndCountOps() {
  const uint16_t* insns = code_item_->insns_;
  size_t insns_size = code_item_->insns_size_in_code_units_;
  const Instruction* inst = Instruction::At(insns);
  size_t new_instance_count = 0;
  size_t monitor_enter_count = 0;
  size_t dex_pc = 0;

  while (dex_pc < insns_size) {
    Instruction::Code opcode = inst->Opcode();
    if (opcode == Instruction::NEW_INSTANCE) {
      new_instance_count++;
    } else if (opcode == Instruction::MONITOR_ENTER) {
      monitor_enter_count++;
    } else if (opcode == Instruction::CHECK_CAST) {
      has_check_casts_ = true;
    } else if ((inst->Opcode() == Instruction::INVOKE_VIRTUAL) ||
              (inst->Opcode() ==  Instruction::INVOKE_VIRTUAL_RANGE) ||
              (inst->Opcode() == Instruction::INVOKE_INTERFACE) ||
              (inst->Opcode() == Instruction::INVOKE_INTERFACE_RANGE)) {
      has_virtual_or_interface_invokes_ = true;
    }
    size_t inst_size = inst->SizeInCodeUnits();
    insn_flags_[dex_pc].SetLengthInCodeUnits(inst_size);
    dex_pc += inst_size;
    inst = inst->Next();
  }

  if (dex_pc != insns_size) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "code did not end where expected ("
                                      << dex_pc << " vs. " << insns_size << ")";
    return false;
  }

  new_instance_count_ = new_instance_count;
  monitor_enter_count_ = monitor_enter_count;
  return true;
}

bool MethodVerifier::ScanTryCatchBlocks() {
  uint32_t tries_size = code_item_->tries_size_;
  if (tries_size == 0) {
    return true;
  }
  uint32_t insns_size = code_item_->insns_size_in_code_units_;
  const DexFile::TryItem* tries = DexFile::GetTryItems(*code_item_, 0);

  for (uint32_t idx = 0; idx < tries_size; idx++) {
    const DexFile::TryItem* try_item = &tries[idx];
    uint32_t start = try_item->start_addr_;
    uint32_t end = start + try_item->insn_count_;
    if ((start >= end) || (start >= insns_size) || (end > insns_size)) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "bad exception entry: startAddr=" << start
                                        << " endAddr=" << end << " (size=" << insns_size << ")";
      return false;
    }
    if (!insn_flags_[start].IsOpcode()) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD)
          << "'try' block starts inside an instruction (" << start << ")";
      return false;
    }
    for (uint32_t dex_pc = start; dex_pc < end;
        dex_pc += insn_flags_[dex_pc].GetLengthInCodeUnits()) {
      insn_flags_[dex_pc].SetInTry();
    }
  }
  // Iterate over each of the handlers to verify target addresses.
  const byte* handlers_ptr = DexFile::GetCatchHandlerData(*code_item_, 0);
  uint32_t handlers_size = DecodeUnsignedLeb128(&handlers_ptr);
  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  for (uint32_t idx = 0; idx < handlers_size; idx++) {
    CatchHandlerIterator iterator(handlers_ptr);
    for (; iterator.HasNext(); iterator.Next()) {
      uint32_t dex_pc= iterator.GetHandlerAddress();
      if (!insn_flags_[dex_pc].IsOpcode()) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD)
            << "exception handler starts at bad address (" << dex_pc << ")";
        return false;
      }
      insn_flags_[dex_pc].SetBranchTarget();
      // Ensure exception types are resolved so that they don't need resolution to be delivered,
      // unresolved exception types will be ignored by exception delivery
      if (iterator.GetHandlerTypeIndex() != DexFile::kDexNoIndex16) {
        mirror::Class* exception_type = linker->ResolveType(*dex_file_,
                                                            iterator.GetHandlerTypeIndex(),
                                                            dex_cache_, class_loader_);
        if (exception_type == NULL) {
          DCHECK(Thread::Current()->IsExceptionPending());
          Thread::Current()->ClearException();
        }
      }
    }
    handlers_ptr = iterator.EndDataPointer();
  }
  return true;
}

bool MethodVerifier::VerifyInstructions() {
  const Instruction* inst = Instruction::At(code_item_->insns_);

  /* Flag the start of the method as a branch target, and a GC point due to stack overflow errors */
  insn_flags_[0].SetBranchTarget();
  insn_flags_[0].SetCompileTimeInfoPoint();

  uint32_t insns_size = code_item_->insns_size_in_code_units_;
  for (uint32_t dex_pc = 0; dex_pc < insns_size;) {
    if (!VerifyInstruction(inst, dex_pc)) {
      DCHECK_NE(failures_.size(), 0U);
      return false;
    }
    /* Flag instructions that are garbage collection points */
    // All invoke points are marked as "Throw" points already.
    // We are relying on this to also count all the invokes as interesting.
    if (inst->IsBranch() || inst->IsSwitch() || inst->IsThrow()) {
      insn_flags_[dex_pc].SetCompileTimeInfoPoint();
    } else if (inst->IsReturn()) {
      insn_flags_[dex_pc].SetCompileTimeInfoPointAndReturn();
    }
    dex_pc += inst->SizeInCodeUnits();
    inst = inst->Next();
  }
  return true;
}

bool MethodVerifier::VerifyInstruction(const Instruction* inst, uint32_t code_offset) {
  DecodedInstruction dec_insn(inst);
  bool result = true;
  switch (inst->GetVerifyTypeArgumentA()) {
    case Instruction::kVerifyRegA:
      result = result && CheckRegisterIndex(dec_insn.vA);
      break;
    case Instruction::kVerifyRegAWide:
      result = result && CheckWideRegisterIndex(dec_insn.vA);
      break;
  }
  switch (inst->GetVerifyTypeArgumentB()) {
    case Instruction::kVerifyRegB:
      result = result && CheckRegisterIndex(dec_insn.vB);
      break;
    case Instruction::kVerifyRegBField:
      result = result && CheckFieldIndex(dec_insn.vB);
      break;
    case Instruction::kVerifyRegBMethod:
      result = result && CheckMethodIndex(dec_insn.vB);
      break;
    case Instruction::kVerifyRegBNewInstance:
      result = result && CheckNewInstance(dec_insn.vB);
      break;
    case Instruction::kVerifyRegBString:
      result = result && CheckStringIndex(dec_insn.vB);
      break;
    case Instruction::kVerifyRegBType:
      result = result && CheckTypeIndex(dec_insn.vB);
      break;
    case Instruction::kVerifyRegBWide:
      result = result && CheckWideRegisterIndex(dec_insn.vB);
      break;
  }
  switch (inst->GetVerifyTypeArgumentC()) {
    case Instruction::kVerifyRegC:
      result = result && CheckRegisterIndex(dec_insn.vC);
      break;
    case Instruction::kVerifyRegCField:
      result = result && CheckFieldIndex(dec_insn.vC);
      break;
    case Instruction::kVerifyRegCNewArray:
      result = result && CheckNewArray(dec_insn.vC);
      break;
    case Instruction::kVerifyRegCType:
      result = result && CheckTypeIndex(dec_insn.vC);
      break;
    case Instruction::kVerifyRegCWide:
      result = result && CheckWideRegisterIndex(dec_insn.vC);
      break;
  }
  switch (inst->GetVerifyExtraFlags()) {
    case Instruction::kVerifyArrayData:
      result = result && CheckArrayData(code_offset);
      break;
    case Instruction::kVerifyBranchTarget:
      result = result && CheckBranchTarget(code_offset);
      break;
    case Instruction::kVerifySwitchTargets:
      result = result && CheckSwitchTargets(code_offset);
      break;
    case Instruction::kVerifyVarArg:
      result = result && CheckVarArgRegs(dec_insn.vA, dec_insn.arg);
      break;
    case Instruction::kVerifyVarArgRange:
      result = result && CheckVarArgRangeRegs(dec_insn.vA, dec_insn.vC);
      break;
    case Instruction::kVerifyError:
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "unexpected opcode " << inst->Name();
      result = false;
      break;
  }
  return result;
}

bool MethodVerifier::CheckRegisterIndex(uint32_t idx) {
  if (idx >= code_item_->registers_size_) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "register index out of range (" << idx << " >= "
                                      << code_item_->registers_size_ << ")";
    return false;
  }
  return true;
}

bool MethodVerifier::CheckWideRegisterIndex(uint32_t idx) {
  if (idx + 1 >= code_item_->registers_size_) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "wide register index out of range (" << idx
                                      << "+1 >= " << code_item_->registers_size_ << ")";
    return false;
  }
  return true;
}

bool MethodVerifier::CheckFieldIndex(uint32_t idx) {
  if (idx >= dex_file_->GetHeader().field_ids_size_) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "bad field index " << idx << " (max "
                                      << dex_file_->GetHeader().field_ids_size_ << ")";
    return false;
  }
  return true;
}

bool MethodVerifier::CheckMethodIndex(uint32_t idx) {
  if (idx >= dex_file_->GetHeader().method_ids_size_) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "bad method index " << idx << " (max "
                                      << dex_file_->GetHeader().method_ids_size_ << ")";
    return false;
  }
  return true;
}

bool MethodVerifier::CheckNewInstance(uint32_t idx) {
  if (idx >= dex_file_->GetHeader().type_ids_size_) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "bad type index " << idx << " (max "
                                      << dex_file_->GetHeader().type_ids_size_ << ")";
    return false;
  }
  // We don't need the actual class, just a pointer to the class name.
  const char* descriptor = dex_file_->StringByTypeIdx(idx);
  if (descriptor[0] != 'L') {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "can't call new-instance on type '" << descriptor << "'";
    return false;
  }
  return true;
}

bool MethodVerifier::CheckStringIndex(uint32_t idx) {
  if (idx >= dex_file_->GetHeader().string_ids_size_) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "bad string index " << idx << " (max "
                                      << dex_file_->GetHeader().string_ids_size_ << ")";
    return false;
  }
  return true;
}

bool MethodVerifier::CheckTypeIndex(uint32_t idx) {
  if (idx >= dex_file_->GetHeader().type_ids_size_) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "bad type index " << idx << " (max "
                                      << dex_file_->GetHeader().type_ids_size_ << ")";
    return false;
  }
  return true;
}

bool MethodVerifier::CheckNewArray(uint32_t idx) {
  if (idx >= dex_file_->GetHeader().type_ids_size_) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "bad type index " << idx << " (max "
                                      << dex_file_->GetHeader().type_ids_size_ << ")";
    return false;
  }
  int bracket_count = 0;
  const char* descriptor = dex_file_->StringByTypeIdx(idx);
  const char* cp = descriptor;
  while (*cp++ == '[') {
    bracket_count++;
  }
  if (bracket_count == 0) {
    /* The given class must be an array type. */
    Fail(VERIFY_ERROR_BAD_CLASS_HARD)
        << "can't new-array class '" << descriptor << "' (not an array)";
    return false;
  } else if (bracket_count > 255) {
    /* It is illegal to create an array of more than 255 dimensions. */
    Fail(VERIFY_ERROR_BAD_CLASS_HARD)
        << "can't new-array class '" << descriptor << "' (exceeds limit)";
    return false;
  }
  return true;
}

bool MethodVerifier::CheckArrayData(uint32_t cur_offset) {
  const uint32_t insn_count = code_item_->insns_size_in_code_units_;
  const uint16_t* insns = code_item_->insns_ + cur_offset;
  const uint16_t* array_data;
  int32_t array_data_offset;

  DCHECK_LT(cur_offset, insn_count);
  /* make sure the start of the array data table is in range */
  array_data_offset = insns[1] | (((int32_t) insns[2]) << 16);
  if ((int32_t) cur_offset + array_data_offset < 0 ||
      cur_offset + array_data_offset + 2 >= insn_count) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid array data start: at " << cur_offset
                                      << ", data offset " << array_data_offset
                                      << ", count " << insn_count;
    return false;
  }
  /* offset to array data table is a relative branch-style offset */
  array_data = insns + array_data_offset;
  /* make sure the table is 32-bit aligned */
  if ((((uint32_t) array_data) & 0x03) != 0) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "unaligned array data table: at " << cur_offset
                                      << ", data offset " << array_data_offset;
    return false;
  }
  uint32_t value_width = array_data[1];
  uint32_t value_count = *reinterpret_cast<const uint32_t*>(&array_data[2]);
  uint32_t table_size = 4 + (value_width * value_count + 1) / 2;
  /* make sure the end of the switch is in range */
  if (cur_offset + array_data_offset + table_size > insn_count) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid array data end: at " << cur_offset
                                      << ", data offset " << array_data_offset << ", end "
                                      << cur_offset + array_data_offset + table_size
                                      << ", count " << insn_count;
    return false;
  }
  return true;
}

bool MethodVerifier::CheckBranchTarget(uint32_t cur_offset) {
  int32_t offset;
  bool isConditional, selfOkay;
  if (!GetBranchOffset(cur_offset, &offset, &isConditional, &selfOkay)) {
    return false;
  }
  if (!selfOkay && offset == 0) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "branch offset of zero not allowed at"
                                      << reinterpret_cast<void*>(cur_offset);
    return false;
  }
  // Check for 32-bit overflow. This isn't strictly necessary if we can depend on the runtime
  // to have identical "wrap-around" behavior, but it's unwise to depend on that.
  if (((int64_t) cur_offset + (int64_t) offset) != (int64_t) (cur_offset + offset)) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "branch target overflow "
                                      << reinterpret_cast<void*>(cur_offset) << " +" << offset;
    return false;
  }
  const uint32_t insn_count = code_item_->insns_size_in_code_units_;
  int32_t abs_offset = cur_offset + offset;
  if (abs_offset < 0 ||
      (uint32_t) abs_offset >= insn_count ||
      !insn_flags_[abs_offset].IsOpcode()) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid branch target " << offset << " (-> "
                                      << reinterpret_cast<void*>(abs_offset) << ") at "
                                      << reinterpret_cast<void*>(cur_offset);
    return false;
  }
  insn_flags_[abs_offset].SetBranchTarget();
  return true;
}

bool MethodVerifier::GetBranchOffset(uint32_t cur_offset, int32_t* pOffset, bool* pConditional,
                                  bool* selfOkay) {
  const uint16_t* insns = code_item_->insns_ + cur_offset;
  *pConditional = false;
  *selfOkay = false;
  switch (*insns & 0xff) {
    case Instruction::GOTO:
      *pOffset = ((int16_t) *insns) >> 8;
      break;
    case Instruction::GOTO_32:
      *pOffset = insns[1] | (((uint32_t) insns[2]) << 16);
      *selfOkay = true;
      break;
    case Instruction::GOTO_16:
      *pOffset = (int16_t) insns[1];
      break;
    case Instruction::IF_EQ:
    case Instruction::IF_NE:
    case Instruction::IF_LT:
    case Instruction::IF_GE:
    case Instruction::IF_GT:
    case Instruction::IF_LE:
    case Instruction::IF_EQZ:
    case Instruction::IF_NEZ:
    case Instruction::IF_LTZ:
    case Instruction::IF_GEZ:
    case Instruction::IF_GTZ:
    case Instruction::IF_LEZ:
      *pOffset = (int16_t) insns[1];
      *pConditional = true;
      break;
    default:
      return false;
      break;
  }
  return true;
}

bool MethodVerifier::CheckSwitchTargets(uint32_t cur_offset) {
  const uint32_t insn_count = code_item_->insns_size_in_code_units_;
  DCHECK_LT(cur_offset, insn_count);
  const uint16_t* insns = code_item_->insns_ + cur_offset;
  /* make sure the start of the switch is in range */
  int32_t switch_offset = insns[1] | ((int32_t) insns[2]) << 16;
  if ((int32_t) cur_offset + switch_offset < 0 || cur_offset + switch_offset + 2 >= insn_count) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid switch start: at " << cur_offset
                                      << ", switch offset " << switch_offset
                                      << ", count " << insn_count;
    return false;
  }
  /* offset to switch table is a relative branch-style offset */
  const uint16_t* switch_insns = insns + switch_offset;
  /* make sure the table is 32-bit aligned */
  if ((((uint32_t) switch_insns) & 0x03) != 0) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "unaligned switch table: at " << cur_offset
                                      << ", switch offset " << switch_offset;
    return false;
  }
  uint32_t switch_count = switch_insns[1];
  int32_t keys_offset, targets_offset;
  uint16_t expected_signature;
  if ((*insns & 0xff) == Instruction::PACKED_SWITCH) {
    /* 0=sig, 1=count, 2/3=firstKey */
    targets_offset = 4;
    keys_offset = -1;
    expected_signature = Instruction::kPackedSwitchSignature;
  } else {
    /* 0=sig, 1=count, 2..count*2 = keys */
    keys_offset = 2;
    targets_offset = 2 + 2 * switch_count;
    expected_signature = Instruction::kSparseSwitchSignature;
  }
  uint32_t table_size = targets_offset + switch_count * 2;
  if (switch_insns[0] != expected_signature) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD)
        << StringPrintf("wrong signature for switch table (%x, wanted %x)",
                        switch_insns[0], expected_signature);
    return false;
  }
  /* make sure the end of the switch is in range */
  if (cur_offset + switch_offset + table_size > (uint32_t) insn_count) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid switch end: at " << cur_offset
                                      << ", switch offset " << switch_offset
                                      << ", end " << (cur_offset + switch_offset + table_size)
                                      << ", count " << insn_count;
    return false;
  }
  /* for a sparse switch, verify the keys are in ascending order */
  if (keys_offset > 0 && switch_count > 1) {
    int32_t last_key = switch_insns[keys_offset] | (switch_insns[keys_offset + 1] << 16);
    for (uint32_t targ = 1; targ < switch_count; targ++) {
      int32_t key = (int32_t) switch_insns[keys_offset + targ * 2] |
                    (int32_t) (switch_insns[keys_offset + targ * 2 + 1] << 16);
      if (key <= last_key) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid packed switch: last key=" << last_key
                                          << ", this=" << key;
        return false;
      }
      last_key = key;
    }
  }
  /* verify each switch target */
  for (uint32_t targ = 0; targ < switch_count; targ++) {
    int32_t offset = (int32_t) switch_insns[targets_offset + targ * 2] |
                     (int32_t) (switch_insns[targets_offset + targ * 2 + 1] << 16);
    int32_t abs_offset = cur_offset + offset;
    if (abs_offset < 0 ||
        abs_offset >= (int32_t) insn_count ||
        !insn_flags_[abs_offset].IsOpcode()) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid switch target " << offset
                                        << " (-> " << reinterpret_cast<void*>(abs_offset) << ") at "
                                        << reinterpret_cast<void*>(cur_offset)
                                        << "[" << targ << "]";
      return false;
    }
    insn_flags_[abs_offset].SetBranchTarget();
  }
  return true;
}

bool MethodVerifier::CheckVarArgRegs(uint32_t vA, uint32_t arg[]) {
  if (vA > 5) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid arg count (" << vA << ") in non-range invoke)";
    return false;
  }
  uint16_t registers_size = code_item_->registers_size_;
  for (uint32_t idx = 0; idx < vA; idx++) {
    if (arg[idx] >= registers_size) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid reg index (" << arg[idx]
                                        << ") in non-range invoke (>= " << registers_size << ")";
      return false;
    }
  }

  return true;
}

bool MethodVerifier::CheckVarArgRangeRegs(uint32_t vA, uint32_t vC) {
  uint16_t registers_size = code_item_->registers_size_;
  // vA/vC are unsigned 8-bit/16-bit quantities for /range instructions, so there's no risk of
  // integer overflow when adding them here.
  if (vA + vC > registers_size) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid reg index " << vA << "+" << vC
                                      << " in range invoke (> " << registers_size << ")";
    return false;
  }
  return true;
}

static const std::vector<uint8_t>* CreateLengthPrefixedDexGcMap(
    const std::vector<uint8_t>& gc_map) {
  std::vector<uint8_t>* length_prefixed_gc_map = new std::vector<uint8_t>;
  length_prefixed_gc_map->reserve(gc_map.size() + 4);
  length_prefixed_gc_map->push_back((gc_map.size() & 0xff000000) >> 24);
  length_prefixed_gc_map->push_back((gc_map.size() & 0x00ff0000) >> 16);
  length_prefixed_gc_map->push_back((gc_map.size() & 0x0000ff00) >> 8);
  length_prefixed_gc_map->push_back((gc_map.size() & 0x000000ff) >> 0);
  length_prefixed_gc_map->insert(length_prefixed_gc_map->end(),
                                 gc_map.begin(),
                                 gc_map.end());
  DCHECK_EQ(gc_map.size() + 4, length_prefixed_gc_map->size());
  DCHECK_EQ(gc_map.size(),
            static_cast<size_t>((length_prefixed_gc_map->at(0) << 24) |
                                (length_prefixed_gc_map->at(1) << 16) |
                                (length_prefixed_gc_map->at(2) << 8) |
                                (length_prefixed_gc_map->at(3) << 0)));
  return length_prefixed_gc_map;
}

bool MethodVerifier::VerifyCodeFlow() {
  uint16_t registers_size = code_item_->registers_size_;
  uint32_t insns_size = code_item_->insns_size_in_code_units_;

  if (registers_size * insns_size > 4*1024*1024) {
    LOG(WARNING) << "warning: method is huge (regs=" << registers_size
                 << " insns_size=" << insns_size << ")";
  }
  /* Create and initialize table holding register status */
  reg_table_.Init(kTrackCompilerInterestPoints,
                  insn_flags_.get(),
                  insns_size,
                  registers_size,
                  this);


  work_line_.reset(new RegisterLine(registers_size, this));
  saved_line_.reset(new RegisterLine(registers_size, this));

  /* Initialize register types of method arguments. */
  if (!SetTypesFromSignature()) {
    DCHECK_NE(failures_.size(), 0U);
    std::string prepend("Bad signature in ");
    prepend += PrettyMethod(dex_method_idx_, *dex_file_);
    PrependToLastFailMessage(prepend);
    return false;
  }
  /* Perform code flow verification. */
  if (!CodeFlowVerifyMethod()) {
    DCHECK_NE(failures_.size(), 0U);
    return false;
  }

  // Compute information for compiler.
  if (Runtime::Current()->IsCompiler()) {
    MethodReference ref(dex_file_, dex_method_idx_);
    bool compile = IsCandidateForCompilation(ref, method_access_flags_);
    if (compile) {
      /* Generate a register map and add it to the method. */
      UniquePtr<const std::vector<uint8_t> > map(GenerateGcMap());
      if (map.get() == NULL) {
        DCHECK_NE(failures_.size(), 0U);
        return false;  // Not a real failure, but a failure to encode
      }
      if (kIsDebugBuild) {
        VerifyGcMap(*map);
      }
      const std::vector<uint8_t>* dex_gc_map = CreateLengthPrefixedDexGcMap(*(map.get()));
      verifier::MethodVerifier::SetDexGcMap(ref, *dex_gc_map);
    }

    if (has_check_casts_) {
      MethodVerifier::MethodSafeCastSet* method_to_safe_casts = GenerateSafeCastSet();
      if (method_to_safe_casts != NULL) {
        SetSafeCastMap(ref, method_to_safe_casts);
      }
    }

    if (has_virtual_or_interface_invokes_) {
      MethodVerifier::PcToConcreteMethodMap* pc_to_concrete_method = GenerateDevirtMap();
      if (pc_to_concrete_method != NULL) {
        SetDevirtMap(ref, pc_to_concrete_method);
      }
    }
  }
  return true;
}

std::ostream& MethodVerifier::DumpFailures(std::ostream& os) {
  DCHECK_EQ(failures_.size(), failure_messages_.size());
  if (VLOG_IS_ON(verifier)) {
      for (size_t i = 0; i < failures_.size(); ++i) {
          os << failure_messages_[i]->str() << "\n";
      }
  }
  return os;
}

extern "C" void MethodVerifierGdbDump(MethodVerifier* v)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  v->Dump(std::cerr);
}

void MethodVerifier::Dump(std::ostream& os) {
  if (code_item_ == NULL) {
    os << "Native method\n";
    return;
  }
  {
    os << "Register Types:\n";
    Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
    std::ostream indent_os(&indent_filter);
    reg_types_.Dump(indent_os);
  }
  os << "Dumping instructions and register lines:\n";
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indent_os(&indent_filter);
  const Instruction* inst = Instruction::At(code_item_->insns_);
  for (size_t dex_pc = 0; dex_pc < code_item_->insns_size_in_code_units_;
      dex_pc += insn_flags_[dex_pc].GetLengthInCodeUnits()) {
    RegisterLine* reg_line = reg_table_.GetLine(dex_pc);
    if (reg_line != NULL) {
      indent_os << reg_line->Dump() << "\n";
    }
    indent_os << StringPrintf("0x%04zx", dex_pc) << ": " << insn_flags_[dex_pc].ToString() << " ";
    const bool kDumpHexOfInstruction = false;
    if (kDumpHexOfInstruction) {
      indent_os << inst->DumpHex(5) << " ";
    }
    indent_os << inst->DumpString(dex_file_) << "\n";
    inst = inst->Next();
  }
}

static bool IsPrimitiveDescriptor(char descriptor) {
  switch (descriptor) {
    case 'I':
    case 'C':
    case 'S':
    case 'B':
    case 'Z':
    case 'F':
    case 'D':
    case 'J':
      return true;
    default:
      return false;
  }
}

bool MethodVerifier::SetTypesFromSignature() {
  RegisterLine* reg_line = reg_table_.GetLine(0);
  int arg_start = code_item_->registers_size_ - code_item_->ins_size_;
  size_t expected_args = code_item_->ins_size_;   /* long/double count as two */

  DCHECK_GE(arg_start, 0);      /* should have been verified earlier */
  // Include the "this" pointer.
  size_t cur_arg = 0;
  if (!IsStatic()) {
    // If this is a constructor for a class other than java.lang.Object, mark the first ("this")
    // argument as uninitialized. This restricts field access until the superclass constructor is
    // called.
    const RegType& declaring_class = GetDeclaringClass();
    if (IsConstructor() && !declaring_class.IsJavaLangObject()) {
      reg_line->SetRegisterType(arg_start + cur_arg,
                                reg_types_.UninitializedThisArgument(declaring_class));
    } else {
      reg_line->SetRegisterType(arg_start + cur_arg, declaring_class);
    }
    cur_arg++;
  }

  const DexFile::ProtoId& proto_id =
      dex_file_->GetMethodPrototype(dex_file_->GetMethodId(dex_method_idx_));
  DexFileParameterIterator iterator(*dex_file_, proto_id);

  for (; iterator.HasNext(); iterator.Next()) {
    const char* descriptor = iterator.GetDescriptor();
    if (descriptor == NULL) {
      LOG(FATAL) << "Null descriptor";
    }
    if (cur_arg >= expected_args) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "expected " << expected_args
                                        << " args, found more (" << descriptor << ")";
      return false;
    }
    switch (descriptor[0]) {
      case 'L':
      case '[':
        // We assume that reference arguments are initialized. The only way it could be otherwise
        // (assuming the caller was verified) is if the current method is <init>, but in that case
        // it's effectively considered initialized the instant we reach here (in the sense that we
        // can return without doing anything or call virtual methods).
        {
          const RegType& reg_type = reg_types_.FromDescriptor(class_loader_, descriptor, false);
          reg_line->SetRegisterType(arg_start + cur_arg, reg_type);
        }
        break;
      case 'Z':
        reg_line->SetRegisterType(arg_start + cur_arg, reg_types_.Boolean());
        break;
      case 'C':
        reg_line->SetRegisterType(arg_start + cur_arg, reg_types_.Char());
        break;
      case 'B':
        reg_line->SetRegisterType(arg_start + cur_arg, reg_types_.Byte());
        break;
      case 'I':
        reg_line->SetRegisterType(arg_start + cur_arg, reg_types_.Integer());
        break;
      case 'S':
        reg_line->SetRegisterType(arg_start + cur_arg, reg_types_.Short());
        break;
      case 'F':
        reg_line->SetRegisterType(arg_start + cur_arg, reg_types_.Float());
        break;
      case 'J':
      case 'D': {
        const RegType& lo_half = descriptor[0] == 'J' ? reg_types_.LongLo() : reg_types_.DoubleLo();
        const RegType& hi_half = descriptor[0] == 'J' ? reg_types_.LongHi() : reg_types_.DoubleHi();
        reg_line->SetRegisterTypeWide(arg_start + cur_arg, lo_half, hi_half);
        cur_arg++;
        break;
      }
      default:
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "unexpected signature type char '"
                                          << descriptor << "'";
        return false;
    }
    cur_arg++;
  }
  if (cur_arg != expected_args) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "expected " << expected_args
                                      << " arguments, found " << cur_arg;
    return false;
  }
  const char* descriptor = dex_file_->GetReturnTypeDescriptor(proto_id);
  // Validate return type. We don't do the type lookup; just want to make sure that it has the right
  // format. Only major difference from the method argument format is that 'V' is supported.
  bool result;
  if (IsPrimitiveDescriptor(descriptor[0]) || descriptor[0] == 'V') {
    result = descriptor[1] == '\0';
  } else if (descriptor[0] == '[') {  // single/multi-dimensional array of object/primitive
    size_t i = 0;
    do {
      i++;
    } while (descriptor[i] == '[');  // process leading [
    if (descriptor[i] == 'L') {  // object array
      do {
        i++;  // find closing ;
      } while (descriptor[i] != ';' && descriptor[i] != '\0');
      result = descriptor[i] == ';';
    } else {  // primitive array
      result = IsPrimitiveDescriptor(descriptor[i]) && descriptor[i + 1] == '\0';
    }
  } else if (descriptor[0] == 'L') {
    // could be more thorough here, but shouldn't be required
    size_t i = 0;
    do {
      i++;
    } while (descriptor[i] != ';' && descriptor[i] != '\0');
    result = descriptor[i] == ';';
  } else {
    result = false;
  }
  if (!result) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "unexpected char in return type descriptor '"
                                      << descriptor << "'";
  }
  return result;
}

bool MethodVerifier::CodeFlowVerifyMethod() {
  const uint16_t* insns = code_item_->insns_;
  const uint32_t insns_size = code_item_->insns_size_in_code_units_;

  /* Begin by marking the first instruction as "changed". */
  insn_flags_[0].SetChanged();
  uint32_t start_guess = 0;

  /* Continue until no instructions are marked "changed". */
  while (true) {
    // Find the first marked one. Use "start_guess" as a way to find one quickly.
    uint32_t insn_idx = start_guess;
    for (; insn_idx < insns_size; insn_idx++) {
      if (insn_flags_[insn_idx].IsChanged())
        break;
    }
    if (insn_idx == insns_size) {
      if (start_guess != 0) {
        /* try again, starting from the top */
        start_guess = 0;
        continue;
      } else {
        /* all flags are clear */
        break;
      }
    }
    // We carry the working set of registers from instruction to instruction. If this address can
    // be the target of a branch (or throw) instruction, or if we're skipping around chasing
    // "changed" flags, we need to load the set of registers from the table.
    // Because we always prefer to continue on to the next instruction, we should never have a
    // situation where we have a stray "changed" flag set on an instruction that isn't a branch
    // target.
    work_insn_idx_ = insn_idx;
    if (insn_flags_[insn_idx].IsBranchTarget()) {
      work_line_->CopyFromLine(reg_table_.GetLine(insn_idx));
    } else {
#ifndef NDEBUG
      /*
       * Sanity check: retrieve the stored register line (assuming
       * a full table) and make sure it actually matches.
       */
      RegisterLine* register_line = reg_table_.GetLine(insn_idx);
      if (register_line != NULL) {
        if (work_line_->CompareLine(register_line) != 0) {
          Dump(std::cout);
          std::cout << info_messages_.str();
          LOG(FATAL) << "work_line diverged in " << PrettyMethod(dex_method_idx_, *dex_file_)
                     << "@" << reinterpret_cast<void*>(work_insn_idx_) << "\n"
                     << " work_line=" << *work_line_ << "\n"
                     << "  expected=" << *register_line;
        }
      }
#endif
    }
    if (!CodeFlowVerifyInstruction(&start_guess)) {
      std::string prepend(PrettyMethod(dex_method_idx_, *dex_file_));
      prepend += " failed to verify: ";
      PrependToLastFailMessage(prepend);
      return false;
    }
    /* Clear "changed" and mark as visited. */
    insn_flags_[insn_idx].SetVisited();
    insn_flags_[insn_idx].ClearChanged();
  }

  if (gDebugVerify) {
    /*
     * Scan for dead code. There's nothing "evil" about dead code
     * (besides the wasted space), but it indicates a flaw somewhere
     * down the line, possibly in the verifier.
     *
     * If we've substituted "always throw" instructions into the stream,
     * we are almost certainly going to have some dead code.
     */
    int dead_start = -1;
    uint32_t insn_idx = 0;
    for (; insn_idx < insns_size; insn_idx += insn_flags_[insn_idx].GetLengthInCodeUnits()) {
      /*
       * Switch-statement data doesn't get "visited" by scanner. It
       * may or may not be preceded by a padding NOP (for alignment).
       */
      if (insns[insn_idx] == Instruction::kPackedSwitchSignature ||
          insns[insn_idx] == Instruction::kSparseSwitchSignature ||
          insns[insn_idx] == Instruction::kArrayDataSignature ||
          (insns[insn_idx] == Instruction::NOP && (insn_idx + 1 < insns_size) &&
           (insns[insn_idx + 1] == Instruction::kPackedSwitchSignature ||
            insns[insn_idx + 1] == Instruction::kSparseSwitchSignature ||
            insns[insn_idx + 1] == Instruction::kArrayDataSignature))) {
        insn_flags_[insn_idx].SetVisited();
      }

      if (!insn_flags_[insn_idx].IsVisited()) {
        if (dead_start < 0)
          dead_start = insn_idx;
      } else if (dead_start >= 0) {
        LogVerifyInfo() << "dead code " << reinterpret_cast<void*>(dead_start)
                        << "-" << reinterpret_cast<void*>(insn_idx - 1);
        dead_start = -1;
      }
    }
    if (dead_start >= 0) {
      LogVerifyInfo() << "dead code " << reinterpret_cast<void*>(dead_start)
                      << "-" << reinterpret_cast<void*>(insn_idx - 1);
    }
    // To dump the state of the verify after a method, do something like:
    // if (PrettyMethod(dex_method_idx_, *dex_file_) ==
    //     "boolean java.lang.String.equals(java.lang.Object)") {
    //   LOG(INFO) << info_messages_.str();
    // }
  }
  return true;
}

bool MethodVerifier::CodeFlowVerifyInstruction(uint32_t* start_guess) {
  // If we're doing FindLocksAtDexPc, check whether we're at the dex pc we care about.
  // We want the state _before_ the instruction, for the case where the dex pc we're
  // interested in is itself a monitor-enter instruction (which is a likely place
  // for a thread to be suspended).
  if (monitor_enter_dex_pcs_ != NULL && work_insn_idx_ == interesting_dex_pc_) {
    monitor_enter_dex_pcs_->clear();  // The new work line is more accurate than the previous one.
    for (size_t i = 0; i < work_line_->GetMonitorEnterCount(); ++i) {
      monitor_enter_dex_pcs_->push_back(work_line_->GetMonitorEnterDexPc(i));
    }
  }

  /*
   * Once we finish decoding the instruction, we need to figure out where
   * we can go from here. There are three possible ways to transfer
   * control to another statement:
   *
   * (1) Continue to the next instruction. Applies to all but
   *     unconditional branches, method returns, and exception throws.
   * (2) Branch to one or more possible locations. Applies to branches
   *     and switch statements.
   * (3) Exception handlers. Applies to any instruction that can
   *     throw an exception that is handled by an encompassing "try"
   *     block.
   *
   * We can also return, in which case there is no successor instruction
   * from this point.
   *
   * The behavior can be determined from the opcode flags.
   */
  const uint16_t* insns = code_item_->insns_ + work_insn_idx_;
  const Instruction* inst = Instruction::At(insns);
  int opcode_flags = Instruction::FlagsOf(inst->Opcode());

  int32_t branch_target = 0;
  bool just_set_result = false;
  if (gDebugVerify) {
    // Generate processing back trace to debug verifier
    LogVerifyInfo() << "Processing " << inst->DumpString(dex_file_) << "\n"
                    << *work_line_.get() << "\n";
  }

  /*
   * Make a copy of the previous register state. If the instruction
   * can throw an exception, we will copy/merge this into the "catch"
   * address rather than work_line, because we don't want the result
   * from the "successful" code path (e.g. a check-cast that "improves"
   * a type) to be visible to the exception handler.
   */
  if ((opcode_flags & Instruction::kThrow) != 0 && CurrentInsnFlags()->IsInTry()) {
    saved_line_->CopyFromLine(work_line_.get());
  } else {
#ifndef NDEBUG
    saved_line_->FillWithGarbage();
#endif
  }


  // We need to ensure the work line is consistent while performing validation. When we spot a
  // peephole pattern we compute a new line for either the fallthrough instruction or the
  // branch target.
  UniquePtr<RegisterLine> branch_line;
  UniquePtr<RegisterLine> fallthrough_line;

  switch (inst->Opcode()) {
    case Instruction::NOP:
      /*
       * A "pure" NOP has no effect on anything. Data tables start with
       * a signature that looks like a NOP; if we see one of these in
       * the course of executing code then we have a problem.
       */
      if (inst->VRegA_10x() != 0) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "encountered data table in instruction stream";
      }
      break;

    case Instruction::MOVE:
      work_line_->CopyRegister1(inst->VRegA_12x(), inst->VRegB_12x(), kTypeCategory1nr);
      break;
    case Instruction::MOVE_FROM16:
      work_line_->CopyRegister1(inst->VRegA_22x(), inst->VRegB_22x(), kTypeCategory1nr);
      break;
    case Instruction::MOVE_16:
      work_line_->CopyRegister1(inst->VRegA_32x(), inst->VRegB_32x(), kTypeCategory1nr);
      break;
    case Instruction::MOVE_WIDE:
      work_line_->CopyRegister2(inst->VRegA_12x(), inst->VRegB_12x());
      break;
    case Instruction::MOVE_WIDE_FROM16:
      work_line_->CopyRegister2(inst->VRegA_22x(), inst->VRegB_22x());
      break;
    case Instruction::MOVE_WIDE_16:
      work_line_->CopyRegister2(inst->VRegA_32x(), inst->VRegB_32x());
      break;
    case Instruction::MOVE_OBJECT:
      work_line_->CopyRegister1(inst->VRegA_12x(), inst->VRegB_12x(), kTypeCategoryRef);
      break;
    case Instruction::MOVE_OBJECT_FROM16:
      work_line_->CopyRegister1(inst->VRegA_22x(), inst->VRegB_22x(), kTypeCategoryRef);
      break;
    case Instruction::MOVE_OBJECT_16:
      work_line_->CopyRegister1(inst->VRegA_32x(), inst->VRegB_32x(), kTypeCategoryRef);
      break;

    /*
     * The move-result instructions copy data out of a "pseudo-register"
     * with the results from the last method invocation. In practice we
     * might want to hold the result in an actual CPU register, so the
     * Dalvik spec requires that these only appear immediately after an
     * invoke or filled-new-array.
     *
     * These calls invalidate the "result" register. (This is now
     * redundant with the reset done below, but it can make the debug info
     * easier to read in some cases.)
     */
    case Instruction::MOVE_RESULT:
      work_line_->CopyResultRegister1(inst->VRegA_11x(), false);
      break;
    case Instruction::MOVE_RESULT_WIDE:
      work_line_->CopyResultRegister2(inst->VRegA_11x());
      break;
    case Instruction::MOVE_RESULT_OBJECT:
      work_line_->CopyResultRegister1(inst->VRegA_11x(), true);
      break;

    case Instruction::MOVE_EXCEPTION: {
      /*
       * This statement can only appear as the first instruction in an exception handler. We verify
       * that as part of extracting the exception type from the catch block list.
       */
      const RegType& res_type = GetCaughtExceptionType();
      work_line_->SetRegisterType(inst->VRegA_11x(), res_type);
      break;
    }
    case Instruction::RETURN_VOID:
      if (!IsConstructor() || work_line_->CheckConstructorReturn()) {
        if (!GetMethodReturnType().IsConflict()) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "return-void not expected";
        }
      }
      break;
    case Instruction::RETURN:
      if (!IsConstructor() || work_line_->CheckConstructorReturn()) {
        /* check the method signature */
        const RegType& return_type = GetMethodReturnType();
        if (!return_type.IsCategory1Types()) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "unexpected non-category 1 return type "
                                            << return_type;
        } else {
          // Compilers may generate synthetic functions that write byte values into boolean fields.
          // Also, it may use integer values for boolean, byte, short, and character return types.
          const uint32_t vregA = inst->VRegA_11x();
          const RegType& src_type = work_line_->GetRegisterType(vregA);
          bool use_src = ((return_type.IsBoolean() && src_type.IsByte()) ||
                          ((return_type.IsBoolean() || return_type.IsByte() ||
                           return_type.IsShort() || return_type.IsChar()) &&
                           src_type.IsInteger()));
          /* check the register contents */
          bool success =
              work_line_->VerifyRegisterType(vregA, use_src ? src_type : return_type);
          if (!success) {
            AppendToLastFailMessage(StringPrintf(" return-1nr on invalid register v%d", vregA));
          }
        }
      }
      break;
    case Instruction::RETURN_WIDE:
      if (!IsConstructor() || work_line_->CheckConstructorReturn()) {
        /* check the method signature */
        const RegType& return_type = GetMethodReturnType();
        if (!return_type.IsCategory2Types()) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "return-wide not expected";
        } else {
          /* check the register contents */
          const uint32_t vregA = inst->VRegA_11x();
          bool success = work_line_->VerifyRegisterType(vregA, return_type);
          if (!success) {
            AppendToLastFailMessage(StringPrintf(" return-wide on invalid register v%d", vregA));
          }
        }
      }
      break;
    case Instruction::RETURN_OBJECT:
      if (!IsConstructor() || work_line_->CheckConstructorReturn()) {
        const RegType& return_type = GetMethodReturnType();
        if (!return_type.IsReferenceTypes()) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "return-object not expected";
        } else {
          /* return_type is the *expected* return type, not register value */
          DCHECK(!return_type.IsZero());
          DCHECK(!return_type.IsUninitializedReference());
          const uint32_t vregA = inst->VRegA_11x();
          const RegType& reg_type = work_line_->GetRegisterType(vregA);
          // Disallow returning uninitialized values and verify that the reference in vAA is an
          // instance of the "return_type"
          if (reg_type.IsUninitializedTypes()) {
            Fail(VERIFY_ERROR_BAD_CLASS_SOFT) << "returning uninitialized object '"
                                              << reg_type << "'";
          } else if (!return_type.IsAssignableFrom(reg_type)) {
            if (reg_type.IsUnresolvedTypes() || return_type.IsUnresolvedTypes()) {
              Fail(VERIFY_ERROR_NO_CLASS) << " can't resolve returned type '" << return_type
                  << "' or '" << reg_type << "'";
            } else {
              Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "returning '" << reg_type
                  << "', but expected from declaration '" << return_type << "'";
            }
          }
        }
      }
      break;

      /* could be boolean, int, float, or a null reference */
    case Instruction::CONST_4: {
      int32_t val = static_cast<int32_t>(inst->VRegB_11n() << 28) >> 28;
      work_line_->SetRegisterType(inst->VRegA_11n(), reg_types_.FromCat1Const(val, true));
      break;
    }
    case Instruction::CONST_16: {
      int16_t val = static_cast<int16_t>(inst->VRegB_21s());
      work_line_->SetRegisterType(inst->VRegA_21s(), reg_types_.FromCat1Const(val, true));
      break;
    }
    case Instruction::CONST:
      work_line_->SetRegisterType(inst->VRegA_31i(),
                                  reg_types_.FromCat1Const(inst->VRegB_31i(), true));
      break;
    case Instruction::CONST_HIGH16:
      work_line_->SetRegisterType(inst->VRegA_21h(),
                                  reg_types_.FromCat1Const(inst->VRegB_21h() << 16, true));
      break;
      /* could be long or double; resolved upon use */
    case Instruction::CONST_WIDE_16: {
      int64_t val = static_cast<int16_t>(inst->VRegB_21s());
      const RegType& lo = reg_types_.FromCat2ConstLo(static_cast<int32_t>(val), true);
      const RegType& hi = reg_types_.FromCat2ConstHi(static_cast<int32_t>(val >> 32), true);
      work_line_->SetRegisterTypeWide(inst->VRegA_21s(), lo, hi);
      break;
    }
    case Instruction::CONST_WIDE_32: {
      int64_t val = static_cast<int32_t>(inst->VRegB_31i());
      const RegType& lo = reg_types_.FromCat2ConstLo(static_cast<int32_t>(val), true);
      const RegType& hi = reg_types_.FromCat2ConstHi(static_cast<int32_t>(val >> 32), true);
      work_line_->SetRegisterTypeWide(inst->VRegA_31i(), lo, hi);
      break;
    }
    case Instruction::CONST_WIDE: {
      int64_t val = inst->VRegB_51l();
      const RegType& lo = reg_types_.FromCat2ConstLo(static_cast<int32_t>(val), true);
      const RegType& hi = reg_types_.FromCat2ConstHi(static_cast<int32_t>(val >> 32), true);
      work_line_->SetRegisterTypeWide(inst->VRegA_51l(), lo, hi);
      break;
    }
    case Instruction::CONST_WIDE_HIGH16: {
      int64_t val = static_cast<uint64_t>(inst->VRegB_21h()) << 48;
      const RegType& lo = reg_types_.FromCat2ConstLo(static_cast<int32_t>(val), true);
      const RegType& hi = reg_types_.FromCat2ConstHi(static_cast<int32_t>(val >> 32), true);
      work_line_->SetRegisterTypeWide(inst->VRegA_21h(), lo, hi);
      break;
    }
    case Instruction::CONST_STRING:
      work_line_->SetRegisterType(inst->VRegA_21c(), reg_types_.JavaLangString());
      break;
    case Instruction::CONST_STRING_JUMBO:
      work_line_->SetRegisterType(inst->VRegA_31c(), reg_types_.JavaLangString());
      break;
    case Instruction::CONST_CLASS: {
      // Get type from instruction if unresolved then we need an access check
      // TODO: check Compiler::CanAccessTypeWithoutChecks returns false when res_type is unresolved
      const RegType& res_type = ResolveClassAndCheckAccess(inst->VRegB_21c());
      // Register holds class, ie its type is class, on error it will hold Conflict.
      work_line_->SetRegisterType(inst->VRegA_21c(),
                                  res_type.IsConflict() ? res_type
                                                        : reg_types_.JavaLangClass(true));
      break;
    }
    case Instruction::MONITOR_ENTER:
      work_line_->PushMonitor(inst->VRegA_11x(), work_insn_idx_);
      break;
    case Instruction::MONITOR_EXIT:
      /*
       * monitor-exit instructions are odd. They can throw exceptions,
       * but when they do they act as if they succeeded and the PC is
       * pointing to the following instruction. (This behavior goes back
       * to the need to handle asynchronous exceptions, a now-deprecated
       * feature that Dalvik doesn't support.)
       *
       * In practice we don't need to worry about this. The only
       * exceptions that can be thrown from monitor-exit are for a
       * null reference and -exit without a matching -enter. If the
       * structured locking checks are working, the former would have
       * failed on the -enter instruction, and the latter is impossible.
       *
       * This is fortunate, because issue 3221411 prevents us from
       * chasing the "can throw" path when monitor verification is
       * enabled. If we can fully verify the locking we can ignore
       * some catch blocks (which will show up as "dead" code when
       * we skip them here); if we can't, then the code path could be
       * "live" so we still need to check it.
       */
      opcode_flags &= ~Instruction::kThrow;
      work_line_->PopMonitor(inst->VRegA_11x());
      break;

    case Instruction::CHECK_CAST:
    case Instruction::INSTANCE_OF: {
      /*
       * If this instruction succeeds, we will "downcast" register vA to the type in vB. (This
       * could be a "upcast" -- not expected, so we don't try to address it.)
       *
       * If it fails, an exception is thrown, which we deal with later by ignoring the update to
       * dec_insn.vA when branching to a handler.
       */
      const bool is_checkcast = (inst->Opcode() == Instruction::CHECK_CAST);
      const uint32_t type_idx = (is_checkcast) ? inst->VRegB_21c() : inst->VRegC_22c();
      const RegType& res_type = ResolveClassAndCheckAccess(type_idx);
      if (res_type.IsConflict()) {
        DCHECK_NE(failures_.size(), 0U);
        if (!is_checkcast) {
          work_line_->SetRegisterType(inst->VRegA_22c(), reg_types_.Boolean());
        }
        break;  // bad class
      }
      // TODO: check Compiler::CanAccessTypeWithoutChecks returns false when res_type is unresolved
      uint32_t orig_type_reg = (is_checkcast) ? inst->VRegA_21c() : inst->VRegB_22c();
      const RegType& orig_type = work_line_->GetRegisterType(orig_type_reg);
      if (!res_type.IsNonZeroReferenceTypes()) {
        if (is_checkcast) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "check-cast on unexpected class " << res_type;
        } else {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "instance-of on unexpected class " << res_type;
        }
      } else if (!orig_type.IsReferenceTypes()) {
        if (is_checkcast) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "check-cast on non-reference in v" << orig_type_reg;
        } else {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "instance-of on non-reference in v" << orig_type_reg;
        }
      } else {
        if (is_checkcast) {
          work_line_->SetRegisterType(inst->VRegA_21c(), res_type);
        } else {
          work_line_->SetRegisterType(inst->VRegA_22c(), reg_types_.Boolean());
        }
      }
      break;
    }
    case Instruction::ARRAY_LENGTH: {
      const RegType& res_type = work_line_->GetRegisterType(inst->VRegB_12x());
      if (res_type.IsReferenceTypes()) {
        if (!res_type.IsArrayTypes() && !res_type.IsZero()) {  // ie not an array or null
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "array-length on non-array " << res_type;
        } else {
          work_line_->SetRegisterType(inst->VRegA_12x(), reg_types_.Integer());
        }
      }
      break;
    }
    case Instruction::NEW_INSTANCE: {
      const RegType& res_type = ResolveClassAndCheckAccess(inst->VRegB_21c());
      if (res_type.IsConflict()) {
        DCHECK_NE(failures_.size(), 0U);
        break;  // bad class
      }
      // TODO: check Compiler::CanAccessTypeWithoutChecks returns false when res_type is unresolved
      // can't create an instance of an interface or abstract class */
      if (!res_type.IsInstantiableTypes()) {
        Fail(VERIFY_ERROR_INSTANTIATION)
            << "new-instance on primitive, interface or abstract class" << res_type;
        // Soft failure so carry on to set register type.
      }
      const RegType& uninit_type = reg_types_.Uninitialized(res_type, work_insn_idx_);
      // Any registers holding previous allocations from this address that have not yet been
      // initialized must be marked invalid.
      work_line_->MarkUninitRefsAsInvalid(uninit_type);
      // add the new uninitialized reference to the register state
      work_line_->SetRegisterType(inst->VRegA_21c(), uninit_type);
      break;
    }
    case Instruction::NEW_ARRAY:
      VerifyNewArray(inst, false, false);
      break;
    case Instruction::FILLED_NEW_ARRAY:
      VerifyNewArray(inst, true, false);
      just_set_result = true;  // Filled new array sets result register
      break;
    case Instruction::FILLED_NEW_ARRAY_RANGE:
      VerifyNewArray(inst, true, true);
      just_set_result = true;  // Filled new array range sets result register
      break;
    case Instruction::CMPL_FLOAT:
    case Instruction::CMPG_FLOAT:
      if (!work_line_->VerifyRegisterType(inst->VRegB_23x(), reg_types_.Float())) {
        break;
      }
      if (!work_line_->VerifyRegisterType(inst->VRegC_23x(), reg_types_.Float())) {
        break;
      }
      work_line_->SetRegisterType(inst->VRegA_23x(), reg_types_.Integer());
      break;
    case Instruction::CMPL_DOUBLE:
    case Instruction::CMPG_DOUBLE:
      if (!work_line_->VerifyRegisterTypeWide(inst->VRegB_23x(), reg_types_.DoubleLo(),
                                              reg_types_.DoubleHi())) {
        break;
      }
      if (!work_line_->VerifyRegisterTypeWide(inst->VRegC_23x(), reg_types_.DoubleLo(),
                                              reg_types_.DoubleHi())) {
        break;
      }
      work_line_->SetRegisterType(inst->VRegA_23x(), reg_types_.Integer());
      break;
    case Instruction::CMP_LONG:
      if (!work_line_->VerifyRegisterTypeWide(inst->VRegB_23x(), reg_types_.LongLo(),
                                              reg_types_.LongHi())) {
        break;
      }
      if (!work_line_->VerifyRegisterTypeWide(inst->VRegC_23x(), reg_types_.LongLo(),
                                              reg_types_.LongHi())) {
        break;
      }
      work_line_->SetRegisterType(inst->VRegA_23x(), reg_types_.Integer());
      break;
    case Instruction::THROW: {
      const RegType& res_type = work_line_->GetRegisterType(inst->VRegA_11x());
      if (!reg_types_.JavaLangThrowable(false).IsAssignableFrom(res_type)) {
        Fail(res_type.IsUnresolvedTypes() ? VERIFY_ERROR_NO_CLASS : VERIFY_ERROR_BAD_CLASS_SOFT)
            << "thrown class " << res_type << " not instanceof Throwable";
      }
      break;
    }
    case Instruction::GOTO:
    case Instruction::GOTO_16:
    case Instruction::GOTO_32:
      /* no effect on or use of registers */
      break;

    case Instruction::PACKED_SWITCH:
    case Instruction::SPARSE_SWITCH:
      /* verify that vAA is an integer, or can be converted to one */
      work_line_->VerifyRegisterType(inst->VRegA_31t(), reg_types_.Integer());
      break;

    case Instruction::FILL_ARRAY_DATA: {
      /* Similar to the verification done for APUT */
      const RegType& array_type = work_line_->GetRegisterType(inst->VRegA_31t());
      /* array_type can be null if the reg type is Zero */
      if (!array_type.IsZero()) {
        if (!array_type.IsArrayTypes()) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid fill-array-data with array type "
                                            << array_type;
        } else {
          const RegType& component_type = reg_types_.GetComponentType(array_type, class_loader_);
          DCHECK(!component_type.IsConflict());
          if (component_type.IsNonZeroReferenceTypes()) {
            Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid fill-array-data with component type "
                                              << component_type;
          } else {
            // Now verify if the element width in the table matches the element width declared in
            // the array
            const uint16_t* array_data = insns + (insns[1] | (((int32_t) insns[2]) << 16));
            if (array_data[0] != Instruction::kArrayDataSignature) {
              Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid magic for array-data";
            } else {
              size_t elem_width = Primitive::ComponentSize(component_type.GetPrimitiveType());
              // Since we don't compress the data in Dex, expect to see equal width of data stored
              // in the table and expected from the array class.
              if (array_data[1] != elem_width) {
                Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "array-data size mismatch (" << array_data[1]
                                                  << " vs " << elem_width << ")";
              }
            }
          }
        }
      }
      break;
    }
    case Instruction::IF_EQ:
    case Instruction::IF_NE: {
      const RegType& reg_type1 = work_line_->GetRegisterType(inst->VRegA_22t());
      const RegType& reg_type2 = work_line_->GetRegisterType(inst->VRegB_22t());
      bool mismatch = false;
      if (reg_type1.IsZero()) {  // zero then integral or reference expected
        mismatch = !reg_type2.IsReferenceTypes() && !reg_type2.IsIntegralTypes();
      } else if (reg_type1.IsReferenceTypes()) {  // both references?
        mismatch = !reg_type2.IsReferenceTypes();
      } else {  // both integral?
        mismatch = !reg_type1.IsIntegralTypes() || !reg_type2.IsIntegralTypes();
      }
      if (mismatch) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "args to if-eq/if-ne (" << reg_type1 << ","
                                          << reg_type2 << ") must both be references or integral";
      }
      break;
    }
    case Instruction::IF_LT:
    case Instruction::IF_GE:
    case Instruction::IF_GT:
    case Instruction::IF_LE: {
      const RegType& reg_type1 = work_line_->GetRegisterType(inst->VRegA_22t());
      const RegType& reg_type2 = work_line_->GetRegisterType(inst->VRegB_22t());
      if (!reg_type1.IsIntegralTypes() || !reg_type2.IsIntegralTypes()) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "args to 'if' (" << reg_type1 << ","
                                          << reg_type2 << ") must be integral";
      }
      break;
    }
    case Instruction::IF_EQZ:
    case Instruction::IF_NEZ: {
      const RegType& reg_type = work_line_->GetRegisterType(inst->VRegA_21t());
      if (!reg_type.IsReferenceTypes() && !reg_type.IsIntegralTypes()) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "type " << reg_type
                                          << " unexpected as arg to if-eqz/if-nez";
      }

      // Find previous instruction - its existence is a precondition to peephole optimization.
      uint32_t instance_of_idx = 0;
      if (0 != work_insn_idx_) {
        instance_of_idx = work_insn_idx_ - 1;
        while (0 != instance_of_idx && !insn_flags_[instance_of_idx].IsOpcode()) {
          instance_of_idx--;
        }
        CHECK(insn_flags_[instance_of_idx].IsOpcode());
      } else {
        break;
      }

      const Instruction* instance_of_inst = Instruction::At(code_item_->insns_ + instance_of_idx);

      /* Check for peep-hole pattern of:
       *    ...;
       *    instance-of vX, vY, T;
       *    ifXXX vX, label ;
       *    ...;
       * label:
       *    ...;
       * and sharpen the type of vY to be type T.
       * Note, this pattern can't be if:
       *  - if there are other branches to this branch,
       *  - when vX == vY.
       */
      if (!CurrentInsnFlags()->IsBranchTarget() &&
          (Instruction::INSTANCE_OF == instance_of_inst->Opcode()) &&
          (inst->VRegA_21t() == instance_of_inst->VRegA_22c()) &&
          (instance_of_inst->VRegA_22c() != instance_of_inst->VRegB_22c())) {
        // Check that the we are not attempting conversion to interface types,
        // which is not done because of the multiple inheritance implications.
        // Also don't change the type if it would result in an upcast.
        const RegType& orig_type = work_line_->GetRegisterType(instance_of_inst->VRegB_22c());
        const RegType& cast_type = ResolveClassAndCheckAccess(instance_of_inst->VRegC_22c());

        if (!cast_type.IsUnresolvedTypes() && !orig_type.IsUnresolvedTypes() &&
            !cast_type.GetClass()->IsInterface() && !cast_type.IsAssignableFrom(orig_type)) {
          RegisterLine* update_line = new RegisterLine(code_item_->registers_size_, this);
          if (inst->Opcode() == Instruction::IF_EQZ) {
            fallthrough_line.reset(update_line);
          } else {
            branch_line.reset(update_line);
          }
          update_line->CopyFromLine(work_line_.get());
          update_line->SetRegisterType(instance_of_inst->VRegB_22c(), cast_type);
          if (!insn_flags_[instance_of_idx].IsBranchTarget() && 0 != instance_of_idx) {
            // See if instance-of was preceded by a move-object operation, common due to the small
            // register encoding space of instance-of, and propagate type information to the source
            // of the move-object.
            uint32_t move_idx = instance_of_idx - 1;
            while (0 != move_idx && !insn_flags_[move_idx].IsOpcode()) {
              move_idx--;
            }
            CHECK(insn_flags_[move_idx].IsOpcode());
            const Instruction* move_inst = Instruction::At(code_item_->insns_ + move_idx);
            switch (move_inst->Opcode()) {
              case Instruction::MOVE_OBJECT:
                if (move_inst->VRegA_12x() == instance_of_inst->VRegB_22c()) {
                  update_line->SetRegisterType(move_inst->VRegB_12x(), cast_type);
                }
                break;
              case Instruction::MOVE_OBJECT_FROM16:
                if (move_inst->VRegA_22x() == instance_of_inst->VRegB_22c()) {
                  update_line->SetRegisterType(move_inst->VRegB_22x(), cast_type);
                }
                break;
              case Instruction::MOVE_OBJECT_16:
                if (move_inst->VRegA_32x() == instance_of_inst->VRegB_22c()) {
                  update_line->SetRegisterType(move_inst->VRegB_32x(), cast_type);
                }
                break;
              default:
                break;
            }
          }
        }
      }

      break;
    }
    case Instruction::IF_LTZ:
    case Instruction::IF_GEZ:
    case Instruction::IF_GTZ:
    case Instruction::IF_LEZ: {
      const RegType& reg_type = work_line_->GetRegisterType(inst->VRegA_21t());
      if (!reg_type.IsIntegralTypes()) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "type " << reg_type
                                          << " unexpected as arg to if-ltz/if-gez/if-gtz/if-lez";
      }
      break;
    }
    case Instruction::AGET_BOOLEAN:
      VerifyAGet(inst, reg_types_.Boolean(), true);
      break;
    case Instruction::AGET_BYTE:
      VerifyAGet(inst, reg_types_.Byte(), true);
      break;
    case Instruction::AGET_CHAR:
      VerifyAGet(inst, reg_types_.Char(), true);
      break;
    case Instruction::AGET_SHORT:
      VerifyAGet(inst, reg_types_.Short(), true);
      break;
    case Instruction::AGET:
      VerifyAGet(inst, reg_types_.Integer(), true);
      break;
    case Instruction::AGET_WIDE:
      VerifyAGet(inst, reg_types_.LongLo(), true);
      break;
    case Instruction::AGET_OBJECT:
      VerifyAGet(inst, reg_types_.JavaLangObject(false), false);
      break;

    case Instruction::APUT_BOOLEAN:
      VerifyAPut(inst, reg_types_.Boolean(), true);
      break;
    case Instruction::APUT_BYTE:
      VerifyAPut(inst, reg_types_.Byte(), true);
      break;
    case Instruction::APUT_CHAR:
      VerifyAPut(inst, reg_types_.Char(), true);
      break;
    case Instruction::APUT_SHORT:
      VerifyAPut(inst, reg_types_.Short(), true);
      break;
    case Instruction::APUT:
      VerifyAPut(inst, reg_types_.Integer(), true);
      break;
    case Instruction::APUT_WIDE:
      VerifyAPut(inst, reg_types_.LongLo(), true);
      break;
    case Instruction::APUT_OBJECT:
      VerifyAPut(inst, reg_types_.JavaLangObject(false), false);
      break;

    case Instruction::IGET_BOOLEAN:
      VerifyISGet(inst, reg_types_.Boolean(), true, false);
      break;
    case Instruction::IGET_BYTE:
      VerifyISGet(inst, reg_types_.Byte(), true, false);
      break;
    case Instruction::IGET_CHAR:
      VerifyISGet(inst, reg_types_.Char(), true, false);
      break;
    case Instruction::IGET_SHORT:
      VerifyISGet(inst, reg_types_.Short(), true, false);
      break;
    case Instruction::IGET:
      VerifyISGet(inst, reg_types_.Integer(), true, false);
      break;
    case Instruction::IGET_WIDE:
      VerifyISGet(inst, reg_types_.LongLo(), true, false);
      break;
    case Instruction::IGET_OBJECT:
      VerifyISGet(inst, reg_types_.JavaLangObject(false), false, false);
      break;

    case Instruction::IPUT_BOOLEAN:
      VerifyISPut(inst, reg_types_.Boolean(), true, false);
      break;
    case Instruction::IPUT_BYTE:
      VerifyISPut(inst, reg_types_.Byte(), true, false);
      break;
    case Instruction::IPUT_CHAR:
      VerifyISPut(inst, reg_types_.Char(), true, false);
      break;
    case Instruction::IPUT_SHORT:
      VerifyISPut(inst, reg_types_.Short(), true, false);
      break;
    case Instruction::IPUT:
      VerifyISPut(inst, reg_types_.Integer(), true, false);
      break;
    case Instruction::IPUT_WIDE:
      VerifyISPut(inst, reg_types_.LongLo(), true, false);
      break;
    case Instruction::IPUT_OBJECT:
      VerifyISPut(inst, reg_types_.JavaLangObject(false), false, false);
      break;

    case Instruction::SGET_BOOLEAN:
      VerifyISGet(inst, reg_types_.Boolean(), true, true);
      break;
    case Instruction::SGET_BYTE:
      VerifyISGet(inst, reg_types_.Byte(), true, true);
      break;
    case Instruction::SGET_CHAR:
      VerifyISGet(inst, reg_types_.Char(), true, true);
      break;
    case Instruction::SGET_SHORT:
      VerifyISGet(inst, reg_types_.Short(), true, true);
      break;
    case Instruction::SGET:
      VerifyISGet(inst, reg_types_.Integer(), true, true);
      break;
    case Instruction::SGET_WIDE:
      VerifyISGet(inst, reg_types_.LongLo(), true, true);
      break;
    case Instruction::SGET_OBJECT:
      VerifyISGet(inst, reg_types_.JavaLangObject(false), false, true);
      break;

    case Instruction::SPUT_BOOLEAN:
      VerifyISPut(inst, reg_types_.Boolean(), true, true);
      break;
    case Instruction::SPUT_BYTE:
      VerifyISPut(inst, reg_types_.Byte(), true, true);
      break;
    case Instruction::SPUT_CHAR:
      VerifyISPut(inst, reg_types_.Char(), true, true);
      break;
    case Instruction::SPUT_SHORT:
      VerifyISPut(inst, reg_types_.Short(), true, true);
      break;
    case Instruction::SPUT:
      VerifyISPut(inst, reg_types_.Integer(), true, true);
      break;
    case Instruction::SPUT_WIDE:
      VerifyISPut(inst, reg_types_.LongLo(), true, true);
      break;
    case Instruction::SPUT_OBJECT:
      VerifyISPut(inst, reg_types_.JavaLangObject(false), false, true);
      break;

    case Instruction::INVOKE_VIRTUAL:
    case Instruction::INVOKE_VIRTUAL_RANGE:
    case Instruction::INVOKE_SUPER:
    case Instruction::INVOKE_SUPER_RANGE: {
      bool is_range = (inst->Opcode() == Instruction::INVOKE_VIRTUAL_RANGE ||
                       inst->Opcode() == Instruction::INVOKE_SUPER_RANGE);
      bool is_super =  (inst->Opcode() == Instruction::INVOKE_SUPER ||
                        inst->Opcode() == Instruction::INVOKE_SUPER_RANGE);
      mirror::ArtMethod* called_method = VerifyInvocationArgs(inst, METHOD_VIRTUAL,
                                                                   is_range, is_super);
      const char* descriptor;
      if (called_method == NULL) {
        uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
        const DexFile::MethodId& method_id = dex_file_->GetMethodId(method_idx);
        uint32_t return_type_idx = dex_file_->GetProtoId(method_id.proto_idx_).return_type_idx_;
        descriptor =  dex_file_->StringByTypeIdx(return_type_idx);
      } else {
        descriptor = MethodHelper(called_method).GetReturnTypeDescriptor();
      }
      const RegType& return_type = reg_types_.FromDescriptor(class_loader_, descriptor, false);
      if (!return_type.IsLowHalf()) {
        work_line_->SetResultRegisterType(return_type);
      } else {
        work_line_->SetResultRegisterTypeWide(return_type, return_type.HighHalf(&reg_types_));
      }
      just_set_result = true;
      break;
    }
    case Instruction::INVOKE_DIRECT:
    case Instruction::INVOKE_DIRECT_RANGE: {
      bool is_range = (inst->Opcode() == Instruction::INVOKE_DIRECT_RANGE);
      mirror::ArtMethod* called_method = VerifyInvocationArgs(inst, METHOD_DIRECT,
                                                                   is_range, false);
      const char* return_type_descriptor;
      bool is_constructor;
      if (called_method == NULL) {
        uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
        const DexFile::MethodId& method_id = dex_file_->GetMethodId(method_idx);
        is_constructor = StringPiece(dex_file_->GetMethodName(method_id)) == "<init>";
        uint32_t return_type_idx = dex_file_->GetProtoId(method_id.proto_idx_).return_type_idx_;
        return_type_descriptor =  dex_file_->StringByTypeIdx(return_type_idx);
      } else {
        is_constructor = called_method->IsConstructor();
        return_type_descriptor = MethodHelper(called_method).GetReturnTypeDescriptor();
      }
      if (is_constructor) {
        /*
         * Some additional checks when calling a constructor. We know from the invocation arg check
         * that the "this" argument is an instance of called_method->klass. Now we further restrict
         * that to require that called_method->klass is the same as this->klass or this->super,
         * allowing the latter only if the "this" argument is the same as the "this" argument to
         * this method (which implies that we're in a constructor ourselves).
         */
        const RegType& this_type = work_line_->GetInvocationThis(inst, is_range);
        if (this_type.IsConflict())  // failure.
          break;

        /* no null refs allowed (?) */
        if (this_type.IsZero()) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "unable to initialize null ref";
          break;
        }

        /* must be in same class or in superclass */
        // const RegType& this_super_klass = this_type.GetSuperClass(&reg_types_);
        // TODO: re-enable constructor type verification
        // if (this_super_klass.IsConflict()) {
          // Unknown super class, fail so we re-check at runtime.
          // Fail(VERIFY_ERROR_BAD_CLASS_SOFT) << "super class unknown for '" << this_type << "'";
          // break;
        // }

        /* arg must be an uninitialized reference */
        if (!this_type.IsUninitializedTypes()) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Expected initialization on uninitialized reference "
              << this_type;
          break;
        }

        /*
         * Replace the uninitialized reference with an initialized one. We need to do this for all
         * registers that have the same object instance in them, not just the "this" register.
         */
        work_line_->MarkRefsAsInitialized(this_type);
      }
      const RegType& return_type = reg_types_.FromDescriptor(class_loader_, return_type_descriptor,
                                                             false);
      if (!return_type.IsLowHalf()) {
        work_line_->SetResultRegisterType(return_type);
      } else {
        work_line_->SetResultRegisterTypeWide(return_type, return_type.HighHalf(&reg_types_));
      }
      just_set_result = true;
      break;
    }
    case Instruction::INVOKE_STATIC:
    case Instruction::INVOKE_STATIC_RANGE: {
        bool is_range = (inst->Opcode() == Instruction::INVOKE_STATIC_RANGE);
        mirror::ArtMethod* called_method = VerifyInvocationArgs(inst,
                                                                     METHOD_STATIC,
                                                                     is_range,
                                                                     false);
        const char* descriptor;
        if (called_method == NULL) {
          uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
          const DexFile::MethodId& method_id = dex_file_->GetMethodId(method_idx);
          uint32_t return_type_idx = dex_file_->GetProtoId(method_id.proto_idx_).return_type_idx_;
          descriptor =  dex_file_->StringByTypeIdx(return_type_idx);
        } else {
          descriptor = MethodHelper(called_method).GetReturnTypeDescriptor();
        }
        const RegType& return_type =  reg_types_.FromDescriptor(class_loader_, descriptor, false);
        if (!return_type.IsLowHalf()) {
          work_line_->SetResultRegisterType(return_type);
        } else {
          work_line_->SetResultRegisterTypeWide(return_type, return_type.HighHalf(&reg_types_));
        }
        just_set_result = true;
      }
      break;
    case Instruction::INVOKE_INTERFACE:
    case Instruction::INVOKE_INTERFACE_RANGE: {
      bool is_range =  (inst->Opcode() == Instruction::INVOKE_INTERFACE_RANGE);
      mirror::ArtMethod* abs_method = VerifyInvocationArgs(inst,
                                                                METHOD_INTERFACE,
                                                                is_range,
                                                                false);
      if (abs_method != NULL) {
        mirror::Class* called_interface = abs_method->GetDeclaringClass();
        if (!called_interface->IsInterface() && !called_interface->IsObjectClass()) {
          Fail(VERIFY_ERROR_CLASS_CHANGE) << "expected interface class in invoke-interface '"
              << PrettyMethod(abs_method) << "'";
          break;
        }
      }
      /* Get the type of the "this" arg, which should either be a sub-interface of called
       * interface or Object (see comments in RegType::JoinClass).
       */
      const RegType& this_type = work_line_->GetInvocationThis(inst, is_range);
      if (this_type.IsZero()) {
        /* null pointer always passes (and always fails at runtime) */
      } else {
        if (this_type.IsUninitializedTypes()) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "interface call on uninitialized object "
              << this_type;
          break;
        }
        // In the past we have tried to assert that "called_interface" is assignable
        // from "this_type.GetClass()", however, as we do an imprecise Join
        // (RegType::JoinClass) we don't have full information on what interfaces are
        // implemented by "this_type". For example, two classes may implement the same
        // interfaces and have a common parent that doesn't implement the interface. The
        // join will set "this_type" to the parent class and a test that this implements
        // the interface will incorrectly fail.
      }
      /*
       * We don't have an object instance, so we can't find the concrete method. However, all of
       * the type information is in the abstract method, so we're good.
       */
      const char* descriptor;
      if (abs_method == NULL) {
        uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
        const DexFile::MethodId& method_id = dex_file_->GetMethodId(method_idx);
        uint32_t return_type_idx = dex_file_->GetProtoId(method_id.proto_idx_).return_type_idx_;
        descriptor =  dex_file_->StringByTypeIdx(return_type_idx);
      } else {
        descriptor = MethodHelper(abs_method).GetReturnTypeDescriptor();
      }
      const RegType& return_type = reg_types_.FromDescriptor(class_loader_, descriptor, false);
      if (!return_type.IsLowHalf()) {
        work_line_->SetResultRegisterType(return_type);
      } else {
        work_line_->SetResultRegisterTypeWide(return_type, return_type.HighHalf(&reg_types_));
      }
      just_set_result = true;
      break;
    }
    case Instruction::NEG_INT:
    case Instruction::NOT_INT:
      work_line_->CheckUnaryOp(inst, reg_types_.Integer(), reg_types_.Integer());
      break;
    case Instruction::NEG_LONG:
    case Instruction::NOT_LONG:
      work_line_->CheckUnaryOpWide(inst, reg_types_.LongLo(), reg_types_.LongHi(),
                                   reg_types_.LongLo(), reg_types_.LongHi());
      break;
    case Instruction::NEG_FLOAT:
      work_line_->CheckUnaryOp(inst, reg_types_.Float(), reg_types_.Float());
      break;
    case Instruction::NEG_DOUBLE:
      work_line_->CheckUnaryOpWide(inst, reg_types_.DoubleLo(), reg_types_.DoubleHi(),
                                   reg_types_.DoubleLo(), reg_types_.DoubleHi());
      break;
    case Instruction::INT_TO_LONG:
      work_line_->CheckUnaryOpToWide(inst, reg_types_.LongLo(), reg_types_.LongHi(),
                                     reg_types_.Integer());
      break;
    case Instruction::INT_TO_FLOAT:
      work_line_->CheckUnaryOp(inst, reg_types_.Float(), reg_types_.Integer());
      break;
    case Instruction::INT_TO_DOUBLE:
      work_line_->CheckUnaryOpToWide(inst, reg_types_.DoubleLo(), reg_types_.DoubleHi(),
                                     reg_types_.Integer());
      break;
    case Instruction::LONG_TO_INT:
      work_line_->CheckUnaryOpFromWide(inst, reg_types_.Integer(),
                                       reg_types_.LongLo(), reg_types_.LongHi());
      break;
    case Instruction::LONG_TO_FLOAT:
      work_line_->CheckUnaryOpFromWide(inst, reg_types_.Float(),
                                       reg_types_.LongLo(), reg_types_.LongHi());
      break;
    case Instruction::LONG_TO_DOUBLE:
      work_line_->CheckUnaryOpWide(inst, reg_types_.DoubleLo(), reg_types_.DoubleHi(),
                                   reg_types_.LongLo(), reg_types_.LongHi());
      break;
    case Instruction::FLOAT_TO_INT:
      work_line_->CheckUnaryOp(inst, reg_types_.Integer(), reg_types_.Float());
      break;
    case Instruction::FLOAT_TO_LONG:
      work_line_->CheckUnaryOpToWide(inst, reg_types_.LongLo(), reg_types_.LongHi(),
                                     reg_types_.Float());
      break;
    case Instruction::FLOAT_TO_DOUBLE:
      work_line_->CheckUnaryOpToWide(inst, reg_types_.DoubleLo(), reg_types_.DoubleHi(),
                                     reg_types_.Float());
      break;
    case Instruction::DOUBLE_TO_INT:
      work_line_->CheckUnaryOpFromWide(inst, reg_types_.Integer(),
                                       reg_types_.DoubleLo(), reg_types_.DoubleHi());
      break;
    case Instruction::DOUBLE_TO_LONG:
      work_line_->CheckUnaryOpWide(inst, reg_types_.LongLo(), reg_types_.LongHi(),
                                   reg_types_.DoubleLo(), reg_types_.DoubleHi());
      break;
    case Instruction::DOUBLE_TO_FLOAT:
      work_line_->CheckUnaryOpFromWide(inst, reg_types_.Float(),
                                       reg_types_.DoubleLo(), reg_types_.DoubleHi());
      break;
    case Instruction::INT_TO_BYTE:
      work_line_->CheckUnaryOp(inst, reg_types_.Byte(), reg_types_.Integer());
      break;
    case Instruction::INT_TO_CHAR:
      work_line_->CheckUnaryOp(inst, reg_types_.Char(), reg_types_.Integer());
      break;
    case Instruction::INT_TO_SHORT:
      work_line_->CheckUnaryOp(inst, reg_types_.Short(), reg_types_.Integer());
      break;

    case Instruction::ADD_INT:
    case Instruction::SUB_INT:
    case Instruction::MUL_INT:
    case Instruction::REM_INT:
    case Instruction::DIV_INT:
    case Instruction::SHL_INT:
    case Instruction::SHR_INT:
    case Instruction::USHR_INT:
      work_line_->CheckBinaryOp(inst, reg_types_.Integer(), reg_types_.Integer(),
                                reg_types_.Integer(), false);
      break;
    case Instruction::AND_INT:
    case Instruction::OR_INT:
    case Instruction::XOR_INT:
      work_line_->CheckBinaryOp(inst, reg_types_.Integer(), reg_types_.Integer(),
                                reg_types_.Integer(), true);
      break;
    case Instruction::ADD_LONG:
    case Instruction::SUB_LONG:
    case Instruction::MUL_LONG:
    case Instruction::DIV_LONG:
    case Instruction::REM_LONG:
    case Instruction::AND_LONG:
    case Instruction::OR_LONG:
    case Instruction::XOR_LONG:
      work_line_->CheckBinaryOpWide(inst, reg_types_.LongLo(), reg_types_.LongHi(),
                                    reg_types_.LongLo(), reg_types_.LongHi(),
                                    reg_types_.LongLo(), reg_types_.LongHi());
      break;
    case Instruction::SHL_LONG:
    case Instruction::SHR_LONG:
    case Instruction::USHR_LONG:
      /* shift distance is Int, making these different from other binary operations */
      work_line_->CheckBinaryOpWideShift(inst, reg_types_.LongLo(), reg_types_.LongHi(),
                                         reg_types_.Integer());
      break;
    case Instruction::ADD_FLOAT:
    case Instruction::SUB_FLOAT:
    case Instruction::MUL_FLOAT:
    case Instruction::DIV_FLOAT:
    case Instruction::REM_FLOAT:
      work_line_->CheckBinaryOp(inst,
                                reg_types_.Float(),
                                reg_types_.Float(),
                                reg_types_.Float(),
                                false);
      break;
    case Instruction::ADD_DOUBLE:
    case Instruction::SUB_DOUBLE:
    case Instruction::MUL_DOUBLE:
    case Instruction::DIV_DOUBLE:
    case Instruction::REM_DOUBLE:
      work_line_->CheckBinaryOpWide(inst, reg_types_.DoubleLo(), reg_types_.DoubleHi(),
                                    reg_types_.DoubleLo(), reg_types_.DoubleHi(),
                                    reg_types_.DoubleLo(), reg_types_.DoubleHi());
      break;
    case Instruction::ADD_INT_2ADDR:
    case Instruction::SUB_INT_2ADDR:
    case Instruction::MUL_INT_2ADDR:
    case Instruction::REM_INT_2ADDR:
    case Instruction::SHL_INT_2ADDR:
    case Instruction::SHR_INT_2ADDR:
    case Instruction::USHR_INT_2ADDR:
      work_line_->CheckBinaryOp2addr(inst,
                                     reg_types_.Integer(),
                                     reg_types_.Integer(),
                                     reg_types_.Integer(),
                                     false);
      break;
    case Instruction::AND_INT_2ADDR:
    case Instruction::OR_INT_2ADDR:
    case Instruction::XOR_INT_2ADDR:
      work_line_->CheckBinaryOp2addr(inst,
                                     reg_types_.Integer(),
                                     reg_types_.Integer(),
                                     reg_types_.Integer(),
                                     true);
      break;
    case Instruction::DIV_INT_2ADDR:
      work_line_->CheckBinaryOp2addr(inst,
                                     reg_types_.Integer(),
                                     reg_types_.Integer(),
                                     reg_types_.Integer(),
                                     false);
      break;
    case Instruction::ADD_LONG_2ADDR:
    case Instruction::SUB_LONG_2ADDR:
    case Instruction::MUL_LONG_2ADDR:
    case Instruction::DIV_LONG_2ADDR:
    case Instruction::REM_LONG_2ADDR:
    case Instruction::AND_LONG_2ADDR:
    case Instruction::OR_LONG_2ADDR:
    case Instruction::XOR_LONG_2ADDR:
      work_line_->CheckBinaryOp2addrWide(inst, reg_types_.LongLo(), reg_types_.LongHi(),
                                         reg_types_.LongLo(), reg_types_.LongHi(),
                                         reg_types_.LongLo(), reg_types_.LongHi());
      break;
    case Instruction::SHL_LONG_2ADDR:
    case Instruction::SHR_LONG_2ADDR:
    case Instruction::USHR_LONG_2ADDR:
      work_line_->CheckBinaryOp2addrWideShift(inst, reg_types_.LongLo(), reg_types_.LongHi(),
                                              reg_types_.Integer());
      break;
    case Instruction::ADD_FLOAT_2ADDR:
    case Instruction::SUB_FLOAT_2ADDR:
    case Instruction::MUL_FLOAT_2ADDR:
    case Instruction::DIV_FLOAT_2ADDR:
    case Instruction::REM_FLOAT_2ADDR:
      work_line_->CheckBinaryOp2addr(inst,
                                     reg_types_.Float(),
                                     reg_types_.Float(),
                                     reg_types_.Float(),
                                     false);
      break;
    case Instruction::ADD_DOUBLE_2ADDR:
    case Instruction::SUB_DOUBLE_2ADDR:
    case Instruction::MUL_DOUBLE_2ADDR:
    case Instruction::DIV_DOUBLE_2ADDR:
    case Instruction::REM_DOUBLE_2ADDR:
      work_line_->CheckBinaryOp2addrWide(inst, reg_types_.DoubleLo(), reg_types_.DoubleHi(),
                                         reg_types_.DoubleLo(),  reg_types_.DoubleHi(),
                                         reg_types_.DoubleLo(), reg_types_.DoubleHi());
      break;
    case Instruction::ADD_INT_LIT16:
    case Instruction::RSUB_INT:
    case Instruction::MUL_INT_LIT16:
    case Instruction::DIV_INT_LIT16:
    case Instruction::REM_INT_LIT16:
      work_line_->CheckLiteralOp(inst, reg_types_.Integer(), reg_types_.Integer(), false, true);
      break;
    case Instruction::AND_INT_LIT16:
    case Instruction::OR_INT_LIT16:
    case Instruction::XOR_INT_LIT16:
      work_line_->CheckLiteralOp(inst, reg_types_.Integer(), reg_types_.Integer(), true, true);
      break;
    case Instruction::ADD_INT_LIT8:
    case Instruction::RSUB_INT_LIT8:
    case Instruction::MUL_INT_LIT8:
    case Instruction::DIV_INT_LIT8:
    case Instruction::REM_INT_LIT8:
    case Instruction::SHL_INT_LIT8:
    case Instruction::SHR_INT_LIT8:
    case Instruction::USHR_INT_LIT8:
      work_line_->CheckLiteralOp(inst, reg_types_.Integer(), reg_types_.Integer(), false, false);
      break;
    case Instruction::AND_INT_LIT8:
    case Instruction::OR_INT_LIT8:
    case Instruction::XOR_INT_LIT8:
      work_line_->CheckLiteralOp(inst, reg_types_.Integer(), reg_types_.Integer(), true, false);
      break;

    // Special instructions.
    case Instruction::RETURN_VOID_BARRIER:
      DCHECK(Runtime::Current()->IsStarted()) << PrettyMethod(dex_method_idx_, *dex_file_);
      if (!IsConstructor() || IsStatic()) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "return-void-barrier not expected";
      }
      break;
    // Note: the following instructions encode offsets derived from class linking.
    // As such they use Class*/Field*/AbstractMethod* as these offsets only have
    // meaning if the class linking and resolution were successful.
    case Instruction::IGET_QUICK:
      VerifyIGetQuick(inst, reg_types_.Integer(), true);
      break;
    case Instruction::IGET_WIDE_QUICK:
      VerifyIGetQuick(inst, reg_types_.LongLo(), true);
      break;
    case Instruction::IGET_OBJECT_QUICK:
      VerifyIGetQuick(inst, reg_types_.JavaLangObject(false), false);
      break;
    case Instruction::IPUT_QUICK:
      VerifyIPutQuick(inst, reg_types_.Integer(), true);
      break;
    case Instruction::IPUT_WIDE_QUICK:
      VerifyIPutQuick(inst, reg_types_.LongLo(), true);
      break;
    case Instruction::IPUT_OBJECT_QUICK:
      VerifyIPutQuick(inst, reg_types_.JavaLangObject(false), false);
      break;
    case Instruction::INVOKE_VIRTUAL_QUICK:
    case Instruction::INVOKE_VIRTUAL_RANGE_QUICK: {
      bool is_range = (inst->Opcode() == Instruction::INVOKE_VIRTUAL_RANGE_QUICK);
      mirror::ArtMethod* called_method = VerifyInvokeVirtualQuickArgs(inst, is_range);
      if (called_method != NULL) {
        const char* descriptor = MethodHelper(called_method).GetReturnTypeDescriptor();
        const RegType& return_type = reg_types_.FromDescriptor(class_loader_, descriptor, false);
        if (!return_type.IsLowHalf()) {
          work_line_->SetResultRegisterType(return_type);
        } else {
          work_line_->SetResultRegisterTypeWide(return_type, return_type.HighHalf(&reg_types_));
        }
        just_set_result = true;
      }
      break;
    }

    /* These should never appear during verification. */
    case Instruction::UNUSED_3E:
    case Instruction::UNUSED_3F:
    case Instruction::UNUSED_40:
    case Instruction::UNUSED_41:
    case Instruction::UNUSED_42:
    case Instruction::UNUSED_43:
    case Instruction::UNUSED_79:
    case Instruction::UNUSED_7A:
    case Instruction::UNUSED_EB:
    case Instruction::UNUSED_EC:
    case Instruction::UNUSED_ED:
    case Instruction::UNUSED_EE:
    case Instruction::UNUSED_EF:
    case Instruction::UNUSED_F0:
    case Instruction::UNUSED_F1:
    case Instruction::UNUSED_F2:
    case Instruction::UNUSED_F3:
    case Instruction::UNUSED_F4:
    case Instruction::UNUSED_F5:
    case Instruction::UNUSED_F6:
    case Instruction::UNUSED_F7:
    case Instruction::UNUSED_F8:
    case Instruction::UNUSED_F9:
    case Instruction::UNUSED_FA:
    case Instruction::UNUSED_FB:
    case Instruction::UNUSED_FC:
    case Instruction::UNUSED_FD:
    case Instruction::UNUSED_FE:
    case Instruction::UNUSED_FF:
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Unexpected opcode " << inst->DumpString(dex_file_);
      break;

    /*
     * DO NOT add a "default" clause here. Without it the compiler will
     * complain if an instruction is missing (which is desirable).
     */
  }  // end - switch (dec_insn.opcode)

  if (have_pending_hard_failure_) {
    if (Runtime::Current()->IsCompiler()) {
      /* When compiling, check that the last failure is a hard failure */
      CHECK_EQ(failures_[failures_.size() - 1], VERIFY_ERROR_BAD_CLASS_HARD);
    }
    /* immediate failure, reject class */
    info_messages_ << "Rejecting opcode " << inst->DumpString(dex_file_);
    return false;
  } else if (have_pending_runtime_throw_failure_) {
    /* checking interpreter will throw, mark following code as unreachable */
    opcode_flags = Instruction::kThrow;
  }
  /*
   * If we didn't just set the result register, clear it out. This ensures that you can only use
   * "move-result" immediately after the result is set. (We could check this statically, but it's
   * not expensive and it makes our debugging output cleaner.)
   */
  if (!just_set_result) {
    work_line_->SetResultTypeToUnknown();
  }



  /*
   * Handle "branch". Tag the branch target.
   *
   * NOTE: instructions like Instruction::EQZ provide information about the
   * state of the register when the branch is taken or not taken. For example,
   * somebody could get a reference field, check it for zero, and if the
   * branch is taken immediately store that register in a boolean field
   * since the value is known to be zero. We do not currently account for
   * that, and will reject the code.
   *
   * TODO: avoid re-fetching the branch target
   */
  if ((opcode_flags & Instruction::kBranch) != 0) {
    bool isConditional, selfOkay;
    if (!GetBranchOffset(work_insn_idx_, &branch_target, &isConditional, &selfOkay)) {
      /* should never happen after static verification */
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "bad branch";
      return false;
    }
    DCHECK_EQ(isConditional, (opcode_flags & Instruction::kContinue) != 0);
    if (!CheckNotMoveException(code_item_->insns_, work_insn_idx_ + branch_target)) {
      return false;
    }
    /* update branch target, set "changed" if appropriate */
    if (NULL != branch_line.get()) {
      if (!UpdateRegisters(work_insn_idx_ + branch_target, branch_line.get())) {
        return false;
      }
    } else {
      if (!UpdateRegisters(work_insn_idx_ + branch_target, work_line_.get())) {
        return false;
      }
    }
  }

  /*
   * Handle "switch". Tag all possible branch targets.
   *
   * We've already verified that the table is structurally sound, so we
   * just need to walk through and tag the targets.
   */
  if ((opcode_flags & Instruction::kSwitch) != 0) {
    int offset_to_switch = insns[1] | (((int32_t) insns[2]) << 16);
    const uint16_t* switch_insns = insns + offset_to_switch;
    int switch_count = switch_insns[1];
    int offset_to_targets, targ;

    if ((*insns & 0xff) == Instruction::PACKED_SWITCH) {
      /* 0 = sig, 1 = count, 2/3 = first key */
      offset_to_targets = 4;
    } else {
      /* 0 = sig, 1 = count, 2..count * 2 = keys */
      DCHECK((*insns & 0xff) == Instruction::SPARSE_SWITCH);
      offset_to_targets = 2 + 2 * switch_count;
    }

    /* verify each switch target */
    for (targ = 0; targ < switch_count; targ++) {
      int offset;
      uint32_t abs_offset;

      /* offsets are 32-bit, and only partly endian-swapped */
      offset = switch_insns[offset_to_targets + targ * 2] |
         (((int32_t) switch_insns[offset_to_targets + targ * 2 + 1]) << 16);
      abs_offset = work_insn_idx_ + offset;
      DCHECK_LT(abs_offset, code_item_->insns_size_in_code_units_);
      if (!CheckNotMoveException(code_item_->insns_, abs_offset)) {
        return false;
      }
      if (!UpdateRegisters(abs_offset, work_line_.get()))
        return false;
    }
  }

  /*
   * Handle instructions that can throw and that are sitting in a "try" block. (If they're not in a
   * "try" block when they throw, control transfers out of the method.)
   */
  if ((opcode_flags & Instruction::kThrow) != 0 && insn_flags_[work_insn_idx_].IsInTry()) {
    bool within_catch_all = false;
    CatchHandlerIterator iterator(*code_item_, work_insn_idx_);

    for (; iterator.HasNext(); iterator.Next()) {
      if (iterator.GetHandlerTypeIndex() == DexFile::kDexNoIndex16) {
        within_catch_all = true;
      }
      /*
       * Merge registers into the "catch" block. We want to use the "savedRegs" rather than
       * "work_regs", because at runtime the exception will be thrown before the instruction
       * modifies any registers.
       */
      if (!UpdateRegisters(iterator.GetHandlerAddress(), saved_line_.get())) {
        return false;
      }
    }

    /*
     * If the monitor stack depth is nonzero, there must be a "catch all" handler for this
     * instruction. This does apply to monitor-exit because of async exception handling.
     */
    if (work_line_->MonitorStackDepth() > 0 && !within_catch_all) {
      /*
       * The state in work_line reflects the post-execution state. If the current instruction is a
       * monitor-enter and the monitor stack was empty, we don't need a catch-all (if it throws,
       * it will do so before grabbing the lock).
       */
      if (inst->Opcode() != Instruction::MONITOR_ENTER || work_line_->MonitorStackDepth() != 1) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD)
            << "expected to be within a catch-all for an instruction where a monitor is held";
        return false;
      }
    }
  }

  /* Handle "continue". Tag the next consecutive instruction.
   *  Note: Keep the code handling "continue" case below the "branch" and "switch" cases,
   *        because it changes work_line_ when performing peephole optimization
   *        and this change should not be used in those cases.
   */
  if ((opcode_flags & Instruction::kContinue) != 0) {
    uint32_t next_insn_idx = work_insn_idx_ + CurrentInsnFlags()->GetLengthInCodeUnits();
    if (next_insn_idx >= code_item_->insns_size_in_code_units_) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Execution can walk off end of code area";
      return false;
    }
    // The only way to get to a move-exception instruction is to get thrown there. Make sure the
    // next instruction isn't one.
    if (!CheckNotMoveException(code_item_->insns_, next_insn_idx)) {
      return false;
    }
    if (NULL != fallthrough_line.get()) {
      // Make workline consistent with fallthrough computed from peephole optimization.
      work_line_->CopyFromLine(fallthrough_line.get());
    }
    if (insn_flags_[next_insn_idx].IsReturn()) {
      // For returns we only care about the operand to the return, all other registers are dead.
      const Instruction* ret_inst = Instruction::At(code_item_->insns_ + next_insn_idx);
      Instruction::Code opcode = ret_inst->Opcode();
      if ((opcode == Instruction::RETURN_VOID) || (opcode == Instruction::RETURN_VOID_BARRIER)) {
        work_line_->MarkAllRegistersAsConflicts();
      } else {
        if (opcode == Instruction::RETURN_WIDE) {
          work_line_->MarkAllRegistersAsConflictsExceptWide(ret_inst->VRegA_11x());
        } else {
          work_line_->MarkAllRegistersAsConflictsExcept(ret_inst->VRegA_11x());
        }
      }
    }
    RegisterLine* next_line = reg_table_.GetLine(next_insn_idx);
    if (next_line != NULL) {
      // Merge registers into what we have for the next instruction,
      // and set the "changed" flag if needed.
      if (!UpdateRegisters(next_insn_idx, work_line_.get())) {
        return false;
      }
    } else {
      /*
       * We're not recording register data for the next instruction, so we don't know what the
       * prior state was. We have to assume that something has changed and re-evaluate it.
       */
      insn_flags_[next_insn_idx].SetChanged();
    }
  }

  /* If we're returning from the method, make sure monitor stack is empty. */
  if ((opcode_flags & Instruction::kReturn) != 0) {
    if (!work_line_->VerifyMonitorStackEmpty()) {
      return false;
    }
  }

  /*
   * Update start_guess. Advance to the next instruction of that's
   * possible, otherwise use the branch target if one was found. If
   * neither of those exists we're in a return or throw; leave start_guess
   * alone and let the caller sort it out.
   */
  if ((opcode_flags & Instruction::kContinue) != 0) {
    *start_guess = work_insn_idx_ + insn_flags_[work_insn_idx_].GetLengthInCodeUnits();
  } else if ((opcode_flags & Instruction::kBranch) != 0) {
    /* we're still okay if branch_target is zero */
    *start_guess = work_insn_idx_ + branch_target;
  }

  DCHECK_LT(*start_guess, code_item_->insns_size_in_code_units_);
  DCHECK(insn_flags_[*start_guess].IsOpcode());

  return true;
}  // NOLINT(readability/fn_size)

const RegType& MethodVerifier::ResolveClassAndCheckAccess(uint32_t class_idx) {
  const char* descriptor = dex_file_->StringByTypeIdx(class_idx);
  const RegType& referrer = GetDeclaringClass();
  mirror::Class* klass = dex_cache_->GetResolvedType(class_idx);
  const RegType& result =
      klass != NULL ? reg_types_.FromClass(descriptor, klass,
                                           klass->CannotBeAssignedFromOtherTypes())
                    : reg_types_.FromDescriptor(class_loader_, descriptor, false);
  if (result.IsConflict()) {
    Fail(VERIFY_ERROR_BAD_CLASS_SOFT) << "accessing broken descriptor '" << descriptor
        << "' in " << referrer;
    return result;
  }
  if (klass == NULL && !result.IsUnresolvedTypes()) {
    dex_cache_->SetResolvedType(class_idx, result.GetClass());
  }
  // Check if access is allowed. Unresolved types use xxxWithAccessCheck to
  // check at runtime if access is allowed and so pass here. If result is
  // primitive, skip the access check.
  if (result.IsNonZeroReferenceTypes() && !result.IsUnresolvedTypes() &&
      !referrer.IsUnresolvedTypes() && !referrer.CanAccess(result)) {
    Fail(VERIFY_ERROR_ACCESS_CLASS) << "illegal class access: '"
                                    << referrer << "' -> '" << result << "'";
  }
  return result;
}

const RegType& MethodVerifier::GetCaughtExceptionType() {
  const RegType* common_super = NULL;
  if (code_item_->tries_size_ != 0) {
    const byte* handlers_ptr = DexFile::GetCatchHandlerData(*code_item_, 0);
    uint32_t handlers_size = DecodeUnsignedLeb128(&handlers_ptr);
    for (uint32_t i = 0; i < handlers_size; i++) {
      CatchHandlerIterator iterator(handlers_ptr);
      for (; iterator.HasNext(); iterator.Next()) {
        if (iterator.GetHandlerAddress() == (uint32_t) work_insn_idx_) {
          if (iterator.GetHandlerTypeIndex() == DexFile::kDexNoIndex16) {
            common_super = &reg_types_.JavaLangThrowable(false);
          } else {
            const RegType& exception = ResolveClassAndCheckAccess(iterator.GetHandlerTypeIndex());
            if (common_super == NULL) {
              // Unconditionally assign for the first handler. We don't assert this is a Throwable
              // as that is caught at runtime
              common_super = &exception;
            } else if (!reg_types_.JavaLangThrowable(false).IsAssignableFrom(exception)) {
              if (exception.IsUnresolvedTypes()) {
                // We don't know enough about the type. Fail here and let runtime handle it.
                Fail(VERIFY_ERROR_NO_CLASS) << "unresolved exception class " << exception;
                return exception;
              } else {
                Fail(VERIFY_ERROR_BAD_CLASS_SOFT) << "unexpected non-exception class " << exception;
                return reg_types_.Conflict();
              }
            } else if (common_super->Equals(exception)) {
              // odd case, but nothing to do
            } else {
              common_super = &common_super->Merge(exception, &reg_types_);
              CHECK(reg_types_.JavaLangThrowable(false).IsAssignableFrom(*common_super));
            }
          }
        }
      }
      handlers_ptr = iterator.EndDataPointer();
    }
  }
  if (common_super == NULL) {
    /* no catch blocks, or no catches with classes we can find */
    Fail(VERIFY_ERROR_BAD_CLASS_SOFT) << "unable to find exception handler";
    return reg_types_.Conflict();
  }
  return *common_super;
}

mirror::ArtMethod* MethodVerifier::ResolveMethodAndCheckAccess(uint32_t dex_method_idx,
                                                                    MethodType method_type) {
  const DexFile::MethodId& method_id = dex_file_->GetMethodId(dex_method_idx);
  const RegType& klass_type = ResolveClassAndCheckAccess(method_id.class_idx_);
  if (klass_type.IsConflict()) {
    std::string append(" in attempt to access method ");
    append += dex_file_->GetMethodName(method_id);
    AppendToLastFailMessage(append);
    return NULL;
  }
  if (klass_type.IsUnresolvedTypes()) {
    return NULL;  // Can't resolve Class so no more to do here
  }
  mirror::Class* klass = klass_type.GetClass();
  const RegType& referrer = GetDeclaringClass();
  mirror::ArtMethod* res_method = dex_cache_->GetResolvedMethod(dex_method_idx);
  if (res_method == NULL) {
    const char* name = dex_file_->GetMethodName(method_id);
    std::string signature(dex_file_->CreateMethodSignature(method_id.proto_idx_, NULL));

    if (method_type == METHOD_DIRECT || method_type == METHOD_STATIC) {
      res_method = klass->FindDirectMethod(name, signature);
    } else if (method_type == METHOD_INTERFACE) {
      res_method = klass->FindInterfaceMethod(name, signature);
    } else {
      res_method = klass->FindVirtualMethod(name, signature);
    }
    if (res_method != NULL) {
      dex_cache_->SetResolvedMethod(dex_method_idx, res_method);
    } else {
      // If a virtual or interface method wasn't found with the expected type, look in
      // the direct methods. This can happen when the wrong invoke type is used or when
      // a class has changed, and will be flagged as an error in later checks.
      if (method_type == METHOD_INTERFACE || method_type == METHOD_VIRTUAL) {
        res_method = klass->FindDirectMethod(name, signature);
      }
      if (res_method == NULL) {
        Fail(VERIFY_ERROR_NO_METHOD) << "couldn't find method "
                                     << PrettyDescriptor(klass) << "." << name
                                     << " " << signature;
        return NULL;
      }
    }
  }
  // Make sure calls to constructors are "direct". There are additional restrictions but we don't
  // enforce them here.
  if (res_method->IsConstructor() && method_type != METHOD_DIRECT) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "rejecting non-direct call to constructor "
                                      << PrettyMethod(res_method);
    return NULL;
  }
  // Disallow any calls to class initializers.
  if (MethodHelper(res_method).IsClassInitializer()) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "rejecting call to class initializer "
                                      << PrettyMethod(res_method);
    return NULL;
  }
  // Check if access is allowed.
  if (!referrer.CanAccessMember(res_method->GetDeclaringClass(), res_method->GetAccessFlags())) {
    Fail(VERIFY_ERROR_ACCESS_METHOD) << "illegal method access (call " << PrettyMethod(res_method)
                                     << " from " << referrer << ")";
    return res_method;
  }
  // Check that invoke-virtual and invoke-super are not used on private methods of the same class.
  if (res_method->IsPrivate() && method_type == METHOD_VIRTUAL) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invoke-super/virtual can't be used on private method "
                                      << PrettyMethod(res_method);
    return NULL;
  }
  // Check that interface methods match interface classes.
  if (klass->IsInterface() && method_type != METHOD_INTERFACE) {
    Fail(VERIFY_ERROR_CLASS_CHANGE) << "non-interface method " << PrettyMethod(res_method)
                                    << " is in an interface class " << PrettyClass(klass);
    return NULL;
  } else if (!klass->IsInterface() && method_type == METHOD_INTERFACE) {
    Fail(VERIFY_ERROR_CLASS_CHANGE) << "interface method " << PrettyMethod(res_method)
                                    << " is in a non-interface class " << PrettyClass(klass);
    return NULL;
  }
  // See if the method type implied by the invoke instruction matches the access flags for the
  // target method.
  if ((method_type == METHOD_DIRECT && !res_method->IsDirect()) ||
      (method_type == METHOD_STATIC && !res_method->IsStatic()) ||
      ((method_type == METHOD_VIRTUAL || method_type == METHOD_INTERFACE) && res_method->IsDirect())
      ) {
    Fail(VERIFY_ERROR_CLASS_CHANGE) << "invoke type (" << method_type << ") does not match method "
                                       " type of " << PrettyMethod(res_method);
    return NULL;
  }
  return res_method;
}

mirror::ArtMethod* MethodVerifier::VerifyInvocationArgs(const Instruction* inst,
                                                             MethodType method_type,
                                                             bool is_range,
                                                             bool is_super) {
  // Resolve the method. This could be an abstract or concrete method depending on what sort of call
  // we're making.
  const uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
  mirror::ArtMethod* res_method = ResolveMethodAndCheckAccess(method_idx, method_type);
  if (res_method == NULL) {  // error or class is unresolved
    return NULL;
  }

  // If we're using invoke-super(method), make sure that the executing method's class' superclass
  // has a vtable entry for the target method.
  if (is_super) {
    DCHECK(method_type == METHOD_VIRTUAL);
    const RegType& super = GetDeclaringClass().GetSuperClass(&reg_types_);
    if (super.IsUnresolvedTypes()) {
      Fail(VERIFY_ERROR_NO_METHOD) << "unknown super class in invoke-super from "
                                   << PrettyMethod(dex_method_idx_, *dex_file_)
                                   << " to super " << PrettyMethod(res_method);
      return NULL;
    }
    mirror::Class* super_klass = super.GetClass();
    if (res_method->GetMethodIndex() >= super_klass->GetVTable()->GetLength()) {
      MethodHelper mh(res_method);
      Fail(VERIFY_ERROR_NO_METHOD) << "invalid invoke-super from "
                                   << PrettyMethod(dex_method_idx_, *dex_file_)
                                   << " to super " << super
                                   << "." << mh.GetName()
                                   << mh.GetSignature();
      return NULL;
    }
  }
  // We use vAA as our expected arg count, rather than res_method->insSize, because we need to
  // match the call to the signature. Also, we might be calling through an abstract method
  // definition (which doesn't have register count values).
  const size_t expected_args = (is_range) ? inst->VRegA_3rc() : inst->VRegA_35c();
  /* caught by static verifier */
  DCHECK(is_range || expected_args <= 5);
  if (expected_args > code_item_->outs_size_) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid argument count (" << expected_args
        << ") exceeds outsSize (" << code_item_->outs_size_ << ")";
    return NULL;
  }

  /*
   * Check the "this" argument, which must be an instance of the class that declared the method.
   * For an interface class, we don't do the full interface merge (see JoinClass), so we can't do a
   * rigorous check here (which is okay since we have to do it at runtime).
   */
  size_t actual_args = 0;
  if (!res_method->IsStatic()) {
    const RegType& actual_arg_type = work_line_->GetInvocationThis(inst, is_range);
    if (actual_arg_type.IsConflict()) {  // GetInvocationThis failed.
      return NULL;
    }
    if (actual_arg_type.IsUninitializedReference() && !res_method->IsConstructor()) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "'this' arg must be initialized";
      return NULL;
    }
    if (method_type != METHOD_INTERFACE && !actual_arg_type.IsZero()) {
      mirror::Class* klass = res_method->GetDeclaringClass();
      const RegType& res_method_class =
          reg_types_.FromClass(ClassHelper(klass).GetDescriptor(), klass,
                               klass->CannotBeAssignedFromOtherTypes());
      if (!res_method_class.IsAssignableFrom(actual_arg_type)) {
        Fail(actual_arg_type.IsUnresolvedTypes() ? VERIFY_ERROR_NO_CLASS:
            VERIFY_ERROR_BAD_CLASS_SOFT) << "'this' argument '" << actual_arg_type
            << "' not instance of '" << res_method_class << "'";
        return NULL;
      }
    }
    actual_args++;
  }
  /*
   * Process the target method's signature. This signature may or may not
   * have been verified, so we can't assume it's properly formed.
   */
  MethodHelper mh(res_method);
  const DexFile::TypeList* params = mh.GetParameterTypeList();
  size_t params_size = params == NULL ? 0 : params->Size();
  uint32_t arg[5];
  if (!is_range) {
    inst->GetArgs(arg);
  }
  for (size_t param_index = 0; param_index < params_size; param_index++) {
    if (actual_args >= expected_args) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Rejecting invalid call to '" << PrettyMethod(res_method)
          << "'. Expected " << expected_args << " arguments, processing argument " << actual_args
          << " (where longs/doubles count twice).";
      return NULL;
    }
    const char* descriptor =
        mh.GetTypeDescriptorFromTypeIdx(params->GetTypeItem(param_index).type_idx_);
    if (descriptor == NULL) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Rejecting invocation of " << PrettyMethod(res_method)
          << " missing signature component";
      return NULL;
    }
    const RegType& reg_type = reg_types_.FromDescriptor(class_loader_, descriptor, false);
    uint32_t get_reg = is_range ? inst->VRegC_3rc() + actual_args : arg[actual_args];
    if (reg_type.IsIntegralTypes()) {
      const RegType& src_type = work_line_->GetRegisterType(get_reg);
      if (!src_type.IsIntegralTypes()) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "register v" << get_reg << " has type " << src_type
                                          << " but expected " << reg_type;
        return res_method;
      }
    } else if (!work_line_->VerifyRegisterType(get_reg, reg_type)) {
      return res_method;
    }
    actual_args = reg_type.IsLongOrDoubleTypes() ? actual_args + 2 : actual_args + 1;
  }
  if (actual_args != expected_args) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Rejecting invocation of " << PrettyMethod(res_method)
        << " expected " << expected_args << " arguments, found " << actual_args;
    return NULL;
  } else {
    return res_method;
  }
}

mirror::ArtMethod* MethodVerifier::GetQuickInvokedMethod(const Instruction* inst,
                                                              RegisterLine* reg_line,
                                                              bool is_range) {
  DCHECK(inst->Opcode() == Instruction::INVOKE_VIRTUAL_QUICK ||
         inst->Opcode() == Instruction::INVOKE_VIRTUAL_RANGE_QUICK);
  const RegType& actual_arg_type = reg_line->GetInvocationThis(inst, is_range);
  if (actual_arg_type.IsConflict()) {  // GetInvocationThis failed.
    return NULL;
  }
  mirror::Class* this_class = NULL;
  if (!actual_arg_type.IsUnresolvedTypes()) {
    this_class = actual_arg_type.GetClass();
  } else {
    const std::string& descriptor(actual_arg_type.GetDescriptor());
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    this_class = class_linker->FindClass(descriptor.c_str(), class_loader_);
    if (this_class == NULL) {
      Thread::Current()->ClearException();
      // Look for a system class
      this_class = class_linker->FindClass(descriptor.c_str(), NULL);
    }
  }
  if (this_class == NULL) {
    return NULL;
  }
  mirror::ObjectArray<mirror::ArtMethod>* vtable = this_class->GetVTable();
  CHECK(vtable != NULL);
  uint16_t vtable_index = is_range ? inst->VRegB_3rc() : inst->VRegB_35c();
  CHECK(vtable_index < vtable->GetLength());
  mirror::ArtMethod* res_method = vtable->Get(vtable_index);
  CHECK(!Thread::Current()->IsExceptionPending());
  return res_method;
}

mirror::ArtMethod* MethodVerifier::VerifyInvokeVirtualQuickArgs(const Instruction* inst,
                                                                     bool is_range) {
  DCHECK(Runtime::Current()->IsStarted());
  mirror::ArtMethod* res_method = GetQuickInvokedMethod(inst, work_line_.get(),
                                                             is_range);
  if (res_method == NULL) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Cannot infer method from " << inst->Name();
    return NULL;
  }
  CHECK(!res_method->IsDirect() && !res_method->IsStatic());

  // We use vAA as our expected arg count, rather than res_method->insSize, because we need to
  // match the call to the signature. Also, we might be calling through an abstract method
  // definition (which doesn't have register count values).
  const RegType& actual_arg_type = work_line_->GetInvocationThis(inst, is_range);
  if (actual_arg_type.IsConflict()) {  // GetInvocationThis failed.
    return NULL;
  }
  const size_t expected_args = (is_range) ? inst->VRegA_3rc() : inst->VRegA_35c();
  /* caught by static verifier */
  DCHECK(is_range || expected_args <= 5);
  if (expected_args > code_item_->outs_size_) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid argument count (" << expected_args
        << ") exceeds outsSize (" << code_item_->outs_size_ << ")";
    return NULL;
  }

  /*
   * Check the "this" argument, which must be an instance of the class that declared the method.
   * For an interface class, we don't do the full interface merge (see JoinClass), so we can't do a
   * rigorous check here (which is okay since we have to do it at runtime).
   */
  if (actual_arg_type.IsUninitializedReference() && !res_method->IsConstructor()) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "'this' arg must be initialized";
    return NULL;
  }
  if (!actual_arg_type.IsZero()) {
    mirror::Class* klass = res_method->GetDeclaringClass();
    const RegType& res_method_class =
        reg_types_.FromClass(ClassHelper(klass).GetDescriptor(), klass,
                             klass->CannotBeAssignedFromOtherTypes());
    if (!res_method_class.IsAssignableFrom(actual_arg_type)) {
      Fail(actual_arg_type.IsUnresolvedTypes() ? VERIFY_ERROR_NO_CLASS :
          VERIFY_ERROR_BAD_CLASS_SOFT) << "'this' argument '" << actual_arg_type
          << "' not instance of '" << res_method_class << "'";
      return NULL;
    }
  }
  /*
   * Process the target method's signature. This signature may or may not
   * have been verified, so we can't assume it's properly formed.
   */
  MethodHelper mh(res_method);
  const DexFile::TypeList* params = mh.GetParameterTypeList();
  size_t params_size = params == NULL ? 0 : params->Size();
  uint32_t arg[5];
  if (!is_range) {
    inst->GetArgs(arg);
  }
  size_t actual_args = 1;
  for (size_t param_index = 0; param_index < params_size; param_index++) {
    if (actual_args >= expected_args) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Rejecting invalid call to '" << PrettyMethod(res_method)
                                        << "'. Expected " << expected_args
                                         << " arguments, processing argument " << actual_args
                                        << " (where longs/doubles count twice).";
      return NULL;
    }
    const char* descriptor =
        mh.GetTypeDescriptorFromTypeIdx(params->GetTypeItem(param_index).type_idx_);
    if (descriptor == NULL) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Rejecting invocation of " << PrettyMethod(res_method)
                                        << " missing signature component";
      return NULL;
    }
    const RegType& reg_type = reg_types_.FromDescriptor(class_loader_, descriptor, false);
    uint32_t get_reg = is_range ? inst->VRegC_3rc() + actual_args : arg[actual_args];
    if (!work_line_->VerifyRegisterType(get_reg, reg_type)) {
      return res_method;
    }
    actual_args = reg_type.IsLongOrDoubleTypes() ? actual_args + 2 : actual_args + 1;
  }
  if (actual_args != expected_args) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Rejecting invocation of " << PrettyMethod(res_method)
              << " expected " << expected_args << " arguments, found " << actual_args;
    return NULL;
  } else {
    return res_method;
  }
}

void MethodVerifier::VerifyNewArray(const Instruction* inst, bool is_filled, bool is_range) {
  uint32_t type_idx;
  if (!is_filled) {
    DCHECK_EQ(inst->Opcode(), Instruction::NEW_ARRAY);
    type_idx = inst->VRegC_22c();
  } else if (!is_range) {
    DCHECK_EQ(inst->Opcode(), Instruction::FILLED_NEW_ARRAY);
    type_idx = inst->VRegB_35c();
  } else {
    DCHECK_EQ(inst->Opcode(), Instruction::FILLED_NEW_ARRAY_RANGE);
    type_idx = inst->VRegB_3rc();
  }
  const RegType& res_type = ResolveClassAndCheckAccess(type_idx);
  if (res_type.IsConflict()) {  // bad class
    DCHECK_NE(failures_.size(), 0U);
  } else {
    // TODO: check Compiler::CanAccessTypeWithoutChecks returns false when res_type is unresolved
    if (!res_type.IsArrayTypes()) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "new-array on non-array class " << res_type;
    } else if (!is_filled) {
      /* make sure "size" register is valid type */
      work_line_->VerifyRegisterType(inst->VRegB_22c(), reg_types_.Integer());
      /* set register type to array class */
      const RegType& precise_type = reg_types_.FromUninitialized(res_type);
      work_line_->SetRegisterType(inst->VRegA_22c(), precise_type);
    } else {
      // Verify each register. If "arg_count" is bad, VerifyRegisterType() will run off the end of
      // the list and fail. It's legal, if silly, for arg_count to be zero.
      const RegType& expected_type = reg_types_.GetComponentType(res_type, class_loader_);
      uint32_t arg_count = (is_range) ? inst->VRegA_3rc() : inst->VRegA_35c();
      uint32_t arg[5];
      if (!is_range) {
        inst->GetArgs(arg);
      }
      for (size_t ui = 0; ui < arg_count; ui++) {
        uint32_t get_reg = is_range ? inst->VRegC_3rc() + ui : arg[ui];
        if (!work_line_->VerifyRegisterType(get_reg, expected_type)) {
          work_line_->SetResultRegisterType(reg_types_.Conflict());
          return;
        }
      }
      // filled-array result goes into "result" register
      const RegType& precise_type = reg_types_.FromUninitialized(res_type);
      work_line_->SetResultRegisterType(precise_type);
    }
  }
}

void MethodVerifier::VerifyAGet(const Instruction* inst,
                                const RegType& insn_type, bool is_primitive) {
  const RegType& index_type = work_line_->GetRegisterType(inst->VRegC_23x());
  if (!index_type.IsArrayIndexTypes()) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Invalid reg type for array index (" << index_type << ")";
  } else {
    const RegType& array_type = work_line_->GetRegisterType(inst->VRegB_23x());
    if (array_type.IsZero()) {
      // Null array class; this code path will fail at runtime. Infer a merge-able type from the
      // instruction type. TODO: have a proper notion of bottom here.
      if (!is_primitive || insn_type.IsCategory1Types()) {
        // Reference or category 1
        work_line_->SetRegisterType(inst->VRegA_23x(), reg_types_.Zero());
      } else {
        // Category 2
        work_line_->SetRegisterTypeWide(inst->VRegA_23x(), reg_types_.FromCat2ConstLo(0, false),
                                        reg_types_.FromCat2ConstHi(0, false));
      }
    } else if (!array_type.IsArrayTypes()) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "not array type " << array_type << " with aget";
    } else {
      /* verify the class */
      const RegType& component_type = reg_types_.GetComponentType(array_type, class_loader_);
      if (!component_type.IsReferenceTypes() && !is_primitive) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "primitive array type " << array_type
            << " source for aget-object";
      } else if (component_type.IsNonZeroReferenceTypes() && is_primitive) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "reference array type " << array_type
            << " source for category 1 aget";
      } else if (is_primitive && !insn_type.Equals(component_type) &&
                 !((insn_type.IsInteger() && component_type.IsFloat()) ||
                 (insn_type.IsLong() && component_type.IsDouble()))) {
        Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "array type " << array_type
            << " incompatible with aget of type " << insn_type;
      } else {
        // Use knowledge of the field type which is stronger than the type inferred from the
        // instruction, which can't differentiate object types and ints from floats, longs from
        // doubles.
        if (!component_type.IsLowHalf()) {
          work_line_->SetRegisterType(inst->VRegA_23x(), component_type);
        } else {
          work_line_->SetRegisterTypeWide(inst->VRegA_23x(), component_type,
                                          component_type.HighHalf(&reg_types_));
        }
      }
    }
  }
}

void MethodVerifier::VerifyPrimitivePut(const RegType& target_type, const RegType& insn_type,
                                        const uint32_t vregA) {
  // Primitive assignability rules are weaker than regular assignability rules.
  bool instruction_compatible;
  bool value_compatible;
  const RegType& value_type = work_line_->GetRegisterType(vregA);
  if (target_type.IsIntegralTypes()) {
    instruction_compatible = target_type.Equals(insn_type);
    value_compatible = value_type.IsIntegralTypes();
  } else if (target_type.IsFloat()) {
    instruction_compatible = insn_type.IsInteger();  // no put-float, so expect put-int
    value_compatible = value_type.IsFloatTypes();
  } else if (target_type.IsLong()) {
    instruction_compatible = insn_type.IsLong();
    value_compatible = value_type.IsLongTypes();
  } else if (target_type.IsDouble()) {
    instruction_compatible = insn_type.IsLong();  // no put-double, so expect put-long
    value_compatible = value_type.IsDoubleTypes();
  } else {
    instruction_compatible = false;  // reference with primitive store
    value_compatible = false;  // unused
  }
  if (!instruction_compatible) {
    // This is a global failure rather than a class change failure as the instructions and
    // the descriptors for the type should have been consistent within the same file at
    // compile time.
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "put insn has type '" << insn_type
        << "' but expected type '" << target_type << "'";
    return;
  }
  if (!value_compatible) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "unexpected value in v" << vregA
        << " of type " << value_type << " but expected " << target_type << " for put";
    return;
  }
}

void MethodVerifier::VerifyAPut(const Instruction* inst,
                             const RegType& insn_type, bool is_primitive) {
  const RegType& index_type = work_line_->GetRegisterType(inst->VRegC_23x());
  if (!index_type.IsArrayIndexTypes()) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Invalid reg type for array index (" << index_type << ")";
  } else {
    const RegType& array_type = work_line_->GetRegisterType(inst->VRegB_23x());
    if (array_type.IsZero()) {
      // Null array type; this code path will fail at runtime. Infer a merge-able type from the
      // instruction type.
    } else if (!array_type.IsArrayTypes()) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "not array type " << array_type << " with aput";
    } else {
      const RegType& component_type = reg_types_.GetComponentType(array_type, class_loader_);
      const uint32_t vregA = inst->VRegA_23x();
      if (is_primitive) {
        VerifyPrimitivePut(component_type, insn_type, vregA);
      } else {
        if (!component_type.IsReferenceTypes()) {
          Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "primitive array type " << array_type
              << " source for aput-object";
        } else {
          // The instruction agrees with the type of array, confirm the value to be stored does too
          // Note: we use the instruction type (rather than the component type) for aput-object as
          // incompatible classes will be caught at runtime as an array store exception
          work_line_->VerifyRegisterType(vregA, insn_type);
        }
      }
    }
  }
}

mirror::ArtField* MethodVerifier::GetStaticField(int field_idx) {
  const DexFile::FieldId& field_id = dex_file_->GetFieldId(field_idx);
  // Check access to class
  const RegType& klass_type = ResolveClassAndCheckAccess(field_id.class_idx_);
  if (klass_type.IsConflict()) {  // bad class
    AppendToLastFailMessage(StringPrintf(" in attempt to access static field %d (%s) in %s",
                                         field_idx, dex_file_->GetFieldName(field_id),
                                         dex_file_->GetFieldDeclaringClassDescriptor(field_id)));
    return NULL;
  }
  if (klass_type.IsUnresolvedTypes()) {
    return NULL;  // Can't resolve Class so no more to do here, will do checking at runtime.
  }
  mirror::ArtField* field = Runtime::Current()->GetClassLinker()->ResolveFieldJLS(*dex_file_,
                                                                               field_idx,
                                                                               dex_cache_,
                                                                               class_loader_);
  if (field == NULL) {
    VLOG(verifier) << "Unable to resolve static field " << field_idx << " ("
              << dex_file_->GetFieldName(field_id) << ") in "
              << dex_file_->GetFieldDeclaringClassDescriptor(field_id);
    DCHECK(Thread::Current()->IsExceptionPending());
    Thread::Current()->ClearException();
    return NULL;
  } else if (!GetDeclaringClass().CanAccessMember(field->GetDeclaringClass(),
                                                  field->GetAccessFlags())) {
    Fail(VERIFY_ERROR_ACCESS_FIELD) << "cannot access static field " << PrettyField(field)
                                    << " from " << GetDeclaringClass();
    return NULL;
  } else if (!field->IsStatic()) {
    Fail(VERIFY_ERROR_CLASS_CHANGE) << "expected field " << PrettyField(field) << " to be static";
    return NULL;
  } else {
    return field;
  }
}

mirror::ArtField* MethodVerifier::GetInstanceField(const RegType& obj_type, int field_idx) {
  const DexFile::FieldId& field_id = dex_file_->GetFieldId(field_idx);
  // Check access to class
  const RegType& klass_type = ResolveClassAndCheckAccess(field_id.class_idx_);
  if (klass_type.IsConflict()) {
    AppendToLastFailMessage(StringPrintf(" in attempt to access instance field %d (%s) in %s",
                                         field_idx, dex_file_->GetFieldName(field_id),
                                         dex_file_->GetFieldDeclaringClassDescriptor(field_id)));
    return NULL;
  }
  if (klass_type.IsUnresolvedTypes()) {
    return NULL;  // Can't resolve Class so no more to do here
  }
  mirror::ArtField* field = Runtime::Current()->GetClassLinker()->ResolveFieldJLS(*dex_file_,
                                                                               field_idx,
                                                                               dex_cache_,
                                                                               class_loader_);
  if (field == NULL) {
    VLOG(verifier) << "Unable to resolve instance field " << field_idx << " ("
              << dex_file_->GetFieldName(field_id) << ") in "
              << dex_file_->GetFieldDeclaringClassDescriptor(field_id);
    DCHECK(Thread::Current()->IsExceptionPending());
    Thread::Current()->ClearException();
    return NULL;
  } else if (!GetDeclaringClass().CanAccessMember(field->GetDeclaringClass(),
                                                  field->GetAccessFlags())) {
    Fail(VERIFY_ERROR_ACCESS_FIELD) << "cannot access instance field " << PrettyField(field)
                                    << " from " << GetDeclaringClass();
    return NULL;
  } else if (field->IsStatic()) {
    Fail(VERIFY_ERROR_CLASS_CHANGE) << "expected field " << PrettyField(field)
                                    << " to not be static";
    return NULL;
  } else if (obj_type.IsZero()) {
    // Cannot infer and check type, however, access will cause null pointer exception
    return field;
  } else {
    mirror::Class* klass = field->GetDeclaringClass();
    const RegType& field_klass =
        reg_types_.FromClass(dex_file_->GetFieldDeclaringClassDescriptor(field_id),
                             klass, klass->CannotBeAssignedFromOtherTypes());
    if (obj_type.IsUninitializedTypes() &&
        (!IsConstructor() || GetDeclaringClass().Equals(obj_type) ||
            !field_klass.Equals(GetDeclaringClass()))) {
      // Field accesses through uninitialized references are only allowable for constructors where
      // the field is declared in this class
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "cannot access instance field " << PrettyField(field)
                                        << " of a not fully initialized object within the context"
                                        << " of " << PrettyMethod(dex_method_idx_, *dex_file_);
      return NULL;
    } else if (!field_klass.IsAssignableFrom(obj_type)) {
      // Trying to access C1.field1 using reference of type C2, which is neither C1 or a sub-class
      // of C1. For resolution to occur the declared class of the field must be compatible with
      // obj_type, we've discovered this wasn't so, so report the field didn't exist.
      Fail(VERIFY_ERROR_NO_FIELD) << "cannot access instance field " << PrettyField(field)
                                  << " from object of type " << obj_type;
      return NULL;
    } else {
      return field;
    }
  }
}

void MethodVerifier::VerifyISGet(const Instruction* inst, const RegType& insn_type,
                                 bool is_primitive, bool is_static) {
  uint32_t field_idx = is_static ? inst->VRegB_21c() : inst->VRegC_22c();
  mirror::ArtField* field;
  if (is_static) {
    field = GetStaticField(field_idx);
  } else {
    const RegType& object_type = work_line_->GetRegisterType(inst->VRegB_22c());
    field = GetInstanceField(object_type, field_idx);
  }
  const char* descriptor;
  mirror::ClassLoader* loader;
  if (field != NULL) {
    descriptor = FieldHelper(field).GetTypeDescriptor();
    loader = field->GetDeclaringClass()->GetClassLoader();
  } else {
    const DexFile::FieldId& field_id = dex_file_->GetFieldId(field_idx);
    descriptor = dex_file_->GetFieldTypeDescriptor(field_id);
    loader = class_loader_;
  }
  const RegType& field_type = reg_types_.FromDescriptor(loader, descriptor, false);
  const uint32_t vregA = (is_static) ? inst->VRegA_21c() : inst->VRegA_22c();
  if (is_primitive) {
    if (field_type.Equals(insn_type) ||
        (field_type.IsFloat() && insn_type.IsInteger()) ||
        (field_type.IsDouble() && insn_type.IsLong())) {
      // expected that read is of the correct primitive type or that int reads are reading
      // floats or long reads are reading doubles
    } else {
      // This is a global failure rather than a class change failure as the instructions and
      // the descriptors for the type should have been consistent within the same file at
      // compile time
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "expected field " << PrettyField(field)
                                        << " to be of type '" << insn_type
                                        << "' but found type '" << field_type << "' in get";
      return;
    }
  } else {
    if (!insn_type.IsAssignableFrom(field_type)) {
      Fail(VERIFY_ERROR_BAD_CLASS_SOFT) << "expected field " << PrettyField(field)
                                        << " to be compatible with type '" << insn_type
                                        << "' but found type '" << field_type
                                        << "' in get-object";
      work_line_->SetRegisterType(vregA, reg_types_.Conflict());
      return;
    }
  }
  if (!field_type.IsLowHalf()) {
    work_line_->SetRegisterType(vregA, field_type);
  } else {
    work_line_->SetRegisterTypeWide(vregA, field_type, field_type.HighHalf(&reg_types_));
  }
}

void MethodVerifier::VerifyISPut(const Instruction* inst, const RegType& insn_type,
                                 bool is_primitive, bool is_static) {
  uint32_t field_idx = is_static ? inst->VRegB_21c() : inst->VRegC_22c();
  mirror::ArtField* field;
  if (is_static) {
    field = GetStaticField(field_idx);
  } else {
    const RegType& object_type = work_line_->GetRegisterType(inst->VRegB_22c());
    field = GetInstanceField(object_type, field_idx);
  }
  const char* descriptor;
  mirror::ClassLoader* loader;
  if (field != NULL) {
    descriptor = FieldHelper(field).GetTypeDescriptor();
    loader = field->GetDeclaringClass()->GetClassLoader();
  } else {
    const DexFile::FieldId& field_id = dex_file_->GetFieldId(field_idx);
    descriptor = dex_file_->GetFieldTypeDescriptor(field_id);
    loader = class_loader_;
  }
  const RegType& field_type = reg_types_.FromDescriptor(loader, descriptor, false);
  if (field != NULL) {
    if (field->IsFinal() && field->GetDeclaringClass() != GetDeclaringClass().GetClass()) {
      Fail(VERIFY_ERROR_ACCESS_FIELD) << "cannot modify final field " << PrettyField(field)
                                      << " from other class " << GetDeclaringClass();
      return;
    }
  }
  const uint32_t vregA = (is_static) ? inst->VRegA_21c() : inst->VRegA_22c();
  if (is_primitive) {
    VerifyPrimitivePut(field_type, insn_type, vregA);
  } else {
    if (!insn_type.IsAssignableFrom(field_type)) {
      Fail(VERIFY_ERROR_BAD_CLASS_SOFT) << "expected field " << PrettyField(field)
                                        << " to be compatible with type '" << insn_type
                                        << "' but found type '" << field_type
                                        << "' in put-object";
      return;
    }
    work_line_->VerifyRegisterType(vregA, field_type);
  }
}

// Look for an instance field with this offset.
// TODO: we may speed up the search if offsets are sorted by doing a quick search.
static mirror::ArtField* FindInstanceFieldWithOffset(const mirror::Class* klass,
                                                  uint32_t field_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  const mirror::ObjectArray<mirror::ArtField>* instance_fields = klass->GetIFields();
  if (instance_fields != NULL) {
    for (int32_t i = 0, e = instance_fields->GetLength(); i < e; ++i) {
      mirror::ArtField* field = instance_fields->Get(i);
      if (field->GetOffset().Uint32Value() == field_offset) {
        return field;
      }
    }
  }
  // We did not find field in class: look into superclass.
  if (klass->GetSuperClass() != NULL) {
    return FindInstanceFieldWithOffset(klass->GetSuperClass(), field_offset);
  } else {
    return NULL;
  }
}

// Returns the access field of a quick field access (iget/iput-quick) or NULL
// if it cannot be found.
mirror::ArtField* MethodVerifier::GetQuickFieldAccess(const Instruction* inst,
                                                   RegisterLine* reg_line) {
  DCHECK(inst->Opcode() == Instruction::IGET_QUICK ||
         inst->Opcode() == Instruction::IGET_WIDE_QUICK ||
         inst->Opcode() == Instruction::IGET_OBJECT_QUICK ||
         inst->Opcode() == Instruction::IPUT_QUICK ||
         inst->Opcode() == Instruction::IPUT_WIDE_QUICK ||
         inst->Opcode() == Instruction::IPUT_OBJECT_QUICK);
  const RegType& object_type = reg_line->GetRegisterType(inst->VRegB_22c());
  mirror::Class* object_class = NULL;
  if (!object_type.IsUnresolvedTypes()) {
    object_class = object_type.GetClass();
  } else {
    // We need to resolve the class from its descriptor.
    const std::string& descriptor(object_type.GetDescriptor());
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    object_class = class_linker->FindClass(descriptor.c_str(), class_loader_);
    if (object_class == NULL) {
      Thread::Current()->ClearException();
      // Look for a system class
      object_class = class_linker->FindClass(descriptor.c_str(), NULL);
    }
  }
  if (object_class == NULL) {
    // Failed to get the Class* from reg type.
    LOG(WARNING) << "Failed to get Class* from " << object_type;
    return NULL;
  }
  uint32_t field_offset = static_cast<uint32_t>(inst->VRegC_22c());
  return FindInstanceFieldWithOffset(object_class, field_offset);
}

void MethodVerifier::VerifyIGetQuick(const Instruction* inst, const RegType& insn_type,
                                     bool is_primitive) {
  DCHECK(Runtime::Current()->IsStarted());
  mirror::ArtField* field = GetQuickFieldAccess(inst, work_line_.get());
  if (field == NULL) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Cannot infer field from " << inst->Name();
    return;
  }
  const char* descriptor = FieldHelper(field).GetTypeDescriptor();
  mirror::ClassLoader* loader = field->GetDeclaringClass()->GetClassLoader();
  const RegType& field_type = reg_types_.FromDescriptor(loader, descriptor, false);
  const uint32_t vregA = inst->VRegA_22c();
  if (is_primitive) {
    if (field_type.Equals(insn_type) ||
        (field_type.IsFloat() && insn_type.IsIntegralTypes()) ||
        (field_type.IsDouble() && insn_type.IsLongTypes())) {
      // expected that read is of the correct primitive type or that int reads are reading
      // floats or long reads are reading doubles
    } else {
      // This is a global failure rather than a class change failure as the instructions and
      // the descriptors for the type should have been consistent within the same file at
      // compile time
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "expected field " << PrettyField(field)
                                        << " to be of type '" << insn_type
                                        << "' but found type '" << field_type << "' in get";
      return;
    }
  } else {
    if (!insn_type.IsAssignableFrom(field_type)) {
      Fail(VERIFY_ERROR_BAD_CLASS_SOFT) << "expected field " << PrettyField(field)
                                        << " to be compatible with type '" << insn_type
                                        << "' but found type '" << field_type
                                        << "' in get-object";
      work_line_->SetRegisterType(vregA, reg_types_.Conflict());
      return;
    }
  }
  if (!field_type.IsLowHalf()) {
    work_line_->SetRegisterType(vregA, field_type);
  } else {
    work_line_->SetRegisterTypeWide(vregA, field_type, field_type.HighHalf(&reg_types_));
  }
}

void MethodVerifier::VerifyIPutQuick(const Instruction* inst, const RegType& insn_type,
                                     bool is_primitive) {
  DCHECK(Runtime::Current()->IsStarted());
  mirror::ArtField* field = GetQuickFieldAccess(inst, work_line_.get());
  if (field == NULL) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Cannot infer field from " << inst->Name();
    return;
  }
  const char* descriptor = FieldHelper(field).GetTypeDescriptor();
  mirror::ClassLoader* loader = field->GetDeclaringClass()->GetClassLoader();
  const RegType& field_type = reg_types_.FromDescriptor(loader, descriptor, false);
  if (field != NULL) {
    if (field->IsFinal() && field->GetDeclaringClass() != GetDeclaringClass().GetClass()) {
      Fail(VERIFY_ERROR_ACCESS_FIELD) << "cannot modify final field " << PrettyField(field)
                                      << " from other class " << GetDeclaringClass();
      return;
    }
  }
  const uint32_t vregA = inst->VRegA_22c();
  if (is_primitive) {
    // Primitive field assignability rules are weaker than regular assignability rules
    bool instruction_compatible;
    bool value_compatible;
    const RegType& value_type = work_line_->GetRegisterType(vregA);
    if (field_type.IsIntegralTypes()) {
      instruction_compatible = insn_type.IsIntegralTypes();
      value_compatible = value_type.IsIntegralTypes();
    } else if (field_type.IsFloat()) {
      instruction_compatible = insn_type.IsInteger();  // no [is]put-float, so expect [is]put-int
      value_compatible = value_type.IsFloatTypes();
    } else if (field_type.IsLong()) {
      instruction_compatible = insn_type.IsLong();
      value_compatible = value_type.IsLongTypes();
    } else if (field_type.IsDouble()) {
      instruction_compatible = insn_type.IsLong();  // no [is]put-double, so expect [is]put-long
      value_compatible = value_type.IsDoubleTypes();
    } else {
      instruction_compatible = false;  // reference field with primitive store
      value_compatible = false;  // unused
    }
    if (!instruction_compatible) {
      // This is a global failure rather than a class change failure as the instructions and
      // the descriptors for the type should have been consistent within the same file at
      // compile time
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "expected field " << PrettyField(field)
                                        << " to be of type '" << insn_type
                                        << "' but found type '" << field_type
                                        << "' in put";
      return;
    }
    if (!value_compatible) {
      Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "unexpected value in v" << vregA
          << " of type " << value_type
          << " but expected " << field_type
          << " for store to " << PrettyField(field) << " in put";
      return;
    }
  } else {
    if (!insn_type.IsAssignableFrom(field_type)) {
      Fail(VERIFY_ERROR_BAD_CLASS_SOFT) << "expected field " << PrettyField(field)
                                        << " to be compatible with type '" << insn_type
                                        << "' but found type '" << field_type
                                        << "' in put-object";
      return;
    }
    work_line_->VerifyRegisterType(vregA, field_type);
  }
}

bool MethodVerifier::CheckNotMoveException(const uint16_t* insns, int insn_idx) {
  if ((insns[insn_idx] & 0xff) == Instruction::MOVE_EXCEPTION) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "invalid use of move-exception";
    return false;
  }
  return true;
}

bool MethodVerifier::UpdateRegisters(uint32_t next_insn, const RegisterLine* merge_line) {
  bool changed = true;
  RegisterLine* target_line = reg_table_.GetLine(next_insn);
  if (!insn_flags_[next_insn].IsVisitedOrChanged()) {
    /*
     * We haven't processed this instruction before, and we haven't touched the registers here, so
     * there's nothing to "merge". Copy the registers over and mark it as changed. (This is the
     * only way a register can transition out of "unknown", so this is not just an optimization.)
     */
    if (!insn_flags_[next_insn].IsReturn()) {
      target_line->CopyFromLine(merge_line);
    } else {
      // Verify that the monitor stack is empty on return.
      if (!merge_line->VerifyMonitorStackEmpty()) {
        return false;
      }
      // For returns we only care about the operand to the return, all other registers are dead.
      // Initialize them as conflicts so they don't add to GC and deoptimization information.
      const Instruction* ret_inst = Instruction::At(code_item_->insns_ + next_insn);
      Instruction::Code opcode = ret_inst->Opcode();
      if ((opcode == Instruction::RETURN_VOID) || (opcode == Instruction::RETURN_VOID_BARRIER)) {
        target_line->MarkAllRegistersAsConflicts();
      } else {
        target_line->CopyFromLine(merge_line);
        if (opcode == Instruction::RETURN_WIDE) {
          target_line->MarkAllRegistersAsConflictsExceptWide(ret_inst->VRegA_11x());
        } else {
          target_line->MarkAllRegistersAsConflictsExcept(ret_inst->VRegA_11x());
        }
      }
    }
  } else {
    UniquePtr<RegisterLine> copy(gDebugVerify ?
                                 new RegisterLine(target_line->NumRegs(), this) :
                                 NULL);
    if (gDebugVerify) {
      copy->CopyFromLine(target_line);
    }
    changed = target_line->MergeRegisters(merge_line);
    if (have_pending_hard_failure_) {
      return false;
    }
    if (gDebugVerify && changed) {
      LogVerifyInfo() << "Merging at [" << reinterpret_cast<void*>(work_insn_idx_) << "]"
                      << " to [" << reinterpret_cast<void*>(next_insn) << "]: " << "\n"
                      << *copy.get() << "  MERGE\n"
                      << *merge_line << "  ==\n"
                      << *target_line << "\n";
    }
  }
  if (changed) {
    insn_flags_[next_insn].SetChanged();
  }
  return true;
}

InstructionFlags* MethodVerifier::CurrentInsnFlags() {
  return &insn_flags_[work_insn_idx_];
}

const RegType& MethodVerifier::GetMethodReturnType() {
  const DexFile::MethodId& method_id = dex_file_->GetMethodId(dex_method_idx_);
  const DexFile::ProtoId& proto_id = dex_file_->GetMethodPrototype(method_id);
  uint16_t return_type_idx = proto_id.return_type_idx_;
  const char* descriptor = dex_file_->GetTypeDescriptor(dex_file_->GetTypeId(return_type_idx));
  return reg_types_.FromDescriptor(class_loader_, descriptor, false);
}

const RegType& MethodVerifier::GetDeclaringClass() {
  if (declaring_class_ == NULL) {
    const DexFile::MethodId& method_id = dex_file_->GetMethodId(dex_method_idx_);
    const char* descriptor
        = dex_file_->GetTypeDescriptor(dex_file_->GetTypeId(method_id.class_idx_));
    if (mirror_method_ != NULL) {
      mirror::Class* klass = mirror_method_->GetDeclaringClass();
      declaring_class_ = &reg_types_.FromClass(descriptor, klass,
                                               klass->CannotBeAssignedFromOtherTypes());
    } else {
      declaring_class_ = &reg_types_.FromDescriptor(class_loader_, descriptor, false);
    }
  }
  return *declaring_class_;
}

void MethodVerifier::ComputeGcMapSizes(size_t* gc_points, size_t* ref_bitmap_bits,
                                       size_t* log2_max_gc_pc) {
  size_t local_gc_points = 0;
  size_t max_insn = 0;
  size_t max_ref_reg = -1;
  for (size_t i = 0; i < code_item_->insns_size_in_code_units_; i++) {
    if (insn_flags_[i].IsCompileTimeInfoPoint()) {
      local_gc_points++;
      max_insn = i;
      RegisterLine* line = reg_table_.GetLine(i);
      max_ref_reg = line->GetMaxNonZeroReferenceReg(max_ref_reg);
    }
  }
  *gc_points = local_gc_points;
  *ref_bitmap_bits = max_ref_reg + 1;  // if max register is 0 we need 1 bit to encode (ie +1)
  size_t i = 0;
  while ((1U << i) <= max_insn) {
    i++;
  }
  *log2_max_gc_pc = i;
}

MethodVerifier::MethodSafeCastSet* MethodVerifier::GenerateSafeCastSet() {
  /*
   * Walks over the method code and adds any cast instructions in which
   * the type cast is implicit to a set, which is used in the code generation
   * to elide these casts.
   */
  if (!failure_messages_.empty()) {
    return NULL;
  }
  UniquePtr<MethodSafeCastSet> mscs;
  const Instruction* inst = Instruction::At(code_item_->insns_);
  const Instruction* end = Instruction::At(code_item_->insns_ +
                                           code_item_->insns_size_in_code_units_);

  for (; inst < end; inst = inst->Next()) {
    if (Instruction::CHECK_CAST != inst->Opcode()) {
      continue;
    }
    uint32_t dex_pc = inst->GetDexPc(code_item_->insns_);
    RegisterLine* line = reg_table_.GetLine(dex_pc);
    const RegType& reg_type(line->GetRegisterType(inst->VRegA_21c()));
    const RegType& cast_type = ResolveClassAndCheckAccess(inst->VRegB_21c());
    if (cast_type.IsStrictlyAssignableFrom(reg_type)) {
      if (mscs.get() == NULL) {
        mscs.reset(new MethodSafeCastSet());
      }
      mscs->insert(dex_pc);
    }
  }
  return mscs.release();
}

MethodVerifier::PcToConcreteMethodMap* MethodVerifier::GenerateDevirtMap() {
  // It is risky to rely on reg_types for sharpening in cases of soft
  // verification, we might end up sharpening to a wrong implementation. Just abort.
  if (!failure_messages_.empty()) {
    return NULL;
  }

  UniquePtr<PcToConcreteMethodMap> pc_to_concrete_method_map;
  const uint16_t* insns = code_item_->insns_;
  const Instruction* inst = Instruction::At(insns);
  const Instruction* end = Instruction::At(insns + code_item_->insns_size_in_code_units_);

  for (; inst < end; inst = inst->Next()) {
    bool is_virtual   = (inst->Opcode() == Instruction::INVOKE_VIRTUAL) ||
        (inst->Opcode() ==  Instruction::INVOKE_VIRTUAL_RANGE);
    bool is_interface = (inst->Opcode() == Instruction::INVOKE_INTERFACE) ||
        (inst->Opcode() == Instruction::INVOKE_INTERFACE_RANGE);

    if (!is_interface && !is_virtual) {
      continue;
    }
    // Get reg type for register holding the reference to the object that will be dispatched upon.
    uint32_t dex_pc = inst->GetDexPc(insns);
    RegisterLine* line = reg_table_.GetLine(dex_pc);
    bool is_range = (inst->Opcode() ==  Instruction::INVOKE_VIRTUAL_RANGE) ||
        (inst->Opcode() ==  Instruction::INVOKE_INTERFACE_RANGE);
    const RegType&
        reg_type(line->GetRegisterType(is_range ? inst->VRegC_3rc() : inst->VRegC_35c()));

    if (!reg_type.HasClass()) {
      // We will compute devirtualization information only when we know the Class of the reg type.
      continue;
    }
    mirror::Class* reg_class = reg_type.GetClass();
    if (reg_class->IsInterface()) {
      // We can't devirtualize when the known type of the register is an interface.
      continue;
    }
    if (reg_class->IsAbstract() && !reg_class->IsArrayClass()) {
      // We can't devirtualize abstract classes except on arrays of abstract classes.
      continue;
    }
    mirror::ArtMethod* abstract_method =
        dex_cache_->GetResolvedMethod(is_range ? inst->VRegB_3rc() : inst->VRegB_35c());
    if (abstract_method == NULL) {
      // If the method is not found in the cache this means that it was never found
      // by ResolveMethodAndCheckAccess() called when verifying invoke_*.
      continue;
    }
    // Find the concrete method.
    mirror::ArtMethod* concrete_method = NULL;
    if (is_interface) {
      concrete_method = reg_type.GetClass()->FindVirtualMethodForInterface(abstract_method);
    }
    if (is_virtual) {
      concrete_method = reg_type.GetClass()->FindVirtualMethodForVirtual(abstract_method);
    }
    if (concrete_method == NULL || concrete_method->IsAbstract()) {
      // In cases where concrete_method is not found, or is abstract, continue to the next invoke.
      continue;
    }
    if (reg_type.IsPreciseReference() || concrete_method->IsFinal() ||
        concrete_method->GetDeclaringClass()->IsFinal()) {
      // If we knew exactly the class being dispatched upon, or if the target method cannot be
      // overridden record the target to be used in the compiler driver.
      if (pc_to_concrete_method_map.get() == NULL) {
        pc_to_concrete_method_map.reset(new PcToConcreteMethodMap());
      }
      MethodReference concrete_ref(
          concrete_method->GetDeclaringClass()->GetDexCache()->GetDexFile(),
          concrete_method->GetDexMethodIndex());
      pc_to_concrete_method_map->Put(dex_pc, concrete_ref);
    }
  }
  return pc_to_concrete_method_map.release();
}

const std::vector<uint8_t>* MethodVerifier::GenerateGcMap() {
  size_t num_entries, ref_bitmap_bits, pc_bits;
  ComputeGcMapSizes(&num_entries, &ref_bitmap_bits, &pc_bits);
  // There's a single byte to encode the size of each bitmap
  if (ref_bitmap_bits >= (8 /* bits per byte */ * 8192 /* 13-bit size */ )) {
    // TODO: either a better GC map format or per method failures
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Cannot encode GC map for method with "
       << ref_bitmap_bits << " registers";
    return NULL;
  }
  size_t ref_bitmap_bytes = (ref_bitmap_bits + 7) / 8;
  // There are 2 bytes to encode the number of entries
  if (num_entries >= 65536) {
    // TODO: either a better GC map format or per method failures
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Cannot encode GC map for method with "
       << num_entries << " entries";
    return NULL;
  }
  size_t pc_bytes;
  RegisterMapFormat format;
  if (pc_bits <= 8) {
    format = kRegMapFormatCompact8;
    pc_bytes = 1;
  } else if (pc_bits <= 16) {
    format = kRegMapFormatCompact16;
    pc_bytes = 2;
  } else {
    // TODO: either a better GC map format or per method failures
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Cannot encode GC map for method with "
       << (1 << pc_bits) << " instructions (number is rounded up to nearest power of 2)";
    return NULL;
  }
  size_t table_size = ((pc_bytes + ref_bitmap_bytes) * num_entries) + 4;
  std::vector<uint8_t>* table = new std::vector<uint8_t>;
  if (table == NULL) {
    Fail(VERIFY_ERROR_BAD_CLASS_HARD) << "Failed to encode GC map (size=" << table_size << ")";
    return NULL;
  }
  table->reserve(table_size);
  // Write table header
  table->push_back(format | ((ref_bitmap_bytes >> DexPcToReferenceMap::kRegMapFormatShift) &
                             ~DexPcToReferenceMap::kRegMapFormatMask));
  table->push_back(ref_bitmap_bytes & 0xFF);
  table->push_back(num_entries & 0xFF);
  table->push_back((num_entries >> 8) & 0xFF);
  // Write table data
  for (size_t i = 0; i < code_item_->insns_size_in_code_units_; i++) {
    if (insn_flags_[i].IsCompileTimeInfoPoint()) {
      table->push_back(i & 0xFF);
      if (pc_bytes == 2) {
        table->push_back((i >> 8) & 0xFF);
      }
      RegisterLine* line = reg_table_.GetLine(i);
      line->WriteReferenceBitMap(*table, ref_bitmap_bytes);
    }
  }
  DCHECK_EQ(table->size(), table_size);
  return table;
}

void MethodVerifier::VerifyGcMap(const std::vector<uint8_t>& data) {
  // Check that for every GC point there is a map entry, there aren't entries for non-GC points,
  // that the table data is well formed and all references are marked (or not) in the bitmap
  DexPcToReferenceMap map(&data[0], data.size());
  size_t map_index = 0;
  for (size_t i = 0; i < code_item_->insns_size_in_code_units_; i++) {
    const uint8_t* reg_bitmap = map.FindBitMap(i, false);
    if (insn_flags_[i].IsCompileTimeInfoPoint()) {
      CHECK_LT(map_index, map.NumEntries());
      CHECK_EQ(map.GetDexPc(map_index), i);
      CHECK_EQ(map.GetBitMap(map_index), reg_bitmap);
      map_index++;
      RegisterLine* line = reg_table_.GetLine(i);
      for (size_t j = 0; j < code_item_->registers_size_; j++) {
        if (line->GetRegisterType(j).IsNonZeroReferenceTypes()) {
          CHECK_LT(j / 8, map.RegWidth());
          CHECK_EQ((reg_bitmap[j / 8] >> (j % 8)) & 1, 1);
        } else if ((j / 8) < map.RegWidth()) {
          CHECK_EQ((reg_bitmap[j / 8] >> (j % 8)) & 1, 0);
        } else {
          // If a register doesn't contain a reference then the bitmap may be shorter than the line
        }
      }
    } else {
      CHECK(reg_bitmap == NULL);
    }
  }
}

void MethodVerifier::SetDexGcMap(MethodReference ref, const std::vector<uint8_t>& gc_map) {
  DCHECK(Runtime::Current()->IsCompiler());
  {
    WriterMutexLock mu(Thread::Current(), *dex_gc_maps_lock_);
    DexGcMapTable::iterator it = dex_gc_maps_->find(ref);
    if (it != dex_gc_maps_->end()) {
      delete it->second;
      dex_gc_maps_->erase(it);
    }
    dex_gc_maps_->Put(ref, &gc_map);
  }
  DCHECK(GetDexGcMap(ref) != NULL);
}


void  MethodVerifier::SetSafeCastMap(MethodReference ref, const MethodSafeCastSet* cast_set) {
  DCHECK(Runtime::Current()->IsCompiler());
  WriterMutexLock mu(Thread::Current(), *safecast_map_lock_);
  SafeCastMap::iterator it = safecast_map_->find(ref);
  if (it != safecast_map_->end()) {
    delete it->second;
    safecast_map_->erase(it);
  }
  safecast_map_->Put(ref, cast_set);
  DCHECK(safecast_map_->find(ref) != safecast_map_->end());
}

bool MethodVerifier::IsSafeCast(MethodReference ref, uint32_t pc) {
  DCHECK(Runtime::Current()->IsCompiler());
  ReaderMutexLock mu(Thread::Current(), *safecast_map_lock_);
  SafeCastMap::const_iterator it = safecast_map_->find(ref);
  if (it == safecast_map_->end()) {
    return false;
  }

  // Look up the cast address in the set of safe casts
  MethodVerifier::MethodSafeCastSet::const_iterator cast_it = it->second->find(pc);
  return cast_it != it->second->end();
}

const std::vector<uint8_t>* MethodVerifier::GetDexGcMap(MethodReference ref) {
  DCHECK(Runtime::Current()->IsCompiler());
  ReaderMutexLock mu(Thread::Current(), *dex_gc_maps_lock_);
  DexGcMapTable::const_iterator it = dex_gc_maps_->find(ref);
  CHECK(it != dex_gc_maps_->end())
    << "Didn't find GC map for: " << PrettyMethod(ref.dex_method_index, *ref.dex_file);
  CHECK(it->second != NULL);
  return it->second;
}

void  MethodVerifier::SetDevirtMap(MethodReference ref,
                                   const PcToConcreteMethodMap* devirt_map) {
  DCHECK(Runtime::Current()->IsCompiler());
  WriterMutexLock mu(Thread::Current(), *devirt_maps_lock_);
  DevirtualizationMapTable::iterator it = devirt_maps_->find(ref);
  if (it != devirt_maps_->end()) {
    delete it->second;
    devirt_maps_->erase(it);
  }

  devirt_maps_->Put(ref, devirt_map);
  DCHECK(devirt_maps_->find(ref) != devirt_maps_->end());
}

const MethodReference* MethodVerifier::GetDevirtMap(const MethodReference& ref,
                                                                    uint32_t dex_pc) {
  DCHECK(Runtime::Current()->IsCompiler());
  ReaderMutexLock mu(Thread::Current(), *devirt_maps_lock_);
  DevirtualizationMapTable::const_iterator it = devirt_maps_->find(ref);
  if (it == devirt_maps_->end()) {
    return NULL;
  }

  // Look up the PC in the map, get the concrete method to execute and return its reference.
  MethodVerifier::PcToConcreteMethodMap::const_iterator pc_to_concrete_method
      = it->second->find(dex_pc);
  if (pc_to_concrete_method != it->second->end()) {
    return &(pc_to_concrete_method->second);
  } else {
    return NULL;
  }
}

std::vector<int32_t> MethodVerifier::DescribeVRegs(uint32_t dex_pc) {
  RegisterLine* line = reg_table_.GetLine(dex_pc);
  std::vector<int32_t> result;
  for (size_t i = 0; i < line->NumRegs(); ++i) {
    const RegType& type = line->GetRegisterType(i);
    if (type.IsConstant()) {
      result.push_back(type.IsPreciseConstant() ? kConstant : kImpreciseConstant);
      result.push_back(type.ConstantValue());
    } else if (type.IsConstantLo()) {
      result.push_back(type.IsPreciseConstantLo() ? kConstant : kImpreciseConstant);
      result.push_back(type.ConstantValueLo());
    } else if (type.IsConstantHi()) {
      result.push_back(type.IsPreciseConstantHi() ? kConstant : kImpreciseConstant);
      result.push_back(type.ConstantValueHi());
    } else if (type.IsIntegralTypes()) {
      result.push_back(kIntVReg);
      result.push_back(0);
    } else if (type.IsFloat()) {
      result.push_back(kFloatVReg);
      result.push_back(0);
    } else if (type.IsLong()) {
      result.push_back(kLongLoVReg);
      result.push_back(0);
      result.push_back(kLongHiVReg);
      result.push_back(0);
      ++i;
    } else if (type.IsDouble()) {
      result.push_back(kDoubleLoVReg);
      result.push_back(0);
      result.push_back(kDoubleHiVReg);
      result.push_back(0);
      ++i;
    } else if (type.IsUndefined() || type.IsConflict() || type.IsHighHalf()) {
      result.push_back(kUndefined);
      result.push_back(0);
    } else {
      CHECK(type.IsNonZeroReferenceTypes());
      result.push_back(kReferenceVReg);
      result.push_back(0);
    }
  }
  return result;
}

bool MethodVerifier::IsCandidateForCompilation(MethodReference& method_ref,
                                               const uint32_t access_flags) {
#ifdef ART_SEA_IR_MODE
    bool use_sea = Runtime::Current()->IsSeaIRMode();
    use_sea = use_sea && (std::string::npos != PrettyMethod(
                          method_ref.dex_method_index, *(method_ref.dex_file)).find("fibonacci"));
    if (use_sea) return true;
#endif
  // Don't compile class initializers, ever.
  if (((access_flags & kAccConstructor) != 0) && ((access_flags & kAccStatic) != 0)) {
    return false;
  }
  return (Runtime::Current()->GetCompilerFilter() != Runtime::kInterpretOnly);
}

ReaderWriterMutex* MethodVerifier::dex_gc_maps_lock_ = NULL;
MethodVerifier::DexGcMapTable* MethodVerifier::dex_gc_maps_ = NULL;

ReaderWriterMutex* MethodVerifier::safecast_map_lock_ = NULL;
MethodVerifier::SafeCastMap* MethodVerifier::safecast_map_ = NULL;

ReaderWriterMutex* MethodVerifier::devirt_maps_lock_ = NULL;
MethodVerifier::DevirtualizationMapTable* MethodVerifier::devirt_maps_ = NULL;

ReaderWriterMutex* MethodVerifier::rejected_classes_lock_ = NULL;
MethodVerifier::RejectedClassesTable* MethodVerifier::rejected_classes_ = NULL;

void MethodVerifier::Init() {
  if (Runtime::Current()->IsCompiler()) {
    dex_gc_maps_lock_ = new ReaderWriterMutex("verifier GC maps lock");
    Thread* self = Thread::Current();
    {
      WriterMutexLock mu(self, *dex_gc_maps_lock_);
      dex_gc_maps_ = new MethodVerifier::DexGcMapTable;
    }

    safecast_map_lock_ = new ReaderWriterMutex("verifier Cast Elision lock");
    {
      WriterMutexLock mu(self, *safecast_map_lock_);
      safecast_map_ = new MethodVerifier::SafeCastMap();
    }

    devirt_maps_lock_ = new ReaderWriterMutex("verifier Devirtualization lock");

    {
      WriterMutexLock mu(self, *devirt_maps_lock_);
      devirt_maps_ = new MethodVerifier::DevirtualizationMapTable();
    }

    rejected_classes_lock_ = new ReaderWriterMutex("verifier rejected classes lock");
    {
      WriterMutexLock mu(self, *rejected_classes_lock_);
      rejected_classes_ = new MethodVerifier::RejectedClassesTable;
    }
  }
  art::verifier::RegTypeCache::Init();
}

void MethodVerifier::Shutdown() {
  if (Runtime::Current()->IsCompiler()) {
    Thread* self = Thread::Current();
    {
      WriterMutexLock mu(self, *dex_gc_maps_lock_);
      STLDeleteValues(dex_gc_maps_);
      delete dex_gc_maps_;
      dex_gc_maps_ = NULL;
    }
    delete dex_gc_maps_lock_;
    dex_gc_maps_lock_ = NULL;

    {
      WriterMutexLock mu(self, *safecast_map_lock_);
      STLDeleteValues(safecast_map_);
      delete safecast_map_;
      safecast_map_ = NULL;
    }
    delete safecast_map_lock_;
    safecast_map_lock_ = NULL;

    {
      WriterMutexLock mu(self, *devirt_maps_lock_);
      STLDeleteValues(devirt_maps_);
      delete devirt_maps_;
      devirt_maps_ = NULL;
    }
    delete devirt_maps_lock_;
    devirt_maps_lock_ = NULL;

    {
      WriterMutexLock mu(self, *rejected_classes_lock_);
      delete rejected_classes_;
      rejected_classes_ = NULL;
    }
    delete rejected_classes_lock_;
    rejected_classes_lock_ = NULL;
  }
  verifier::RegTypeCache::ShutDown();
}

void MethodVerifier::AddRejectedClass(ClassReference ref) {
  DCHECK(Runtime::Current()->IsCompiler());
  {
    WriterMutexLock mu(Thread::Current(), *rejected_classes_lock_);
    rejected_classes_->insert(ref);
  }
  CHECK(IsClassRejected(ref));
}

bool MethodVerifier::IsClassRejected(ClassReference ref) {
  DCHECK(Runtime::Current()->IsCompiler());
  ReaderMutexLock mu(Thread::Current(), *rejected_classes_lock_);
  return (rejected_classes_->find(ref) != rejected_classes_->end());
}

}  // namespace verifier
}  // namespace art
