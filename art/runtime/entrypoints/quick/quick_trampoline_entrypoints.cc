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

#include "callee_save_frame.h"
#include "common_throws.h"
#include "dex_file-inl.h"
#include "dex_instruction-inl.h"
#include "entrypoints/entrypoint_utils.h"
#include "interpreter/interpreter.h"
#include "invoke_arg_array_builder.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "object_utils.h"
#include "runtime.h"



namespace art {

// Visits the arguments as saved to the stack by a Runtime::kRefAndArgs callee save frame.
class QuickArgumentVisitor {
 public:
// Offset to first (not the Method*) argument in a Runtime::kRefAndArgs callee save frame.
// Size of Runtime::kRefAndArgs callee save frame.
// Size of Method* and register parameters in out stack arguments.
#if defined(__arm__)
  // The callee save frame is pointed to by SP.
  // | argN       |  |
  // | ...        |  |
  // | arg4       |  |
  // | arg3 spill |  |  Caller's frame
  // | arg2 spill |  |
  // | arg1 spill |  |
  // | Method*    | ---
  // | LR         |
  // | ...        |    callee saves
  // | R3         |    arg3
  // | R2         |    arg2
  // | R1         |    arg1
  // | R0         |
  // | Method*    |  <- sp
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__R1_OFFSET 8
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__LR_OFFSET 44
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE 48
#define QUICK_STACK_ARG_SKIP 16
#elif defined(__mips__)
  // The callee save frame is pointed to by SP.
  // | argN       |  |
  // | ...        |  |
  // | arg4       |  |
  // | arg3 spill |  |  Caller's frame
  // | arg2 spill |  |
  // | arg1 spill |  |
  // | Method*    | ---
  // | RA         |
  // | ...        |    callee saves
  // | A3         |    arg3
  // | A2         |    arg2
  // | A1         |    arg1
  // | A0/Method* |  <- sp
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__R1_OFFSET 4
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__LR_OFFSET 60
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE 64
#define QUICK_STACK_ARG_SKIP 16
#elif defined(__i386__)
  // The callee save frame is pointed to by SP.
  // | argN        |  |
  // | ...         |  |
  // | arg4        |  |
  // | arg3 spill  |  |  Caller's frame
  // | arg2 spill  |  |
  // | arg1 spill  |  |
  // | Method*     | ---
  // | Return      |
  // | EBP,ESI,EDI |    callee saves
  // | EBX         |    arg3
  // | EDX         |    arg2
  // | ECX         |    arg1
  // | EAX/Method* |  <- sp
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__R1_OFFSET 4
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__LR_OFFSET 28
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE 32
#define QUICK_STACK_ARG_SKIP 16
#else
#error "Unsupported architecture"
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__R1_OFFSET 0
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__LR_OFFSET 0
#define QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE 0
#define QUICK_STACK_ARG_SKIP 0
#endif

  static mirror::ArtMethod* GetCallingMethod(mirror::ArtMethod** sp) {
    byte* previous_sp = reinterpret_cast<byte*>(sp) +
        QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE;
    return *reinterpret_cast<mirror::ArtMethod**>(previous_sp);
  }

  static uintptr_t GetCallingPc(mirror::ArtMethod** sp) {
    byte* lr = reinterpret_cast<byte*>(sp) + QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__LR_OFFSET;
    return *reinterpret_cast<uintptr_t*>(lr);
  }

  QuickArgumentVisitor(mirror::ArtMethod** sp, bool is_static,
                       const char* shorty, uint32_t shorty_len)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) :
    is_static_(is_static), shorty_(shorty), shorty_len_(shorty_len),
    args_in_regs_(ComputeArgsInRegs(is_static, shorty, shorty_len)),
    num_params_((is_static ? 0 : 1) + shorty_len - 1),  // +1 for this, -1 for return type
    reg_args_(reinterpret_cast<byte*>(sp) + QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__R1_OFFSET),
    stack_args_(reinterpret_cast<byte*>(sp) + QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE
                + QUICK_STACK_ARG_SKIP),
    cur_args_(reg_args_),
    cur_arg_index_(0),
    param_index_(0),
    is_split_long_or_double_(false) {
    DCHECK_EQ(static_cast<size_t>(QUICK_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE),
              Runtime::Current()->GetCalleeSaveMethod(Runtime::kRefsAndArgs)->GetFrameSizeInBytes());
  }

  virtual ~QuickArgumentVisitor() {}

  virtual void Visit() = 0;

  Primitive::Type GetParamPrimitiveType() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    size_t index = param_index_;
    if (is_static_) {
      index++;  // 0th argument must skip return value at start of the shorty
    } else if (index == 0) {
      return Primitive::kPrimNot;
    }
    CHECK_LT(index, shorty_len_);
    return Primitive::GetType(shorty_[index]);
  }

  byte* GetParamAddress() const {
    return cur_args_ + (cur_arg_index_ * kPointerSize);
  }

  bool IsSplitLongOrDouble() const {
    return is_split_long_or_double_;
  }

  bool IsParamAReference() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetParamPrimitiveType() == Primitive::kPrimNot;
  }

  bool IsParamALongOrDouble() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Primitive::Type type = GetParamPrimitiveType();
    return type == Primitive::kPrimLong || type == Primitive::kPrimDouble;
  }

  uint64_t ReadSplitLongParam() const {
    DCHECK(IsSplitLongOrDouble());
    uint64_t low_half = *reinterpret_cast<uint32_t*>(GetParamAddress());
    uint64_t high_half = *reinterpret_cast<uint32_t*>(stack_args_);
    return (low_half & 0xffffffffULL) | (high_half << 32);
  }

  void VisitArguments() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    for (cur_arg_index_ = 0;  cur_arg_index_ < args_in_regs_ && param_index_ < num_params_; ) {
      is_split_long_or_double_ = (cur_arg_index_ == 2) && IsParamALongOrDouble();
      Visit();
      cur_arg_index_ += (IsParamALongOrDouble() ? 2 : 1);
      param_index_++;
    }
    cur_args_ = stack_args_;
    cur_arg_index_ = is_split_long_or_double_ ? 1 : 0;
    is_split_long_or_double_ = false;
    while (param_index_ < num_params_) {
      Visit();
      cur_arg_index_ += (IsParamALongOrDouble() ? 2 : 1);
      param_index_++;
    }
  }

 private:
  static size_t ComputeArgsInRegs(bool is_static, const char* shorty, uint32_t shorty_len)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    size_t args_in_regs = (is_static ? 0 : 1);
    for (size_t i = 0; i < shorty_len; i++) {
      char s = shorty[i];
      if (s == 'J' || s == 'D') {
        args_in_regs += 2;
      } else {
        args_in_regs++;
      }
      if (args_in_regs > 3) {
        args_in_regs = 3;
        break;
      }
    }
    return args_in_regs;
  }

  const bool is_static_;
  const char* const shorty_;
  const uint32_t shorty_len_;
  const size_t args_in_regs_;
  const size_t num_params_;
  byte* const reg_args_;
  byte* const stack_args_;
  byte* cur_args_;
  size_t cur_arg_index_;
  size_t param_index_;
  // Does a 64bit parameter straddle the register and stack arguments?
  bool is_split_long_or_double_;
};

// Visits arguments on the stack placing them into the shadow frame.
class BuildQuickShadowFrameVisitor : public QuickArgumentVisitor {
 public:
  BuildQuickShadowFrameVisitor(mirror::ArtMethod** sp,
      bool is_static, const char* shorty,
       uint32_t shorty_len, ShadowFrame& sf, size_t first_arg_reg) :
    QuickArgumentVisitor(sp, is_static, shorty, shorty_len), sf_(sf), cur_reg_(first_arg_reg) {}

  virtual void Visit() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Primitive::Type type = GetParamPrimitiveType();
    switch (type) {
      case Primitive::kPrimLong:  // Fall-through.
      case Primitive::kPrimDouble:
        if (IsSplitLongOrDouble()) {
          sf_.SetVRegLong(cur_reg_, ReadSplitLongParam());
        } else {
          sf_.SetVRegLong(cur_reg_, *reinterpret_cast<jlong*>(GetParamAddress()));
        }
        ++cur_reg_;
        break;
      case Primitive::kPrimNot:
        sf_.SetVRegReference(cur_reg_, *reinterpret_cast<mirror::Object**>(GetParamAddress()));
        break;
      case Primitive::kPrimBoolean:  // Fall-through.
      case Primitive::kPrimByte:     // Fall-through.
      case Primitive::kPrimChar:     // Fall-through.
      case Primitive::kPrimShort:    // Fall-through.
      case Primitive::kPrimInt:      // Fall-through.
      case Primitive::kPrimFloat:
        sf_.SetVReg(cur_reg_, *reinterpret_cast<jint*>(GetParamAddress()));
        break;
      case Primitive::kPrimVoid:
        LOG(FATAL) << "UNREACHABLE";
        break;
    }
    ++cur_reg_;
  }

 private:
  ShadowFrame& sf_;
  size_t cur_reg_;

  DISALLOW_COPY_AND_ASSIGN(BuildQuickShadowFrameVisitor);
};

extern "C" uint64_t artQuickToInterpreterBridge(mirror::ArtMethod* method, Thread* self,
                                                mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Ensure we don't get thread suspension until the object arguments are safely in the shadow
  // frame.
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsAndArgs);

  if (method->IsAbstract()) {
    ThrowAbstractMethodError(method);
    return 0;
  } else {
    const char* old_cause = self->StartAssertNoThreadSuspension("Building interpreter shadow frame");
    MethodHelper mh(method);
    const DexFile::CodeItem* code_item = mh.GetCodeItem();
    uint16_t num_regs = code_item->registers_size_;
    void* memory = alloca(ShadowFrame::ComputeSize(num_regs));
    ShadowFrame* shadow_frame(ShadowFrame::Create(num_regs, NULL,  // No last shadow coming from quick.
                                                  method, 0, memory));
    size_t first_arg_reg = code_item->registers_size_ - code_item->ins_size_;
    BuildQuickShadowFrameVisitor shadow_frame_builder(sp, mh.IsStatic(), mh.GetShorty(),
                                                 mh.GetShortyLength(),
                                                 *shadow_frame, first_arg_reg);
    shadow_frame_builder.VisitArguments();
    // Push a transition back into managed code onto the linked list in thread.
    ManagedStack fragment;
    self->PushManagedStackFragment(&fragment);
    self->PushShadowFrame(shadow_frame);
    self->EndAssertNoThreadSuspension(old_cause);

    if (method->IsStatic() && !method->GetDeclaringClass()->IsInitializing()) {
      // Ensure static method's class is initialized.
      if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(method->GetDeclaringClass(),
                                                                   true, true)) {
        DCHECK(Thread::Current()->IsExceptionPending());
        self->PopManagedStackFragment(fragment);
        return 0;
      }
    }

    JValue result = interpreter::EnterInterpreterFromStub(self, mh, code_item, *shadow_frame);
    // Pop transition.
    self->PopManagedStackFragment(fragment);
    return result.GetJ();
  }
}

// Visits arguments on the stack placing them into the args vector, Object* arguments are converted
// to jobjects.
class BuildQuickArgumentVisitor : public QuickArgumentVisitor {
 public:
  BuildQuickArgumentVisitor(mirror::ArtMethod** sp, bool is_static, const char* shorty,
                            uint32_t shorty_len, ScopedObjectAccessUnchecked* soa,
                            std::vector<jvalue>* args) :
    QuickArgumentVisitor(sp, is_static, shorty, shorty_len), soa_(soa), args_(args) {}

  virtual void Visit() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    jvalue val;
    Primitive::Type type = GetParamPrimitiveType();
    switch (type) {
      case Primitive::kPrimNot: {
        mirror::Object* obj = *reinterpret_cast<mirror::Object**>(GetParamAddress());
        val.l = soa_->AddLocalReference<jobject>(obj);
        break;
      }
      case Primitive::kPrimLong:  // Fall-through.
      case Primitive::kPrimDouble:
        if (IsSplitLongOrDouble()) {
          val.j = ReadSplitLongParam();
        } else {
          val.j = *reinterpret_cast<jlong*>(GetParamAddress());
        }
        break;
      case Primitive::kPrimBoolean:  // Fall-through.
      case Primitive::kPrimByte:     // Fall-through.
      case Primitive::kPrimChar:     // Fall-through.
      case Primitive::kPrimShort:    // Fall-through.
      case Primitive::kPrimInt:      // Fall-through.
      case Primitive::kPrimFloat:
        val.i =  *reinterpret_cast<jint*>(GetParamAddress());
        break;
      case Primitive::kPrimVoid:
        LOG(FATAL) << "UNREACHABLE";
        val.j = 0;
        break;
    }
    args_->push_back(val);
  }

 private:
  ScopedObjectAccessUnchecked* soa_;
  std::vector<jvalue>* args_;

  DISALLOW_COPY_AND_ASSIGN(BuildQuickArgumentVisitor);
};

// Handler for invocation on proxy methods. On entry a frame will exist for the proxy object method
// which is responsible for recording callee save registers. We explicitly place into jobjects the
// incoming reference arguments (so they survive GC). We invoke the invocation handler, which is a
// field within the proxy object, which will box the primitive arguments and deal with error cases.
extern "C" uint64_t artQuickProxyInvokeHandler(mirror::ArtMethod* proxy_method,
                                               mirror::Object* receiver,
                                               Thread* self, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(proxy_method->IsProxyMethod()) << PrettyMethod(proxy_method);
  DCHECK(receiver->GetClass()->IsProxyClass()) << PrettyMethod(proxy_method);
  // Ensure we don't get thread suspension until the object arguments are safely in jobjects.
  const char* old_cause =
      self->StartAssertNoThreadSuspension("Adding to IRT proxy object arguments");
  // Register the top of the managed stack, making stack crawlable.
  DCHECK_EQ(*sp, proxy_method) << PrettyMethod(proxy_method);
  self->SetTopOfStack(sp, 0);
  DCHECK_EQ(proxy_method->GetFrameSizeInBytes(),
            Runtime::Current()->GetCalleeSaveMethod(Runtime::kRefsAndArgs)->GetFrameSizeInBytes())
      << PrettyMethod(proxy_method);
  self->VerifyStack();
  // Start new JNI local reference state.
  JNIEnvExt* env = self->GetJniEnv();
  ScopedObjectAccessUnchecked soa(env);
  ScopedJniEnvLocalRefState env_state(env);
  // Create local ref. copies of proxy method and the receiver.
  jobject rcvr_jobj = soa.AddLocalReference<jobject>(receiver);

  // Placing arguments into args vector and remove the receiver.
  MethodHelper proxy_mh(proxy_method);
  DCHECK(!proxy_mh.IsStatic()) << PrettyMethod(proxy_method);
  std::vector<jvalue> args;
  BuildQuickArgumentVisitor local_ref_visitor(sp, proxy_mh.IsStatic(), proxy_mh.GetShorty(),
                                              proxy_mh.GetShortyLength(), &soa, &args);

  local_ref_visitor.VisitArguments();
  DCHECK_GT(args.size(), 0U) << PrettyMethod(proxy_method);
  args.erase(args.begin());

  // Convert proxy method into expected interface method.
  mirror::ArtMethod* interface_method = proxy_method->FindOverriddenMethod();
  DCHECK(interface_method != NULL) << PrettyMethod(proxy_method);
  DCHECK(!interface_method->IsProxyMethod()) << PrettyMethod(interface_method);
  jobject interface_method_jobj = soa.AddLocalReference<jobject>(interface_method);

  // All naked Object*s should now be in jobjects, so its safe to go into the main invoke code
  // that performs allocations.
  self->EndAssertNoThreadSuspension(old_cause);
  JValue result = InvokeProxyInvocationHandler(soa, proxy_mh.GetShorty(),
                                               rcvr_jobj, interface_method_jobj, args);
  return result.GetJ();
}

// Read object references held in arguments from quick frames and place in a JNI local references,
// so they don't get garbage collected.
class RememberFoGcArgumentVisitor : public QuickArgumentVisitor {
 public:
  RememberFoGcArgumentVisitor(mirror::ArtMethod** sp, bool is_static, const char* shorty,
                              uint32_t shorty_len, ScopedObjectAccessUnchecked* soa) :
    QuickArgumentVisitor(sp, is_static, shorty, shorty_len), soa_(soa) {}

  virtual void Visit() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (IsParamAReference()) {
      soa_->AddLocalReference<jobject>(*reinterpret_cast<mirror::Object**>(GetParamAddress()));
    }
  }

 private:
  ScopedObjectAccessUnchecked* soa_;

  DISALLOW_COPY_AND_ASSIGN(RememberFoGcArgumentVisitor);
};

// Lazily resolve a method for quick. Called by stub code.
extern "C" const void* artQuickResolutionTrampoline(mirror::ArtMethod* called,
                                                    mirror::Object* receiver,
                                                    Thread* thread, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(thread, sp, Runtime::kRefsAndArgs);
  // Start new JNI local reference state
  JNIEnvExt* env = thread->GetJniEnv();
  ScopedObjectAccessUnchecked soa(env);
  ScopedJniEnvLocalRefState env_state(env);
  const char* old_cause = thread->StartAssertNoThreadSuspension("Quick method resolution set up");

  // Compute details about the called method (avoid GCs)
  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  mirror::ArtMethod* caller = QuickArgumentVisitor::GetCallingMethod(sp);
  InvokeType invoke_type;
  const DexFile* dex_file;
  uint32_t dex_method_idx;
  if (called->IsRuntimeMethod()) {
    uint32_t dex_pc = caller->ToDexPc(QuickArgumentVisitor::GetCallingPc(sp));
    const DexFile::CodeItem* code;
    {
      MethodHelper mh(caller);
      dex_file = &mh.GetDexFile();
      code = mh.GetCodeItem();
    }
    CHECK_LT(dex_pc, code->insns_size_in_code_units_);
    const Instruction* instr = Instruction::At(&code->insns_[dex_pc]);
    Instruction::Code instr_code = instr->Opcode();
    bool is_range;
    switch (instr_code) {
      case Instruction::INVOKE_DIRECT:
        invoke_type = kDirect;
        is_range = false;
        break;
      case Instruction::INVOKE_DIRECT_RANGE:
        invoke_type = kDirect;
        is_range = true;
        break;
      case Instruction::INVOKE_STATIC:
        invoke_type = kStatic;
        is_range = false;
        break;
      case Instruction::INVOKE_STATIC_RANGE:
        invoke_type = kStatic;
        is_range = true;
        break;
      case Instruction::INVOKE_SUPER:
        invoke_type = kSuper;
        is_range = false;
        break;
      case Instruction::INVOKE_SUPER_RANGE:
        invoke_type = kSuper;
        is_range = true;
        break;
      case Instruction::INVOKE_VIRTUAL:
        invoke_type = kVirtual;
        is_range = false;
        break;
      case Instruction::INVOKE_VIRTUAL_RANGE:
        invoke_type = kVirtual;
        is_range = true;
        break;
      case Instruction::INVOKE_INTERFACE:
        invoke_type = kInterface;
        is_range = false;
        break;
      case Instruction::INVOKE_INTERFACE_RANGE:
        invoke_type = kInterface;
        is_range = true;
        break;
      default:
        LOG(FATAL) << "Unexpected call into trampoline: " << instr->DumpString(NULL);
        // Avoid used uninitialized warnings.
        invoke_type = kDirect;
        is_range = false;
    }
    dex_method_idx = (is_range) ? instr->VRegB_3rc() : instr->VRegB_35c();

  } else {
    invoke_type = kStatic;
    dex_file = &MethodHelper(called).GetDexFile();
    dex_method_idx = called->GetDexMethodIndex();
  }
  uint32_t shorty_len;
  const char* shorty =
      dex_file->GetMethodShorty(dex_file->GetMethodId(dex_method_idx), &shorty_len);
  RememberFoGcArgumentVisitor visitor(sp, invoke_type == kStatic, shorty, shorty_len, &soa);
  visitor.VisitArguments();
  thread->EndAssertNoThreadSuspension(old_cause);
  // Resolve method filling in dex cache.
  if (called->IsRuntimeMethod()) {
    called = linker->ResolveMethod(dex_method_idx, caller, invoke_type);
  }
  const void* code = NULL;
  if (LIKELY(!thread->IsExceptionPending())) {
    // Incompatible class change should have been handled in resolve method.
    CHECK(!called->CheckIncompatibleClassChange(invoke_type));
    // Refine called method based on receiver.
    if (invoke_type == kVirtual) {
      called = receiver->GetClass()->FindVirtualMethodForVirtual(called);
    } else if (invoke_type == kInterface) {
      called = receiver->GetClass()->FindVirtualMethodForInterface(called);
    }
    // Ensure that the called method's class is initialized.
    mirror::Class* called_class = called->GetDeclaringClass();
    linker->EnsureInitialized(called_class, true, true);
    if (LIKELY(called_class->IsInitialized())) {
      code = called->GetEntryPointFromCompiledCode();
    } else if (called_class->IsInitializing()) {
      if (invoke_type == kStatic) {
        // Class is still initializing, go to oat and grab code (trampoline must be left in place
        // until class is initialized to stop races between threads).
        code = linker->GetOatCodeFor(called);
      } else {
        // No trampoline for non-static methods.
        code = called->GetEntryPointFromCompiledCode();
      }
    } else {
      DCHECK(called_class->IsErroneous());
    }
  }
  CHECK_EQ(code == NULL, thread->IsExceptionPending());
#ifdef MOVING_GARBAGE_COLLECTOR
  // TODO: locally saved objects may have moved during a GC during resolution. Need to update the
  //       registers so that the stale objects aren't passed to the method we've resolved.
    UNIMPLEMENTED(WARNING);
#endif
  // Place called method in callee-save frame to be placed as first argument to quick method.
  *sp = called;
  return code;
}

}  // namespace art
