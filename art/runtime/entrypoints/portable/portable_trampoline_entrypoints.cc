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

#ifndef ART_RUNTIME_ENTRYPOINTS_PORTABLE_PORTABLE_ARGUMENT_VISITOR_H_
#define ART_RUNTIME_ENTRYPOINTS_PORTABLE_PORTABLE_ARGUMENT_VISITOR_H_

#include "dex_instruction-inl.h"
#include "entrypoints/entrypoint_utils.h"
#include "interpreter/interpreter.h"
#include "mirror/art_method-inl.h"
#include "mirror/object-inl.h"
#include "object_utils.h"
#include "scoped_thread_state_change.h"

namespace art {

// Visits the arguments as saved to the stack by a Runtime::kRefAndArgs callee save frame.
class PortableArgumentVisitor {
 public:
// Offset to first (not the Method*) argument in a Runtime::kRefAndArgs callee save frame.
// Size of Runtime::kRefAndArgs callee save frame.
// Size of Method* and register parameters in out stack arguments.
#if defined(__arm__)
#define PORTABLE_CALLEE_SAVE_FRAME__REF_AND_ARGS__R1_OFFSET 8
#define PORTABLE_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE 48
#define PORTABLE_STACK_ARG_SKIP 0
#elif defined(__mips__)
#define PORTABLE_CALLEE_SAVE_FRAME__REF_AND_ARGS__R1_OFFSET 4
#define PORTABLE_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE 64
#define PORTABLE_STACK_ARG_SKIP 16
#elif defined(__i386__)
// For x86 there are no register arguments and the stack pointer will point directly to the called
// method argument passed by the caller.
#define PORTABLE_CALLEE_SAVE_FRAME__REF_AND_ARGS__R1_OFFSET 0
#define PORTABLE_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE 0
#define PORTABLE_STACK_ARG_SKIP 4
#else
#error "Unsupported architecture"
#define PORTABLE_CALLEE_SAVE_FRAME__REF_AND_ARGS__R1_OFFSET 0
#define PORTABLE_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE 0
#define PORTABLE_STACK_ARG_SKIP 0
#endif

  PortableArgumentVisitor(MethodHelper& caller_mh, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) :
    caller_mh_(caller_mh),
    args_in_regs_(ComputeArgsInRegs(caller_mh)),
    num_params_(caller_mh.NumArgs()),
    reg_args_(reinterpret_cast<byte*>(sp) + PORTABLE_CALLEE_SAVE_FRAME__REF_AND_ARGS__R1_OFFSET),
    stack_args_(reinterpret_cast<byte*>(sp) + PORTABLE_CALLEE_SAVE_FRAME__REF_AND_ARGS__FRAME_SIZE
                + PORTABLE_STACK_ARG_SKIP),
    cur_args_(reg_args_),
    cur_arg_index_(0),
    param_index_(0) {
  }

  virtual ~PortableArgumentVisitor() {}

  virtual void Visit() = 0;

  bool IsParamAReference() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return caller_mh_.IsParamAReference(param_index_);
  }

  bool IsParamALongOrDouble() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return caller_mh_.IsParamALongOrDouble(param_index_);
  }

  Primitive::Type GetParamPrimitiveType() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return caller_mh_.GetParamPrimitiveType(param_index_);
  }

  byte* GetParamAddress() const {
    return cur_args_ + (cur_arg_index_ * kPointerSize);
  }

  void VisitArguments() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    for (cur_arg_index_ = 0;  cur_arg_index_ < args_in_regs_ && param_index_ < num_params_; ) {
#if (defined(__arm__) || defined(__mips__))
      if (IsParamALongOrDouble() && cur_arg_index_ == 2) {
        break;
      }
#endif
      Visit();
      cur_arg_index_ += (IsParamALongOrDouble() ? 2 : 1);
      param_index_++;
    }
    cur_args_ = stack_args_;
    cur_arg_index_ = 0;
    while (param_index_ < num_params_) {
#if (defined(__arm__) || defined(__mips__))
      if (IsParamALongOrDouble() && cur_arg_index_ % 2 != 0) {
        cur_arg_index_++;
      }
#endif
      Visit();
      cur_arg_index_ += (IsParamALongOrDouble() ? 2 : 1);
      param_index_++;
    }
  }

 private:
  static size_t ComputeArgsInRegs(MethodHelper& mh) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
#if (defined(__i386__))
    return 0;
#else
    size_t args_in_regs = 0;
    size_t num_params = mh.NumArgs();
    for (size_t i = 0; i < num_params; i++) {
      args_in_regs = args_in_regs + (mh.IsParamALongOrDouble(i) ? 2 : 1);
      if (args_in_regs > 3) {
        args_in_regs = 3;
        break;
      }
    }
    return args_in_regs;
#endif
  }
  MethodHelper& caller_mh_;
  const size_t args_in_regs_;
  const size_t num_params_;
  byte* const reg_args_;
  byte* const stack_args_;
  byte* cur_args_;
  size_t cur_arg_index_;
  size_t param_index_;
};

// Visits arguments on the stack placing them into the shadow frame.
class BuildPortableShadowFrameVisitor : public PortableArgumentVisitor {
 public:
  BuildPortableShadowFrameVisitor(MethodHelper& caller_mh, mirror::ArtMethod** sp,
      ShadowFrame& sf, size_t first_arg_reg) :
    PortableArgumentVisitor(caller_mh, sp), sf_(sf), cur_reg_(first_arg_reg) { }
  virtual void Visit() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Primitive::Type type = GetParamPrimitiveType();
    switch (type) {
      case Primitive::kPrimLong:  // Fall-through.
      case Primitive::kPrimDouble:
        sf_.SetVRegLong(cur_reg_, *reinterpret_cast<jlong*>(GetParamAddress()));
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

  DISALLOW_COPY_AND_ASSIGN(BuildPortableShadowFrameVisitor);
};

extern "C" uint64_t artPortableToInterpreterBridge(mirror::ArtMethod* method, Thread* self,
                                                   mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Ensure we don't get thread suspension until the object arguments are safely in the shadow
  // frame.
  // FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsAndArgs);

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
    BuildPortableShadowFrameVisitor shadow_frame_builder(mh, sp,
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
class BuildPortableArgumentVisitor : public PortableArgumentVisitor {
 public:
  BuildPortableArgumentVisitor(MethodHelper& caller_mh, mirror::ArtMethod** sp,
                               ScopedObjectAccessUnchecked& soa, std::vector<jvalue>& args) :
    PortableArgumentVisitor(caller_mh, sp), soa_(soa), args_(args) {}

  virtual void Visit() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    jvalue val;
    Primitive::Type type = GetParamPrimitiveType();
    switch (type) {
      case Primitive::kPrimNot: {
        mirror::Object* obj = *reinterpret_cast<mirror::Object**>(GetParamAddress());
        val.l = soa_.AddLocalReference<jobject>(obj);
        break;
      }
      case Primitive::kPrimLong:  // Fall-through.
      case Primitive::kPrimDouble:
        val.j = *reinterpret_cast<jlong*>(GetParamAddress());
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
    args_.push_back(val);
  }

 private:
  ScopedObjectAccessUnchecked& soa_;
  std::vector<jvalue>& args_;

  DISALLOW_COPY_AND_ASSIGN(BuildPortableArgumentVisitor);
};

// Handler for invocation on proxy methods. On entry a frame will exist for the proxy object method
// which is responsible for recording callee save registers. We explicitly place into jobjects the
// incoming reference arguments (so they survive GC). We invoke the invocation handler, which is a
// field within the proxy object, which will box the primitive arguments and deal with error cases.
extern "C" uint64_t artPortableProxyInvokeHandler(mirror::ArtMethod* proxy_method,
                                                  mirror::Object* receiver,
                                                  Thread* self, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Ensure we don't get thread suspension until the object arguments are safely in jobjects.
  const char* old_cause =
      self->StartAssertNoThreadSuspension("Adding to IRT proxy object arguments");
  self->VerifyStack();
  // Start new JNI local reference state.
  JNIEnvExt* env = self->GetJniEnv();
  ScopedObjectAccessUnchecked soa(env);
  ScopedJniEnvLocalRefState env_state(env);
  // Create local ref. copies of proxy method and the receiver.
  jobject rcvr_jobj = soa.AddLocalReference<jobject>(receiver);

  // Placing arguments into args vector and remove the receiver.
  MethodHelper proxy_mh(proxy_method);
  std::vector<jvalue> args;
  BuildPortableArgumentVisitor local_ref_visitor(proxy_mh, sp, soa, args);
  local_ref_visitor.VisitArguments();
  args.erase(args.begin());

  // Convert proxy method into expected interface method.
  mirror::ArtMethod* interface_method = proxy_method->FindOverriddenMethod();
  DCHECK(interface_method != NULL);
  DCHECK(!interface_method->IsProxyMethod()) << PrettyMethod(interface_method);
  jobject interface_method_jobj = soa.AddLocalReference<jobject>(interface_method);

  // All naked Object*s should now be in jobjects, so its safe to go into the main invoke code
  // that performs allocations.
  self->EndAssertNoThreadSuspension(old_cause);
  JValue result = InvokeProxyInvocationHandler(soa, proxy_mh.GetShorty(),
                                               rcvr_jobj, interface_method_jobj, args);
  return result.GetJ();
}

// Lazily resolve a method for portable. Called by stub code.
extern "C" const void* artPortableResolutionTrampoline(mirror::ArtMethod* called,
                                                       mirror::Object* receiver,
                                                       Thread* thread,
                                                       mirror::ArtMethod** called_addr)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint32_t dex_pc;
  mirror::ArtMethod* caller = thread->GetCurrentMethod(&dex_pc);

  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  InvokeType invoke_type;
  bool is_range;
  if (called->IsRuntimeMethod()) {
    const DexFile::CodeItem* code = MethodHelper(caller).GetCodeItem();
    CHECK_LT(dex_pc, code->insns_size_in_code_units_);
    const Instruction* instr = Instruction::At(&code->insns_[dex_pc]);
    Instruction::Code instr_code = instr->Opcode();
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
        is_range = true;
    }
    uint32_t dex_method_idx = (is_range) ? instr->VRegB_3rc() : instr->VRegB_35c();
    called = linker->ResolveMethod(dex_method_idx, caller, invoke_type);
    // Incompatible class change should have been handled in resolve method.
    CHECK(!called->CheckIncompatibleClassChange(invoke_type));
    // Refine called method based on receiver.
    if (invoke_type == kVirtual) {
      called = receiver->GetClass()->FindVirtualMethodForVirtual(called);
    } else if (invoke_type == kInterface) {
      called = receiver->GetClass()->FindVirtualMethodForInterface(called);
    }
  } else {
    CHECK(called->IsStatic()) << PrettyMethod(called);
    invoke_type = kStatic;
    // Incompatible class change should have been handled in resolve method.
    CHECK(!called->CheckIncompatibleClassChange(invoke_type));
  }
  const void* code = NULL;
  if (LIKELY(!thread->IsExceptionPending())) {
    // Ensure that the called method's class is initialized.
    mirror::Class* called_class = called->GetDeclaringClass();
    linker->EnsureInitialized(called_class, true, true);
    if (LIKELY(called_class->IsInitialized())) {
      code = called->GetEntryPointFromCompiledCode();
      // TODO: remove this after we solve the link issue.
      {  // for lazy link.
        if (code == NULL) {
          code = linker->GetOatCodeFor(called);
        }
      }
    } else if (called_class->IsInitializing()) {
      if (invoke_type == kStatic) {
        // Class is still initializing, go to oat and grab code (trampoline must be left in place
        // until class is initialized to stop races between threads).
        code = linker->GetOatCodeFor(called);
      } else {
        // No trampoline for non-static methods.
        code = called->GetEntryPointFromCompiledCode();
        // TODO: remove this after we solve the link issue.
        {  // for lazy link.
          if (code == NULL) {
            code = linker->GetOatCodeFor(called);
          }
        }
      }
    } else {
      DCHECK(called_class->IsErroneous());
    }
  }
  if (LIKELY(code != NULL)) {
    // Expect class to at least be initializing.
    DCHECK(called->GetDeclaringClass()->IsInitializing());
    // Don't want infinite recursion.
    DCHECK(code != GetResolutionTrampoline(linker));
    // Set up entry into main method
    *called_addr = called;
  }
  return code;
}

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_PORTABLE_PORTABLE_ARGUMENT_VISITOR_H_
