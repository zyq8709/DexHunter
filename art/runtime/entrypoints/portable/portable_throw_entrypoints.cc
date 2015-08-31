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

#include "dex_instruction.h"
#include "entrypoints/entrypoint_utils.h"
#include "mirror/art_method-inl.h"
#include "mirror/object-inl.h"

namespace art {

extern "C" void art_portable_throw_div_zero_from_code() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ThrowArithmeticExceptionDivideByZero();
}

extern "C" void art_portable_throw_array_bounds_from_code(int32_t index, int32_t length)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ThrowArrayIndexOutOfBoundsException(index, length);
}

extern "C" void art_portable_throw_no_such_method_from_code(int32_t method_idx)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ThrowNoSuchMethodError(method_idx);
}

extern "C" void art_portable_throw_null_pointer_exception_from_code(uint32_t dex_pc)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // TODO: remove dex_pc argument from caller.
  UNUSED(dex_pc);
  Thread* self = Thread::Current();
  ThrowLocation throw_location = self->GetCurrentLocationForThrow();
  ThrowNullPointerExceptionFromDexPC(throw_location);
}

extern "C" void art_portable_throw_stack_overflow_from_code() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ThrowStackOverflowError(Thread::Current());
}

extern "C" void art_portable_throw_exception_from_code(mirror::Throwable* exception)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Thread* self = Thread::Current();
  ThrowLocation throw_location = self->GetCurrentLocationForThrow();
  if (exception == NULL) {
    ThrowNullPointerException(NULL, "throw with null exception");
  } else {
    self->SetException(throw_location, exception);
  }
}

extern "C" void* art_portable_get_and_clear_exception(Thread* self)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(self->IsExceptionPending());
  // TODO: make this inline.
  mirror::Throwable* exception = self->GetException(NULL);
  self->ClearException();
  return exception;
}

extern "C" int32_t art_portable_find_catch_block_from_code(mirror::ArtMethod* current_method,
                                                           uint32_t ti_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Thread* self = Thread::Current();  // TODO: make an argument.
  ThrowLocation throw_location;
  mirror::Throwable* exception = self->GetException(&throw_location);
  // Check for special deoptimization exception.
  if (UNLIKELY(reinterpret_cast<int32_t>(exception) == -1)) {
    return -1;
  }
  mirror::Class* exception_type = exception->GetClass();
  MethodHelper mh(current_method);
  const DexFile::CodeItem* code_item = mh.GetCodeItem();
  DCHECK_LT(ti_offset, code_item->tries_size_);
  const DexFile::TryItem* try_item = DexFile::GetTryItems(*code_item, ti_offset);

  int iter_index = 0;
  int result = -1;
  uint32_t catch_dex_pc = -1;
  // Iterate over the catch handlers associated with dex_pc
  for (CatchHandlerIterator it(*code_item, *try_item); it.HasNext(); it.Next()) {
    uint16_t iter_type_idx = it.GetHandlerTypeIndex();
    // Catch all case
    if (iter_type_idx == DexFile::kDexNoIndex16) {
      catch_dex_pc = it.GetHandlerAddress();
      result = iter_index;
      break;
    }
    // Does this catch exception type apply?
    mirror::Class* iter_exception_type = mh.GetDexCacheResolvedType(iter_type_idx);
    if (UNLIKELY(iter_exception_type == NULL)) {
      // TODO: check, the verifier (class linker?) should take care of resolving all exception
      //       classes early.
      LOG(WARNING) << "Unresolved exception class when finding catch block: "
          << mh.GetTypeDescriptorFromTypeIdx(iter_type_idx);
    } else if (iter_exception_type->IsAssignableFrom(exception_type)) {
      catch_dex_pc = it.GetHandlerAddress();
      result = iter_index;
      break;
    }
    ++iter_index;
  }
  if (result != -1) {
    // Handler found.
    Runtime::Current()->GetInstrumentation()->ExceptionCaughtEvent(self,
                                                                   throw_location,
                                                                   current_method,
                                                                   catch_dex_pc,
                                                                   exception);
    // If the catch block has no move-exception then clear the exception for it.
    const Instruction* first_catch_instr = Instruction::At(&mh.GetCodeItem()->insns_[catch_dex_pc]);
    if (first_catch_instr->Opcode() != Instruction::MOVE_EXCEPTION) {
      self->ClearException();
    }
  }
  return result;
}

}  // namespace art
