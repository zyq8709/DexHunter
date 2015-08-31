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

#include "instrumentation.h"

#include <sys/uio.h>

#include "atomic_integer.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "debugger.h"
#include "dex_file-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "nth_caller_visitor.h"
#if !defined(ART_USE_PORTABLE_COMPILER)
#include "entrypoints/quick/quick_entrypoints.h"
#endif
#include "object_utils.h"
#include "os.h"
#include "scoped_thread_state_change.h"
#include "thread.h"
#include "thread_list.h"

namespace art {
namespace instrumentation {

static bool InstallStubsClassVisitor(mirror::Class* klass, void* arg)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Instrumentation* instrumentation = reinterpret_cast<Instrumentation*>(arg);
  return instrumentation->InstallStubsForClass(klass);
}

bool Instrumentation::InstallStubsForClass(mirror::Class* klass) {
  bool uninstall = !entry_exit_stubs_installed_ && !interpreter_stubs_installed_;
  ClassLinker* class_linker = NULL;
  if (uninstall) {
    class_linker = Runtime::Current()->GetClassLinker();
  }
  bool is_initialized = klass->IsInitialized();
  for (size_t i = 0; i < klass->NumDirectMethods(); i++) {
    mirror::ArtMethod* method = klass->GetDirectMethod(i);
    if (!method->IsAbstract() && !method->IsProxyMethod()) {
      const void* new_code;
      if (uninstall) {
        if (forced_interpret_only_ && !method->IsNative()) {
          new_code = GetCompiledCodeToInterpreterBridge();
        } else if (is_initialized || !method->IsStatic() || method->IsConstructor()) {
          new_code = class_linker->GetOatCodeFor(method);
        } else {
          new_code = GetResolutionTrampoline(class_linker);
        }
      } else {  // !uninstall
        if (!interpreter_stubs_installed_ || method->IsNative()) {
          new_code = GetQuickInstrumentationEntryPoint();
        } else {
          new_code = GetCompiledCodeToInterpreterBridge();
        }
      }
      method->SetEntryPointFromCompiledCode(new_code);
    }
  }
  for (size_t i = 0; i < klass->NumVirtualMethods(); i++) {
    mirror::ArtMethod* method = klass->GetVirtualMethod(i);
    if (!method->IsAbstract() && !method->IsProxyMethod()) {
      const void* new_code;
      if (uninstall) {
        if (forced_interpret_only_ && !method->IsNative()) {
          new_code = GetCompiledCodeToInterpreterBridge();
        } else {
          new_code = class_linker->GetOatCodeFor(method);
        }
      } else {  // !uninstall
        if (!interpreter_stubs_installed_ || method->IsNative()) {
          new_code = GetQuickInstrumentationEntryPoint();
        } else {
          new_code = GetCompiledCodeToInterpreterBridge();
        }
      }
      method->SetEntryPointFromCompiledCode(new_code);
    }
  }
  return true;
}

// Places the instrumentation exit pc as the return PC for every quick frame. This also allows
// deoptimization of quick frames to interpreter frames.
static void InstrumentationInstallStack(Thread* thread, void* arg)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  struct InstallStackVisitor : public StackVisitor {
    InstallStackVisitor(Thread* thread, Context* context, uintptr_t instrumentation_exit_pc)
        : StackVisitor(thread, context),  instrumentation_stack_(thread->GetInstrumentationStack()),
          instrumentation_exit_pc_(instrumentation_exit_pc), last_return_pc_(0) {}

    virtual bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      mirror::ArtMethod* m = GetMethod();
      if (GetCurrentQuickFrame() == NULL) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  Ignoring a shadow frame. Frame " << GetFrameId()
              << " Method=" << PrettyMethod(m);
        }
        return true;  // Ignore shadow frames.
      }
      if (m == NULL) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  Skipping upcall. Frame " << GetFrameId();
        }
        last_return_pc_ = 0;
        return true;  // Ignore upcalls.
      }
      if (m->IsRuntimeMethod()) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  Skipping runtime method. Frame " << GetFrameId();
        }
        last_return_pc_ = GetReturnPc();
        return true;  // Ignore unresolved methods since they will be instrumented after resolution.
      }
      if (kVerboseInstrumentation) {
        LOG(INFO) << "  Installing exit stub in " << DescribeLocation();
      }
      uintptr_t return_pc = GetReturnPc();
      CHECK_NE(return_pc, instrumentation_exit_pc_);
      CHECK_NE(return_pc, 0U);
      InstrumentationStackFrame instrumentation_frame(GetThisObject(), m, return_pc, GetFrameId(),
                                                      false);
      if (kVerboseInstrumentation) {
        LOG(INFO) << "Pushing frame " << instrumentation_frame.Dump();
      }
      instrumentation_stack_->push_back(instrumentation_frame);
      dex_pcs_.push_back(m->ToDexPc(last_return_pc_));
      SetReturnPc(instrumentation_exit_pc_);
      last_return_pc_ = return_pc;
      return true;  // Continue.
    }
    std::deque<InstrumentationStackFrame>* const instrumentation_stack_;
    std::vector<uint32_t> dex_pcs_;
    const uintptr_t instrumentation_exit_pc_;
    uintptr_t last_return_pc_;
  };
  if (kVerboseInstrumentation) {
    std::string thread_name;
    thread->GetThreadName(thread_name);
    LOG(INFO) << "Installing exit stubs in " << thread_name;
  }
  UniquePtr<Context> context(Context::Create());
  uintptr_t instrumentation_exit_pc = GetQuickInstrumentationExitPc();
  InstallStackVisitor visitor(thread, context.get(), instrumentation_exit_pc);
  visitor.WalkStack(true);

  // Create method enter events for all methods current on the thread's stack.
  Instrumentation* instrumentation = reinterpret_cast<Instrumentation*>(arg);
  typedef std::deque<InstrumentationStackFrame>::const_reverse_iterator It;
  for (It it = thread->GetInstrumentationStack()->rbegin(),
       end = thread->GetInstrumentationStack()->rend(); it != end; ++it) {
    mirror::Object* this_object = (*it).this_object_;
    mirror::ArtMethod* method = (*it).method_;
    uint32_t dex_pc = visitor.dex_pcs_.back();
    visitor.dex_pcs_.pop_back();
    instrumentation->MethodEnterEvent(thread, this_object, method, dex_pc);
  }
  thread->VerifyStack();
}

// Removes the instrumentation exit pc as the return PC for every quick frame.
static void InstrumentationRestoreStack(Thread* thread, void* arg)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  struct RestoreStackVisitor : public StackVisitor {
    RestoreStackVisitor(Thread* thread, uintptr_t instrumentation_exit_pc,
                        Instrumentation* instrumentation)
        : StackVisitor(thread, NULL), thread_(thread),
          instrumentation_exit_pc_(instrumentation_exit_pc),
          instrumentation_(instrumentation),
          instrumentation_stack_(thread->GetInstrumentationStack()),
          frames_removed_(0) {}

    virtual bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      if (instrumentation_stack_->size() == 0) {
        return false;  // Stop.
      }
      mirror::ArtMethod* m = GetMethod();
      if (GetCurrentQuickFrame() == NULL) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  Ignoring a shadow frame. Frame " << GetFrameId() << " Method=" << PrettyMethod(m);
        }
        return true;  // Ignore shadow frames.
      }
      if (m == NULL) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  Skipping upcall. Frame " << GetFrameId();
        }
        return true;  // Ignore upcalls.
      }
      bool removed_stub = false;
      // TODO: make this search more efficient?
      for (InstrumentationStackFrame instrumentation_frame : *instrumentation_stack_) {
        if (instrumentation_frame.frame_id_ == GetFrameId()) {
          if (kVerboseInstrumentation) {
            LOG(INFO) << "  Removing exit stub in " << DescribeLocation();
          }
          if (instrumentation_frame.interpreter_entry_) {
            CHECK(m == Runtime::Current()->GetCalleeSaveMethod(Runtime::kRefsAndArgs));
          } else {
            CHECK(m == instrumentation_frame.method_) << PrettyMethod(m);
          }
          SetReturnPc(instrumentation_frame.return_pc_);
          // Create the method exit events. As the methods didn't really exit the result is 0.
          instrumentation_->MethodExitEvent(thread_, instrumentation_frame.this_object_, m,
                                            GetDexPc(), JValue());
          frames_removed_++;
          removed_stub = true;
          break;
        }
      }
      if (!removed_stub) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  No exit stub in " << DescribeLocation();
        }
      }
      return true;  // Continue.
    }
    Thread* const thread_;
    const uintptr_t instrumentation_exit_pc_;
    Instrumentation* const instrumentation_;
    std::deque<instrumentation::InstrumentationStackFrame>* const instrumentation_stack_;
    size_t frames_removed_;
  };
  if (kVerboseInstrumentation) {
    std::string thread_name;
    thread->GetThreadName(thread_name);
    LOG(INFO) << "Removing exit stubs in " << thread_name;
  }
  std::deque<instrumentation::InstrumentationStackFrame>* stack = thread->GetInstrumentationStack();
  if (stack->size() > 0) {
    Instrumentation* instrumentation = reinterpret_cast<Instrumentation*>(arg);
    uintptr_t instrumentation_exit_pc = GetQuickInstrumentationExitPc();
    RestoreStackVisitor visitor(thread, instrumentation_exit_pc, instrumentation);
    visitor.WalkStack(true);
    CHECK_EQ(visitor.frames_removed_, stack->size());
    while (stack->size() > 0) {
      stack->pop_front();
    }
  }
}

void Instrumentation::AddListener(InstrumentationListener* listener, uint32_t events) {
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());
  bool require_entry_exit_stubs = false;
  bool require_interpreter = false;
  if ((events & kMethodEntered) != 0) {
    method_entry_listeners_.push_back(listener);
    require_entry_exit_stubs = true;
    have_method_entry_listeners_ = true;
  }
  if ((events & kMethodExited) != 0) {
    method_exit_listeners_.push_back(listener);
    require_entry_exit_stubs = true;
    have_method_exit_listeners_ = true;
  }
  if ((events & kMethodUnwind) != 0) {
    method_unwind_listeners_.push_back(listener);
    have_method_unwind_listeners_ = true;
  }
  if ((events & kDexPcMoved) != 0) {
    dex_pc_listeners_.push_back(listener);
    require_interpreter = true;
    have_dex_pc_listeners_ = true;
  }
  if ((events & kExceptionCaught) != 0) {
    exception_caught_listeners_.push_back(listener);
    have_exception_caught_listeners_ = true;
  }
  ConfigureStubs(require_entry_exit_stubs, require_interpreter);
}

void Instrumentation::RemoveListener(InstrumentationListener* listener, uint32_t events) {
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());
  bool require_entry_exit_stubs = false;
  bool require_interpreter = false;

  if ((events & kMethodEntered) != 0) {
    bool contains = std::find(method_entry_listeners_.begin(), method_entry_listeners_.end(),
                              listener) != method_entry_listeners_.end();
    if (contains) {
      method_entry_listeners_.remove(listener);
    }
    have_method_entry_listeners_ = method_entry_listeners_.size() > 0;
    require_entry_exit_stubs |= have_method_entry_listeners_;
  }
  if ((events & kMethodExited) != 0) {
    bool contains = std::find(method_exit_listeners_.begin(), method_exit_listeners_.end(),
                              listener) != method_exit_listeners_.end();
    if (contains) {
      method_exit_listeners_.remove(listener);
    }
    have_method_exit_listeners_ = method_exit_listeners_.size() > 0;
    require_entry_exit_stubs |= have_method_exit_listeners_;
  }
  if ((events & kMethodUnwind) != 0) {
    method_unwind_listeners_.remove(listener);
  }
  if ((events & kDexPcMoved) != 0) {
    bool contains = std::find(dex_pc_listeners_.begin(), dex_pc_listeners_.end(),
                              listener) != dex_pc_listeners_.end();
    if (contains) {
      dex_pc_listeners_.remove(listener);
    }
    have_dex_pc_listeners_ = dex_pc_listeners_.size() > 0;
    require_interpreter |= have_dex_pc_listeners_;
  }
  if ((events & kExceptionCaught) != 0) {
    exception_caught_listeners_.remove(listener);
    have_exception_caught_listeners_ = exception_caught_listeners_.size() > 0;
  }
  ConfigureStubs(require_entry_exit_stubs, require_interpreter);
}

void Instrumentation::ConfigureStubs(bool require_entry_exit_stubs, bool require_interpreter) {
  interpret_only_ = require_interpreter || forced_interpret_only_;
  // Compute what level of instrumentation is required and compare to current.
  int desired_level, current_level;
  if (require_interpreter) {
    desired_level = 2;
  } else if (require_entry_exit_stubs) {
    desired_level = 1;
  } else {
    desired_level = 0;
  }
  if (interpreter_stubs_installed_) {
    current_level = 2;
  } else if (entry_exit_stubs_installed_) {
    current_level = 1;
  } else {
    current_level = 0;
  }
  if (desired_level == current_level) {
    // We're already set.
    return;
  }
  Thread* self = Thread::Current();
  Runtime* runtime = Runtime::Current();
  Locks::thread_list_lock_->AssertNotHeld(self);
  if (desired_level > 0) {
    if (require_interpreter) {
      interpreter_stubs_installed_ = true;
    } else {
      CHECK(require_entry_exit_stubs);
      entry_exit_stubs_installed_ = true;
    }
    runtime->GetClassLinker()->VisitClasses(InstallStubsClassVisitor, this);
    instrumentation_stubs_installed_ = true;
    MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
    runtime->GetThreadList()->ForEach(InstrumentationInstallStack, this);
  } else {
    interpreter_stubs_installed_ = false;
    entry_exit_stubs_installed_ = false;
    runtime->GetClassLinker()->VisitClasses(InstallStubsClassVisitor, this);
    instrumentation_stubs_installed_ = false;
    MutexLock mu(self, *Locks::thread_list_lock_);
    Runtime::Current()->GetThreadList()->ForEach(InstrumentationRestoreStack, this);
  }
}

void Instrumentation::UpdateMethodsCode(mirror::ArtMethod* method, const void* code) const {
  if (LIKELY(!instrumentation_stubs_installed_)) {
    method->SetEntryPointFromCompiledCode(code);
  } else {
    if (!interpreter_stubs_installed_ || method->IsNative()) {
      method->SetEntryPointFromCompiledCode(GetQuickInstrumentationEntryPoint());
    } else {
      method->SetEntryPointFromCompiledCode(GetCompiledCodeToInterpreterBridge());
    }
  }
}

const void* Instrumentation::GetQuickCodeFor(const mirror::ArtMethod* method) const {
  Runtime* runtime = Runtime::Current();
  if (LIKELY(!instrumentation_stubs_installed_)) {
    const void* code = method->GetEntryPointFromCompiledCode();
    DCHECK(code != NULL);
    if (LIKELY(code != GetQuickResolutionTrampoline(runtime->GetClassLinker()) &&
               code != GetQuickToInterpreterBridge())) {
      return code;
    }
  }
  return runtime->GetClassLinker()->GetOatCodeFor(method);
}

void Instrumentation::MethodEnterEventImpl(Thread* thread, mirror::Object* this_object,
                                           const mirror::ArtMethod* method,
                                           uint32_t dex_pc) const {
  auto it = method_entry_listeners_.begin();
  bool is_end = (it == method_entry_listeners_.end());
  // Implemented this way to prevent problems caused by modification of the list while iterating.
  while (!is_end) {
    InstrumentationListener* cur = *it;
    ++it;
    is_end = (it == method_entry_listeners_.end());
    cur->MethodEntered(thread, this_object, method, dex_pc);
  }
}

void Instrumentation::MethodExitEventImpl(Thread* thread, mirror::Object* this_object,
                                          const mirror::ArtMethod* method,
                                          uint32_t dex_pc, const JValue& return_value) const {
  auto it = method_exit_listeners_.begin();
  bool is_end = (it == method_exit_listeners_.end());
  // Implemented this way to prevent problems caused by modification of the list while iterating.
  while (!is_end) {
    InstrumentationListener* cur = *it;
    ++it;
    is_end = (it == method_exit_listeners_.end());
    cur->MethodExited(thread, this_object, method, dex_pc, return_value);
  }
}

void Instrumentation::MethodUnwindEvent(Thread* thread, mirror::Object* this_object,
                                        const mirror::ArtMethod* method,
                                        uint32_t dex_pc) const {
  if (have_method_unwind_listeners_) {
    for (InstrumentationListener* listener : method_unwind_listeners_) {
      listener->MethodUnwind(thread, method, dex_pc);
    }
  }
}

void Instrumentation::DexPcMovedEventImpl(Thread* thread, mirror::Object* this_object,
                                          const mirror::ArtMethod* method,
                                          uint32_t dex_pc) const {
  // TODO: STL copy-on-write collection? The copy below is due to the debug listener having an
  // action where it can remove itself as a listener and break the iterator. The copy only works
  // around the problem and in general we may have to move to something like reference counting to
  // ensure listeners are deleted correctly.
  std::list<InstrumentationListener*> copy(dex_pc_listeners_);
  for (InstrumentationListener* listener : copy) {
    listener->DexPcMoved(thread, this_object, method, dex_pc);
  }
}

void Instrumentation::ExceptionCaughtEvent(Thread* thread, const ThrowLocation& throw_location,
                                           mirror::ArtMethod* catch_method,
                                           uint32_t catch_dex_pc,
                                           mirror::Throwable* exception_object) {
  if (have_exception_caught_listeners_) {
    DCHECK_EQ(thread->GetException(NULL), exception_object);
    thread->ClearException();
    for (InstrumentationListener* listener : exception_caught_listeners_) {
      listener->ExceptionCaught(thread, throw_location, catch_method, catch_dex_pc, exception_object);
    }
    thread->SetException(throw_location, exception_object);
  }
}

static void CheckStackDepth(Thread* self, const InstrumentationStackFrame& instrumentation_frame,
                            int delta)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  size_t frame_id = StackVisitor::ComputeNumFrames(self) + delta;
  if (frame_id != instrumentation_frame.frame_id_) {
    LOG(ERROR) << "Expected frame_id=" << frame_id << " but found "
        << instrumentation_frame.frame_id_;
    StackVisitor::DescribeStack(self);
    CHECK_EQ(frame_id, instrumentation_frame.frame_id_);
  }
}

void Instrumentation::PushInstrumentationStackFrame(Thread* self, mirror::Object* this_object,
                                                    mirror::ArtMethod* method,
                                                    uintptr_t lr, bool interpreter_entry) {
  // We have a callee-save frame meaning this value is guaranteed to never be 0.
  size_t frame_id = StackVisitor::ComputeNumFrames(self);
  std::deque<instrumentation::InstrumentationStackFrame>* stack = self->GetInstrumentationStack();
  if (kVerboseInstrumentation) {
    LOG(INFO) << "Entering " << PrettyMethod(method) << " from PC " << reinterpret_cast<void*>(lr);
  }
  instrumentation::InstrumentationStackFrame instrumentation_frame(this_object, method, lr,
                                                                   frame_id, interpreter_entry);
  stack->push_front(instrumentation_frame);

  MethodEnterEvent(self, this_object, method, 0);
}

uint64_t Instrumentation::PopInstrumentationStackFrame(Thread* self, uintptr_t* return_pc,
                                                       uint64_t gpr_result, uint64_t fpr_result) {
  // Do the pop.
  std::deque<instrumentation::InstrumentationStackFrame>* stack = self->GetInstrumentationStack();
  CHECK_GT(stack->size(), 0U);
  InstrumentationStackFrame instrumentation_frame = stack->front();
  stack->pop_front();

  // Set return PC and check the sanity of the stack.
  *return_pc = instrumentation_frame.return_pc_;
  CheckStackDepth(self, instrumentation_frame, 0);

  mirror::ArtMethod* method = instrumentation_frame.method_;
  char return_shorty = MethodHelper(method).GetShorty()[0];
  JValue return_value;
  if (return_shorty == 'V') {
    return_value.SetJ(0);
  } else if (return_shorty == 'F' || return_shorty == 'D') {
    return_value.SetJ(fpr_result);
  } else {
    return_value.SetJ(gpr_result);
  }
  // TODO: improve the dex pc information here, requires knowledge of current PC as opposed to
  //       return_pc.
  uint32_t dex_pc = DexFile::kDexNoIndex;
  mirror::Object* this_object = instrumentation_frame.this_object_;
  MethodExitEvent(self, this_object, instrumentation_frame.method_, dex_pc, return_value);

  bool deoptimize = false;
  if (interpreter_stubs_installed_) {
    // Deoptimize unless we're returning to an upcall.
    NthCallerVisitor visitor(self, 1, true);
    visitor.WalkStack(true);
    deoptimize = visitor.caller != NULL;
    if (deoptimize && kVerboseInstrumentation) {
      LOG(INFO) << "Deoptimizing into " << PrettyMethod(visitor.caller);
    }
  }
  if (deoptimize) {
    if (kVerboseInstrumentation) {
      LOG(INFO) << "Deoptimizing from " << PrettyMethod(method)
          << " result is " << std::hex << return_value.GetJ();
    }
    self->SetDeoptimizationReturnValue(return_value);
    return static_cast<uint64_t>(GetQuickDeoptimizationEntryPoint()) |
        (static_cast<uint64_t>(*return_pc) << 32);
  } else {
    if (kVerboseInstrumentation) {
      LOG(INFO) << "Returning from " << PrettyMethod(method)
                << " to PC " << reinterpret_cast<void*>(*return_pc);
    }
    return *return_pc;
  }
}

void Instrumentation::PopMethodForUnwind(Thread* self, bool is_deoptimization) const {
  // Do the pop.
  std::deque<instrumentation::InstrumentationStackFrame>* stack = self->GetInstrumentationStack();
  CHECK_GT(stack->size(), 0U);
  InstrumentationStackFrame instrumentation_frame = stack->front();
  // TODO: bring back CheckStackDepth(self, instrumentation_frame, 2);
  stack->pop_front();

  mirror::ArtMethod* method = instrumentation_frame.method_;
  if (is_deoptimization) {
    if (kVerboseInstrumentation) {
      LOG(INFO) << "Popping for deoptimization " << PrettyMethod(method);
    }
  } else {
    if (kVerboseInstrumentation) {
      LOG(INFO) << "Popping for unwind " << PrettyMethod(method);
    }

    // Notify listeners of method unwind.
    // TODO: improve the dex pc information here, requires knowledge of current PC as opposed to
    //       return_pc.
    uint32_t dex_pc = DexFile::kDexNoIndex;
    MethodUnwindEvent(self, instrumentation_frame.this_object_, method, dex_pc);
  }
}

std::string InstrumentationStackFrame::Dump() const {
  std::ostringstream os;
  os << "Frame " << frame_id_ << " " << PrettyMethod(method_) << ":"
      << reinterpret_cast<void*>(return_pc_) << " this=" << reinterpret_cast<void*>(this_object_);
  return os.str();
}

}  // namespace instrumentation
}  // namespace art
