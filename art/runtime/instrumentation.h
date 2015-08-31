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

#ifndef ART_RUNTIME_INSTRUMENTATION_H_
#define ART_RUNTIME_INSTRUMENTATION_H_

#include "base/macros.h"
#include "locks.h"

#include <stdint.h>
#include <list>

namespace art {
namespace mirror {
  class ArtMethod;
  class Class;
  class Object;
  class Throwable;
}  // namespace mirror
union JValue;
class Thread;
class ThrowLocation;

namespace instrumentation {

const bool kVerboseInstrumentation = false;

// Instrumentation event listener API. Registered listeners will get the appropriate call back for
// the events they are listening for. The call backs supply the thread, method and dex_pc the event
// occurred upon. The thread may or may not be Thread::Current().
struct InstrumentationListener {
  InstrumentationListener() {}
  virtual ~InstrumentationListener() {}

  // Call-back for when a method is entered.
  virtual void MethodEntered(Thread* thread, mirror::Object* this_object,
                             const mirror::ArtMethod* method,
                             uint32_t dex_pc) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  // Call-back for when a method is exited.
  // TODO: its likely passing the return value would be useful, however, we may need to get and
  //       parse the shorty to determine what kind of register holds the result.
  virtual void MethodExited(Thread* thread, mirror::Object* this_object,
                            const mirror::ArtMethod* method, uint32_t dex_pc,
                            const JValue& return_value)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  // Call-back for when a method is popped due to an exception throw. A method will either cause a
  // MethodExited call-back or a MethodUnwind call-back when its activation is removed.
  virtual void MethodUnwind(Thread* thread, const mirror::ArtMethod* method,
                            uint32_t dex_pc) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  // Call-back for when the dex pc moves in a method.
  virtual void DexPcMoved(Thread* thread, mirror::Object* this_object,
                          const mirror::ArtMethod* method, uint32_t new_dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  // Call-back when an exception is caught.
  virtual void ExceptionCaught(Thread* thread, const ThrowLocation& throw_location,
                               mirror::ArtMethod* catch_method, uint32_t catch_dex_pc,
                               mirror::Throwable* exception_object)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;
};

// Instrumentation is a catch-all for when extra information is required from the runtime. The
// typical use for instrumentation is for profiling and debugging. Instrumentation may add stubs
// to method entry and exit, it may also force execution to be switched to the interpreter and
// trigger deoptimization.
class Instrumentation {
 public:
  enum InstrumentationEvent {
    kMethodEntered = 1,
    kMethodExited = 2,
    kMethodUnwind = 4,
    kDexPcMoved = 8,
    kExceptionCaught = 16
  };

  Instrumentation() :
      instrumentation_stubs_installed_(false), entry_exit_stubs_installed_(false),
      interpreter_stubs_installed_(false),
      interpret_only_(false), forced_interpret_only_(false),
      have_method_entry_listeners_(false), have_method_exit_listeners_(false),
      have_method_unwind_listeners_(false), have_dex_pc_listeners_(false),
      have_exception_caught_listeners_(false) {}

  // Add a listener to be notified of the masked together sent of instrumentation events. This
  // suspend the runtime to install stubs. You are expected to hold the mutator lock as a proxy
  // for saying you should have suspended all threads (installing stubs while threads are running
  // will break).
  void AddListener(InstrumentationListener* listener, uint32_t events)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::thread_list_lock_, Locks::classlinker_classes_lock_);

  // Removes a listener possibly removing instrumentation stubs.
  void RemoveListener(InstrumentationListener* listener, uint32_t events)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::thread_list_lock_, Locks::classlinker_classes_lock_);

  // Update the code of a method respecting any installed stubs.
  void UpdateMethodsCode(mirror::ArtMethod* method, const void* code) const;

  // Get the quick code for the given method. More efficient than asking the class linker as it
  // will short-cut to GetCode if instrumentation and static method resolution stubs aren't
  // installed.
  const void* GetQuickCodeFor(const mirror::ArtMethod* method) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void ForceInterpretOnly() {
    interpret_only_ = true;
    forced_interpret_only_ = true;
  }

  // Called by ArtMethod::Invoke to determine dispatch mechanism.
  bool InterpretOnly() const {
    return interpret_only_;
  }

  bool ShouldPortableCodeDeoptimize() const {
    return instrumentation_stubs_installed_;
  }

  bool AreExitStubsInstalled() const {
    return instrumentation_stubs_installed_;
  }

  bool HasMethodEntryListeners() const {
    return have_method_entry_listeners_;
  }

  bool HasMethodExitListeners() const {
    return have_method_exit_listeners_;
  }

  bool HasDexPcListeners() const {
    return have_dex_pc_listeners_;
  }

  // Inform listeners that a method has been entered. A dex PC is provided as we may install
  // listeners into executing code and get method enter events for methods already on the stack.
  void MethodEnterEvent(Thread* thread, mirror::Object* this_object,
                        const mirror::ArtMethod* method, uint32_t dex_pc) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (UNLIKELY(HasMethodEntryListeners())) {
      MethodEnterEventImpl(thread, this_object, method, dex_pc);
    }
  }

  // Inform listeners that a method has been exited.
  void MethodExitEvent(Thread* thread, mirror::Object* this_object,
                       const mirror::ArtMethod* method, uint32_t dex_pc,
                       const JValue& return_value) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (UNLIKELY(HasMethodExitListeners())) {
      MethodExitEventImpl(thread, this_object, method, dex_pc, return_value);
    }
  }

  // Inform listeners that a method has been exited due to an exception.
  void MethodUnwindEvent(Thread* thread, mirror::Object* this_object,
                         const mirror::ArtMethod* method, uint32_t dex_pc) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Inform listeners that the dex pc has moved (only supported by the interpreter).
  void DexPcMovedEvent(Thread* thread, mirror::Object* this_object,
                       const mirror::ArtMethod* method, uint32_t dex_pc) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (UNLIKELY(HasDexPcListeners())) {
      DexPcMovedEventImpl(thread, this_object, method, dex_pc);
    }
  }

  // Inform listeners that an exception was caught.
  void ExceptionCaughtEvent(Thread* thread, const ThrowLocation& throw_location,
                            mirror::ArtMethod* catch_method, uint32_t catch_dex_pc,
                            mirror::Throwable* exception_object)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Called when an instrumented method is entered. The intended link register (lr) is saved so
  // that returning causes a branch to the method exit stub. Generates method enter events.
  void PushInstrumentationStackFrame(Thread* self, mirror::Object* this_object,
                                     mirror::ArtMethod* method, uintptr_t lr,
                                     bool interpreter_entry)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Called when an instrumented method is exited. Removes the pushed instrumentation frame
  // returning the intended link register. Generates method exit events.
  uint64_t PopInstrumentationStackFrame(Thread* self, uintptr_t* return_pc, uint64_t gpr_result,
                                        uint64_t fpr_result)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Pops an instrumentation frame from the current thread and generate an unwind event.
  void PopMethodForUnwind(Thread* self, bool is_deoptimization) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Call back for configure stubs.
  bool InstallStubsForClass(mirror::Class* klass) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  // Does the job of installing or removing instrumentation code within methods.
  void ConfigureStubs(bool require_entry_exit_stubs, bool require_interpreter)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::thread_list_lock_, Locks::classlinker_classes_lock_);

  void MethodEnterEventImpl(Thread* thread, mirror::Object* this_object,
                            const mirror::ArtMethod* method, uint32_t dex_pc) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void MethodExitEventImpl(Thread* thread, mirror::Object* this_object,
                           const mirror::ArtMethod* method,
                           uint32_t dex_pc, const JValue& return_value) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void DexPcMovedEventImpl(Thread* thread, mirror::Object* this_object,
                           const mirror::ArtMethod* method, uint32_t dex_pc) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Have we hijacked ArtMethod::code_ so that it calls instrumentation/interpreter code?
  bool instrumentation_stubs_installed_;

  // Have we hijacked ArtMethod::code_ to reference the enter/exit stubs?
  bool entry_exit_stubs_installed_;

  // Have we hijacked ArtMethod::code_ to reference the enter interpreter stub?
  bool interpreter_stubs_installed_;

  // Do we need the fidelity of events that we only get from running within the interpreter?
  bool interpret_only_;

  // Did the runtime request we only run in the interpreter? ie -Xint mode.
  bool forced_interpret_only_;

  // Do we have any listeners for method entry events? Short-cut to avoid taking the
  // instrumentation_lock_.
  bool have_method_entry_listeners_;

  // Do we have any listeners for method exit events? Short-cut to avoid taking the
  // instrumentation_lock_.
  bool have_method_exit_listeners_;

  // Do we have any listeners for method unwind events? Short-cut to avoid taking the
  // instrumentation_lock_.
  bool have_method_unwind_listeners_;

  // Do we have any listeners for dex move events? Short-cut to avoid taking the
  // instrumentation_lock_.
  bool have_dex_pc_listeners_;

  // Do we have any exception caught listeners? Short-cut to avoid taking the instrumentation_lock_.
  bool have_exception_caught_listeners_;

  // The event listeners, written to with the mutator_lock_ exclusively held.
  std::list<InstrumentationListener*> method_entry_listeners_ GUARDED_BY(Locks::mutator_lock_);
  std::list<InstrumentationListener*> method_exit_listeners_ GUARDED_BY(Locks::mutator_lock_);
  std::list<InstrumentationListener*> method_unwind_listeners_ GUARDED_BY(Locks::mutator_lock_);
  std::list<InstrumentationListener*> dex_pc_listeners_ GUARDED_BY(Locks::mutator_lock_);
  std::list<InstrumentationListener*> exception_caught_listeners_ GUARDED_BY(Locks::mutator_lock_);

  DISALLOW_COPY_AND_ASSIGN(Instrumentation);
};

// An element in the instrumentation side stack maintained in art::Thread.
struct InstrumentationStackFrame {
  InstrumentationStackFrame(mirror::Object* this_object, mirror::ArtMethod* method,
                            uintptr_t return_pc, size_t frame_id, bool interpreter_entry)
      : this_object_(this_object), method_(method), return_pc_(return_pc), frame_id_(frame_id),
        interpreter_entry_(interpreter_entry) {
  }

  std::string Dump() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::Object* this_object_;
  mirror::ArtMethod* method_;
  const uintptr_t return_pc_;
  const size_t frame_id_;
  const bool interpreter_entry_;
};

}  // namespace instrumentation
}  // namespace art

#endif  // ART_RUNTIME_INSTRUMENTATION_H_
