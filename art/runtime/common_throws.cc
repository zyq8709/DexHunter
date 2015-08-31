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

#include "common_throws.h"

#include "base/logging.h"
#include "class_linker-inl.h"
#include "dex_file-inl.h"
#include "dex_instruction-inl.h"
#include "invoke_type.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "object_utils.h"
#include "thread.h"
#include "verifier/method_verifier.h"

#include <sstream>

namespace art {

static void AddReferrerLocation(std::ostream& os, const mirror::Class* referrer)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (referrer != NULL) {
    ClassHelper kh(referrer);
    std::string location(kh.GetLocation());
    if (!location.empty()) {
      os << " (declaration of '" << PrettyDescriptor(referrer)
            << "' appears in " << location << ")";
    }
  }
}

static void ThrowException(const ThrowLocation* throw_location, const char* exception_descriptor,
                           const mirror::Class* referrer, const char* fmt, va_list* args = NULL)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::ostringstream msg;
  if (args != NULL) {
    std::string vmsg;
    StringAppendV(&vmsg, fmt, *args);
    msg << vmsg;
  } else {
    msg << fmt;
  }
  AddReferrerLocation(msg, referrer);
  Thread* self = Thread::Current();
  if (throw_location == NULL) {
    ThrowLocation computed_throw_location = self->GetCurrentLocationForThrow();
    self->ThrowNewException(computed_throw_location, exception_descriptor, msg.str().c_str());
  } else {
    self->ThrowNewException(*throw_location, exception_descriptor, msg.str().c_str());
  }
}

// AbstractMethodError

void ThrowAbstractMethodError(const mirror::ArtMethod* method) {
  ThrowException(NULL, "Ljava/lang/AbstractMethodError;", NULL,
                 StringPrintf("abstract method \"%s\"",
                              PrettyMethod(method).c_str()).c_str());
}

// ArithmeticException

void ThrowArithmeticExceptionDivideByZero() {
  ThrowException(NULL, "Ljava/lang/ArithmeticException;", NULL, "divide by zero");
}

// ArrayIndexOutOfBoundsException

void ThrowArrayIndexOutOfBoundsException(int index, int length) {
  ThrowException(NULL, "Ljava/lang/ArrayIndexOutOfBoundsException;", NULL,
                 StringPrintf("length=%d; index=%d", length, index).c_str());
}

// ArrayStoreException

void ThrowArrayStoreException(const mirror::Class* element_class,
                              const mirror::Class* array_class) {
  ThrowException(NULL, "Ljava/lang/ArrayStoreException;", NULL,
                 StringPrintf("%s cannot be stored in an array of type %s",
                              PrettyDescriptor(element_class).c_str(),
                              PrettyDescriptor(array_class).c_str()).c_str());
}

// ClassCastException

void ThrowClassCastException(const mirror::Class* dest_type, const mirror::Class* src_type) {
  ThrowException(NULL, "Ljava/lang/ClassCastException;", NULL,
                 StringPrintf("%s cannot be cast to %s",
                              PrettyDescriptor(src_type).c_str(),
                              PrettyDescriptor(dest_type).c_str()).c_str());
}

void ThrowClassCastException(const ThrowLocation* throw_location, const char* msg) {
  ThrowException(throw_location, "Ljava/lang/ClassCastException;", NULL, msg);
}

// ClassCircularityError

void ThrowClassCircularityError(mirror::Class* c) {
  std::ostringstream msg;
  msg << PrettyDescriptor(c);
  ThrowException(NULL, "Ljava/lang/ClassCircularityError;", c, msg.str().c_str());
}

// ClassFormatError

void ThrowClassFormatError(const mirror::Class* referrer, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  ThrowException(NULL, "Ljava/lang/ClassFormatError;", referrer, fmt, &args);
  va_end(args);}

// IllegalAccessError

void ThrowIllegalAccessErrorClass(mirror::Class* referrer, mirror::Class* accessed) {
  std::ostringstream msg;
  msg << "Illegal class access: '" << PrettyDescriptor(referrer) << "' attempting to access '"
      << PrettyDescriptor(accessed) << "'";
  ThrowException(NULL, "Ljava/lang/IllegalAccessError;", referrer, msg.str().c_str());
}

void ThrowIllegalAccessErrorClassForMethodDispatch(mirror::Class* referrer, mirror::Class* accessed,
                                                   const mirror::ArtMethod* caller,
                                                   const mirror::ArtMethod* called,
                                                   InvokeType type) {
  std::ostringstream msg;
  msg << "Illegal class access ('" << PrettyDescriptor(referrer) << "' attempting to access '"
      << PrettyDescriptor(accessed) << "') in attempt to invoke " << type
      << " method " << PrettyMethod(called).c_str();
  ThrowException(NULL, "Ljava/lang/IllegalAccessError;", referrer, msg.str().c_str());
}

void ThrowIllegalAccessErrorMethod(mirror::Class* referrer, mirror::ArtMethod* accessed) {
  std::ostringstream msg;
  msg << "Method '" << PrettyMethod(accessed) << "' is inaccessible to class '"
      << PrettyDescriptor(referrer) << "'";
  ThrowException(NULL, "Ljava/lang/IllegalAccessError;", referrer, msg.str().c_str());
}

void ThrowIllegalAccessErrorField(mirror::Class* referrer, mirror::ArtField* accessed) {
  std::ostringstream msg;
  msg << "Field '" << PrettyField(accessed, false) << "' is inaccessible to class '"
      << PrettyDescriptor(referrer) << "'";
  ThrowException(NULL, "Ljava/lang/IllegalAccessError;", referrer, msg.str().c_str());
}

void ThrowIllegalAccessErrorFinalField(const mirror::ArtMethod* referrer,
                                       mirror::ArtField* accessed) {
  std::ostringstream msg;
  msg << "Final field '" << PrettyField(accessed, false) << "' cannot be written to by method '"
      << PrettyMethod(referrer) << "'";
  ThrowException(NULL, "Ljava/lang/IllegalAccessError;",
                 referrer != NULL ? referrer->GetClass() : NULL,
                 msg.str().c_str());
}

void ThrowIllegalAccessError(mirror::Class* referrer, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  ThrowException(NULL, "Ljava/lang/IllegalAccessError;", referrer, fmt, &args);
  va_end(args);
}

// IllegalArgumentException

void ThrowIllegalArgumentException(const ThrowLocation* throw_location, const char* msg) {
  ThrowException(throw_location, "Ljava/lang/IllegalArgumentException;", NULL, msg);
}


// IncompatibleClassChangeError

void ThrowIncompatibleClassChangeError(InvokeType expected_type, InvokeType found_type,
                                       mirror::ArtMethod* method,
                                       const mirror::ArtMethod* referrer) {
  std::ostringstream msg;
  msg << "The method '" << PrettyMethod(method) << "' was expected to be of type "
      << expected_type << " but instead was found to be of type " << found_type;
  ThrowException(NULL, "Ljava/lang/IncompatibleClassChangeError;",
                 referrer != NULL ? referrer->GetClass() : NULL,
                 msg.str().c_str());
}

void ThrowIncompatibleClassChangeErrorClassForInterfaceDispatch(const mirror::ArtMethod* interface_method,
                                                                mirror::Object* this_object,
                                                                const mirror::ArtMethod* referrer) {
  // Referrer is calling interface_method on this_object, however, the interface_method isn't
  // implemented by this_object.
  CHECK(this_object != NULL);
  std::ostringstream msg;
  msg << "Class '" << PrettyDescriptor(this_object->GetClass())
      << "' does not implement interface '"
      << PrettyDescriptor(interface_method->GetDeclaringClass())
      << "' in call to '" << PrettyMethod(interface_method) << "'";
  ThrowException(NULL, "Ljava/lang/IncompatibleClassChangeError;",
                 referrer != NULL ? referrer->GetClass() : NULL,
                 msg.str().c_str());
}

void ThrowIncompatibleClassChangeErrorField(const mirror::ArtField* resolved_field, bool is_static,
                                            const mirror::ArtMethod* referrer) {
  std::ostringstream msg;
  msg << "Expected '" << PrettyField(resolved_field) << "' to be a "
      << (is_static ? "static" : "instance") << " field" << " rather than a "
      << (is_static ? "instance" : "static") << " field";
  ThrowException(NULL, "Ljava/lang/IncompatibleClassChangeError;", referrer->GetClass(),
                 msg.str().c_str());
}

void ThrowIncompatibleClassChangeError(const mirror::Class* referrer, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  ThrowException(NULL, "Ljava/lang/IncompatibleClassChangeError;", referrer, fmt, &args);
  va_end(args);
}

// LinkageError

void ThrowLinkageError(const mirror::Class* referrer, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  ThrowException(NULL, "Ljava/lang/LinkageError;", referrer, fmt, &args);
  va_end(args);
}

// NegativeArraySizeException

void ThrowNegativeArraySizeException(int size) {
  ThrowException(NULL, "Ljava/lang/NegativeArraySizeException;", NULL,
                 StringPrintf("%d", size).c_str());
}

void ThrowNegativeArraySizeException(const char* msg) {
  ThrowException(NULL, "Ljava/lang/NegativeArraySizeException;", NULL, msg);
}

// NoSuchFieldError

void ThrowNoSuchFieldError(const StringPiece& scope, mirror::Class* c,
                           const StringPiece& type, const StringPiece& name)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ClassHelper kh(c);
  std::ostringstream msg;
  msg << "No " << scope << "field " << name << " of type " << type
      << " in class " << kh.GetDescriptor() << " or its superclasses";
  ThrowException(NULL, "Ljava/lang/NoSuchFieldError;", c, msg.str().c_str());
}

// NoSuchMethodError

void ThrowNoSuchMethodError(InvokeType type, mirror::Class* c, const StringPiece& name,
                            const StringPiece& signature) {
  std::ostringstream msg;
  ClassHelper kh(c);
  msg << "No " << type << " method " << name << signature
      << " in class " << kh.GetDescriptor() << " or its super classes";
  ThrowException(NULL, "Ljava/lang/NoSuchMethodError;", c, msg.str().c_str());
}

void ThrowNoSuchMethodError(uint32_t method_idx) {
  Thread* self = Thread::Current();
  ThrowLocation throw_location = self->GetCurrentLocationForThrow();
  mirror::DexCache* dex_cache = throw_location.GetMethod()->GetDeclaringClass()->GetDexCache();
  const DexFile& dex_file = *dex_cache->GetDexFile();
  std::ostringstream msg;
  msg << "No method '" << PrettyMethod(method_idx, dex_file, true) << "'";
  ThrowException(&throw_location, "Ljava/lang/NoSuchMethodError;",
                 throw_location.GetMethod()->GetDeclaringClass(), msg.str().c_str());
}

// NullPointerException

void ThrowNullPointerExceptionForFieldAccess(const ThrowLocation& throw_location,
                                             mirror::ArtField* field, bool is_read) {
  std::ostringstream msg;
  msg << "Attempt to " << (is_read ? "read from" : "write to")
      << " field '" << PrettyField(field, true) << "' on a null object reference";
  ThrowException(&throw_location, "Ljava/lang/NullPointerException;", NULL, msg.str().c_str());
}

static void ThrowNullPointerExceptionForMethodAccessImpl(const ThrowLocation& throw_location,
                                                         uint32_t method_idx,
                                                         const DexFile& dex_file,
                                                         InvokeType type)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::ostringstream msg;
  msg << "Attempt to invoke " << type << " method '"
      << PrettyMethod(method_idx, dex_file, true) << "' on a null object reference";
  ThrowException(&throw_location, "Ljava/lang/NullPointerException;", NULL, msg.str().c_str());
}

void ThrowNullPointerExceptionForMethodAccess(const ThrowLocation& throw_location,
                                              uint32_t method_idx,
                                              InvokeType type) {
  mirror::DexCache* dex_cache = throw_location.GetMethod()->GetDeclaringClass()->GetDexCache();
  const DexFile& dex_file = *dex_cache->GetDexFile();
  ThrowNullPointerExceptionForMethodAccessImpl(throw_location, method_idx,
                                               dex_file, type);
}

void ThrowNullPointerExceptionForMethodAccess(const ThrowLocation& throw_location,
                                              mirror::ArtMethod* method,
                                              InvokeType type) {
  mirror::DexCache* dex_cache = method->GetDeclaringClass()->GetDexCache();
  const DexFile& dex_file = *dex_cache->GetDexFile();
  ThrowNullPointerExceptionForMethodAccessImpl(throw_location, method->GetDexMethodIndex(),
                                               dex_file, type);
}

void ThrowNullPointerExceptionFromDexPC(const ThrowLocation& throw_location) {
  const DexFile::CodeItem* code = MethodHelper(throw_location.GetMethod()).GetCodeItem();
  uint32_t throw_dex_pc = throw_location.GetDexPc();
  CHECK_LT(throw_dex_pc, code->insns_size_in_code_units_);
  const Instruction* instr = Instruction::At(&code->insns_[throw_dex_pc]);
  switch (instr->Opcode()) {
    case Instruction::INVOKE_DIRECT:
      ThrowNullPointerExceptionForMethodAccess(throw_location, instr->VRegB_35c(), kDirect);
      break;
    case Instruction::INVOKE_DIRECT_RANGE:
      ThrowNullPointerExceptionForMethodAccess(throw_location, instr->VRegB_3rc(), kDirect);
      break;
    case Instruction::INVOKE_VIRTUAL:
      ThrowNullPointerExceptionForMethodAccess(throw_location, instr->VRegB_35c(), kVirtual);
      break;
    case Instruction::INVOKE_VIRTUAL_RANGE:
      ThrowNullPointerExceptionForMethodAccess(throw_location, instr->VRegB_3rc(), kVirtual);
      break;
    case Instruction::INVOKE_INTERFACE:
      ThrowNullPointerExceptionForMethodAccess(throw_location, instr->VRegB_35c(), kInterface);
      break;
    case Instruction::INVOKE_INTERFACE_RANGE:
      ThrowNullPointerExceptionForMethodAccess(throw_location, instr->VRegB_3rc(), kInterface);
      break;
    case Instruction::INVOKE_VIRTUAL_QUICK:
    case Instruction::INVOKE_VIRTUAL_RANGE_QUICK: {
      // Since we replaced the method index, we ask the verifier to tell us which
      // method is invoked at this location.
      mirror::ArtMethod* method =
          verifier::MethodVerifier::FindInvokedMethodAtDexPc(throw_location.GetMethod(),
                                                             throw_location.GetDexPc());
      if (method != NULL) {
        // NPE with precise message.
        ThrowNullPointerExceptionForMethodAccess(throw_location, method, kVirtual);
      } else {
        // NPE with imprecise message.
        ThrowNullPointerException(&throw_location,
                                  "Attempt to invoke a virtual method on a null object reference");
      }
      break;
    }
    case Instruction::IGET:
    case Instruction::IGET_WIDE:
    case Instruction::IGET_OBJECT:
    case Instruction::IGET_BOOLEAN:
    case Instruction::IGET_BYTE:
    case Instruction::IGET_CHAR:
    case Instruction::IGET_SHORT: {
      mirror::ArtField* field =
          Runtime::Current()->GetClassLinker()->ResolveField(instr->VRegC_22c(),
                                                             throw_location.GetMethod(), false);
      ThrowNullPointerExceptionForFieldAccess(throw_location, field, true /* read */);
      break;
    }
    case Instruction::IGET_QUICK:
    case Instruction::IGET_WIDE_QUICK:
    case Instruction::IGET_OBJECT_QUICK: {
      // Since we replaced the field index, we ask the verifier to tell us which
      // field is accessed at this location.
      mirror::ArtField* field =
          verifier::MethodVerifier::FindAccessedFieldAtDexPc(throw_location.GetMethod(),
                                                             throw_location.GetDexPc());
      if (field != NULL) {
        // NPE with precise message.
        ThrowNullPointerExceptionForFieldAccess(throw_location, field, true /* read */);
      } else {
        // NPE with imprecise message.
        ThrowNullPointerException(&throw_location,
                                  "Attempt to read from a field on a null object reference");
      }
      break;
    }
    case Instruction::IPUT:
    case Instruction::IPUT_WIDE:
    case Instruction::IPUT_OBJECT:
    case Instruction::IPUT_BOOLEAN:
    case Instruction::IPUT_BYTE:
    case Instruction::IPUT_CHAR:
    case Instruction::IPUT_SHORT: {
      mirror::ArtField* field =
          Runtime::Current()->GetClassLinker()->ResolveField(instr->VRegC_22c(),
                                                             throw_location.GetMethod(), false);
      ThrowNullPointerExceptionForFieldAccess(throw_location, field, false /* write */);
      break;
    }
    case Instruction::IPUT_QUICK:
    case Instruction::IPUT_WIDE_QUICK:
    case Instruction::IPUT_OBJECT_QUICK: {
      // Since we replaced the field index, we ask the verifier to tell us which
      // field is accessed at this location.
      mirror::ArtField* field =
          verifier::MethodVerifier::FindAccessedFieldAtDexPc(throw_location.GetMethod(),
                                                             throw_location.GetDexPc());
      if (field != NULL) {
        // NPE with precise message.
        ThrowNullPointerExceptionForFieldAccess(throw_location, field, false /* write */);
      } else {
        // NPE with imprecise message.
        ThrowNullPointerException(&throw_location,
                                  "Attempt to write to a field on a null object reference");
      }
      break;
    }
    case Instruction::AGET:
    case Instruction::AGET_WIDE:
    case Instruction::AGET_OBJECT:
    case Instruction::AGET_BOOLEAN:
    case Instruction::AGET_BYTE:
    case Instruction::AGET_CHAR:
    case Instruction::AGET_SHORT:
      ThrowException(&throw_location, "Ljava/lang/NullPointerException;", NULL,
                     "Attempt to read from null array");
      break;
    case Instruction::APUT:
    case Instruction::APUT_WIDE:
    case Instruction::APUT_OBJECT:
    case Instruction::APUT_BOOLEAN:
    case Instruction::APUT_BYTE:
    case Instruction::APUT_CHAR:
    case Instruction::APUT_SHORT:
      ThrowException(&throw_location, "Ljava/lang/NullPointerException;", NULL,
                     "Attempt to write to null array");
      break;
    case Instruction::ARRAY_LENGTH:
      ThrowException(&throw_location, "Ljava/lang/NullPointerException;", NULL,
                     "Attempt to get length of null array");
      break;
    default: {
      // TODO: We should have covered all the cases where we expect a NPE above, this
      //       message/logging is so we can improve any cases we've missed in the future.
      const DexFile& dex_file =
          *throw_location.GetMethod()->GetDeclaringClass()->GetDexCache()->GetDexFile();
      ThrowException(&throw_location, "Ljava/lang/NullPointerException;", NULL,
                     StringPrintf("Null pointer exception during instruction '%s'",
                                  instr->DumpString(&dex_file).c_str()).c_str());
      break;
    }
  }
}

void ThrowNullPointerException(const ThrowLocation* throw_location, const char* msg) {
  ThrowException(throw_location, "Ljava/lang/NullPointerException;", NULL, msg);
}

// RuntimeException

void ThrowRuntimeException(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  ThrowException(NULL, "Ljava/lang/RuntimeException;", NULL, fmt, &args);
  va_end(args);
}

// VerifyError

void ThrowVerifyError(const mirror::Class* referrer, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  ThrowException(NULL, "Ljava/lang/VerifyError;", referrer, fmt, &args);
  va_end(args);
}

}  // namespace art
