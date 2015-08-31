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

/*
 * Dalvik-specific side of debugger support.  (The JDWP code is intended to
 * be relatively generic.)
 */
#ifndef ART_RUNTIME_DEBUGGER_H_
#define ART_RUNTIME_DEBUGGER_H_

#include <pthread.h>

#include <string>

#include "jdwp/jdwp.h"
#include "jni.h"
#include "jvalue.h"
#include "root_visitor.h"
#include "thread_state.h"

namespace art {
namespace mirror {
class ArtMethod;
class Class;
class Object;
class Throwable;
}  // namespace mirror
struct AllocRecord;
class Thread;
class ThrowLocation;

/*
 * Invoke-during-breakpoint support.
 */
struct DebugInvokeReq {
  DebugInvokeReq()
      : ready(false), invoke_needed_(false),
        receiver_(NULL), thread_(NULL), class_(NULL), method_(NULL),
        arg_count_(0), arg_values_(NULL), options_(0), error(JDWP::ERR_NONE),
        result_tag(JDWP::JT_VOID), exception(0),
        lock_("a DebugInvokeReq lock", kBreakpointInvokeLock),
        cond_("a DebugInvokeReq condition variable", lock_) {
  }

  /* boolean; only set when we're in the tail end of an event handler */
  bool ready;

  /* boolean; set if the JDWP thread wants this thread to do work */
  bool invoke_needed_;

  /* request */
  mirror::Object* receiver_;      /* not used for ClassType.InvokeMethod */
  mirror::Object* thread_;
  mirror::Class* class_;
  mirror::ArtMethod* method_;
  uint32_t arg_count_;
  uint64_t* arg_values_;   /* will be NULL if arg_count_ == 0 */
  uint32_t options_;

  /* result */
  JDWP::JdwpError error;
  JDWP::JdwpTag result_tag;
  JValue result_value;
  JDWP::ObjectId exception;

  /* condition variable to wait on while the method executes */
  Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  ConditionVariable cond_ GUARDED_BY(lock_);
};

class Dbg {
 public:
  static bool ParseJdwpOptions(const std::string& options);
  static void SetJdwpAllowed(bool allowed);

  static void StartJdwp();
  static void StopJdwp();

  // Invoked by the GC in case we need to keep DDMS informed.
  static void GcDidFinish() LOCKS_EXCLUDED(Locks::mutator_lock_);

  // Return the DebugInvokeReq for the current thread.
  static DebugInvokeReq* GetInvokeReq();

  static Thread* GetDebugThread();
  static void ClearWaitForEventThread();

  /*
   * Enable/disable breakpoints and step modes.  Used to provide a heads-up
   * when the debugger attaches.
   */
  static void Connected();
  static void GoActive() LOCKS_EXCLUDED(Locks::breakpoint_lock_, Locks::mutator_lock_);
  static void Disconnected() LOCKS_EXCLUDED(Locks::mutator_lock_);
  static void Disposed();

  // Returns true if we're actually debugging with a real debugger, false if it's
  // just DDMS (or nothing at all).
  static bool IsDebuggerActive();

  // Returns true if we had -Xrunjdwp or -agentlib:jdwp= on the command line.
  static bool IsJdwpConfigured();

  static bool IsDisposed();

  /*
   * Time, in milliseconds, since the last debugger activity.  Does not
   * include DDMS activity.  Returns -1 if there has been no activity.
   * Returns 0 if we're in the middle of handling a debugger request.
   */
  static int64_t LastDebuggerActivity();

  static void UndoDebuggerSuspensions();

  /*
   * Class, Object, Array
   */
  static std::string GetClassName(JDWP::RefTypeId id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetClassObject(JDWP::RefTypeId id, JDWP::ObjectId& class_object_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetSuperclass(JDWP::RefTypeId id, JDWP::RefTypeId& superclass_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetClassLoader(JDWP::RefTypeId id, JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetModifiers(JDWP::RefTypeId id, JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetReflectedType(JDWP::RefTypeId class_id, JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void GetClassList(std::vector<JDWP::RefTypeId>& classes)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetClassInfo(JDWP::RefTypeId class_id, JDWP::JdwpTypeTag* pTypeTag,
                                      uint32_t* pStatus, std::string* pDescriptor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void FindLoadedClassBySignature(const char* descriptor, std::vector<JDWP::RefTypeId>& ids)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetReferenceType(JDWP::ObjectId object_id, JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetSignature(JDWP::RefTypeId ref_type_id, std::string& signature)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetSourceFile(JDWP::RefTypeId ref_type_id, std::string& source_file)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetObjectTag(JDWP::ObjectId object_id, uint8_t& tag)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static size_t GetTagWidth(JDWP::JdwpTag tag);

  static JDWP::JdwpError GetArrayLength(JDWP::ObjectId array_id, int& length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError OutputArray(JDWP::ObjectId array_id, int offset, int count,
                                     JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError SetArrayElements(JDWP::ObjectId array_id, int offset, int count,
                                          JDWP::Request& request)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::ObjectId CreateString(const std::string& str)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError CreateObject(JDWP::RefTypeId class_id, JDWP::ObjectId& new_object)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError CreateArrayObject(JDWP::RefTypeId array_class_id, uint32_t length,
                                           JDWP::ObjectId& new_array)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool MatchType(JDWP::RefTypeId instance_class_id, JDWP::RefTypeId class_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  //
  // Monitors.
  //
  static JDWP::JdwpError GetMonitorInfo(JDWP::ObjectId object_id, JDWP::ExpandBuf* reply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetOwnedMonitors(JDWP::ObjectId thread_id,
                                          std::vector<JDWP::ObjectId>& monitors,
                                          std::vector<uint32_t>& stack_depths)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetContendedMonitor(JDWP::ObjectId thread_id, JDWP::ObjectId& contended_monitor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  //
  // Heap.
  //
  static JDWP::JdwpError GetInstanceCounts(const std::vector<JDWP::RefTypeId>& class_ids,
                                           std::vector<uint64_t>& counts)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetInstances(JDWP::RefTypeId class_id, int32_t max_count,
                                      std::vector<JDWP::ObjectId>& instances)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetReferringObjects(JDWP::ObjectId object_id, int32_t max_count,
                                             std::vector<JDWP::ObjectId>& referring_objects)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError DisableCollection(JDWP::ObjectId object_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError EnableCollection(JDWP::ObjectId object_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError IsCollected(JDWP::ObjectId object_id, bool& is_collected)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DisposeObject(JDWP::ObjectId object_id, uint32_t reference_count)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  //
  // Methods and fields.
  //
  static std::string GetMethodName(JDWP::MethodId method_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError OutputDeclaredFields(JDWP::RefTypeId ref_type_id, bool with_generic,
                                              JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError OutputDeclaredMethods(JDWP::RefTypeId ref_type_id, bool with_generic,
                                               JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError OutputDeclaredInterfaces(JDWP::RefTypeId ref_type_id,
                                                  JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void OutputLineTable(JDWP::RefTypeId ref_type_id, JDWP::MethodId method_id,
                              JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void OutputVariableTable(JDWP::RefTypeId ref_type_id, JDWP::MethodId id, bool with_generic,
                                  JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetBytecodes(JDWP::RefTypeId class_id, JDWP::MethodId method_id,
                                      std::vector<uint8_t>& bytecodes)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static std::string GetFieldName(JDWP::FieldId field_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpTag GetFieldBasicTag(JDWP::FieldId field_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpTag GetStaticFieldBasicTag(JDWP::FieldId field_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetFieldValue(JDWP::ObjectId object_id, JDWP::FieldId field_id,
                                       JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError SetFieldValue(JDWP::ObjectId object_id, JDWP::FieldId field_id,
                                       uint64_t value, int width)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetStaticFieldValue(JDWP::RefTypeId ref_type_id, JDWP::FieldId field_id,
                                             JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError SetStaticFieldValue(JDWP::FieldId field_id, uint64_t value, int width)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static std::string StringToUtf8(JDWP::ObjectId string_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Thread, ThreadGroup, Frame
   */
  static JDWP::JdwpError GetThreadName(JDWP::ObjectId thread_id, std::string& name)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::thread_list_lock_);
  static JDWP::JdwpError GetThreadGroup(JDWP::ObjectId thread_id, JDWP::ExpandBuf* pReply);
  static std::string GetThreadGroupName(JDWP::ObjectId thread_group_id);
  static JDWP::ObjectId GetThreadGroupParent(JDWP::ObjectId thread_group_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::ObjectId GetSystemThreadGroupId()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::ObjectId GetMainThreadGroupId();

  static JDWP::JdwpThreadStatus ToJdwpThreadStatus(ThreadState state);
  static JDWP::JdwpError GetThreadStatus(JDWP::ObjectId thread_id, JDWP::JdwpThreadStatus* pThreadStatus, JDWP::JdwpSuspendStatus* pSuspendStatus);
  static JDWP::JdwpError GetThreadDebugSuspendCount(JDWP::ObjectId thread_id, JDWP::ExpandBuf* pReply);
  // static void WaitForSuspend(JDWP::ObjectId thread_id);

  // Fills 'thread_ids' with the threads in the given thread group. If thread_group_id == 0,
  // returns all threads.
  static void GetThreads(JDWP::ObjectId thread_group_id, std::vector<JDWP::ObjectId>& thread_ids)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void GetChildThreadGroups(JDWP::ObjectId thread_group_id, std::vector<JDWP::ObjectId>& child_thread_group_ids);

  static JDWP::JdwpError GetThreadFrameCount(JDWP::ObjectId thread_id, size_t& result);
  static JDWP::JdwpError GetThreadFrames(JDWP::ObjectId thread_id, size_t start_frame,
                                         size_t frame_count, JDWP::ExpandBuf* buf)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::ObjectId GetThreadSelfId()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void SuspendVM()
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);
  static void ResumeVM();
  static JDWP::JdwpError SuspendThread(JDWP::ObjectId thread_id, bool request_suspension = true)
      LOCKS_EXCLUDED(Locks::mutator_lock_,
                     Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);

  static void ResumeThread(JDWP::ObjectId thread_id)
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void SuspendSelf();

  static JDWP::JdwpError GetThisObject(JDWP::ObjectId thread_id, JDWP::FrameId frame_id,
                                       JDWP::ObjectId* result)
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void GetLocalValue(JDWP::ObjectId thread_id, JDWP::FrameId frame_id, int slot,
                            JDWP::JdwpTag tag, uint8_t* buf, size_t expectedLen)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void SetLocalValue(JDWP::ObjectId thread_id, JDWP::FrameId frame_id, int slot,
                            JDWP::JdwpTag tag, uint64_t value, size_t width)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::JdwpError Interrupt(JDWP::ObjectId thread_id);

  /*
   * Debugger notification
   */
  enum {
    kBreakpoint     = 0x01,
    kSingleStep     = 0x02,
    kMethodEntry    = 0x04,
    kMethodExit     = 0x08,
  };
  static void PostLocationEvent(const mirror::ArtMethod* method, int pcOffset,
                                mirror::Object* thisPtr, int eventFlags)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void PostException(Thread* thread, const ThrowLocation& throw_location,
                            mirror::ArtMethod* catch_method,
                            uint32_t catch_dex_pc, mirror::Throwable* exception)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void PostThreadStart(Thread* t)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void PostThreadDeath(Thread* t)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void PostClassPrepare(mirror::Class* c)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void UpdateDebugger(Thread* thread, mirror::Object* this_object,
                             const mirror::ArtMethod* method, uint32_t new_dex_pc)
      LOCKS_EXCLUDED(Locks::breakpoint_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void WatchLocation(const JDWP::JdwpLocation* pLoc)
      LOCKS_EXCLUDED(Locks::breakpoint_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void UnwatchLocation(const JDWP::JdwpLocation* pLoc)
      LOCKS_EXCLUDED(Locks::breakpoint_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError ConfigureStep(JDWP::ObjectId thread_id, JDWP::JdwpStepSize size,
                                       JDWP::JdwpStepDepth depth)
      LOCKS_EXCLUDED(Locks::breakpoint_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void UnconfigureStep(JDWP::ObjectId thread_id) LOCKS_EXCLUDED(Locks::breakpoint_lock_);

  static JDWP::JdwpError InvokeMethod(JDWP::ObjectId thread_id, JDWP::ObjectId object_id,
                                      JDWP::RefTypeId class_id, JDWP::MethodId method_id,
                                      uint32_t arg_count, uint64_t* arg_values,
                                      JDWP::JdwpTag* arg_types, uint32_t options,
                                      JDWP::JdwpTag* pResultTag, uint64_t* pResultValue,
                                      JDWP::ObjectId* pExceptObj)
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void ExecuteMethod(DebugInvokeReq* pReq);

  /*
   * DDM support.
   */
  static void DdmSendThreadNotification(Thread* t, uint32_t type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmSetThreadNotification(bool enable);
  static bool DdmHandlePacket(JDWP::Request& request, uint8_t** pReplyBuf, int* pReplyLen);
  static void DdmConnected() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmDisconnected() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmSendChunk(uint32_t type, const std::vector<uint8_t>& bytes)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmSendChunk(uint32_t type, size_t len, const uint8_t* buf)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmSendChunkV(uint32_t type, const iovec* iov, int iov_count)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Recent allocation tracking support.
   */
  static void RecordAllocation(mirror::Class* type, size_t byte_count)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void SetAllocTrackingEnabled(bool enabled);
  static inline bool IsAllocTrackingEnabled() { return recent_allocation_records_ != NULL; }
  static jbyteArray GetRecentAllocations() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DumpRecentAllocations();

  enum HpifWhen {
    HPIF_WHEN_NEVER = 0,
    HPIF_WHEN_NOW = 1,
    HPIF_WHEN_NEXT_GC = 2,
    HPIF_WHEN_EVERY_GC = 3
  };
  static int DdmHandleHpifChunk(HpifWhen when)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  enum HpsgWhen {
    HPSG_WHEN_NEVER = 0,
    HPSG_WHEN_EVERY_GC = 1,
  };
  enum HpsgWhat {
    HPSG_WHAT_MERGED_OBJECTS = 0,
    HPSG_WHAT_DISTINCT_OBJECTS = 1,
  };
  static bool DdmHandleHpsgNhsgChunk(HpsgWhen when, HpsgWhat what, bool native);

  static void DdmSendHeapInfo(HpifWhen reason)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmSendHeapSegments(bool native)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  static void DdmBroadcast(bool connect) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void PostThreadStartOrStop(Thread*, uint32_t)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static AllocRecord* recent_allocation_records_;
};

#define CHUNK_TYPE(_name) \
    static_cast<uint32_t>((_name)[0] << 24 | (_name)[1] << 16 | (_name)[2] << 8 | (_name)[3])

}  // namespace art

#endif  // ART_RUNTIME_DEBUGGER_H_
