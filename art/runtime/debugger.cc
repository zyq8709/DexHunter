/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "debugger.h"

#include <sys/uio.h>

#include <set>

#include "arch/context.h"
#include "class_linker.h"
#include "class_linker-inl.h"
#include "dex_file-inl.h"
#include "dex_instruction.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/space/large_object_space.h"
#include "gc/space/space-inl.h"
#include "invoke_arg_array_builder.h"
#include "jdwp/object_registry.h"
#include "mirror/art_field-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/throwable.h"
#include "object_utils.h"
#include "safe_map.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "ScopedPrimitiveArray.h"
#include "sirt_ref.h"
#include "stack_indirect_reference_table.h"
#include "thread_list.h"
#include "throw_location.h"
#include "utf.h"
#include "well_known_classes.h"

#ifdef HAVE_ANDROID_OS
#include "cutils/properties.h"
#endif

namespace art {

static const size_t kMaxAllocRecordStackDepth = 16;  // Max 255.
static const size_t kDefaultNumAllocRecords = 64*1024;  // Must be a power of 2.

struct AllocRecordStackTraceElement {
  mirror::ArtMethod* method;
  uint32_t dex_pc;

  int32_t LineNumber() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return MethodHelper(method).GetLineNumFromDexPC(dex_pc);
  }
};

struct AllocRecord {
  mirror::Class* type;
  size_t byte_count;
  uint16_t thin_lock_id;
  AllocRecordStackTraceElement stack[kMaxAllocRecordStackDepth];  // Unused entries have NULL method.

  size_t GetDepth() {
    size_t depth = 0;
    while (depth < kMaxAllocRecordStackDepth && stack[depth].method != NULL) {
      ++depth;
    }
    return depth;
  }
};

struct Breakpoint {
  mirror::ArtMethod* method;
  uint32_t dex_pc;
  Breakpoint(mirror::ArtMethod* method, uint32_t dex_pc) : method(method), dex_pc(dex_pc) {}
};

static std::ostream& operator<<(std::ostream& os, const Breakpoint& rhs)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  os << StringPrintf("Breakpoint[%s @%#x]", PrettyMethod(rhs.method).c_str(), rhs.dex_pc);
  return os;
}

struct SingleStepControl {
  // Are we single-stepping right now?
  bool is_active;
  Thread* thread;

  JDWP::JdwpStepSize step_size;
  JDWP::JdwpStepDepth step_depth;

  const mirror::ArtMethod* method;
  int32_t line_number;  // Or -1 for native methods.
  std::set<uint32_t> dex_pcs;
  int stack_depth;
};

class DebugInstrumentationListener : public instrumentation::InstrumentationListener {
 public:
  DebugInstrumentationListener() {}
  virtual ~DebugInstrumentationListener() {}

  virtual void MethodEntered(Thread* thread, mirror::Object* this_object,
                             const mirror::ArtMethod* method, uint32_t dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (method->IsNative()) {
      // TODO: post location events is a suspension point and native method entry stubs aren't.
      return;
    }
    Dbg::PostLocationEvent(method, 0, this_object, Dbg::kMethodEntry);
  }

  virtual void MethodExited(Thread* thread, mirror::Object* this_object,
                            const mirror::ArtMethod* method,
                            uint32_t dex_pc, const JValue& return_value)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    UNUSED(return_value);
    if (method->IsNative()) {
      // TODO: post location events is a suspension point and native method entry stubs aren't.
      return;
    }
    Dbg::PostLocationEvent(method, dex_pc, this_object, Dbg::kMethodExit);
  }

  virtual void MethodUnwind(Thread* thread, const mirror::ArtMethod* method,
                            uint32_t dex_pc) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // We're not recorded to listen to this kind of event, so complain.
    LOG(ERROR) << "Unexpected method unwind event in debugger " << PrettyMethod(method)
        << " " << dex_pc;
  }

  virtual void DexPcMoved(Thread* thread, mirror::Object* this_object,
                          const mirror::ArtMethod* method, uint32_t new_dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Dbg::UpdateDebugger(thread, this_object, method, new_dex_pc);
  }

  virtual void ExceptionCaught(Thread* thread, const ThrowLocation& throw_location,
                               mirror::ArtMethod* catch_method, uint32_t catch_dex_pc,
                               mirror::Throwable* exception_object)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Dbg::PostException(thread, throw_location, catch_method, catch_dex_pc, exception_object);
  }
} gDebugInstrumentationListener;

// JDWP is allowed unless the Zygote forbids it.
static bool gJdwpAllowed = true;

// Was there a -Xrunjdwp or -agentlib:jdwp= argument on the command line?
static bool gJdwpConfigured = false;

// Broken-down JDWP options. (Only valid if IsJdwpConfigured() is true.)
static JDWP::JdwpOptions gJdwpOptions;

// Runtime JDWP state.
static JDWP::JdwpState* gJdwpState = NULL;
static bool gDebuggerConnected;  // debugger or DDMS is connected.
static bool gDebuggerActive;     // debugger is making requests.
static bool gDisposed;           // debugger called VirtualMachine.Dispose, so we should drop the connection.

static bool gDdmThreadNotification = false;

// DDMS GC-related settings.
static Dbg::HpifWhen gDdmHpifWhen = Dbg::HPIF_WHEN_NEVER;
static Dbg::HpsgWhen gDdmHpsgWhen = Dbg::HPSG_WHEN_NEVER;
static Dbg::HpsgWhat gDdmHpsgWhat;
static Dbg::HpsgWhen gDdmNhsgWhen = Dbg::HPSG_WHEN_NEVER;
static Dbg::HpsgWhat gDdmNhsgWhat;

static ObjectRegistry* gRegistry = NULL;

// Recent allocation tracking.
static Mutex gAllocTrackerLock DEFAULT_MUTEX_ACQUIRED_AFTER("AllocTracker lock");
AllocRecord* Dbg::recent_allocation_records_ PT_GUARDED_BY(gAllocTrackerLock) = NULL;  // TODO: CircularBuffer<AllocRecord>
static size_t gAllocRecordMax GUARDED_BY(gAllocTrackerLock) = 0;
static size_t gAllocRecordHead GUARDED_BY(gAllocTrackerLock) = 0;
static size_t gAllocRecordCount GUARDED_BY(gAllocTrackerLock) = 0;

// Breakpoints and single-stepping.
static std::vector<Breakpoint> gBreakpoints GUARDED_BY(Locks::breakpoint_lock_);
static SingleStepControl gSingleStepControl GUARDED_BY(Locks::breakpoint_lock_);

static bool IsBreakpoint(const mirror::ArtMethod* m, uint32_t dex_pc)
    LOCKS_EXCLUDED(Locks::breakpoint_lock_)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  MutexLock mu(Thread::Current(), *Locks::breakpoint_lock_);
  for (size_t i = 0; i < gBreakpoints.size(); ++i) {
    if (gBreakpoints[i].method == m && gBreakpoints[i].dex_pc == dex_pc) {
      VLOG(jdwp) << "Hit breakpoint #" << i << ": " << gBreakpoints[i];
      return true;
    }
  }
  return false;
}

static bool IsSuspendedForDebugger(ScopedObjectAccessUnchecked& soa, Thread* thread) {
  MutexLock mu(soa.Self(), *Locks::thread_suspend_count_lock_);
  // A thread may be suspended for GC; in this code, we really want to know whether
  // there's a debugger suspension active.
  return thread->IsSuspended() && thread->GetDebugSuspendCount() > 0;
}

static mirror::Array* DecodeArray(JDWP::RefTypeId id, JDWP::JdwpError& status)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::Object* o = gRegistry->Get<mirror::Object*>(id);
  if (o == NULL || o == ObjectRegistry::kInvalidObject) {
    status = JDWP::ERR_INVALID_OBJECT;
    return NULL;
  }
  if (!o->IsArrayInstance()) {
    status = JDWP::ERR_INVALID_ARRAY;
    return NULL;
  }
  status = JDWP::ERR_NONE;
  return o->AsArray();
}

static mirror::Class* DecodeClass(JDWP::RefTypeId id, JDWP::JdwpError& status)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::Object* o = gRegistry->Get<mirror::Object*>(id);
  if (o == NULL || o == ObjectRegistry::kInvalidObject) {
    status = JDWP::ERR_INVALID_OBJECT;
    return NULL;
  }
  if (!o->IsClass()) {
    status = JDWP::ERR_INVALID_CLASS;
    return NULL;
  }
  status = JDWP::ERR_NONE;
  return o->AsClass();
}

static JDWP::JdwpError DecodeThread(ScopedObjectAccessUnchecked& soa, JDWP::ObjectId thread_id, Thread*& thread)
    EXCLUSIVE_LOCKS_REQUIRED(Locks::thread_list_lock_)
    LOCKS_EXCLUDED(Locks::thread_suspend_count_lock_)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::Object* thread_peer = gRegistry->Get<mirror::Object*>(thread_id);
  if (thread_peer == NULL || thread_peer == ObjectRegistry::kInvalidObject) {
    // This isn't even an object.
    return JDWP::ERR_INVALID_OBJECT;
  }

  mirror::Class* java_lang_Thread = soa.Decode<mirror::Class*>(WellKnownClasses::java_lang_Thread);
  if (!java_lang_Thread->IsAssignableFrom(thread_peer->GetClass())) {
    // This isn't a thread.
    return JDWP::ERR_INVALID_THREAD;
  }

  thread = Thread::FromManagedThread(soa, thread_peer);
  if (thread == NULL) {
    // This is a java.lang.Thread without a Thread*. Must be a zombie.
    return JDWP::ERR_THREAD_NOT_ALIVE;
  }
  return JDWP::ERR_NONE;
}

static JDWP::JdwpTag BasicTagFromDescriptor(const char* descriptor) {
  // JDWP deliberately uses the descriptor characters' ASCII values for its enum.
  // Note that by "basic" we mean that we don't get more specific than JT_OBJECT.
  return static_cast<JDWP::JdwpTag>(descriptor[0]);
}

static JDWP::JdwpTag TagFromClass(mirror::Class* c)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  CHECK(c != NULL);
  if (c->IsArrayClass()) {
    return JDWP::JT_ARRAY;
  }

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  if (c->IsStringClass()) {
    return JDWP::JT_STRING;
  } else if (c->IsClassClass()) {
    return JDWP::JT_CLASS_OBJECT;
  } else if (class_linker->FindSystemClass("Ljava/lang/Thread;")->IsAssignableFrom(c)) {
    return JDWP::JT_THREAD;
  } else if (class_linker->FindSystemClass("Ljava/lang/ThreadGroup;")->IsAssignableFrom(c)) {
    return JDWP::JT_THREAD_GROUP;
  } else if (class_linker->FindSystemClass("Ljava/lang/ClassLoader;")->IsAssignableFrom(c)) {
    return JDWP::JT_CLASS_LOADER;
  } else {
    return JDWP::JT_OBJECT;
  }
}

/*
 * Objects declared to hold Object might actually hold a more specific
 * type.  The debugger may take a special interest in these (e.g. it
 * wants to display the contents of Strings), so we want to return an
 * appropriate tag.
 *
 * Null objects are tagged JT_OBJECT.
 */
static JDWP::JdwpTag TagFromObject(const mirror::Object* o)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return (o == NULL) ? JDWP::JT_OBJECT : TagFromClass(o->GetClass());
}

static bool IsPrimitiveTag(JDWP::JdwpTag tag) {
  switch (tag) {
  case JDWP::JT_BOOLEAN:
  case JDWP::JT_BYTE:
  case JDWP::JT_CHAR:
  case JDWP::JT_FLOAT:
  case JDWP::JT_DOUBLE:
  case JDWP::JT_INT:
  case JDWP::JT_LONG:
  case JDWP::JT_SHORT:
  case JDWP::JT_VOID:
    return true;
  default:
    return false;
  }
}

/*
 * Handle one of the JDWP name/value pairs.
 *
 * JDWP options are:
 *  help: if specified, show help message and bail
 *  transport: may be dt_socket or dt_shmem
 *  address: for dt_socket, "host:port", or just "port" when listening
 *  server: if "y", wait for debugger to attach; if "n", attach to debugger
 *  timeout: how long to wait for debugger to connect / listen
 *
 * Useful with server=n (these aren't supported yet):
 *  onthrow=<exception-name>: connect to debugger when exception thrown
 *  onuncaught=y|n: connect to debugger when uncaught exception thrown
 *  launch=<command-line>: launch the debugger itself
 *
 * The "transport" option is required, as is "address" if server=n.
 */
static bool ParseJdwpOption(const std::string& name, const std::string& value) {
  if (name == "transport") {
    if (value == "dt_socket") {
      gJdwpOptions.transport = JDWP::kJdwpTransportSocket;
    } else if (value == "dt_android_adb") {
      gJdwpOptions.transport = JDWP::kJdwpTransportAndroidAdb;
    } else {
      LOG(ERROR) << "JDWP transport not supported: " << value;
      return false;
    }
  } else if (name == "server") {
    if (value == "n") {
      gJdwpOptions.server = false;
    } else if (value == "y") {
      gJdwpOptions.server = true;
    } else {
      LOG(ERROR) << "JDWP option 'server' must be 'y' or 'n'";
      return false;
    }
  } else if (name == "suspend") {
    if (value == "n") {
      gJdwpOptions.suspend = false;
    } else if (value == "y") {
      gJdwpOptions.suspend = true;
    } else {
      LOG(ERROR) << "JDWP option 'suspend' must be 'y' or 'n'";
      return false;
    }
  } else if (name == "address") {
    /* this is either <port> or <host>:<port> */
    std::string port_string;
    gJdwpOptions.host.clear();
    std::string::size_type colon = value.find(':');
    if (colon != std::string::npos) {
      gJdwpOptions.host = value.substr(0, colon);
      port_string = value.substr(colon + 1);
    } else {
      port_string = value;
    }
    if (port_string.empty()) {
      LOG(ERROR) << "JDWP address missing port: " << value;
      return false;
    }
    char* end;
    uint64_t port = strtoul(port_string.c_str(), &end, 10);
    if (*end != '\0' || port > 0xffff) {
      LOG(ERROR) << "JDWP address has junk in port field: " << value;
      return false;
    }
    gJdwpOptions.port = port;
  } else if (name == "launch" || name == "onthrow" || name == "oncaught" || name == "timeout") {
    /* valid but unsupported */
    LOG(INFO) << "Ignoring JDWP option '" << name << "'='" << value << "'";
  } else {
    LOG(INFO) << "Ignoring unrecognized JDWP option '" << name << "'='" << value << "'";
  }

  return true;
}

/*
 * Parse the latter half of a -Xrunjdwp/-agentlib:jdwp= string, e.g.:
 * "transport=dt_socket,address=8000,server=y,suspend=n"
 */
bool Dbg::ParseJdwpOptions(const std::string& options) {
  VLOG(jdwp) << "ParseJdwpOptions: " << options;

  std::vector<std::string> pairs;
  Split(options, ',', pairs);

  for (size_t i = 0; i < pairs.size(); ++i) {
    std::string::size_type equals = pairs[i].find('=');
    if (equals == std::string::npos) {
      LOG(ERROR) << "Can't parse JDWP option '" << pairs[i] << "' in '" << options << "'";
      return false;
    }
    ParseJdwpOption(pairs[i].substr(0, equals), pairs[i].substr(equals + 1));
  }

  if (gJdwpOptions.transport == JDWP::kJdwpTransportUnknown) {
    LOG(ERROR) << "Must specify JDWP transport: " << options;
  }
  if (!gJdwpOptions.server && (gJdwpOptions.host.empty() || gJdwpOptions.port == 0)) {
    LOG(ERROR) << "Must specify JDWP host and port when server=n: " << options;
    return false;
  }

  gJdwpConfigured = true;
  return true;
}

void Dbg::StartJdwp() {
  if (!gJdwpAllowed || !IsJdwpConfigured()) {
    // No JDWP for you!
    return;
  }

  CHECK(gRegistry == NULL);
  gRegistry = new ObjectRegistry;

  // Init JDWP if the debugger is enabled. This may connect out to a
  // debugger, passively listen for a debugger, or block waiting for a
  // debugger.
  gJdwpState = JDWP::JdwpState::Create(&gJdwpOptions);
  if (gJdwpState == NULL) {
    // We probably failed because some other process has the port already, which means that
    // if we don't abort the user is likely to think they're talking to us when they're actually
    // talking to that other process.
    LOG(FATAL) << "Debugger thread failed to initialize";
  }

  // If a debugger has already attached, send the "welcome" message.
  // This may cause us to suspend all threads.
  if (gJdwpState->IsActive()) {
    ScopedObjectAccess soa(Thread::Current());
    if (!gJdwpState->PostVMStart()) {
      LOG(WARNING) << "Failed to post 'start' message to debugger";
    }
  }
}

void Dbg::StopJdwp() {
  delete gJdwpState;
  delete gRegistry;
  gRegistry = NULL;
}

void Dbg::GcDidFinish() {
  if (gDdmHpifWhen != HPIF_WHEN_NEVER) {
    ScopedObjectAccess soa(Thread::Current());
    LOG(DEBUG) << "Sending heap info to DDM";
    DdmSendHeapInfo(gDdmHpifWhen);
  }
  if (gDdmHpsgWhen != HPSG_WHEN_NEVER) {
    ScopedObjectAccess soa(Thread::Current());
    LOG(DEBUG) << "Dumping heap to DDM";
    DdmSendHeapSegments(false);
  }
  if (gDdmNhsgWhen != HPSG_WHEN_NEVER) {
    ScopedObjectAccess soa(Thread::Current());
    LOG(DEBUG) << "Dumping native heap to DDM";
    DdmSendHeapSegments(true);
  }
}

void Dbg::SetJdwpAllowed(bool allowed) {
  gJdwpAllowed = allowed;
}

DebugInvokeReq* Dbg::GetInvokeReq() {
  return Thread::Current()->GetInvokeReq();
}

Thread* Dbg::GetDebugThread() {
  return (gJdwpState != NULL) ? gJdwpState->GetDebugThread() : NULL;
}

void Dbg::ClearWaitForEventThread() {
  gJdwpState->ClearWaitForEventThread();
}

void Dbg::Connected() {
  CHECK(!gDebuggerConnected);
  VLOG(jdwp) << "JDWP has attached";
  gDebuggerConnected = true;
  gDisposed = false;
}

void Dbg::Disposed() {
  gDisposed = true;
}

bool Dbg::IsDisposed() {
  return gDisposed;
}

void Dbg::GoActive() {
  // Enable all debugging features, including scans for breakpoints.
  // This is a no-op if we're already active.
  // Only called from the JDWP handler thread.
  if (gDebuggerActive) {
    return;
  }

  {
    // TODO: dalvik only warned if there were breakpoints left over. clear in Dbg::Disconnected?
    MutexLock mu(Thread::Current(), *Locks::breakpoint_lock_);
    CHECK_EQ(gBreakpoints.size(), 0U);
  }

  Runtime* runtime = Runtime::Current();
  runtime->GetThreadList()->SuspendAll();
  Thread* self = Thread::Current();
  ThreadState old_state = self->SetStateUnsafe(kRunnable);
  CHECK_NE(old_state, kRunnable);
  runtime->GetInstrumentation()->AddListener(&gDebugInstrumentationListener,
                                             instrumentation::Instrumentation::kMethodEntered |
                                             instrumentation::Instrumentation::kMethodExited |
                                             instrumentation::Instrumentation::kDexPcMoved |
                                             instrumentation::Instrumentation::kExceptionCaught);
  gDebuggerActive = true;
  CHECK_EQ(self->SetStateUnsafe(old_state), kRunnable);
  runtime->GetThreadList()->ResumeAll();

  LOG(INFO) << "Debugger is active";
}

void Dbg::Disconnected() {
  CHECK(gDebuggerConnected);

  LOG(INFO) << "Debugger is no longer active";

  // Suspend all threads and exclusively acquire the mutator lock. Set the state of the thread
  // to kRunnable to avoid scoped object access transitions. Remove the debugger as a listener
  // and clear the object registry.
  Runtime* runtime = Runtime::Current();
  runtime->GetThreadList()->SuspendAll();
  Thread* self = Thread::Current();
  ThreadState old_state = self->SetStateUnsafe(kRunnable);
  runtime->GetInstrumentation()->RemoveListener(&gDebugInstrumentationListener,
                                                instrumentation::Instrumentation::kMethodEntered |
                                                instrumentation::Instrumentation::kMethodExited |
                                                instrumentation::Instrumentation::kDexPcMoved |
                                                instrumentation::Instrumentation::kExceptionCaught);
  gDebuggerActive = false;
  gRegistry->Clear();
  gDebuggerConnected = false;
  CHECK_EQ(self->SetStateUnsafe(old_state), kRunnable);
  runtime->GetThreadList()->ResumeAll();
}

bool Dbg::IsDebuggerActive() {
  return gDebuggerActive;
}

bool Dbg::IsJdwpConfigured() {
  return gJdwpConfigured;
}

int64_t Dbg::LastDebuggerActivity() {
  return gJdwpState->LastDebuggerActivity();
}

void Dbg::UndoDebuggerSuspensions() {
  Runtime::Current()->GetThreadList()->UndoDebuggerSuspensions();
}

std::string Dbg::GetClassName(JDWP::RefTypeId class_id) {
  mirror::Object* o = gRegistry->Get<mirror::Object*>(class_id);
  if (o == NULL) {
    return "NULL";
  }
  if (o == ObjectRegistry::kInvalidObject) {
    return StringPrintf("invalid object %p", reinterpret_cast<void*>(class_id));
  }
  if (!o->IsClass()) {
    return StringPrintf("non-class %p", o);  // This is only used for debugging output anyway.
  }
  return DescriptorToName(ClassHelper(o->AsClass()).GetDescriptor());
}

JDWP::JdwpError Dbg::GetClassObject(JDWP::RefTypeId id, JDWP::ObjectId& class_object_id) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(id, status);
  if (c == NULL) {
    return status;
  }
  class_object_id = gRegistry->Add(c);
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetSuperclass(JDWP::RefTypeId id, JDWP::RefTypeId& superclass_id) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(id, status);
  if (c == NULL) {
    return status;
  }
  if (c->IsInterface()) {
    // http://code.google.com/p/android/issues/detail?id=20856
    superclass_id = 0;
  } else {
    superclass_id = gRegistry->Add(c->GetSuperClass());
  }
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetClassLoader(JDWP::RefTypeId id, JDWP::ExpandBuf* pReply) {
  mirror::Object* o = gRegistry->Get<mirror::Object*>(id);
  if (o == NULL || o == ObjectRegistry::kInvalidObject) {
    return JDWP::ERR_INVALID_OBJECT;
  }
  expandBufAddObjectId(pReply, gRegistry->Add(o->GetClass()->GetClassLoader()));
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetModifiers(JDWP::RefTypeId id, JDWP::ExpandBuf* pReply) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(id, status);
  if (c == NULL) {
    return status;
  }

  uint32_t access_flags = c->GetAccessFlags() & kAccJavaFlagsMask;

  // Set ACC_SUPER; dex files don't contain this flag, but all classes are supposed to have it set.
  // Class.getModifiers doesn't return it, but JDWP does, so we set it here.
  access_flags |= kAccSuper;

  expandBufAdd4BE(pReply, access_flags);

  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetMonitorInfo(JDWP::ObjectId object_id, JDWP::ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::Object* o = gRegistry->Get<mirror::Object*>(object_id);
  if (o == NULL || o == ObjectRegistry::kInvalidObject) {
    return JDWP::ERR_INVALID_OBJECT;
  }

  // Ensure all threads are suspended while we read objects' lock words.
  Thread* self = Thread::Current();
  Locks::mutator_lock_->SharedUnlock(self);
  Locks::mutator_lock_->ExclusiveLock(self);

  MonitorInfo monitor_info(o);

  Locks::mutator_lock_->ExclusiveUnlock(self);
  Locks::mutator_lock_->SharedLock(self);

  if (monitor_info.owner != NULL) {
    expandBufAddObjectId(reply, gRegistry->Add(monitor_info.owner->GetPeer()));
  } else {
    expandBufAddObjectId(reply, gRegistry->Add(NULL));
  }
  expandBufAdd4BE(reply, monitor_info.entry_count);
  expandBufAdd4BE(reply, monitor_info.waiters.size());
  for (size_t i = 0; i < monitor_info.waiters.size(); ++i) {
    expandBufAddObjectId(reply, gRegistry->Add(monitor_info.waiters[i]->GetPeer()));
  }
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetOwnedMonitors(JDWP::ObjectId thread_id,
                                      std::vector<JDWP::ObjectId>& monitors,
                                      std::vector<uint32_t>& stack_depths)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ScopedObjectAccessUnchecked soa(Thread::Current());
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread;
  JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
  if (error != JDWP::ERR_NONE) {
    return error;
  }
  if (!IsSuspendedForDebugger(soa, thread)) {
    return JDWP::ERR_THREAD_NOT_SUSPENDED;
  }

  struct OwnedMonitorVisitor : public StackVisitor {
    OwnedMonitorVisitor(Thread* thread, Context* context)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        : StackVisitor(thread, context), current_stack_depth(0) {}

    // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
    // annotalysis.
    bool VisitFrame() NO_THREAD_SAFETY_ANALYSIS {
      if (!GetMethod()->IsRuntimeMethod()) {
        Monitor::VisitLocks(this, AppendOwnedMonitors, this);
        ++current_stack_depth;
      }
      return true;
    }

    static void AppendOwnedMonitors(mirror::Object* owned_monitor, void* arg) {
      OwnedMonitorVisitor* visitor = reinterpret_cast<OwnedMonitorVisitor*>(arg);
      visitor->monitors.push_back(owned_monitor);
      visitor->stack_depths.push_back(visitor->current_stack_depth);
    }

    size_t current_stack_depth;
    std::vector<mirror::Object*> monitors;
    std::vector<uint32_t> stack_depths;
  };
  UniquePtr<Context> context(Context::Create());
  OwnedMonitorVisitor visitor(thread, context.get());
  visitor.WalkStack();

  for (size_t i = 0; i < visitor.monitors.size(); ++i) {
    monitors.push_back(gRegistry->Add(visitor.monitors[i]));
    stack_depths.push_back(visitor.stack_depths[i]);
  }

  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetContendedMonitor(JDWP::ObjectId thread_id, JDWP::ObjectId& contended_monitor)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ScopedObjectAccessUnchecked soa(Thread::Current());
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread;
  JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
  if (error != JDWP::ERR_NONE) {
    return error;
  }
  if (!IsSuspendedForDebugger(soa, thread)) {
    return JDWP::ERR_THREAD_NOT_SUSPENDED;
  }

  contended_monitor = gRegistry->Add(Monitor::GetContendedMonitor(thread));

  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetInstanceCounts(const std::vector<JDWP::RefTypeId>& class_ids,
                                       std::vector<uint64_t>& counts)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::vector<mirror::Class*> classes;
  counts.clear();
  for (size_t i = 0; i < class_ids.size(); ++i) {
    JDWP::JdwpError status;
    mirror::Class* c = DecodeClass(class_ids[i], status);
    if (c == NULL) {
      return status;
    }
    classes.push_back(c);
    counts.push_back(0);
  }

  Runtime::Current()->GetHeap()->CountInstances(classes, false, &counts[0]);
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetInstances(JDWP::RefTypeId class_id, int32_t max_count, std::vector<JDWP::ObjectId>& instances)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(class_id, status);
  if (c == NULL) {
    return status;
  }

  std::vector<mirror::Object*> raw_instances;
  Runtime::Current()->GetHeap()->GetInstances(c, max_count, raw_instances);
  for (size_t i = 0; i < raw_instances.size(); ++i) {
    instances.push_back(gRegistry->Add(raw_instances[i]));
  }
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetReferringObjects(JDWP::ObjectId object_id, int32_t max_count,
                                         std::vector<JDWP::ObjectId>& referring_objects)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::Object* o = gRegistry->Get<mirror::Object*>(object_id);
  if (o == NULL || o == ObjectRegistry::kInvalidObject) {
    return JDWP::ERR_INVALID_OBJECT;
  }

  std::vector<mirror::Object*> raw_instances;
  Runtime::Current()->GetHeap()->GetReferringObjects(o, max_count, raw_instances);
  for (size_t i = 0; i < raw_instances.size(); ++i) {
    referring_objects.push_back(gRegistry->Add(raw_instances[i]));
  }
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::DisableCollection(JDWP::ObjectId object_id)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  gRegistry->DisableCollection(object_id);
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::EnableCollection(JDWP::ObjectId object_id)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  gRegistry->EnableCollection(object_id);
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::IsCollected(JDWP::ObjectId object_id, bool& is_collected)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  is_collected = gRegistry->IsCollected(object_id);
  return JDWP::ERR_NONE;
}

void Dbg::DisposeObject(JDWP::ObjectId object_id, uint32_t reference_count)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  gRegistry->DisposeObject(object_id, reference_count);
}

JDWP::JdwpError Dbg::GetReflectedType(JDWP::RefTypeId class_id, JDWP::ExpandBuf* pReply) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(class_id, status);
  if (c == NULL) {
    return status;
  }

  expandBufAdd1(pReply, c->IsInterface() ? JDWP::TT_INTERFACE : JDWP::TT_CLASS);
  expandBufAddRefTypeId(pReply, class_id);
  return JDWP::ERR_NONE;
}

void Dbg::GetClassList(std::vector<JDWP::RefTypeId>& classes) {
  // Get the complete list of reference classes (i.e. all classes except
  // the primitive types).
  // Returns a newly-allocated buffer full of RefTypeId values.
  struct ClassListCreator {
    explicit ClassListCreator(std::vector<JDWP::RefTypeId>& classes) : classes(classes) {
    }

    static bool Visit(mirror::Class* c, void* arg) {
      return reinterpret_cast<ClassListCreator*>(arg)->Visit(c);
    }

    // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
    // annotalysis.
    bool Visit(mirror::Class* c) NO_THREAD_SAFETY_ANALYSIS {
      if (!c->IsPrimitive()) {
        classes.push_back(gRegistry->AddRefType(c));
      }
      return true;
    }

    std::vector<JDWP::RefTypeId>& classes;
  };

  ClassListCreator clc(classes);
  Runtime::Current()->GetClassLinker()->VisitClasses(ClassListCreator::Visit, &clc);
}

JDWP::JdwpError Dbg::GetClassInfo(JDWP::RefTypeId class_id, JDWP::JdwpTypeTag* pTypeTag, uint32_t* pStatus, std::string* pDescriptor) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(class_id, status);
  if (c == NULL) {
    return status;
  }

  if (c->IsArrayClass()) {
    *pStatus = JDWP::CS_VERIFIED | JDWP::CS_PREPARED;
    *pTypeTag = JDWP::TT_ARRAY;
  } else {
    if (c->IsErroneous()) {
      *pStatus = JDWP::CS_ERROR;
    } else {
      *pStatus = JDWP::CS_VERIFIED | JDWP::CS_PREPARED | JDWP::CS_INITIALIZED;
    }
    *pTypeTag = c->IsInterface() ? JDWP::TT_INTERFACE : JDWP::TT_CLASS;
  }

  if (pDescriptor != NULL) {
    *pDescriptor = ClassHelper(c).GetDescriptor();
  }
  return JDWP::ERR_NONE;
}

void Dbg::FindLoadedClassBySignature(const char* descriptor, std::vector<JDWP::RefTypeId>& ids) {
  std::vector<mirror::Class*> classes;
  Runtime::Current()->GetClassLinker()->LookupClasses(descriptor, classes);
  ids.clear();
  for (size_t i = 0; i < classes.size(); ++i) {
    ids.push_back(gRegistry->Add(classes[i]));
  }
}

JDWP::JdwpError Dbg::GetReferenceType(JDWP::ObjectId object_id, JDWP::ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::Object* o = gRegistry->Get<mirror::Object*>(object_id);
  if (o == NULL || o == ObjectRegistry::kInvalidObject) {
    return JDWP::ERR_INVALID_OBJECT;
  }

  JDWP::JdwpTypeTag type_tag;
  if (o->GetClass()->IsArrayClass()) {
    type_tag = JDWP::TT_ARRAY;
  } else if (o->GetClass()->IsInterface()) {
    type_tag = JDWP::TT_INTERFACE;
  } else {
    type_tag = JDWP::TT_CLASS;
  }
  JDWP::RefTypeId type_id = gRegistry->AddRefType(o->GetClass());

  expandBufAdd1(pReply, type_tag);
  expandBufAddRefTypeId(pReply, type_id);

  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetSignature(JDWP::RefTypeId class_id, std::string& signature) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(class_id, status);
  if (c == NULL) {
    return status;
  }
  signature = ClassHelper(c).GetDescriptor();
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetSourceFile(JDWP::RefTypeId class_id, std::string& result) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(class_id, status);
  if (c == NULL) {
    return status;
  }
  result = ClassHelper(c).GetSourceFile();
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetObjectTag(JDWP::ObjectId object_id, uint8_t& tag) {
  mirror::Object* o = gRegistry->Get<mirror::Object*>(object_id);
  if (o == ObjectRegistry::kInvalidObject) {
    return JDWP::ERR_INVALID_OBJECT;
  }
  tag = TagFromObject(o);
  return JDWP::ERR_NONE;
}

size_t Dbg::GetTagWidth(JDWP::JdwpTag tag) {
  switch (tag) {
  case JDWP::JT_VOID:
    return 0;
  case JDWP::JT_BYTE:
  case JDWP::JT_BOOLEAN:
    return 1;
  case JDWP::JT_CHAR:
  case JDWP::JT_SHORT:
    return 2;
  case JDWP::JT_FLOAT:
  case JDWP::JT_INT:
    return 4;
  case JDWP::JT_ARRAY:
  case JDWP::JT_OBJECT:
  case JDWP::JT_STRING:
  case JDWP::JT_THREAD:
  case JDWP::JT_THREAD_GROUP:
  case JDWP::JT_CLASS_LOADER:
  case JDWP::JT_CLASS_OBJECT:
    return sizeof(JDWP::ObjectId);
  case JDWP::JT_DOUBLE:
  case JDWP::JT_LONG:
    return 8;
  default:
    LOG(FATAL) << "Unknown tag " << tag;
    return -1;
  }
}

JDWP::JdwpError Dbg::GetArrayLength(JDWP::ObjectId array_id, int& length) {
  JDWP::JdwpError status;
  mirror::Array* a = DecodeArray(array_id, status);
  if (a == NULL) {
    return status;
  }
  length = a->GetLength();
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::OutputArray(JDWP::ObjectId array_id, int offset, int count, JDWP::ExpandBuf* pReply) {
  JDWP::JdwpError status;
  mirror::Array* a = DecodeArray(array_id, status);
  if (a == NULL) {
    return status;
  }

  if (offset < 0 || count < 0 || offset > a->GetLength() || a->GetLength() - offset < count) {
    LOG(WARNING) << __FUNCTION__ << " access out of bounds: offset=" << offset << "; count=" << count;
    return JDWP::ERR_INVALID_LENGTH;
  }
  std::string descriptor(ClassHelper(a->GetClass()).GetDescriptor());
  JDWP::JdwpTag tag = BasicTagFromDescriptor(descriptor.c_str() + 1);

  expandBufAdd1(pReply, tag);
  expandBufAdd4BE(pReply, count);

  if (IsPrimitiveTag(tag)) {
    size_t width = GetTagWidth(tag);
    uint8_t* dst = expandBufAddSpace(pReply, count * width);
    if (width == 8) {
      const uint64_t* src8 = reinterpret_cast<uint64_t*>(a->GetRawData(sizeof(uint64_t)));
      for (int i = 0; i < count; ++i) JDWP::Write8BE(&dst, src8[offset + i]);
    } else if (width == 4) {
      const uint32_t* src4 = reinterpret_cast<uint32_t*>(a->GetRawData(sizeof(uint32_t)));
      for (int i = 0; i < count; ++i) JDWP::Write4BE(&dst, src4[offset + i]);
    } else if (width == 2) {
      const uint16_t* src2 = reinterpret_cast<uint16_t*>(a->GetRawData(sizeof(uint16_t)));
      for (int i = 0; i < count; ++i) JDWP::Write2BE(&dst, src2[offset + i]);
    } else {
      const uint8_t* src = reinterpret_cast<uint8_t*>(a->GetRawData(sizeof(uint8_t)));
      memcpy(dst, &src[offset * width], count * width);
    }
  } else {
    mirror::ObjectArray<mirror::Object>* oa = a->AsObjectArray<mirror::Object>();
    for (int i = 0; i < count; ++i) {
      mirror::Object* element = oa->Get(offset + i);
      JDWP::JdwpTag specific_tag = (element != NULL) ? TagFromObject(element) : tag;
      expandBufAdd1(pReply, specific_tag);
      expandBufAddObjectId(pReply, gRegistry->Add(element));
    }
  }

  return JDWP::ERR_NONE;
}

template <typename T> void CopyArrayData(mirror::Array* a, JDWP::Request& src, int offset, int count) {
  DCHECK(a->GetClass()->IsPrimitiveArray());

  T* dst = &(reinterpret_cast<T*>(a->GetRawData(sizeof(T)))[offset * sizeof(T)]);
  for (int i = 0; i < count; ++i) {
    *dst++ = src.ReadValue(sizeof(T));
  }
}

JDWP::JdwpError Dbg::SetArrayElements(JDWP::ObjectId array_id, int offset, int count,
                                      JDWP::Request& request)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  JDWP::JdwpError status;
  mirror::Array* dst = DecodeArray(array_id, status);
  if (dst == NULL) {
    return status;
  }

  if (offset < 0 || count < 0 || offset > dst->GetLength() || dst->GetLength() - offset < count) {
    LOG(WARNING) << __FUNCTION__ << " access out of bounds: offset=" << offset << "; count=" << count;
    return JDWP::ERR_INVALID_LENGTH;
  }
  std::string descriptor(ClassHelper(dst->GetClass()).GetDescriptor());
  JDWP::JdwpTag tag = BasicTagFromDescriptor(descriptor.c_str() + 1);

  if (IsPrimitiveTag(tag)) {
    size_t width = GetTagWidth(tag);
    if (width == 8) {
      CopyArrayData<uint64_t>(dst, request, offset, count);
    } else if (width == 4) {
      CopyArrayData<uint32_t>(dst, request, offset, count);
    } else if (width == 2) {
      CopyArrayData<uint16_t>(dst, request, offset, count);
    } else {
      CopyArrayData<uint8_t>(dst, request, offset, count);
    }
  } else {
    mirror::ObjectArray<mirror::Object>* oa = dst->AsObjectArray<mirror::Object>();
    for (int i = 0; i < count; ++i) {
      JDWP::ObjectId id = request.ReadObjectId();
      mirror::Object* o = gRegistry->Get<mirror::Object*>(id);
      if (o == ObjectRegistry::kInvalidObject) {
        return JDWP::ERR_INVALID_OBJECT;
      }
      oa->Set(offset + i, o);
    }
  }

  return JDWP::ERR_NONE;
}

JDWP::ObjectId Dbg::CreateString(const std::string& str) {
  return gRegistry->Add(mirror::String::AllocFromModifiedUtf8(Thread::Current(), str.c_str()));
}

JDWP::JdwpError Dbg::CreateObject(JDWP::RefTypeId class_id, JDWP::ObjectId& new_object) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(class_id, status);
  if (c == NULL) {
    return status;
  }
  new_object = gRegistry->Add(c->AllocObject(Thread::Current()));
  return JDWP::ERR_NONE;
}

/*
 * Used by Eclipse's "Display" view to evaluate "new byte[5]" to get "(byte[]) [0, 0, 0, 0, 0]".
 */
JDWP::JdwpError Dbg::CreateArrayObject(JDWP::RefTypeId array_class_id, uint32_t length,
                                       JDWP::ObjectId& new_array) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(array_class_id, status);
  if (c == NULL) {
    return status;
  }
  new_array = gRegistry->Add(mirror::Array::Alloc(Thread::Current(), c, length));
  return JDWP::ERR_NONE;
}

bool Dbg::MatchType(JDWP::RefTypeId instance_class_id, JDWP::RefTypeId class_id) {
  JDWP::JdwpError status;
  mirror::Class* c1 = DecodeClass(instance_class_id, status);
  CHECK(c1 != NULL);
  mirror::Class* c2 = DecodeClass(class_id, status);
  CHECK(c2 != NULL);
  return c1->IsAssignableFrom(c2);
}

static JDWP::FieldId ToFieldId(const mirror::ArtField* f)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
#ifdef MOVING_GARBAGE_COLLECTOR
  UNIMPLEMENTED(FATAL);
#else
  return static_cast<JDWP::FieldId>(reinterpret_cast<uintptr_t>(f));
#endif
}

static JDWP::MethodId ToMethodId(const mirror::ArtMethod* m)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
#ifdef MOVING_GARBAGE_COLLECTOR
  UNIMPLEMENTED(FATAL);
#else
  return static_cast<JDWP::MethodId>(reinterpret_cast<uintptr_t>(m));
#endif
}

static mirror::ArtField* FromFieldId(JDWP::FieldId fid)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
#ifdef MOVING_GARBAGE_COLLECTOR
  UNIMPLEMENTED(FATAL);
#else
  return reinterpret_cast<mirror::ArtField*>(static_cast<uintptr_t>(fid));
#endif
}

static mirror::ArtMethod* FromMethodId(JDWP::MethodId mid)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
#ifdef MOVING_GARBAGE_COLLECTOR
  UNIMPLEMENTED(FATAL);
#else
  return reinterpret_cast<mirror::ArtMethod*>(static_cast<uintptr_t>(mid));
#endif
}

static void SetLocation(JDWP::JdwpLocation& location, mirror::ArtMethod* m, uint32_t dex_pc)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (m == NULL) {
    memset(&location, 0, sizeof(location));
  } else {
    mirror::Class* c = m->GetDeclaringClass();
    location.type_tag = c->IsInterface() ? JDWP::TT_INTERFACE : JDWP::TT_CLASS;
    location.class_id = gRegistry->Add(c);
    location.method_id = ToMethodId(m);
    location.dex_pc = dex_pc;
  }
}

std::string Dbg::GetMethodName(JDWP::MethodId method_id)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtMethod* m = FromMethodId(method_id);
  return MethodHelper(m).GetName();
}

std::string Dbg::GetFieldName(JDWP::FieldId field_id)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* f = FromFieldId(field_id);
  return FieldHelper(f).GetName();
}

/*
 * Augment the access flags for synthetic methods and fields by setting
 * the (as described by the spec) "0xf0000000 bit".  Also, strip out any
 * flags not specified by the Java programming language.
 */
static uint32_t MangleAccessFlags(uint32_t accessFlags) {
  accessFlags &= kAccJavaFlagsMask;
  if ((accessFlags & kAccSynthetic) != 0) {
    accessFlags |= 0xf0000000;
  }
  return accessFlags;
}

static const uint16_t kEclipseWorkaroundSlot = 1000;

/*
 * Eclipse appears to expect that the "this" reference is in slot zero.
 * If it's not, the "variables" display will show two copies of "this",
 * possibly because it gets "this" from SF.ThisObject and then displays
 * all locals with nonzero slot numbers.
 *
 * So, we remap the item in slot 0 to 1000, and remap "this" to zero.  On
 * SF.GetValues / SF.SetValues we map them back.
 *
 * TODO: jdb uses the value to determine whether a variable is a local or an argument,
 * by checking whether it's less than the number of arguments. To make that work, we'd
 * have to "mangle" all the arguments to come first, not just the implicit argument 'this'.
 */
static uint16_t MangleSlot(uint16_t slot, const char* name) {
  uint16_t newSlot = slot;
  if (strcmp(name, "this") == 0) {
    newSlot = 0;
  } else if (slot == 0) {
    newSlot = kEclipseWorkaroundSlot;
  }
  return newSlot;
}

static uint16_t DemangleSlot(uint16_t slot, mirror::ArtMethod* m)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (slot == kEclipseWorkaroundSlot) {
    return 0;
  } else if (slot == 0) {
    const DexFile::CodeItem* code_item = MethodHelper(m).GetCodeItem();
    CHECK(code_item != NULL) << PrettyMethod(m);
    return code_item->registers_size_ - code_item->ins_size_;
  }
  return slot;
}

JDWP::JdwpError Dbg::OutputDeclaredFields(JDWP::RefTypeId class_id, bool with_generic, JDWP::ExpandBuf* pReply) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(class_id, status);
  if (c == NULL) {
    return status;
  }

  size_t instance_field_count = c->NumInstanceFields();
  size_t static_field_count = c->NumStaticFields();

  expandBufAdd4BE(pReply, instance_field_count + static_field_count);

  for (size_t i = 0; i < instance_field_count + static_field_count; ++i) {
    mirror::ArtField* f = (i < instance_field_count) ? c->GetInstanceField(i) : c->GetStaticField(i - instance_field_count);
    FieldHelper fh(f);
    expandBufAddFieldId(pReply, ToFieldId(f));
    expandBufAddUtf8String(pReply, fh.GetName());
    expandBufAddUtf8String(pReply, fh.GetTypeDescriptor());
    if (with_generic) {
      static const char genericSignature[1] = "";
      expandBufAddUtf8String(pReply, genericSignature);
    }
    expandBufAdd4BE(pReply, MangleAccessFlags(f->GetAccessFlags()));
  }
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::OutputDeclaredMethods(JDWP::RefTypeId class_id, bool with_generic,
                                           JDWP::ExpandBuf* pReply) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(class_id, status);
  if (c == NULL) {
    return status;
  }

  size_t direct_method_count = c->NumDirectMethods();
  size_t virtual_method_count = c->NumVirtualMethods();

  expandBufAdd4BE(pReply, direct_method_count + virtual_method_count);

  for (size_t i = 0; i < direct_method_count + virtual_method_count; ++i) {
    mirror::ArtMethod* m = (i < direct_method_count) ? c->GetDirectMethod(i) : c->GetVirtualMethod(i - direct_method_count);
    MethodHelper mh(m);
    expandBufAddMethodId(pReply, ToMethodId(m));
    expandBufAddUtf8String(pReply, mh.GetName());
    expandBufAddUtf8String(pReply, mh.GetSignature());
    if (with_generic) {
      static const char genericSignature[1] = "";
      expandBufAddUtf8String(pReply, genericSignature);
    }
    expandBufAdd4BE(pReply, MangleAccessFlags(m->GetAccessFlags()));
  }
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::OutputDeclaredInterfaces(JDWP::RefTypeId class_id, JDWP::ExpandBuf* pReply) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(class_id, status);
  if (c == NULL) {
    return status;
  }

  ClassHelper kh(c);
  size_t interface_count = kh.NumDirectInterfaces();
  expandBufAdd4BE(pReply, interface_count);
  for (size_t i = 0; i < interface_count; ++i) {
    expandBufAddRefTypeId(pReply, gRegistry->AddRefType(kh.GetDirectInterface(i)));
  }
  return JDWP::ERR_NONE;
}

void Dbg::OutputLineTable(JDWP::RefTypeId, JDWP::MethodId method_id, JDWP::ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  struct DebugCallbackContext {
    int numItems;
    JDWP::ExpandBuf* pReply;

    static bool Callback(void* context, uint32_t address, uint32_t line_number) {
      DebugCallbackContext* pContext = reinterpret_cast<DebugCallbackContext*>(context);
      expandBufAdd8BE(pContext->pReply, address);
      expandBufAdd4BE(pContext->pReply, line_number);
      pContext->numItems++;
      return false;
    }
  };
  mirror::ArtMethod* m = FromMethodId(method_id);
  MethodHelper mh(m);
  uint64_t start, end;
  if (m->IsNative()) {
    start = -1;
    end = -1;
  } else {
    start = 0;
    // Return the index of the last instruction
    end = mh.GetCodeItem()->insns_size_in_code_units_ - 1;
  }

  expandBufAdd8BE(pReply, start);
  expandBufAdd8BE(pReply, end);

  // Add numLines later
  size_t numLinesOffset = expandBufGetLength(pReply);
  expandBufAdd4BE(pReply, 0);

  DebugCallbackContext context;
  context.numItems = 0;
  context.pReply = pReply;

  mh.GetDexFile().DecodeDebugInfo(mh.GetCodeItem(), m->IsStatic(), m->GetDexMethodIndex(),
                                  DebugCallbackContext::Callback, NULL, &context);

  JDWP::Set4BE(expandBufGetBuffer(pReply) + numLinesOffset, context.numItems);
}

void Dbg::OutputVariableTable(JDWP::RefTypeId, JDWP::MethodId method_id, bool with_generic, JDWP::ExpandBuf* pReply) {
  struct DebugCallbackContext {
    JDWP::ExpandBuf* pReply;
    size_t variable_count;
    bool with_generic;

    static void Callback(void* context, uint16_t slot, uint32_t startAddress, uint32_t endAddress, const char* name, const char* descriptor, const char* signature) {
      DebugCallbackContext* pContext = reinterpret_cast<DebugCallbackContext*>(context);

      VLOG(jdwp) << StringPrintf("    %2zd: %d(%d) '%s' '%s' '%s' actual slot=%d mangled slot=%d", pContext->variable_count, startAddress, endAddress - startAddress, name, descriptor, signature, slot, MangleSlot(slot, name));

      slot = MangleSlot(slot, name);

      expandBufAdd8BE(pContext->pReply, startAddress);
      expandBufAddUtf8String(pContext->pReply, name);
      expandBufAddUtf8String(pContext->pReply, descriptor);
      if (pContext->with_generic) {
        expandBufAddUtf8String(pContext->pReply, signature);
      }
      expandBufAdd4BE(pContext->pReply, endAddress - startAddress);
      expandBufAdd4BE(pContext->pReply, slot);

      ++pContext->variable_count;
    }
  };
  mirror::ArtMethod* m = FromMethodId(method_id);
  MethodHelper mh(m);
  const DexFile::CodeItem* code_item = mh.GetCodeItem();

  // arg_count considers doubles and longs to take 2 units.
  // variable_count considers everything to take 1 unit.
  std::string shorty(mh.GetShorty());
  expandBufAdd4BE(pReply, mirror::ArtMethod::NumArgRegisters(shorty));

  // We don't know the total number of variables yet, so leave a blank and update it later.
  size_t variable_count_offset = expandBufGetLength(pReply);
  expandBufAdd4BE(pReply, 0);

  DebugCallbackContext context;
  context.pReply = pReply;
  context.variable_count = 0;
  context.with_generic = with_generic;

  mh.GetDexFile().DecodeDebugInfo(code_item, m->IsStatic(), m->GetDexMethodIndex(), NULL,
                                  DebugCallbackContext::Callback, &context);

  JDWP::Set4BE(expandBufGetBuffer(pReply) + variable_count_offset, context.variable_count);
}

JDWP::JdwpError Dbg::GetBytecodes(JDWP::RefTypeId, JDWP::MethodId method_id,
                                  std::vector<uint8_t>& bytecodes)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtMethod* m = FromMethodId(method_id);
  if (m == NULL) {
    return JDWP::ERR_INVALID_METHODID;
  }
  MethodHelper mh(m);
  const DexFile::CodeItem* code_item = mh.GetCodeItem();
  size_t byte_count = code_item->insns_size_in_code_units_ * 2;
  const uint8_t* begin = reinterpret_cast<const uint8_t*>(code_item->insns_);
  const uint8_t* end = begin + byte_count;
  for (const uint8_t* p = begin; p != end; ++p) {
    bytecodes.push_back(*p);
  }
  return JDWP::ERR_NONE;
}

JDWP::JdwpTag Dbg::GetFieldBasicTag(JDWP::FieldId field_id) {
  return BasicTagFromDescriptor(FieldHelper(FromFieldId(field_id)).GetTypeDescriptor());
}

JDWP::JdwpTag Dbg::GetStaticFieldBasicTag(JDWP::FieldId field_id) {
  return BasicTagFromDescriptor(FieldHelper(FromFieldId(field_id)).GetTypeDescriptor());
}

static JDWP::JdwpError GetFieldValueImpl(JDWP::RefTypeId ref_type_id, JDWP::ObjectId object_id,
                                         JDWP::FieldId field_id, JDWP::ExpandBuf* pReply,
                                         bool is_static)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  JDWP::JdwpError status;
  mirror::Class* c = DecodeClass(ref_type_id, status);
  if (ref_type_id != 0 && c == NULL) {
    return status;
  }

  mirror::Object* o = gRegistry->Get<mirror::Object*>(object_id);
  if ((!is_static && o == NULL) || o == ObjectRegistry::kInvalidObject) {
    return JDWP::ERR_INVALID_OBJECT;
  }
  mirror::ArtField* f = FromFieldId(field_id);

  mirror::Class* receiver_class = c;
  if (receiver_class == NULL && o != NULL) {
    receiver_class = o->GetClass();
  }
  // TODO: should we give up now if receiver_class is NULL?
  if (receiver_class != NULL && !f->GetDeclaringClass()->IsAssignableFrom(receiver_class)) {
    LOG(INFO) << "ERR_INVALID_FIELDID: " << PrettyField(f) << " " << PrettyClass(receiver_class);
    return JDWP::ERR_INVALID_FIELDID;
  }

  // The RI only enforces the static/non-static mismatch in one direction.
  // TODO: should we change the tests and check both?
  if (is_static) {
    if (!f->IsStatic()) {
      return JDWP::ERR_INVALID_FIELDID;
    }
  } else {
    if (f->IsStatic()) {
      LOG(WARNING) << "Ignoring non-NULL receiver for ObjectReference.SetValues on static field " << PrettyField(f);
    }
  }
  if (f->IsStatic()) {
    o = f->GetDeclaringClass();
  }

  JDWP::JdwpTag tag = BasicTagFromDescriptor(FieldHelper(f).GetTypeDescriptor());

  if (IsPrimitiveTag(tag)) {
    expandBufAdd1(pReply, tag);
    if (tag == JDWP::JT_BOOLEAN || tag == JDWP::JT_BYTE) {
      expandBufAdd1(pReply, f->Get32(o));
    } else if (tag == JDWP::JT_CHAR || tag == JDWP::JT_SHORT) {
      expandBufAdd2BE(pReply, f->Get32(o));
    } else if (tag == JDWP::JT_FLOAT || tag == JDWP::JT_INT) {
      expandBufAdd4BE(pReply, f->Get32(o));
    } else if (tag == JDWP::JT_DOUBLE || tag == JDWP::JT_LONG) {
      expandBufAdd8BE(pReply, f->Get64(o));
    } else {
      LOG(FATAL) << "Unknown tag: " << tag;
    }
  } else {
    mirror::Object* value = f->GetObject(o);
    expandBufAdd1(pReply, TagFromObject(value));
    expandBufAddObjectId(pReply, gRegistry->Add(value));
  }
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetFieldValue(JDWP::ObjectId object_id, JDWP::FieldId field_id,
                                   JDWP::ExpandBuf* pReply) {
  return GetFieldValueImpl(0, object_id, field_id, pReply, false);
}

JDWP::JdwpError Dbg::GetStaticFieldValue(JDWP::RefTypeId ref_type_id, JDWP::FieldId field_id, JDWP::ExpandBuf* pReply) {
  return GetFieldValueImpl(ref_type_id, 0, field_id, pReply, true);
}

static JDWP::JdwpError SetFieldValueImpl(JDWP::ObjectId object_id, JDWP::FieldId field_id,
                                         uint64_t value, int width, bool is_static)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::Object* o = gRegistry->Get<mirror::Object*>(object_id);
  if ((!is_static && o == NULL) || o == ObjectRegistry::kInvalidObject) {
    return JDWP::ERR_INVALID_OBJECT;
  }
  mirror::ArtField* f = FromFieldId(field_id);

  // The RI only enforces the static/non-static mismatch in one direction.
  // TODO: should we change the tests and check both?
  if (is_static) {
    if (!f->IsStatic()) {
      return JDWP::ERR_INVALID_FIELDID;
    }
  } else {
    if (f->IsStatic()) {
      LOG(WARNING) << "Ignoring non-NULL receiver for ObjectReference.SetValues on static field " << PrettyField(f);
    }
  }
  if (f->IsStatic()) {
    o = f->GetDeclaringClass();
  }

  JDWP::JdwpTag tag = BasicTagFromDescriptor(FieldHelper(f).GetTypeDescriptor());

  if (IsPrimitiveTag(tag)) {
    if (tag == JDWP::JT_DOUBLE || tag == JDWP::JT_LONG) {
      CHECK_EQ(width, 8);
      f->Set64(o, value);
    } else {
      CHECK_LE(width, 4);
      f->Set32(o, value);
    }
  } else {
    mirror::Object* v = gRegistry->Get<mirror::Object*>(value);
    if (v == ObjectRegistry::kInvalidObject) {
      return JDWP::ERR_INVALID_OBJECT;
    }
    if (v != NULL) {
      mirror::Class* field_type = FieldHelper(f).GetType();
      if (!field_type->IsAssignableFrom(v->GetClass())) {
        return JDWP::ERR_INVALID_OBJECT;
      }
    }
    f->SetObject(o, v);
  }

  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::SetFieldValue(JDWP::ObjectId object_id, JDWP::FieldId field_id, uint64_t value,
                                   int width) {
  return SetFieldValueImpl(object_id, field_id, value, width, false);
}

JDWP::JdwpError Dbg::SetStaticFieldValue(JDWP::FieldId field_id, uint64_t value, int width) {
  return SetFieldValueImpl(0, field_id, value, width, true);
}

std::string Dbg::StringToUtf8(JDWP::ObjectId string_id) {
  mirror::String* s = gRegistry->Get<mirror::String*>(string_id);
  return s->ToModifiedUtf8();
}

JDWP::JdwpError Dbg::GetThreadName(JDWP::ObjectId thread_id, std::string& name) {
  ScopedObjectAccessUnchecked soa(Thread::Current());
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread;
  JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
  if (error != JDWP::ERR_NONE && error != JDWP::ERR_THREAD_NOT_ALIVE) {
    return error;
  }

  // We still need to report the zombie threads' names, so we can't just call Thread::GetThreadName.
  mirror::Object* thread_object = gRegistry->Get<mirror::Object*>(thread_id);
  mirror::ArtField* java_lang_Thread_name_field =
      soa.DecodeField(WellKnownClasses::java_lang_Thread_name);
  mirror::String* s =
      reinterpret_cast<mirror::String*>(java_lang_Thread_name_field->GetObject(thread_object));
  if (s != NULL) {
    name = s->ToModifiedUtf8();
  }
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetThreadGroup(JDWP::ObjectId thread_id, JDWP::ExpandBuf* pReply) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::Object* thread_object = gRegistry->Get<mirror::Object*>(thread_id);
  if (thread_object == ObjectRegistry::kInvalidObject) {
    return JDWP::ERR_INVALID_OBJECT;
  }

  // Okay, so it's an object, but is it actually a thread?
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread;
  JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
  if (error == JDWP::ERR_THREAD_NOT_ALIVE) {
    // Zombie threads are in the null group.
    expandBufAddObjectId(pReply, JDWP::ObjectId(0));
    return JDWP::ERR_NONE;
  }
  if (error != JDWP::ERR_NONE) {
    return error;
  }

  mirror::Class* c = Runtime::Current()->GetClassLinker()->FindSystemClass("Ljava/lang/Thread;");
  CHECK(c != NULL);
  mirror::ArtField* f = c->FindInstanceField("group", "Ljava/lang/ThreadGroup;");
  CHECK(f != NULL);
  mirror::Object* group = f->GetObject(thread_object);
  CHECK(group != NULL);
  JDWP::ObjectId thread_group_id = gRegistry->Add(group);

  expandBufAddObjectId(pReply, thread_group_id);
  return JDWP::ERR_NONE;
}

std::string Dbg::GetThreadGroupName(JDWP::ObjectId thread_group_id) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::Object* thread_group = gRegistry->Get<mirror::Object*>(thread_group_id);
  CHECK(thread_group != NULL);

  mirror::Class* c = Runtime::Current()->GetClassLinker()->FindSystemClass("Ljava/lang/ThreadGroup;");
  CHECK(c != NULL);
  mirror::ArtField* f = c->FindInstanceField("name", "Ljava/lang/String;");
  CHECK(f != NULL);
  mirror::String* s = reinterpret_cast<mirror::String*>(f->GetObject(thread_group));
  return s->ToModifiedUtf8();
}

JDWP::ObjectId Dbg::GetThreadGroupParent(JDWP::ObjectId thread_group_id) {
  mirror::Object* thread_group = gRegistry->Get<mirror::Object*>(thread_group_id);
  CHECK(thread_group != NULL);

  mirror::Class* c = Runtime::Current()->GetClassLinker()->FindSystemClass("Ljava/lang/ThreadGroup;");
  CHECK(c != NULL);
  mirror::ArtField* f = c->FindInstanceField("parent", "Ljava/lang/ThreadGroup;");
  CHECK(f != NULL);
  mirror::Object* parent = f->GetObject(thread_group);
  return gRegistry->Add(parent);
}

JDWP::ObjectId Dbg::GetSystemThreadGroupId() {
  ScopedObjectAccessUnchecked soa(Thread::Current());
  mirror::ArtField* f = soa.DecodeField(WellKnownClasses::java_lang_ThreadGroup_systemThreadGroup);
  mirror::Object* group = f->GetObject(f->GetDeclaringClass());
  return gRegistry->Add(group);
}

JDWP::ObjectId Dbg::GetMainThreadGroupId() {
  ScopedObjectAccess soa(Thread::Current());
  mirror::ArtField* f = soa.DecodeField(WellKnownClasses::java_lang_ThreadGroup_mainThreadGroup);
  mirror::Object* group = f->GetObject(f->GetDeclaringClass());
  return gRegistry->Add(group);
}

JDWP::JdwpThreadStatus Dbg::ToJdwpThreadStatus(ThreadState state) {
  switch (state) {
    case kBlocked:
      return JDWP::TS_MONITOR;
    case kNative:
    case kRunnable:
    case kSuspended:
      return JDWP::TS_RUNNING;
    case kSleeping:
      return JDWP::TS_SLEEPING;
    case kStarting:
    case kTerminated:
      return JDWP::TS_ZOMBIE;
    case kTimedWaiting:
    case kWaitingForDebuggerSend:
    case kWaitingForDebuggerSuspension:
    case kWaitingForDebuggerToAttach:
    case kWaitingForGcToComplete:
    case kWaitingForCheckPointsToRun:
    case kWaitingForJniOnLoad:
    case kWaitingForSignalCatcherOutput:
    case kWaitingInMainDebuggerLoop:
    case kWaitingInMainSignalCatcherLoop:
    case kWaitingPerformingGc:
    case kWaiting:
      return JDWP::TS_WAIT;
      // Don't add a 'default' here so the compiler can spot incompatible enum changes.
  }
  LOG(FATAL) << "Unknown thread state: " << state;
  return JDWP::TS_ZOMBIE;
}

JDWP::JdwpError Dbg::GetThreadStatus(JDWP::ObjectId thread_id, JDWP::JdwpThreadStatus* pThreadStatus, JDWP::JdwpSuspendStatus* pSuspendStatus) {
  ScopedObjectAccess soa(Thread::Current());

  *pSuspendStatus = JDWP::SUSPEND_STATUS_NOT_SUSPENDED;

  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread;
  JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
  if (error != JDWP::ERR_NONE) {
    if (error == JDWP::ERR_THREAD_NOT_ALIVE) {
      *pThreadStatus = JDWP::TS_ZOMBIE;
      return JDWP::ERR_NONE;
    }
    return error;
  }

  if (IsSuspendedForDebugger(soa, thread)) {
    *pSuspendStatus = JDWP::SUSPEND_STATUS_SUSPENDED;
  }

  *pThreadStatus = ToJdwpThreadStatus(thread->GetState());
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetThreadDebugSuspendCount(JDWP::ObjectId thread_id, JDWP::ExpandBuf* pReply) {
  ScopedObjectAccess soa(Thread::Current());
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread;
  JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
  if (error != JDWP::ERR_NONE) {
    return error;
  }
  MutexLock mu2(soa.Self(), *Locks::thread_suspend_count_lock_);
  expandBufAdd4BE(pReply, thread->GetDebugSuspendCount());
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::Interrupt(JDWP::ObjectId thread_id) {
  ScopedObjectAccess soa(Thread::Current());
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread;
  JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
  if (error != JDWP::ERR_NONE) {
    return error;
  }
  thread->Interrupt();
  return JDWP::ERR_NONE;
}

void Dbg::GetThreads(JDWP::ObjectId thread_group_id, std::vector<JDWP::ObjectId>& thread_ids) {
  class ThreadListVisitor {
   public:
    ThreadListVisitor(const ScopedObjectAccessUnchecked& soa, mirror::Object* desired_thread_group,
                      std::vector<JDWP::ObjectId>& thread_ids)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        : soa_(soa), desired_thread_group_(desired_thread_group), thread_ids_(thread_ids) {}

    static void Visit(Thread* t, void* arg) {
      reinterpret_cast<ThreadListVisitor*>(arg)->Visit(t);
    }

    // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
    // annotalysis.
    void Visit(Thread* t) NO_THREAD_SAFETY_ANALYSIS {
      if (t == Dbg::GetDebugThread()) {
        // Skip the JDWP thread. Some debuggers get bent out of shape when they can't suspend and
        // query all threads, so it's easier if we just don't tell them about this thread.
        return;
      }
      mirror::Object* peer = t->GetPeer();
      if (IsInDesiredThreadGroup(peer)) {
        thread_ids_.push_back(gRegistry->Add(peer));
      }
    }

   private:
    bool IsInDesiredThreadGroup(mirror::Object* peer)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      // peer might be NULL if the thread is still starting up.
      if (peer == NULL) {
        // We can't tell the debugger about this thread yet.
        // TODO: if we identified threads to the debugger by their Thread*
        // rather than their peer's mirror::Object*, we could fix this.
        // Doing so might help us report ZOMBIE threads too.
        return false;
      }
      // Do we want threads from all thread groups?
      if (desired_thread_group_ == NULL) {
        return true;
      }
      mirror::Object* group = soa_.DecodeField(WellKnownClasses::java_lang_Thread_group)->GetObject(peer);
      return (group == desired_thread_group_);
    }

    const ScopedObjectAccessUnchecked& soa_;
    mirror::Object* const desired_thread_group_;
    std::vector<JDWP::ObjectId>& thread_ids_;
  };

  ScopedObjectAccessUnchecked soa(Thread::Current());
  mirror::Object* thread_group = gRegistry->Get<mirror::Object*>(thread_group_id);
  ThreadListVisitor tlv(soa, thread_group, thread_ids);
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Runtime::Current()->GetThreadList()->ForEach(ThreadListVisitor::Visit, &tlv);
}

void Dbg::GetChildThreadGroups(JDWP::ObjectId thread_group_id, std::vector<JDWP::ObjectId>& child_thread_group_ids) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::Object* thread_group = gRegistry->Get<mirror::Object*>(thread_group_id);

  // Get the ArrayList<ThreadGroup> "groups" out of this thread group...
  mirror::ArtField* groups_field = thread_group->GetClass()->FindInstanceField("groups", "Ljava/util/List;");
  mirror::Object* groups_array_list = groups_field->GetObject(thread_group);

  // Get the array and size out of the ArrayList<ThreadGroup>...
  mirror::ArtField* array_field = groups_array_list->GetClass()->FindInstanceField("array", "[Ljava/lang/Object;");
  mirror::ArtField* size_field = groups_array_list->GetClass()->FindInstanceField("size", "I");
  mirror::ObjectArray<mirror::Object>* groups_array =
      array_field->GetObject(groups_array_list)->AsObjectArray<mirror::Object>();
  const int32_t size = size_field->GetInt(groups_array_list);

  // Copy the first 'size' elements out of the array into the result.
  for (int32_t i = 0; i < size; ++i) {
    child_thread_group_ids.push_back(gRegistry->Add(groups_array->Get(i)));
  }
}

static int GetStackDepth(Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  struct CountStackDepthVisitor : public StackVisitor {
    explicit CountStackDepthVisitor(Thread* thread)
        : StackVisitor(thread, NULL), depth(0) {}

    // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
    // annotalysis.
    bool VisitFrame() NO_THREAD_SAFETY_ANALYSIS {
      if (!GetMethod()->IsRuntimeMethod()) {
        ++depth;
      }
      return true;
    }
    size_t depth;
  };

  CountStackDepthVisitor visitor(thread);
  visitor.WalkStack();
  return visitor.depth;
}

JDWP::JdwpError Dbg::GetThreadFrameCount(JDWP::ObjectId thread_id, size_t& result) {
  ScopedObjectAccess soa(Thread::Current());
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread;
  JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
  if (error != JDWP::ERR_NONE) {
    return error;
  }
  if (!IsSuspendedForDebugger(soa, thread)) {
    return JDWP::ERR_THREAD_NOT_SUSPENDED;
  }
  result = GetStackDepth(thread);
  return JDWP::ERR_NONE;
}

JDWP::JdwpError Dbg::GetThreadFrames(JDWP::ObjectId thread_id, size_t start_frame,
                                     size_t frame_count, JDWP::ExpandBuf* buf) {
  class GetFrameVisitor : public StackVisitor {
   public:
    GetFrameVisitor(Thread* thread, size_t start_frame, size_t frame_count, JDWP::ExpandBuf* buf)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        : StackVisitor(thread, NULL), depth_(0),
          start_frame_(start_frame), frame_count_(frame_count), buf_(buf) {
      expandBufAdd4BE(buf_, frame_count_);
    }

    // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
    // annotalysis.
    virtual bool VisitFrame() NO_THREAD_SAFETY_ANALYSIS {
      if (GetMethod()->IsRuntimeMethod()) {
        return true;  // The debugger can't do anything useful with a frame that has no Method*.
      }
      if (depth_ >= start_frame_ + frame_count_) {
        return false;
      }
      if (depth_ >= start_frame_) {
        JDWP::FrameId frame_id(GetFrameId());
        JDWP::JdwpLocation location;
        SetLocation(location, GetMethod(), GetDexPc());
        VLOG(jdwp) << StringPrintf("    Frame %3zd: id=%3lld ", depth_, frame_id) << location;
        expandBufAdd8BE(buf_, frame_id);
        expandBufAddLocation(buf_, location);
      }
      ++depth_;
      return true;
    }

   private:
    size_t depth_;
    const size_t start_frame_;
    const size_t frame_count_;
    JDWP::ExpandBuf* buf_;
  };

  ScopedObjectAccessUnchecked soa(Thread::Current());
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread;
  JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
  if (error != JDWP::ERR_NONE) {
    return error;
  }
  if (!IsSuspendedForDebugger(soa, thread)) {
    return JDWP::ERR_THREAD_NOT_SUSPENDED;
  }
  GetFrameVisitor visitor(thread, start_frame, frame_count, buf);
  visitor.WalkStack();
  return JDWP::ERR_NONE;
}

JDWP::ObjectId Dbg::GetThreadSelfId() {
  ScopedObjectAccessUnchecked soa(Thread::Current());
  return gRegistry->Add(soa.Self()->GetPeer());
}

void Dbg::SuspendVM() {
  Runtime::Current()->GetThreadList()->SuspendAllForDebugger();
}

void Dbg::ResumeVM() {
  Runtime::Current()->GetThreadList()->UndoDebuggerSuspensions();
}

JDWP::JdwpError Dbg::SuspendThread(JDWP::ObjectId thread_id, bool request_suspension) {
  ScopedLocalRef<jobject> peer(Thread::Current()->GetJniEnv(), NULL);
  {
    ScopedObjectAccess soa(Thread::Current());
    peer.reset(soa.AddLocalReference<jobject>(gRegistry->Get<mirror::Object*>(thread_id)));
  }
  if (peer.get() == NULL) {
    return JDWP::ERR_THREAD_NOT_ALIVE;
  }
  // Suspend thread to build stack trace.
  bool timed_out;
  Thread* thread = Thread::SuspendForDebugger(peer.get(), request_suspension, &timed_out);
  if (thread != NULL) {
    return JDWP::ERR_NONE;
  } else if (timed_out) {
    return JDWP::ERR_INTERNAL;
  } else {
    return JDWP::ERR_THREAD_NOT_ALIVE;
  }
}

void Dbg::ResumeThread(JDWP::ObjectId thread_id) {
  ScopedObjectAccessUnchecked soa(Thread::Current());
  mirror::Object* peer = gRegistry->Get<mirror::Object*>(thread_id);
  Thread* thread;
  {
    MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
    thread = Thread::FromManagedThread(soa, peer);
  }
  if (thread == NULL) {
    LOG(WARNING) << "No such thread for resume: " << peer;
    return;
  }
  bool needs_resume;
  {
    MutexLock mu2(soa.Self(), *Locks::thread_suspend_count_lock_);
    needs_resume = thread->GetSuspendCount() > 0;
  }
  if (needs_resume) {
    Runtime::Current()->GetThreadList()->Resume(thread, true);
  }
}

void Dbg::SuspendSelf() {
  Runtime::Current()->GetThreadList()->SuspendSelfForDebugger();
}

struct GetThisVisitor : public StackVisitor {
  GetThisVisitor(Thread* thread, Context* context, JDWP::FrameId frame_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(thread, context), this_object(NULL), frame_id(frame_id) {}

  // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
  // annotalysis.
  virtual bool VisitFrame() NO_THREAD_SAFETY_ANALYSIS {
    if (frame_id != GetFrameId()) {
      return true;  // continue
    } else {
      this_object = GetThisObject();
      return false;
    }
  }

  mirror::Object* this_object;
  JDWP::FrameId frame_id;
};

JDWP::JdwpError Dbg::GetThisObject(JDWP::ObjectId thread_id, JDWP::FrameId frame_id,
                                   JDWP::ObjectId* result) {
  ScopedObjectAccessUnchecked soa(Thread::Current());
  Thread* thread;
  {
    MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
    JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
    if (error != JDWP::ERR_NONE) {
      return error;
    }
    if (!IsSuspendedForDebugger(soa, thread)) {
      return JDWP::ERR_THREAD_NOT_SUSPENDED;
    }
  }
  UniquePtr<Context> context(Context::Create());
  GetThisVisitor visitor(thread, context.get(), frame_id);
  visitor.WalkStack();
  *result = gRegistry->Add(visitor.this_object);
  return JDWP::ERR_NONE;
}

void Dbg::GetLocalValue(JDWP::ObjectId thread_id, JDWP::FrameId frame_id, int slot, JDWP::JdwpTag tag,
                        uint8_t* buf, size_t width) {
  struct GetLocalVisitor : public StackVisitor {
    GetLocalVisitor(Thread* thread, Context* context, JDWP::FrameId frame_id, int slot,
                    JDWP::JdwpTag tag, uint8_t* buf, size_t width)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        : StackVisitor(thread, context), frame_id_(frame_id), slot_(slot), tag_(tag),
          buf_(buf), width_(width) {}

    // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
    // annotalysis.
    bool VisitFrame() NO_THREAD_SAFETY_ANALYSIS {
      if (GetFrameId() != frame_id_) {
        return true;  // Not our frame, carry on.
      }
      // TODO: check that the tag is compatible with the actual type of the slot!
      mirror::ArtMethod* m = GetMethod();
      uint16_t reg = DemangleSlot(slot_, m);

      switch (tag_) {
      case JDWP::JT_BOOLEAN:
        {
          CHECK_EQ(width_, 1U);
          uint32_t intVal = GetVReg(m, reg, kIntVReg);
          VLOG(jdwp) << "get boolean local " << reg << " = " << intVal;
          JDWP::Set1(buf_+1, intVal != 0);
        }
        break;
      case JDWP::JT_BYTE:
        {
          CHECK_EQ(width_, 1U);
          uint32_t intVal = GetVReg(m, reg, kIntVReg);
          VLOG(jdwp) << "get byte local " << reg << " = " << intVal;
          JDWP::Set1(buf_+1, intVal);
        }
        break;
      case JDWP::JT_SHORT:
      case JDWP::JT_CHAR:
        {
          CHECK_EQ(width_, 2U);
          uint32_t intVal = GetVReg(m, reg, kIntVReg);
          VLOG(jdwp) << "get short/char local " << reg << " = " << intVal;
          JDWP::Set2BE(buf_+1, intVal);
        }
        break;
      case JDWP::JT_INT:
        {
          CHECK_EQ(width_, 4U);
          uint32_t intVal = GetVReg(m, reg, kIntVReg);
          VLOG(jdwp) << "get int local " << reg << " = " << intVal;
          JDWP::Set4BE(buf_+1, intVal);
        }
        break;
      case JDWP::JT_FLOAT:
        {
          CHECK_EQ(width_, 4U);
          uint32_t intVal = GetVReg(m, reg, kFloatVReg);
          VLOG(jdwp) << "get int/float local " << reg << " = " << intVal;
          JDWP::Set4BE(buf_+1, intVal);
        }
        break;
      case JDWP::JT_ARRAY:
        {
          CHECK_EQ(width_, sizeof(JDWP::ObjectId));
          mirror::Object* o = reinterpret_cast<mirror::Object*>(GetVReg(m, reg, kReferenceVReg));
          VLOG(jdwp) << "get array local " << reg << " = " << o;
          if (!Runtime::Current()->GetHeap()->IsHeapAddress(o)) {
            LOG(FATAL) << "Register " << reg << " expected to hold array: " << o;
          }
          JDWP::SetObjectId(buf_+1, gRegistry->Add(o));
        }
        break;
      case JDWP::JT_CLASS_LOADER:
      case JDWP::JT_CLASS_OBJECT:
      case JDWP::JT_OBJECT:
      case JDWP::JT_STRING:
      case JDWP::JT_THREAD:
      case JDWP::JT_THREAD_GROUP:
        {
          CHECK_EQ(width_, sizeof(JDWP::ObjectId));
          mirror::Object* o = reinterpret_cast<mirror::Object*>(GetVReg(m, reg, kReferenceVReg));
          VLOG(jdwp) << "get object local " << reg << " = " << o;
          if (!Runtime::Current()->GetHeap()->IsHeapAddress(o)) {
            LOG(FATAL) << "Register " << reg << " expected to hold object: " << o;
          }
          tag_ = TagFromObject(o);
          JDWP::SetObjectId(buf_+1, gRegistry->Add(o));
        }
        break;
      case JDWP::JT_DOUBLE:
        {
          CHECK_EQ(width_, 8U);
          uint32_t lo = GetVReg(m, reg, kDoubleLoVReg);
          uint64_t hi = GetVReg(m, reg + 1, kDoubleHiVReg);
          uint64_t longVal = (hi << 32) | lo;
          VLOG(jdwp) << "get double/long local " << hi << ":" << lo << " = " << longVal;
          JDWP::Set8BE(buf_+1, longVal);
        }
        break;
      case JDWP::JT_LONG:
        {
          CHECK_EQ(width_, 8U);
          uint32_t lo = GetVReg(m, reg, kLongLoVReg);
          uint64_t hi = GetVReg(m, reg + 1, kLongHiVReg);
          uint64_t longVal = (hi << 32) | lo;
          VLOG(jdwp) << "get double/long local " << hi << ":" << lo << " = " << longVal;
          JDWP::Set8BE(buf_+1, longVal);
        }
        break;
      default:
        LOG(FATAL) << "Unknown tag " << tag_;
        break;
      }

      // Prepend tag, which may have been updated.
      JDWP::Set1(buf_, tag_);
      return false;
    }

    const JDWP::FrameId frame_id_;
    const int slot_;
    JDWP::JdwpTag tag_;
    uint8_t* const buf_;
    const size_t width_;
  };

  ScopedObjectAccessUnchecked soa(Thread::Current());
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread;
  JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
  if (error != JDWP::ERR_NONE) {
    return;
  }
  UniquePtr<Context> context(Context::Create());
  GetLocalVisitor visitor(thread, context.get(), frame_id, slot, tag, buf, width);
  visitor.WalkStack();
}

void Dbg::SetLocalValue(JDWP::ObjectId thread_id, JDWP::FrameId frame_id, int slot, JDWP::JdwpTag tag,
                        uint64_t value, size_t width) {
  struct SetLocalVisitor : public StackVisitor {
    SetLocalVisitor(Thread* thread, Context* context,
                    JDWP::FrameId frame_id, int slot, JDWP::JdwpTag tag, uint64_t value,
                    size_t width)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        : StackVisitor(thread, context),
          frame_id_(frame_id), slot_(slot), tag_(tag), value_(value), width_(width) {}

    // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
    // annotalysis.
    bool VisitFrame() NO_THREAD_SAFETY_ANALYSIS {
      if (GetFrameId() != frame_id_) {
        return true;  // Not our frame, carry on.
      }
      // TODO: check that the tag is compatible with the actual type of the slot!
      mirror::ArtMethod* m = GetMethod();
      uint16_t reg = DemangleSlot(slot_, m);

      switch (tag_) {
        case JDWP::JT_BOOLEAN:
        case JDWP::JT_BYTE:
          CHECK_EQ(width_, 1U);
          SetVReg(m, reg, static_cast<uint32_t>(value_), kIntVReg);
          break;
        case JDWP::JT_SHORT:
        case JDWP::JT_CHAR:
          CHECK_EQ(width_, 2U);
          SetVReg(m, reg, static_cast<uint32_t>(value_), kIntVReg);
          break;
        case JDWP::JT_INT:
          CHECK_EQ(width_, 4U);
          SetVReg(m, reg, static_cast<uint32_t>(value_), kIntVReg);
          break;
        case JDWP::JT_FLOAT:
          CHECK_EQ(width_, 4U);
          SetVReg(m, reg, static_cast<uint32_t>(value_), kFloatVReg);
          break;
        case JDWP::JT_ARRAY:
        case JDWP::JT_OBJECT:
        case JDWP::JT_STRING:
        {
          CHECK_EQ(width_, sizeof(JDWP::ObjectId));
          mirror::Object* o = gRegistry->Get<mirror::Object*>(static_cast<JDWP::ObjectId>(value_));
          if (o == ObjectRegistry::kInvalidObject) {
            UNIMPLEMENTED(FATAL) << "return an error code when given an invalid object to store";
          }
          SetVReg(m, reg, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(o)), kReferenceVReg);
        }
        break;
        case JDWP::JT_DOUBLE:
          CHECK_EQ(width_, 8U);
          SetVReg(m, reg, static_cast<uint32_t>(value_), kDoubleLoVReg);
          SetVReg(m, reg + 1, static_cast<uint32_t>(value_ >> 32), kDoubleHiVReg);
          break;
        case JDWP::JT_LONG:
          CHECK_EQ(width_, 8U);
          SetVReg(m, reg, static_cast<uint32_t>(value_), kLongLoVReg);
          SetVReg(m, reg + 1, static_cast<uint32_t>(value_ >> 32), kLongHiVReg);
          break;
        default:
          LOG(FATAL) << "Unknown tag " << tag_;
          break;
      }
      return false;
    }

    const JDWP::FrameId frame_id_;
    const int slot_;
    const JDWP::JdwpTag tag_;
    const uint64_t value_;
    const size_t width_;
  };

  ScopedObjectAccessUnchecked soa(Thread::Current());
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread;
  JDWP::JdwpError error = DecodeThread(soa, thread_id, thread);
  if (error != JDWP::ERR_NONE) {
    return;
  }
  UniquePtr<Context> context(Context::Create());
  SetLocalVisitor visitor(thread, context.get(), frame_id, slot, tag, value, width);
  visitor.WalkStack();
}

void Dbg::PostLocationEvent(const mirror::ArtMethod* m, int dex_pc,
                            mirror::Object* this_object, int event_flags) {
  mirror::Class* c = m->GetDeclaringClass();

  JDWP::JdwpLocation location;
  location.type_tag = c->IsInterface() ? JDWP::TT_INTERFACE : JDWP::TT_CLASS;
  location.class_id = gRegistry->AddRefType(c);
  location.method_id = ToMethodId(m);
  location.dex_pc = m->IsNative() ? -1 : dex_pc;

  // If 'this_object' isn't already in the registry, we know that we're not looking for it,
  // so there's no point adding it to the registry and burning through ids.
  JDWP::ObjectId this_id = 0;
  if (gRegistry->Contains(this_object)) {
    this_id = gRegistry->Add(this_object);
  }
  gJdwpState->PostLocationEvent(&location, this_id, event_flags);
}

void Dbg::PostException(Thread* thread, const ThrowLocation& throw_location,
                        mirror::ArtMethod* catch_method,
                        uint32_t catch_dex_pc, mirror::Throwable* exception_object) {
  if (!IsDebuggerActive()) {
    return;
  }

  JDWP::JdwpLocation jdwp_throw_location;
  SetLocation(jdwp_throw_location, throw_location.GetMethod(), throw_location.GetDexPc());
  JDWP::JdwpLocation catch_location;
  SetLocation(catch_location, catch_method, catch_dex_pc);

  // We need 'this' for InstanceOnly filters.
  JDWP::ObjectId this_id = gRegistry->Add(throw_location.GetThis());
  JDWP::ObjectId exception_id = gRegistry->Add(exception_object);
  JDWP::RefTypeId exception_class_id = gRegistry->AddRefType(exception_object->GetClass());

  gJdwpState->PostException(&jdwp_throw_location, exception_id, exception_class_id, &catch_location,
                            this_id);
}

void Dbg::PostClassPrepare(mirror::Class* c) {
  if (!IsDebuggerActive()) {
    return;
  }

  // OLD-TODO - we currently always send both "verified" and "prepared" since
  // debuggers seem to like that.  There might be some advantage to honesty,
  // since the class may not yet be verified.
  int state = JDWP::CS_VERIFIED | JDWP::CS_PREPARED;
  JDWP::JdwpTypeTag tag = c->IsInterface() ? JDWP::TT_INTERFACE : JDWP::TT_CLASS;
  gJdwpState->PostClassPrepare(tag, gRegistry->Add(c), ClassHelper(c).GetDescriptor(), state);
}

void Dbg::UpdateDebugger(Thread* thread, mirror::Object* this_object,
                         const mirror::ArtMethod* m, uint32_t dex_pc) {
  if (!IsDebuggerActive() || dex_pc == static_cast<uint32_t>(-2) /* fake method exit */) {
    return;
  }

  int event_flags = 0;

  if (IsBreakpoint(m, dex_pc)) {
    event_flags |= kBreakpoint;
  }

  {
    // If the debugger is single-stepping one of our threads, check to
    // see if we're that thread and we've reached a step point.
    MutexLock mu(Thread::Current(), *Locks::breakpoint_lock_);
    if (gSingleStepControl.is_active && gSingleStepControl.thread == thread) {
      CHECK(!m->IsNative());
      if (gSingleStepControl.step_depth == JDWP::SD_INTO) {
        // Step into method calls.  We break when the line number
        // or method pointer changes.  If we're in SS_MIN mode, we
        // always stop.
        if (gSingleStepControl.method != m) {
          event_flags |= kSingleStep;
          VLOG(jdwp) << "SS new method";
        } else if (gSingleStepControl.step_size == JDWP::SS_MIN) {
          event_flags |= kSingleStep;
          VLOG(jdwp) << "SS new instruction";
        } else if (gSingleStepControl.dex_pcs.find(dex_pc) == gSingleStepControl.dex_pcs.end()) {
          event_flags |= kSingleStep;
          VLOG(jdwp) << "SS new line";
        }
      } else if (gSingleStepControl.step_depth == JDWP::SD_OVER) {
        // Step over method calls.  We break when the line number is
        // different and the frame depth is <= the original frame
        // depth.  (We can't just compare on the method, because we
        // might get unrolled past it by an exception, and it's tricky
        // to identify recursion.)

        int stack_depth = GetStackDepth(thread);

        if (stack_depth < gSingleStepControl.stack_depth) {
          // popped up one or more frames, always trigger
          event_flags |= kSingleStep;
          VLOG(jdwp) << "SS method pop";
        } else if (stack_depth == gSingleStepControl.stack_depth) {
          // same depth, see if we moved
          if (gSingleStepControl.step_size == JDWP::SS_MIN) {
            event_flags |= kSingleStep;
            VLOG(jdwp) << "SS new instruction";
          } else if (gSingleStepControl.dex_pcs.find(dex_pc) == gSingleStepControl.dex_pcs.end()) {
            event_flags |= kSingleStep;
            VLOG(jdwp) << "SS new line";
          }
        }
      } else {
        CHECK_EQ(gSingleStepControl.step_depth, JDWP::SD_OUT);
        // Return from the current method.  We break when the frame
        // depth pops up.

        // This differs from the "method exit" break in that it stops
        // with the PC at the next instruction in the returned-to
        // function, rather than the end of the returning function.

        int stack_depth = GetStackDepth(thread);
        if (stack_depth < gSingleStepControl.stack_depth) {
          event_flags |= kSingleStep;
          VLOG(jdwp) << "SS method pop";
        }
      }
    }
  }

  // If there's something interesting going on, see if it matches one
  // of the debugger filters.
  if (event_flags != 0) {
    Dbg::PostLocationEvent(m, dex_pc, this_object, event_flags);
  }
}

void Dbg::WatchLocation(const JDWP::JdwpLocation* location) {
  MutexLock mu(Thread::Current(), *Locks::breakpoint_lock_);
  mirror::ArtMethod* m = FromMethodId(location->method_id);
  gBreakpoints.push_back(Breakpoint(m, location->dex_pc));
  VLOG(jdwp) << "Set breakpoint #" << (gBreakpoints.size() - 1) << ": " << gBreakpoints[gBreakpoints.size() - 1];
}

void Dbg::UnwatchLocation(const JDWP::JdwpLocation* location) {
  MutexLock mu(Thread::Current(), *Locks::breakpoint_lock_);
  mirror::ArtMethod* m = FromMethodId(location->method_id);
  for (size_t i = 0; i < gBreakpoints.size(); ++i) {
    if (gBreakpoints[i].method == m && gBreakpoints[i].dex_pc == location->dex_pc) {
      VLOG(jdwp) << "Removed breakpoint #" << i << ": " << gBreakpoints[i];
      gBreakpoints.erase(gBreakpoints.begin() + i);
      return;
    }
  }
}

// Scoped utility class to suspend a thread so that we may do tasks such as walk its stack. Doesn't
// cause suspension if the thread is the current thread.
class ScopedThreadSuspension {
 public:
  ScopedThreadSuspension(Thread* self, JDWP::ObjectId thread_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) :
      thread_(NULL),
      error_(JDWP::ERR_NONE),
      self_suspend_(false),
      other_suspend_(false) {
    ScopedObjectAccessUnchecked soa(self);
    {
      MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
      error_ = DecodeThread(soa, thread_id, thread_);
    }
    if (error_ == JDWP::ERR_NONE) {
      if (thread_ == soa.Self()) {
        self_suspend_ = true;
      } else {
        soa.Self()->TransitionFromRunnableToSuspended(kWaitingForDebuggerSuspension);
        jobject thread_peer = gRegistry->GetJObject(thread_id);
        bool timed_out;
        Thread* suspended_thread = Thread::SuspendForDebugger(thread_peer, true, &timed_out);
        CHECK_EQ(soa.Self()->TransitionFromSuspendedToRunnable(), kWaitingForDebuggerSuspension);
        if (suspended_thread == NULL) {
          // Thread terminated from under us while suspending.
          error_ = JDWP::ERR_INVALID_THREAD;
        } else {
          CHECK_EQ(suspended_thread, thread_);
          other_suspend_ = true;
        }
      }
    }
  }

  Thread* GetThread() const {
    return thread_;
  }

  JDWP::JdwpError GetError() const {
    return error_;
  }

  ~ScopedThreadSuspension() {
    if (other_suspend_) {
      Runtime::Current()->GetThreadList()->Resume(thread_, true);
    }
  }

 private:
  Thread* thread_;
  JDWP::JdwpError error_;
  bool self_suspend_;
  bool other_suspend_;
};

JDWP::JdwpError Dbg::ConfigureStep(JDWP::ObjectId thread_id, JDWP::JdwpStepSize step_size,
                                   JDWP::JdwpStepDepth step_depth) {
  Thread* self = Thread::Current();
  ScopedThreadSuspension sts(self, thread_id);
  if (sts.GetError() != JDWP::ERR_NONE) {
    return sts.GetError();
  }

  MutexLock mu2(self, *Locks::breakpoint_lock_);
  // TODO: there's no theoretical reason why we couldn't support single-stepping
  // of multiple threads at once, but we never did so historically.
  if (gSingleStepControl.thread != NULL && sts.GetThread() != gSingleStepControl.thread) {
    LOG(WARNING) << "single-step already active for " << *gSingleStepControl.thread
                 << "; switching to " << *sts.GetThread();
  }

  //
  // Work out what Method* we're in, the current line number, and how deep the stack currently
  // is for step-out.
  //

  struct SingleStepStackVisitor : public StackVisitor {
    explicit SingleStepStackVisitor(Thread* thread)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::breakpoint_lock_)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        : StackVisitor(thread, NULL) {
      gSingleStepControl.method = NULL;
      gSingleStepControl.stack_depth = 0;
    }

    // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
    // annotalysis.
    bool VisitFrame() NO_THREAD_SAFETY_ANALYSIS {
      Locks::breakpoint_lock_->AssertHeld(Thread::Current());
      const mirror::ArtMethod* m = GetMethod();
      if (!m->IsRuntimeMethod()) {
        ++gSingleStepControl.stack_depth;
        if (gSingleStepControl.method == NULL) {
          const mirror::DexCache* dex_cache = m->GetDeclaringClass()->GetDexCache();
          gSingleStepControl.method = m;
          gSingleStepControl.line_number = -1;
          if (dex_cache != NULL) {
            const DexFile& dex_file = *dex_cache->GetDexFile();
            gSingleStepControl.line_number = dex_file.GetLineNumFromPC(m, GetDexPc());
          }
        }
      }
      return true;
    }
  };

  SingleStepStackVisitor visitor(sts.GetThread());
  visitor.WalkStack();

  //
  // Find the dex_pc values that correspond to the current line, for line-based single-stepping.
  //

  struct DebugCallbackContext {
    DebugCallbackContext() EXCLUSIVE_LOCKS_REQUIRED(Locks::breakpoint_lock_) {
      last_pc_valid = false;
      last_pc = 0;
    }

    // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
    // annotalysis.
    static bool Callback(void* raw_context, uint32_t address, uint32_t line_number) NO_THREAD_SAFETY_ANALYSIS {
      Locks::breakpoint_lock_->AssertHeld(Thread::Current());
      DebugCallbackContext* context = reinterpret_cast<DebugCallbackContext*>(raw_context);
      if (static_cast<int32_t>(line_number) == gSingleStepControl.line_number) {
        if (!context->last_pc_valid) {
          // Everything from this address until the next line change is ours.
          context->last_pc = address;
          context->last_pc_valid = true;
        }
        // Otherwise, if we're already in a valid range for this line,
        // just keep going (shouldn't really happen)...
      } else if (context->last_pc_valid) {  // and the line number is new
        // Add everything from the last entry up until here to the set
        for (uint32_t dex_pc = context->last_pc; dex_pc < address; ++dex_pc) {
          gSingleStepControl.dex_pcs.insert(dex_pc);
        }
        context->last_pc_valid = false;
      }
      return false;  // There may be multiple entries for any given line.
    }

    // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
    // annotalysis.
    ~DebugCallbackContext() NO_THREAD_SAFETY_ANALYSIS {
      Locks::breakpoint_lock_->AssertHeld(Thread::Current());
      // If the line number was the last in the position table...
      if (last_pc_valid) {
        size_t end = MethodHelper(gSingleStepControl.method).GetCodeItem()->insns_size_in_code_units_;
        for (uint32_t dex_pc = last_pc; dex_pc < end; ++dex_pc) {
          gSingleStepControl.dex_pcs.insert(dex_pc);
        }
      }
    }

    bool last_pc_valid;
    uint32_t last_pc;
  };
  gSingleStepControl.dex_pcs.clear();
  const mirror::ArtMethod* m = gSingleStepControl.method;
  if (m->IsNative()) {
    gSingleStepControl.line_number = -1;
  } else {
    DebugCallbackContext context;
    MethodHelper mh(m);
    mh.GetDexFile().DecodeDebugInfo(mh.GetCodeItem(), m->IsStatic(), m->GetDexMethodIndex(),
                                    DebugCallbackContext::Callback, NULL, &context);
  }

  //
  // Everything else...
  //

  gSingleStepControl.thread = sts.GetThread();
  gSingleStepControl.step_size = step_size;
  gSingleStepControl.step_depth = step_depth;
  gSingleStepControl.is_active = true;

  if (VLOG_IS_ON(jdwp)) {
    VLOG(jdwp) << "Single-step thread: " << *gSingleStepControl.thread;
    VLOG(jdwp) << "Single-step step size: " << gSingleStepControl.step_size;
    VLOG(jdwp) << "Single-step step depth: " << gSingleStepControl.step_depth;
    VLOG(jdwp) << "Single-step current method: " << PrettyMethod(gSingleStepControl.method);
    VLOG(jdwp) << "Single-step current line: " << gSingleStepControl.line_number;
    VLOG(jdwp) << "Single-step current stack depth: " << gSingleStepControl.stack_depth;
    VLOG(jdwp) << "Single-step dex_pc values:";
    for (std::set<uint32_t>::iterator it = gSingleStepControl.dex_pcs.begin() ; it != gSingleStepControl.dex_pcs.end(); ++it) {
      VLOG(jdwp) << StringPrintf(" %#x", *it);
    }
  }

  return JDWP::ERR_NONE;
}

void Dbg::UnconfigureStep(JDWP::ObjectId /*thread_id*/) {
  MutexLock mu(Thread::Current(), *Locks::breakpoint_lock_);

  gSingleStepControl.is_active = false;
  gSingleStepControl.thread = NULL;
  gSingleStepControl.dex_pcs.clear();
}

static char JdwpTagToShortyChar(JDWP::JdwpTag tag) {
  switch (tag) {
    default:
      LOG(FATAL) << "unknown JDWP tag: " << PrintableChar(tag);

    // Primitives.
    case JDWP::JT_BYTE:    return 'B';
    case JDWP::JT_CHAR:    return 'C';
    case JDWP::JT_FLOAT:   return 'F';
    case JDWP::JT_DOUBLE:  return 'D';
    case JDWP::JT_INT:     return 'I';
    case JDWP::JT_LONG:    return 'J';
    case JDWP::JT_SHORT:   return 'S';
    case JDWP::JT_VOID:    return 'V';
    case JDWP::JT_BOOLEAN: return 'Z';

    // Reference types.
    case JDWP::JT_ARRAY:
    case JDWP::JT_OBJECT:
    case JDWP::JT_STRING:
    case JDWP::JT_THREAD:
    case JDWP::JT_THREAD_GROUP:
    case JDWP::JT_CLASS_LOADER:
    case JDWP::JT_CLASS_OBJECT:
      return 'L';
  }
}

JDWP::JdwpError Dbg::InvokeMethod(JDWP::ObjectId thread_id, JDWP::ObjectId object_id,
                                  JDWP::RefTypeId class_id, JDWP::MethodId method_id,
                                  uint32_t arg_count, uint64_t* arg_values,
                                  JDWP::JdwpTag* arg_types, uint32_t options,
                                  JDWP::JdwpTag* pResultTag, uint64_t* pResultValue,
                                  JDWP::ObjectId* pExceptionId) {
  ThreadList* thread_list = Runtime::Current()->GetThreadList();

  Thread* targetThread = NULL;
  DebugInvokeReq* req = NULL;
  Thread* self = Thread::Current();
  {
    ScopedObjectAccessUnchecked soa(self);
    MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
    JDWP::JdwpError error = DecodeThread(soa, thread_id, targetThread);
    if (error != JDWP::ERR_NONE) {
      LOG(ERROR) << "InvokeMethod request for invalid thread id " << thread_id;
      return error;
    }
    req = targetThread->GetInvokeReq();
    if (!req->ready) {
      LOG(ERROR) << "InvokeMethod request for thread not stopped by event: " << *targetThread;
      return JDWP::ERR_INVALID_THREAD;
    }

    /*
     * We currently have a bug where we don't successfully resume the
     * target thread if the suspend count is too deep.  We're expected to
     * require one "resume" for each "suspend", but when asked to execute
     * a method we have to resume fully and then re-suspend it back to the
     * same level.  (The easiest way to cause this is to type "suspend"
     * multiple times in jdb.)
     *
     * It's unclear what this means when the event specifies "resume all"
     * and some threads are suspended more deeply than others.  This is
     * a rare problem, so for now we just prevent it from hanging forever
     * by rejecting the method invocation request.  Without this, we will
     * be stuck waiting on a suspended thread.
     */
    int suspend_count;
    {
      MutexLock mu2(soa.Self(), *Locks::thread_suspend_count_lock_);
      suspend_count = targetThread->GetSuspendCount();
    }
    if (suspend_count > 1) {
      LOG(ERROR) << *targetThread << " suspend count too deep for method invocation: " << suspend_count;
      return JDWP::ERR_THREAD_SUSPENDED;  // Probably not expected here.
    }

    JDWP::JdwpError status;
    mirror::Object* receiver = gRegistry->Get<mirror::Object*>(object_id);
    if (receiver == ObjectRegistry::kInvalidObject) {
      return JDWP::ERR_INVALID_OBJECT;
    }

    mirror::Object* thread = gRegistry->Get<mirror::Object*>(thread_id);
    if (thread == ObjectRegistry::kInvalidObject) {
      return JDWP::ERR_INVALID_OBJECT;
    }
    // TODO: check that 'thread' is actually a java.lang.Thread!

    mirror::Class* c = DecodeClass(class_id, status);
    if (c == NULL) {
      return status;
    }

    mirror::ArtMethod* m = FromMethodId(method_id);
    if (m->IsStatic() != (receiver == NULL)) {
      return JDWP::ERR_INVALID_METHODID;
    }
    if (m->IsStatic()) {
      if (m->GetDeclaringClass() != c) {
        return JDWP::ERR_INVALID_METHODID;
      }
    } else {
      if (!m->GetDeclaringClass()->IsAssignableFrom(c)) {
        return JDWP::ERR_INVALID_METHODID;
      }
    }

    // Check the argument list matches the method.
    MethodHelper mh(m);
    if (mh.GetShortyLength() - 1 != arg_count) {
      return JDWP::ERR_ILLEGAL_ARGUMENT;
    }
    const char* shorty = mh.GetShorty();
    const DexFile::TypeList* types = mh.GetParameterTypeList();
    for (size_t i = 0; i < arg_count; ++i) {
      if (shorty[i + 1] != JdwpTagToShortyChar(arg_types[i])) {
        return JDWP::ERR_ILLEGAL_ARGUMENT;
      }

      if (shorty[i + 1] == 'L') {
        // Did we really get an argument of an appropriate reference type?
        mirror::Class* parameter_type = mh.GetClassFromTypeIdx(types->GetTypeItem(i).type_idx_);
        mirror::Object* argument = gRegistry->Get<mirror::Object*>(arg_values[i]);
        if (argument == ObjectRegistry::kInvalidObject) {
          return JDWP::ERR_INVALID_OBJECT;
        }
        if (!argument->InstanceOf(parameter_type)) {
          return JDWP::ERR_ILLEGAL_ARGUMENT;
        }

        // Turn the on-the-wire ObjectId into a jobject.
        jvalue& v = reinterpret_cast<jvalue&>(arg_values[i]);
        v.l = gRegistry->GetJObject(arg_values[i]);
      }
    }

    req->receiver_ = receiver;
    req->thread_ = thread;
    req->class_ = c;
    req->method_ = m;
    req->arg_count_ = arg_count;
    req->arg_values_ = arg_values;
    req->options_ = options;
    req->invoke_needed_ = true;
  }

  // The fact that we've released the thread list lock is a bit risky --- if the thread goes
  // away we're sitting high and dry -- but we must release this before the ResumeAllThreads
  // call, and it's unwise to hold it during WaitForSuspend.

  {
    /*
     * We change our (JDWP thread) status, which should be THREAD_RUNNING,
     * so we can suspend for a GC if the invoke request causes us to
     * run out of memory.  It's also a good idea to change it before locking
     * the invokeReq mutex, although that should never be held for long.
     */
    self->TransitionFromRunnableToSuspended(kWaitingForDebuggerSend);

    VLOG(jdwp) << "    Transferring control to event thread";
    {
      MutexLock mu(self, req->lock_);

      if ((options & JDWP::INVOKE_SINGLE_THREADED) == 0) {
        VLOG(jdwp) << "      Resuming all threads";
        thread_list->UndoDebuggerSuspensions();
      } else {
        VLOG(jdwp) << "      Resuming event thread only";
        thread_list->Resume(targetThread, true);
      }

      // Wait for the request to finish executing.
      while (req->invoke_needed_) {
        req->cond_.Wait(self);
      }
    }
    VLOG(jdwp) << "    Control has returned from event thread";

    /* wait for thread to re-suspend itself */
    SuspendThread(thread_id, false /* request_suspension */);
    self->TransitionFromSuspendedToRunnable();
  }

  /*
   * Suspend the threads.  We waited for the target thread to suspend
   * itself, so all we need to do is suspend the others.
   *
   * The suspendAllThreads() call will double-suspend the event thread,
   * so we want to resume the target thread once to keep the books straight.
   */
  if ((options & JDWP::INVOKE_SINGLE_THREADED) == 0) {
    self->TransitionFromRunnableToSuspended(kWaitingForDebuggerSuspension);
    VLOG(jdwp) << "      Suspending all threads";
    thread_list->SuspendAllForDebugger();
    self->TransitionFromSuspendedToRunnable();
    VLOG(jdwp) << "      Resuming event thread to balance the count";
    thread_list->Resume(targetThread, true);
  }

  // Copy the result.
  *pResultTag = req->result_tag;
  if (IsPrimitiveTag(req->result_tag)) {
    *pResultValue = req->result_value.GetJ();
  } else {
    *pResultValue = gRegistry->Add(req->result_value.GetL());
  }
  *pExceptionId = req->exception;
  return req->error;
}

void Dbg::ExecuteMethod(DebugInvokeReq* pReq) {
  ScopedObjectAccess soa(Thread::Current());

  // We can be called while an exception is pending. We need
  // to preserve that across the method invocation.
  SirtRef<mirror::Object> old_throw_this_object(soa.Self(), NULL);
  SirtRef<mirror::ArtMethod> old_throw_method(soa.Self(), NULL);
  SirtRef<mirror::Throwable> old_exception(soa.Self(), NULL);
  uint32_t old_throw_dex_pc;
  {
    ThrowLocation old_throw_location;
    mirror::Throwable* old_exception_obj = soa.Self()->GetException(&old_throw_location);
    old_throw_this_object.reset(old_throw_location.GetThis());
    old_throw_method.reset(old_throw_location.GetMethod());
    old_exception.reset(old_exception_obj);
    old_throw_dex_pc = old_throw_location.GetDexPc();
    soa.Self()->ClearException();
  }

  // Translate the method through the vtable, unless the debugger wants to suppress it.
  mirror::ArtMethod* m = pReq->method_;
  if ((pReq->options_ & JDWP::INVOKE_NONVIRTUAL) == 0 && pReq->receiver_ != NULL) {
    mirror::ArtMethod* actual_method = pReq->class_->FindVirtualMethodForVirtualOrInterface(pReq->method_);
    if (actual_method != m) {
      VLOG(jdwp) << "ExecuteMethod translated " << PrettyMethod(m) << " to " << PrettyMethod(actual_method);
      m = actual_method;
    }
  }
  VLOG(jdwp) << "ExecuteMethod " << PrettyMethod(m)
             << " receiver=" << pReq->receiver_
             << " arg_count=" << pReq->arg_count_;
  CHECK(m != NULL);

  CHECK_EQ(sizeof(jvalue), sizeof(uint64_t));

  MethodHelper mh(m);
  ArgArray arg_array(mh.GetShorty(), mh.GetShortyLength());
  arg_array.BuildArgArray(soa, pReq->receiver_, reinterpret_cast<jvalue*>(pReq->arg_values_));
  InvokeWithArgArray(soa, m, &arg_array, &pReq->result_value, mh.GetShorty()[0]);

  mirror::Throwable* exception = soa.Self()->GetException(NULL);
  soa.Self()->ClearException();
  pReq->exception = gRegistry->Add(exception);
  pReq->result_tag = BasicTagFromDescriptor(MethodHelper(m).GetShorty());
  if (pReq->exception != 0) {
    VLOG(jdwp) << "  JDWP invocation returning with exception=" << exception
        << " " << exception->Dump();
    pReq->result_value.SetJ(0);
  } else if (pReq->result_tag == JDWP::JT_OBJECT) {
    /* if no exception thrown, examine object result more closely */
    JDWP::JdwpTag new_tag = TagFromObject(pReq->result_value.GetL());
    if (new_tag != pReq->result_tag) {
      VLOG(jdwp) << "  JDWP promoted result from " << pReq->result_tag << " to " << new_tag;
      pReq->result_tag = new_tag;
    }

    /*
     * Register the object.  We don't actually need an ObjectId yet,
     * but we do need to be sure that the GC won't move or discard the
     * object when we switch out of RUNNING.  The ObjectId conversion
     * will add the object to the "do not touch" list.
     *
     * We can't use the "tracked allocation" mechanism here because
     * the object is going to be handed off to a different thread.
     */
    gRegistry->Add(pReq->result_value.GetL());
  }

  if (old_exception.get() != NULL) {
    ThrowLocation gc_safe_throw_location(old_throw_this_object.get(), old_throw_method.get(),
                                         old_throw_dex_pc);
    soa.Self()->SetException(gc_safe_throw_location, old_exception.get());
  }
}

/*
 * "request" contains a full JDWP packet, possibly with multiple chunks.  We
 * need to process each, accumulate the replies, and ship the whole thing
 * back.
 *
 * Returns "true" if we have a reply.  The reply buffer is newly allocated,
 * and includes the chunk type/length, followed by the data.
 *
 * OLD-TODO: we currently assume that the request and reply include a single
 * chunk.  If this becomes inconvenient we will need to adapt.
 */
bool Dbg::DdmHandlePacket(JDWP::Request& request, uint8_t** pReplyBuf, int* pReplyLen) {
  Thread* self = Thread::Current();
  JNIEnv* env = self->GetJniEnv();

  uint32_t type = request.ReadUnsigned32("type");
  uint32_t length = request.ReadUnsigned32("length");

  // Create a byte[] corresponding to 'request'.
  size_t request_length = request.size();
  ScopedLocalRef<jbyteArray> dataArray(env, env->NewByteArray(request_length));
  if (dataArray.get() == NULL) {
    LOG(WARNING) << "byte[] allocation failed: " << request_length;
    env->ExceptionClear();
    return false;
  }
  env->SetByteArrayRegion(dataArray.get(), 0, request_length, reinterpret_cast<const jbyte*>(request.data()));
  request.Skip(request_length);

  // Run through and find all chunks.  [Currently just find the first.]
  ScopedByteArrayRO contents(env, dataArray.get());
  if (length != request_length) {
    LOG(WARNING) << StringPrintf("bad chunk found (len=%u pktLen=%d)", length, request_length);
    return false;
  }

  // Call "private static Chunk dispatch(int type, byte[] data, int offset, int length)".
  ScopedLocalRef<jobject> chunk(env, env->CallStaticObjectMethod(WellKnownClasses::org_apache_harmony_dalvik_ddmc_DdmServer,
                                                                 WellKnownClasses::org_apache_harmony_dalvik_ddmc_DdmServer_dispatch,
                                                                 type, dataArray.get(), 0, length));
  if (env->ExceptionCheck()) {
    LOG(INFO) << StringPrintf("Exception thrown by dispatcher for 0x%08x", type);
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }

  if (chunk.get() == NULL) {
    return false;
  }

  /*
   * Pull the pieces out of the chunk.  We copy the results into a
   * newly-allocated buffer that the caller can free.  We don't want to
   * continue using the Chunk object because nothing has a reference to it.
   *
   * We could avoid this by returning type/data/offset/length and having
   * the caller be aware of the object lifetime issues, but that
   * integrates the JDWP code more tightly into the rest of the runtime, and doesn't work
   * if we have responses for multiple chunks.
   *
   * So we're pretty much stuck with copying data around multiple times.
   */
  ScopedLocalRef<jbyteArray> replyData(env, reinterpret_cast<jbyteArray>(env->GetObjectField(chunk.get(), WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_data)));
  jint offset = env->GetIntField(chunk.get(), WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_offset);
  length = env->GetIntField(chunk.get(), WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_length);
  type = env->GetIntField(chunk.get(), WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_type);

  VLOG(jdwp) << StringPrintf("DDM reply: type=0x%08x data=%p offset=%d length=%d", type, replyData.get(), offset, length);
  if (length == 0 || replyData.get() == NULL) {
    return false;
  }

  const int kChunkHdrLen = 8;
  uint8_t* reply = new uint8_t[length + kChunkHdrLen];
  if (reply == NULL) {
    LOG(WARNING) << "malloc failed: " << (length + kChunkHdrLen);
    return false;
  }
  JDWP::Set4BE(reply + 0, type);
  JDWP::Set4BE(reply + 4, length);
  env->GetByteArrayRegion(replyData.get(), offset, length, reinterpret_cast<jbyte*>(reply + kChunkHdrLen));

  *pReplyBuf = reply;
  *pReplyLen = length + kChunkHdrLen;

  VLOG(jdwp) << StringPrintf("dvmHandleDdm returning type=%.4s %p len=%d", reinterpret_cast<char*>(reply), reply, length);
  return true;
}

void Dbg::DdmBroadcast(bool connect) {
  VLOG(jdwp) << "Broadcasting DDM " << (connect ? "connect" : "disconnect") << "...";

  Thread* self = Thread::Current();
  if (self->GetState() != kRunnable) {
    LOG(ERROR) << "DDM broadcast in thread state " << self->GetState();
    /* try anyway? */
  }

  JNIEnv* env = self->GetJniEnv();
  jint event = connect ? 1 /*DdmServer.CONNECTED*/ : 2 /*DdmServer.DISCONNECTED*/;
  env->CallStaticVoidMethod(WellKnownClasses::org_apache_harmony_dalvik_ddmc_DdmServer,
                            WellKnownClasses::org_apache_harmony_dalvik_ddmc_DdmServer_broadcast,
                            event);
  if (env->ExceptionCheck()) {
    LOG(ERROR) << "DdmServer.broadcast " << event << " failed";
    env->ExceptionDescribe();
    env->ExceptionClear();
  }
}

void Dbg::DdmConnected() {
  Dbg::DdmBroadcast(true);
}

void Dbg::DdmDisconnected() {
  Dbg::DdmBroadcast(false);
  gDdmThreadNotification = false;
}

/*
 * Send a notification when a thread starts, stops, or changes its name.
 *
 * Because we broadcast the full set of threads when the notifications are
 * first enabled, it's possible for "thread" to be actively executing.
 */
void Dbg::DdmSendThreadNotification(Thread* t, uint32_t type) {
  if (!gDdmThreadNotification) {
    return;
  }

  if (type == CHUNK_TYPE("THDE")) {
    uint8_t buf[4];
    JDWP::Set4BE(&buf[0], t->GetThinLockId());
    Dbg::DdmSendChunk(CHUNK_TYPE("THDE"), 4, buf);
  } else {
    CHECK(type == CHUNK_TYPE("THCR") || type == CHUNK_TYPE("THNM")) << type;
    ScopedObjectAccessUnchecked soa(Thread::Current());
    SirtRef<mirror::String> name(soa.Self(), t->GetThreadName(soa));
    size_t char_count = (name.get() != NULL) ? name->GetLength() : 0;
    const jchar* chars = (name.get() != NULL) ? name->GetCharArray()->GetData() : NULL;

    std::vector<uint8_t> bytes;
    JDWP::Append4BE(bytes, t->GetThinLockId());
    JDWP::AppendUtf16BE(bytes, chars, char_count);
    CHECK_EQ(bytes.size(), char_count*2 + sizeof(uint32_t)*2);
    Dbg::DdmSendChunk(type, bytes);
  }
}

void Dbg::DdmSetThreadNotification(bool enable) {
  // Enable/disable thread notifications.
  gDdmThreadNotification = enable;
  if (enable) {
    // Suspend the VM then post thread start notifications for all threads. Threads attaching will
    // see a suspension in progress and block until that ends. They then post their own start
    // notification.
    SuspendVM();
    std::list<Thread*> threads;
    Thread* self = Thread::Current();
    {
      MutexLock mu(self, *Locks::thread_list_lock_);
      threads = Runtime::Current()->GetThreadList()->GetList();
    }
    {
      ScopedObjectAccess soa(self);
      for (Thread* thread : threads) {
        Dbg::DdmSendThreadNotification(thread, CHUNK_TYPE("THCR"));
      }
    }
    ResumeVM();
  }
}

void Dbg::PostThreadStartOrStop(Thread* t, uint32_t type) {
  if (IsDebuggerActive()) {
    ScopedObjectAccessUnchecked soa(Thread::Current());
    JDWP::ObjectId id = gRegistry->Add(t->GetPeer());
    gJdwpState->PostThreadChange(id, type == CHUNK_TYPE("THCR"));
  }
  Dbg::DdmSendThreadNotification(t, type);
}

void Dbg::PostThreadStart(Thread* t) {
  Dbg::PostThreadStartOrStop(t, CHUNK_TYPE("THCR"));
}

void Dbg::PostThreadDeath(Thread* t) {
  Dbg::PostThreadStartOrStop(t, CHUNK_TYPE("THDE"));
}

void Dbg::DdmSendChunk(uint32_t type, size_t byte_count, const uint8_t* buf) {
  CHECK(buf != NULL);
  iovec vec[1];
  vec[0].iov_base = reinterpret_cast<void*>(const_cast<uint8_t*>(buf));
  vec[0].iov_len = byte_count;
  Dbg::DdmSendChunkV(type, vec, 1);
}

void Dbg::DdmSendChunk(uint32_t type, const std::vector<uint8_t>& bytes) {
  DdmSendChunk(type, bytes.size(), &bytes[0]);
}

void Dbg::DdmSendChunkV(uint32_t type, const iovec* iov, int iov_count) {
  if (gJdwpState == NULL) {
    VLOG(jdwp) << "Debugger thread not active, ignoring DDM send: " << type;
  } else {
    gJdwpState->DdmSendChunkV(type, iov, iov_count);
  }
}

int Dbg::DdmHandleHpifChunk(HpifWhen when) {
  if (when == HPIF_WHEN_NOW) {
    DdmSendHeapInfo(when);
    return true;
  }

  if (when != HPIF_WHEN_NEVER && when != HPIF_WHEN_NEXT_GC && when != HPIF_WHEN_EVERY_GC) {
    LOG(ERROR) << "invalid HpifWhen value: " << static_cast<int>(when);
    return false;
  }

  gDdmHpifWhen = when;
  return true;
}

bool Dbg::DdmHandleHpsgNhsgChunk(Dbg::HpsgWhen when, Dbg::HpsgWhat what, bool native) {
  if (when != HPSG_WHEN_NEVER && when != HPSG_WHEN_EVERY_GC) {
    LOG(ERROR) << "invalid HpsgWhen value: " << static_cast<int>(when);
    return false;
  }

  if (what != HPSG_WHAT_MERGED_OBJECTS && what != HPSG_WHAT_DISTINCT_OBJECTS) {
    LOG(ERROR) << "invalid HpsgWhat value: " << static_cast<int>(what);
    return false;
  }

  if (native) {
    gDdmNhsgWhen = when;
    gDdmNhsgWhat = what;
  } else {
    gDdmHpsgWhen = when;
    gDdmHpsgWhat = what;
  }
  return true;
}

void Dbg::DdmSendHeapInfo(HpifWhen reason) {
  // If there's a one-shot 'when', reset it.
  if (reason == gDdmHpifWhen) {
    if (gDdmHpifWhen == HPIF_WHEN_NEXT_GC) {
      gDdmHpifWhen = HPIF_WHEN_NEVER;
    }
  }

  /*
   * Chunk HPIF (client --> server)
   *
   * Heap Info. General information about the heap,
   * suitable for a summary display.
   *
   *   [u4]: number of heaps
   *
   *   For each heap:
   *     [u4]: heap ID
   *     [u8]: timestamp in ms since Unix epoch
   *     [u1]: capture reason (same as 'when' value from server)
   *     [u4]: max heap size in bytes (-Xmx)
   *     [u4]: current heap size in bytes
   *     [u4]: current number of bytes allocated
   *     [u4]: current number of objects allocated
   */
  uint8_t heap_count = 1;
  gc::Heap* heap = Runtime::Current()->GetHeap();
  std::vector<uint8_t> bytes;
  JDWP::Append4BE(bytes, heap_count);
  JDWP::Append4BE(bytes, 1);  // Heap id (bogus; we only have one heap).
  JDWP::Append8BE(bytes, MilliTime());
  JDWP::Append1BE(bytes, reason);
  JDWP::Append4BE(bytes, heap->GetMaxMemory());  // Max allowed heap size in bytes.
  JDWP::Append4BE(bytes, heap->GetTotalMemory());  // Current heap size in bytes.
  JDWP::Append4BE(bytes, heap->GetBytesAllocated());
  JDWP::Append4BE(bytes, heap->GetObjectsAllocated());
  CHECK_EQ(bytes.size(), 4U + (heap_count * (4 + 8 + 1 + 4 + 4 + 4 + 4)));
  Dbg::DdmSendChunk(CHUNK_TYPE("HPIF"), bytes);
}

enum HpsgSolidity {
  SOLIDITY_FREE = 0,
  SOLIDITY_HARD = 1,
  SOLIDITY_SOFT = 2,
  SOLIDITY_WEAK = 3,
  SOLIDITY_PHANTOM = 4,
  SOLIDITY_FINALIZABLE = 5,
  SOLIDITY_SWEEP = 6,
};

enum HpsgKind {
  KIND_OBJECT = 0,
  KIND_CLASS_OBJECT = 1,
  KIND_ARRAY_1 = 2,
  KIND_ARRAY_2 = 3,
  KIND_ARRAY_4 = 4,
  KIND_ARRAY_8 = 5,
  KIND_UNKNOWN = 6,
  KIND_NATIVE = 7,
};

#define HPSG_PARTIAL (1<<7)
#define HPSG_STATE(solidity, kind) ((uint8_t)((((kind) & 0x7) << 3) | ((solidity) & 0x7)))

class HeapChunkContext {
 public:
  // Maximum chunk size.  Obtain this from the formula:
  // (((maximum_heap_size / ALLOCATION_UNIT_SIZE) + 255) / 256) * 2
  HeapChunkContext(bool merge, bool native)
      : buf_(16384 - 16),
        type_(0),
        merge_(merge) {
    Reset();
    if (native) {
      type_ = CHUNK_TYPE("NHSG");
    } else {
      type_ = merge ? CHUNK_TYPE("HPSG") : CHUNK_TYPE("HPSO");
    }
  }

  ~HeapChunkContext() {
    if (p_ > &buf_[0]) {
      Flush();
    }
  }

  void EnsureHeader(const void* chunk_ptr) {
    if (!needHeader_) {
      return;
    }

    // Start a new HPSx chunk.
    JDWP::Write4BE(&p_, 1);  // Heap id (bogus; we only have one heap).
    JDWP::Write1BE(&p_, 8);  // Size of allocation unit, in bytes.

    JDWP::Write4BE(&p_, reinterpret_cast<uintptr_t>(chunk_ptr));  // virtual address of segment start.
    JDWP::Write4BE(&p_, 0);  // offset of this piece (relative to the virtual address).
    // [u4]: length of piece, in allocation units
    // We won't know this until we're done, so save the offset and stuff in a dummy value.
    pieceLenField_ = p_;
    JDWP::Write4BE(&p_, 0x55555555);
    needHeader_ = false;
  }

  void Flush() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (pieceLenField_ == NULL) {
      // Flush immediately post Reset (maybe back-to-back Flush). Ignore.
      CHECK(needHeader_);
      return;
    }
    // Patch the "length of piece" field.
    CHECK_LE(&buf_[0], pieceLenField_);
    CHECK_LE(pieceLenField_, p_);
    JDWP::Set4BE(pieceLenField_, totalAllocationUnits_);

    Dbg::DdmSendChunk(type_, p_ - &buf_[0], &buf_[0]);
    Reset();
  }

  static void HeapChunkCallback(void* start, void* end, size_t used_bytes, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_,
                            Locks::mutator_lock_) {
    reinterpret_cast<HeapChunkContext*>(arg)->HeapChunkCallback(start, end, used_bytes);
  }

 private:
  enum { ALLOCATION_UNIT_SIZE = 8 };

  void Reset() {
    p_ = &buf_[0];
    startOfNextMemoryChunk_ = NULL;
    totalAllocationUnits_ = 0;
    needHeader_ = true;
    pieceLenField_ = NULL;
  }

  void HeapChunkCallback(void* start, void* /*end*/, size_t used_bytes)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_,
                            Locks::mutator_lock_) {
    // Note: heap call backs cannot manipulate the heap upon which they are crawling, care is taken
    // in the following code not to allocate memory, by ensuring buf_ is of the correct size
    if (used_bytes == 0) {
        if (start == NULL) {
            // Reset for start of new heap.
            startOfNextMemoryChunk_ = NULL;
            Flush();
        }
        // Only process in use memory so that free region information
        // also includes dlmalloc book keeping.
        return;
    }

    /* If we're looking at the native heap, we'll just return
     * (SOLIDITY_HARD, KIND_NATIVE) for all allocated chunks
     */
    bool native = type_ == CHUNK_TYPE("NHSG");

    if (startOfNextMemoryChunk_ != NULL) {
        // Transmit any pending free memory. Native free memory of
        // over kMaxFreeLen could be because of the use of mmaps, so
        // don't report. If not free memory then start a new segment.
        bool flush = true;
        if (start > startOfNextMemoryChunk_) {
            const size_t kMaxFreeLen = 2 * kPageSize;
            void* freeStart = startOfNextMemoryChunk_;
            void* freeEnd = start;
            size_t freeLen = reinterpret_cast<char*>(freeEnd) - reinterpret_cast<char*>(freeStart);
            if (!native || freeLen < kMaxFreeLen) {
                AppendChunk(HPSG_STATE(SOLIDITY_FREE, 0), freeStart, freeLen);
                flush = false;
            }
        }
        if (flush) {
            startOfNextMemoryChunk_ = NULL;
            Flush();
        }
    }
    const mirror::Object* obj = reinterpret_cast<const mirror::Object*>(start);

    // Determine the type of this chunk.
    // OLD-TODO: if context.merge, see if this chunk is different from the last chunk.
    // If it's the same, we should combine them.
    uint8_t state = ExamineObject(obj, native);
    // dlmalloc's chunk header is 2 * sizeof(size_t), but if the previous chunk is in use for an
    // allocation then the first sizeof(size_t) may belong to it.
    const size_t dlMallocOverhead = sizeof(size_t);
    AppendChunk(state, start, used_bytes + dlMallocOverhead);
    startOfNextMemoryChunk_ = reinterpret_cast<char*>(start) + used_bytes + dlMallocOverhead;
  }

  void AppendChunk(uint8_t state, void* ptr, size_t length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Make sure there's enough room left in the buffer.
    // We need to use two bytes for every fractional 256 allocation units used by the chunk plus
    // 17 bytes for any header.
    size_t needed = (((length/ALLOCATION_UNIT_SIZE + 255) / 256) * 2) + 17;
    size_t bytesLeft = buf_.size() - (size_t)(p_ - &buf_[0]);
    if (bytesLeft < needed) {
      Flush();
    }

    bytesLeft = buf_.size() - (size_t)(p_ - &buf_[0]);
    if (bytesLeft < needed) {
      LOG(WARNING) << "Chunk is too big to transmit (chunk_len=" << length << ", "
          << needed << " bytes)";
      return;
    }
    EnsureHeader(ptr);
    // Write out the chunk description.
    length /= ALLOCATION_UNIT_SIZE;   // Convert to allocation units.
    totalAllocationUnits_ += length;
    while (length > 256) {
      *p_++ = state | HPSG_PARTIAL;
      *p_++ = 255;     // length - 1
      length -= 256;
    }
    *p_++ = state;
    *p_++ = length - 1;
  }

  uint8_t ExamineObject(const mirror::Object* o, bool is_native_heap)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_) {
    if (o == NULL) {
      return HPSG_STATE(SOLIDITY_FREE, 0);
    }

    // It's an allocated chunk. Figure out what it is.

    // If we're looking at the native heap, we'll just return
    // (SOLIDITY_HARD, KIND_NATIVE) for all allocated chunks.
    if (is_native_heap) {
      return HPSG_STATE(SOLIDITY_HARD, KIND_NATIVE);
    }

    if (!Runtime::Current()->GetHeap()->IsLiveObjectLocked(o)) {
      return HPSG_STATE(SOLIDITY_HARD, KIND_NATIVE);
    }

    mirror::Class* c = o->GetClass();
    if (c == NULL) {
      // The object was probably just created but hasn't been initialized yet.
      return HPSG_STATE(SOLIDITY_HARD, KIND_OBJECT);
    }

    if (!Runtime::Current()->GetHeap()->IsHeapAddress(c)) {
      LOG(ERROR) << "Invalid class for managed heap object: " << o << " " << c;
      return HPSG_STATE(SOLIDITY_HARD, KIND_UNKNOWN);
    }

    if (c->IsClassClass()) {
      return HPSG_STATE(SOLIDITY_HARD, KIND_CLASS_OBJECT);
    }

    if (c->IsArrayClass()) {
      if (o->IsObjectArray()) {
        return HPSG_STATE(SOLIDITY_HARD, KIND_ARRAY_4);
      }
      switch (c->GetComponentSize()) {
      case 1: return HPSG_STATE(SOLIDITY_HARD, KIND_ARRAY_1);
      case 2: return HPSG_STATE(SOLIDITY_HARD, KIND_ARRAY_2);
      case 4: return HPSG_STATE(SOLIDITY_HARD, KIND_ARRAY_4);
      case 8: return HPSG_STATE(SOLIDITY_HARD, KIND_ARRAY_8);
      }
    }

    return HPSG_STATE(SOLIDITY_HARD, KIND_OBJECT);
  }

  std::vector<uint8_t> buf_;
  uint8_t* p_;
  uint8_t* pieceLenField_;
  void* startOfNextMemoryChunk_;
  size_t totalAllocationUnits_;
  uint32_t type_;
  bool merge_;
  bool needHeader_;

  DISALLOW_COPY_AND_ASSIGN(HeapChunkContext);
};

void Dbg::DdmSendHeapSegments(bool native) {
  Dbg::HpsgWhen when;
  Dbg::HpsgWhat what;
  if (!native) {
    when = gDdmHpsgWhen;
    what = gDdmHpsgWhat;
  } else {
    when = gDdmNhsgWhen;
    what = gDdmNhsgWhat;
  }
  if (when == HPSG_WHEN_NEVER) {
    return;
  }

  // Figure out what kind of chunks we'll be sending.
  CHECK(what == HPSG_WHAT_MERGED_OBJECTS || what == HPSG_WHAT_DISTINCT_OBJECTS) << static_cast<int>(what);

  // First, send a heap start chunk.
  uint8_t heap_id[4];
  JDWP::Set4BE(&heap_id[0], 1);  // Heap id (bogus; we only have one heap).
  Dbg::DdmSendChunk(native ? CHUNK_TYPE("NHST") : CHUNK_TYPE("HPST"), sizeof(heap_id), heap_id);

  // Send a series of heap segment chunks.
  HeapChunkContext context((what == HPSG_WHAT_MERGED_OBJECTS), native);
  if (native) {
    dlmalloc_inspect_all(HeapChunkContext::HeapChunkCallback, &context);
  } else {
    gc::Heap* heap = Runtime::Current()->GetHeap();
    const std::vector<gc::space::ContinuousSpace*>& spaces = heap->GetContinuousSpaces();
    Thread* self = Thread::Current();
    ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
    typedef std::vector<gc::space::ContinuousSpace*>::const_iterator It;
    for (It cur = spaces.begin(), end = spaces.end(); cur != end; ++cur) {
      if ((*cur)->IsDlMallocSpace()) {
        (*cur)->AsDlMallocSpace()->Walk(HeapChunkContext::HeapChunkCallback, &context);
      }
    }
    // Walk the large objects, these are not in the AllocSpace.
    heap->GetLargeObjectsSpace()->Walk(HeapChunkContext::HeapChunkCallback, &context);
  }

  // Finally, send a heap end chunk.
  Dbg::DdmSendChunk(native ? CHUNK_TYPE("NHEN") : CHUNK_TYPE("HPEN"), sizeof(heap_id), heap_id);
}

static size_t GetAllocTrackerMax() {
#ifdef HAVE_ANDROID_OS
  // Check whether there's a system property overriding the number of records.
  const char* propertyName = "dalvik.vm.allocTrackerMax";
  char allocRecordMaxString[PROPERTY_VALUE_MAX];
  if (property_get(propertyName, allocRecordMaxString, "") > 0) {
    char* end;
    size_t value = strtoul(allocRecordMaxString, &end, 10);
    if (*end != '\0') {
      LOG(ERROR) << "Ignoring  " << propertyName << " '" << allocRecordMaxString
                 << "' --- invalid";
      return kDefaultNumAllocRecords;
    }
    if (!IsPowerOfTwo(value)) {
      LOG(ERROR) << "Ignoring  " << propertyName << " '" << allocRecordMaxString
                 << "' --- not power of two";
      return kDefaultNumAllocRecords;
    }
    return value;
  }
#endif
  return kDefaultNumAllocRecords;
}

void Dbg::SetAllocTrackingEnabled(bool enabled) {
  MutexLock mu(Thread::Current(), gAllocTrackerLock);
  if (enabled) {
    if (recent_allocation_records_ == NULL) {
      gAllocRecordMax = GetAllocTrackerMax();
      LOG(INFO) << "Enabling alloc tracker (" << gAllocRecordMax << " entries of "
                << kMaxAllocRecordStackDepth << " frames, taking "
                << PrettySize(sizeof(AllocRecord) * gAllocRecordMax) << ")";
      gAllocRecordHead = gAllocRecordCount = 0;
      recent_allocation_records_ = new AllocRecord[gAllocRecordMax];
      CHECK(recent_allocation_records_ != NULL);
    }
  } else {
    delete[] recent_allocation_records_;
    recent_allocation_records_ = NULL;
  }
}

struct AllocRecordStackVisitor : public StackVisitor {
  AllocRecordStackVisitor(Thread* thread, AllocRecord* record)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(thread, NULL), record(record), depth(0) {}

  // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
  // annotalysis.
  bool VisitFrame() NO_THREAD_SAFETY_ANALYSIS {
    if (depth >= kMaxAllocRecordStackDepth) {
      return false;
    }
    mirror::ArtMethod* m = GetMethod();
    if (!m->IsRuntimeMethod()) {
      record->stack[depth].method = m;
      record->stack[depth].dex_pc = GetDexPc();
      ++depth;
    }
    return true;
  }

  ~AllocRecordStackVisitor() {
    // Clear out any unused stack trace elements.
    for (; depth < kMaxAllocRecordStackDepth; ++depth) {
      record->stack[depth].method = NULL;
      record->stack[depth].dex_pc = 0;
    }
  }

  AllocRecord* record;
  size_t depth;
};

void Dbg::RecordAllocation(mirror::Class* type, size_t byte_count) {
  Thread* self = Thread::Current();
  CHECK(self != NULL);

  MutexLock mu(self, gAllocTrackerLock);
  if (recent_allocation_records_ == NULL) {
    return;
  }

  // Advance and clip.
  if (++gAllocRecordHead == gAllocRecordMax) {
    gAllocRecordHead = 0;
  }

  // Fill in the basics.
  AllocRecord* record = &recent_allocation_records_[gAllocRecordHead];
  record->type = type;
  record->byte_count = byte_count;
  record->thin_lock_id = self->GetThinLockId();

  // Fill in the stack trace.
  AllocRecordStackVisitor visitor(self, record);
  visitor.WalkStack();

  if (gAllocRecordCount < gAllocRecordMax) {
    ++gAllocRecordCount;
  }
}

// Returns the index of the head element.
//
// We point at the most-recently-written record, so if gAllocRecordCount is 1
// we want to use the current element.  Take "head+1" and subtract count
// from it.
//
// We need to handle underflow in our circular buffer, so we add
// gAllocRecordMax and then mask it back down.
static inline int HeadIndex() EXCLUSIVE_LOCKS_REQUIRED(gAllocTrackerLock) {
  return (gAllocRecordHead+1 + gAllocRecordMax - gAllocRecordCount) & (gAllocRecordMax-1);
}

void Dbg::DumpRecentAllocations() {
  ScopedObjectAccess soa(Thread::Current());
  MutexLock mu(soa.Self(), gAllocTrackerLock);
  if (recent_allocation_records_ == NULL) {
    LOG(INFO) << "Not recording tracked allocations";
    return;
  }

  // "i" is the head of the list.  We want to start at the end of the
  // list and move forward to the tail.
  size_t i = HeadIndex();
  size_t count = gAllocRecordCount;

  LOG(INFO) << "Tracked allocations, (head=" << gAllocRecordHead << " count=" << count << ")";
  while (count--) {
    AllocRecord* record = &recent_allocation_records_[i];

    LOG(INFO) << StringPrintf(" Thread %-2d %6zd bytes ", record->thin_lock_id, record->byte_count)
              << PrettyClass(record->type);

    for (size_t stack_frame = 0; stack_frame < kMaxAllocRecordStackDepth; ++stack_frame) {
      const mirror::ArtMethod* m = record->stack[stack_frame].method;
      if (m == NULL) {
        break;
      }
      LOG(INFO) << "    " << PrettyMethod(m) << " line " << record->stack[stack_frame].LineNumber();
    }

    // pause periodically to help logcat catch up
    if ((count % 5) == 0) {
      usleep(40000);
    }

    i = (i + 1) & (gAllocRecordMax-1);
  }
}

class StringTable {
 public:
  StringTable() {
  }

  void Add(const char* s) {
    table_.insert(s);
  }

  size_t IndexOf(const char* s) const {
    auto it = table_.find(s);
    if (it == table_.end()) {
      LOG(FATAL) << "IndexOf(\"" << s << "\") failed";
    }
    return std::distance(table_.begin(), it);
  }

  size_t Size() const {
    return table_.size();
  }

  void WriteTo(std::vector<uint8_t>& bytes) const {
    for (const std::string& str : table_) {
      const char* s = str.c_str();
      size_t s_len = CountModifiedUtf8Chars(s);
      UniquePtr<uint16_t> s_utf16(new uint16_t[s_len]);
      ConvertModifiedUtf8ToUtf16(s_utf16.get(), s);
      JDWP::AppendUtf16BE(bytes, s_utf16.get(), s_len);
    }
  }

 private:
  std::set<std::string> table_;
  DISALLOW_COPY_AND_ASSIGN(StringTable);
};

/*
 * The data we send to DDMS contains everything we have recorded.
 *
 * Message header (all values big-endian):
 * (1b) message header len (to allow future expansion); includes itself
 * (1b) entry header len
 * (1b) stack frame len
 * (2b) number of entries
 * (4b) offset to string table from start of message
 * (2b) number of class name strings
 * (2b) number of method name strings
 * (2b) number of source file name strings
 * For each entry:
 *   (4b) total allocation size
 *   (2b) thread id
 *   (2b) allocated object's class name index
 *   (1b) stack depth
 *   For each stack frame:
 *     (2b) method's class name
 *     (2b) method name
 *     (2b) method source file
 *     (2b) line number, clipped to 32767; -2 if native; -1 if no source
 * (xb) class name strings
 * (xb) method name strings
 * (xb) source file strings
 *
 * As with other DDM traffic, strings are sent as a 4-byte length
 * followed by UTF-16 data.
 *
 * We send up 16-bit unsigned indexes into string tables.  In theory there
 * can be (kMaxAllocRecordStackDepth * gAllocRecordMax) unique strings in
 * each table, but in practice there should be far fewer.
 *
 * The chief reason for using a string table here is to keep the size of
 * the DDMS message to a minimum.  This is partly to make the protocol
 * efficient, but also because we have to form the whole thing up all at
 * once in a memory buffer.
 *
 * We use separate string tables for class names, method names, and source
 * files to keep the indexes small.  There will generally be no overlap
 * between the contents of these tables.
 */
jbyteArray Dbg::GetRecentAllocations() {
  if (false) {
    DumpRecentAllocations();
  }

  Thread* self = Thread::Current();
  std::vector<uint8_t> bytes;
  {
    MutexLock mu(self, gAllocTrackerLock);
    //
    // Part 1: generate string tables.
    //
    StringTable class_names;
    StringTable method_names;
    StringTable filenames;

    int count = gAllocRecordCount;
    int idx = HeadIndex();
    while (count--) {
      AllocRecord* record = &recent_allocation_records_[idx];

      class_names.Add(ClassHelper(record->type).GetDescriptor());

      MethodHelper mh;
      for (size_t i = 0; i < kMaxAllocRecordStackDepth; i++) {
        mirror::ArtMethod* m = record->stack[i].method;
        if (m != NULL) {
          mh.ChangeMethod(m);
          class_names.Add(mh.GetDeclaringClassDescriptor());
          method_names.Add(mh.GetName());
          filenames.Add(mh.GetDeclaringClassSourceFile());
        }
      }

      idx = (idx + 1) & (gAllocRecordMax-1);
    }

    LOG(INFO) << "allocation records: " << gAllocRecordCount;

    //
    // Part 2: Generate the output and store it in the buffer.
    //

    // (1b) message header len (to allow future expansion); includes itself
    // (1b) entry header len
    // (1b) stack frame len
    const int kMessageHeaderLen = 15;
    const int kEntryHeaderLen = 9;
    const int kStackFrameLen = 8;
    JDWP::Append1BE(bytes, kMessageHeaderLen);
    JDWP::Append1BE(bytes, kEntryHeaderLen);
    JDWP::Append1BE(bytes, kStackFrameLen);

    // (2b) number of entries
    // (4b) offset to string table from start of message
    // (2b) number of class name strings
    // (2b) number of method name strings
    // (2b) number of source file name strings
    JDWP::Append2BE(bytes, gAllocRecordCount);
    size_t string_table_offset = bytes.size();
    JDWP::Append4BE(bytes, 0);  // We'll patch this later...
    JDWP::Append2BE(bytes, class_names.Size());
    JDWP::Append2BE(bytes, method_names.Size());
    JDWP::Append2BE(bytes, filenames.Size());

    count = gAllocRecordCount;
    idx = HeadIndex();
    ClassHelper kh;
    while (count--) {
      // For each entry:
      // (4b) total allocation size
      // (2b) thread id
      // (2b) allocated object's class name index
      // (1b) stack depth
      AllocRecord* record = &recent_allocation_records_[idx];
      size_t stack_depth = record->GetDepth();
      kh.ChangeClass(record->type);
      size_t allocated_object_class_name_index = class_names.IndexOf(kh.GetDescriptor());
      JDWP::Append4BE(bytes, record->byte_count);
      JDWP::Append2BE(bytes, record->thin_lock_id);
      JDWP::Append2BE(bytes, allocated_object_class_name_index);
      JDWP::Append1BE(bytes, stack_depth);

      MethodHelper mh;
      for (size_t stack_frame = 0; stack_frame < stack_depth; ++stack_frame) {
        // For each stack frame:
        // (2b) method's class name
        // (2b) method name
        // (2b) method source file
        // (2b) line number, clipped to 32767; -2 if native; -1 if no source
        mh.ChangeMethod(record->stack[stack_frame].method);
        size_t class_name_index = class_names.IndexOf(mh.GetDeclaringClassDescriptor());
        size_t method_name_index = method_names.IndexOf(mh.GetName());
        size_t file_name_index = filenames.IndexOf(mh.GetDeclaringClassSourceFile());
        JDWP::Append2BE(bytes, class_name_index);
        JDWP::Append2BE(bytes, method_name_index);
        JDWP::Append2BE(bytes, file_name_index);
        JDWP::Append2BE(bytes, record->stack[stack_frame].LineNumber());
      }

      idx = (idx + 1) & (gAllocRecordMax-1);
    }

    // (xb) class name strings
    // (xb) method name strings
    // (xb) source file strings
    JDWP::Set4BE(&bytes[string_table_offset], bytes.size());
    class_names.WriteTo(bytes);
    method_names.WriteTo(bytes);
    filenames.WriteTo(bytes);
  }
  JNIEnv* env = self->GetJniEnv();
  jbyteArray result = env->NewByteArray(bytes.size());
  if (result != NULL) {
    env->SetByteArrayRegion(result, 0, bytes.size(), reinterpret_cast<const jbyte*>(&bytes[0]));
  }
  return result;
}

}  // namespace art
