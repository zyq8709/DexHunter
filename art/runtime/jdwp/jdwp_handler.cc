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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "atomic.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stringprintf.h"
#include "debugger.h"
#include "jdwp/jdwp_constants.h"
#include "jdwp/jdwp_event.h"
#include "jdwp/jdwp_expand_buf.h"
#include "jdwp/jdwp_priv.h"
#include "runtime.h"
#include "thread-inl.h"
#include "UniquePtr.h"

namespace art {

namespace JDWP {

std::string DescribeField(const FieldId& field_id) {
  return StringPrintf("%#x (%s)", field_id, Dbg::GetFieldName(field_id).c_str());
}

std::string DescribeMethod(const MethodId& method_id) {
  return StringPrintf("%#x (%s)", method_id, Dbg::GetMethodName(method_id).c_str());
}

std::string DescribeRefTypeId(const RefTypeId& ref_type_id) {
  std::string signature("unknown");
  Dbg::GetSignature(ref_type_id, signature);
  return StringPrintf("%#llx (%s)", ref_type_id, signature.c_str());
}

// Helper function: write a variable-width value into the output input buffer.
static void WriteValue(ExpandBuf* pReply, int width, uint64_t value) {
  switch (width) {
  case 1:     expandBufAdd1(pReply, value); break;
  case 2:     expandBufAdd2BE(pReply, value); break;
  case 4:     expandBufAdd4BE(pReply, value); break;
  case 8:     expandBufAdd8BE(pReply, value); break;
  default:    LOG(FATAL) << width; break;
  }
}

static JdwpError WriteTaggedObject(ExpandBuf* reply, ObjectId object_id)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint8_t tag;
  JdwpError rc = Dbg::GetObjectTag(object_id, tag);
  if (rc == ERR_NONE) {
    expandBufAdd1(reply, tag);
    expandBufAddObjectId(reply, object_id);
  }
  return rc;
}

static JdwpError WriteTaggedObjectList(ExpandBuf* reply, const std::vector<ObjectId>& objects)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  expandBufAdd4BE(reply, objects.size());
  for (size_t i = 0; i < objects.size(); ++i) {
    JdwpError rc = WriteTaggedObject(reply, objects[i]);
    if (rc != ERR_NONE) {
      return rc;
    }
  }
  return ERR_NONE;
}

/*
 * Common code for *_InvokeMethod requests.
 *
 * If "is_constructor" is set, this returns "object_id" rather than the
 * expected-to-be-void return value of the called function.
 */
static JdwpError FinishInvoke(JdwpState*, Request& request, ExpandBuf* pReply,
                              ObjectId thread_id, ObjectId object_id,
                              RefTypeId class_id, MethodId method_id, bool is_constructor)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  CHECK(!is_constructor || object_id != 0);

  int32_t arg_count = request.ReadSigned32("argument count");

  VLOG(jdwp) << StringPrintf("    --> thread_id=%#llx object_id=%#llx", thread_id, object_id);
  VLOG(jdwp) << StringPrintf("        class_id=%#llx method_id=%x %s.%s", class_id,
                             method_id, Dbg::GetClassName(class_id).c_str(),
                             Dbg::GetMethodName(method_id).c_str());
  VLOG(jdwp) << StringPrintf("        %d args:", arg_count);

  UniquePtr<JdwpTag[]> argTypes(arg_count > 0 ? new JdwpTag[arg_count] : NULL);
  UniquePtr<uint64_t[]> argValues(arg_count > 0 ? new uint64_t[arg_count] : NULL);
  for (int32_t i = 0; i < arg_count; ++i) {
    argTypes[i] = request.ReadTag();
    size_t width = Dbg::GetTagWidth(argTypes[i]);
    argValues[i] = request.ReadValue(width);
    VLOG(jdwp) << "          " << argTypes[i] << StringPrintf("(%zd): %#llx", width, argValues[i]);
  }

  uint32_t options = request.ReadUnsigned32("InvokeOptions bit flags");
  VLOG(jdwp) << StringPrintf("        options=0x%04x%s%s", options,
                             (options & INVOKE_SINGLE_THREADED) ? " (SINGLE_THREADED)" : "",
                             (options & INVOKE_NONVIRTUAL) ? " (NONVIRTUAL)" : "");

  JdwpTag resultTag;
  uint64_t resultValue;
  ObjectId exceptObjId;
  JdwpError err = Dbg::InvokeMethod(thread_id, object_id, class_id, method_id, arg_count, argValues.get(), argTypes.get(), options, &resultTag, &resultValue, &exceptObjId);
  if (err != ERR_NONE) {
    return err;
  }

  if (err == ERR_NONE) {
    if (is_constructor) {
      // If we invoked a constructor (which actually returns void), return the receiver,
      // unless we threw, in which case we return NULL.
      resultTag = JT_OBJECT;
      resultValue = (exceptObjId == 0) ? object_id : 0;
    }

    size_t width = Dbg::GetTagWidth(resultTag);
    expandBufAdd1(pReply, resultTag);
    if (width != 0) {
      WriteValue(pReply, width, resultValue);
    }
    expandBufAdd1(pReply, JT_OBJECT);
    expandBufAddObjectId(pReply, exceptObjId);

    VLOG(jdwp) << "  --> returned " << resultTag << StringPrintf(" %#llx (except=%#llx)", resultValue, exceptObjId);

    /* show detailed debug output */
    if (resultTag == JT_STRING && exceptObjId == 0) {
      if (resultValue != 0) {
        VLOG(jdwp) << "      string '" << Dbg::StringToUtf8(resultValue) << "'";
      } else {
        VLOG(jdwp) << "      string (null)";
      }
    }
  }

  return err;
}

static JdwpError VM_Version(JdwpState*, Request&, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Text information on runtime version.
  std::string version(StringPrintf("Android Runtime %s", Runtime::Current()->GetVersion()));
  expandBufAddUtf8String(pReply, version);

  // JDWP version numbers, major and minor.
  expandBufAdd4BE(pReply, 1);
  expandBufAdd4BE(pReply, 6);

  // "java.version".
  expandBufAddUtf8String(pReply, "1.6.0");

  // "java.vm.name".
  expandBufAddUtf8String(pReply, "Dalvik");

  return ERR_NONE;
}

/*
 * Given a class JNI signature (e.g. "Ljava/lang/Error;"), return the
 * referenceTypeID.  We need to send back more than one if the class has
 * been loaded by multiple class loaders.
 */
static JdwpError VM_ClassesBySignature(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::string classDescriptor(request.ReadUtf8String());

  std::vector<RefTypeId> ids;
  Dbg::FindLoadedClassBySignature(classDescriptor.c_str(), ids);

  expandBufAdd4BE(pReply, ids.size());

  for (size_t i = 0; i < ids.size(); ++i) {
    // Get class vs. interface and status flags.
    JDWP::JdwpTypeTag type_tag;
    uint32_t class_status;
    JDWP::JdwpError status = Dbg::GetClassInfo(ids[i], &type_tag, &class_status, NULL);
    if (status != ERR_NONE) {
      return status;
    }

    expandBufAdd1(pReply, type_tag);
    expandBufAddRefTypeId(pReply, ids[i]);
    expandBufAdd4BE(pReply, class_status);
  }

  return ERR_NONE;
}

/*
 * Handle request for the thread IDs of all running threads.
 *
 * We exclude ourselves from the list, because we don't allow ourselves
 * to be suspended, and that violates some JDWP expectations.
 */
static JdwpError VM_AllThreads(JdwpState*, Request&, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::vector<ObjectId> thread_ids;
  Dbg::GetThreads(0, thread_ids);

  expandBufAdd4BE(pReply, thread_ids.size());
  for (uint32_t i = 0; i < thread_ids.size(); ++i) {
    expandBufAddObjectId(pReply, thread_ids[i]);
  }

  return ERR_NONE;
}

/*
 * List all thread groups that do not have a parent.
 */
static JdwpError VM_TopLevelThreadGroups(JdwpState*, Request&, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  /*
   * TODO: maintain a list of parentless thread groups in the VM.
   *
   * For now, just return "system".  Application threads are created
   * in "main", which is a child of "system".
   */
  uint32_t groups = 1;
  expandBufAdd4BE(pReply, groups);
  ObjectId thread_group_id = Dbg::GetSystemThreadGroupId();
  expandBufAddObjectId(pReply, thread_group_id);

  return ERR_NONE;
}

/*
 * Respond with the sizes of the basic debugger types.
 *
 * All IDs are 8 bytes.
 */
static JdwpError VM_IDSizes(JdwpState*, Request&, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  expandBufAdd4BE(pReply, sizeof(FieldId));
  expandBufAdd4BE(pReply, sizeof(MethodId));
  expandBufAdd4BE(pReply, sizeof(ObjectId));
  expandBufAdd4BE(pReply, sizeof(RefTypeId));
  expandBufAdd4BE(pReply, sizeof(FrameId));
  return ERR_NONE;
}

static JdwpError VM_Dispose(JdwpState*, Request&, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Dbg::Disposed();
  return ERR_NONE;
}

/*
 * Suspend the execution of the application running in the VM (i.e. suspend
 * all threads).
 *
 * This needs to increment the "suspend count" on all threads.
 */
static JdwpError VM_Suspend(JdwpState*, Request&, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Thread* self = Thread::Current();
  self->TransitionFromRunnableToSuspended(kWaitingForDebuggerSuspension);
  Dbg::SuspendVM();
  self->TransitionFromSuspendedToRunnable();
  return ERR_NONE;
}

/*
 * Resume execution.  Decrements the "suspend count" of all threads.
 */
static JdwpError VM_Resume(JdwpState*, Request&, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Dbg::ResumeVM();
  return ERR_NONE;
}

static JdwpError VM_Exit(JdwpState* state, Request& request, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint32_t exit_status = request.ReadUnsigned32("exit_status");
  state->ExitAfterReplying(exit_status);
  return ERR_NONE;
}

/*
 * Create a new string in the VM and return its ID.
 *
 * (Ctrl-Shift-I in Eclipse on an array of objects causes it to create the
 * string "java.util.Arrays".)
 */
static JdwpError VM_CreateString(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::string str(request.ReadUtf8String());
  ObjectId stringId = Dbg::CreateString(str);
  if (stringId == 0) {
    return ERR_OUT_OF_MEMORY;
  }
  expandBufAddObjectId(pReply, stringId);
  return ERR_NONE;
}

static JdwpError VM_ClassPaths(JdwpState*, Request&, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  expandBufAddUtf8String(pReply, "/");

  std::vector<std::string> class_path;
  Split(Runtime::Current()->GetClassPathString(), ':', class_path);
  expandBufAdd4BE(pReply, class_path.size());
  for (size_t i = 0; i < class_path.size(); ++i) {
    expandBufAddUtf8String(pReply, class_path[i]);
  }

  std::vector<std::string> boot_class_path;
  Split(Runtime::Current()->GetBootClassPathString(), ':', boot_class_path);
  expandBufAdd4BE(pReply, boot_class_path.size());
  for (size_t i = 0; i < boot_class_path.size(); ++i) {
    expandBufAddUtf8String(pReply, boot_class_path[i]);
  }

  return ERR_NONE;
}

static JdwpError VM_DisposeObjects(JdwpState*, Request& request, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  size_t object_count = request.ReadUnsigned32("object_count");
  for (size_t i = 0; i < object_count; ++i) {
    ObjectId object_id = request.ReadObjectId();
    uint32_t reference_count = request.ReadUnsigned32("reference_count");
    Dbg::DisposeObject(object_id, reference_count);
  }
  return ERR_NONE;
}

static JdwpError VM_Capabilities(JdwpState*, Request&, ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  expandBufAdd1(reply, false);   // canWatchFieldModification
  expandBufAdd1(reply, false);   // canWatchFieldAccess
  expandBufAdd1(reply, true);    // canGetBytecodes
  expandBufAdd1(reply, true);    // canGetSyntheticAttribute
  expandBufAdd1(reply, true);    // canGetOwnedMonitorInfo
  expandBufAdd1(reply, true);    // canGetCurrentContendedMonitor
  expandBufAdd1(reply, true);    // canGetMonitorInfo
  return ERR_NONE;
}

static JdwpError VM_CapabilitiesNew(JdwpState*, Request& request, ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // The first few capabilities are the same as those reported by the older call.
  VM_Capabilities(NULL, request, reply);

  expandBufAdd1(reply, false);   // canRedefineClasses
  expandBufAdd1(reply, false);   // canAddMethod
  expandBufAdd1(reply, false);   // canUnrestrictedlyRedefineClasses
  expandBufAdd1(reply, false);   // canPopFrames
  expandBufAdd1(reply, false);   // canUseInstanceFilters
  expandBufAdd1(reply, false);   // canGetSourceDebugExtension
  expandBufAdd1(reply, false);   // canRequestVMDeathEvent
  expandBufAdd1(reply, false);   // canSetDefaultStratum
  expandBufAdd1(reply, true);    // 1.6: canGetInstanceInfo
  expandBufAdd1(reply, false);   // 1.6: canRequestMonitorEvents
  expandBufAdd1(reply, true);    // 1.6: canGetMonitorFrameInfo
  expandBufAdd1(reply, false);   // 1.6: canUseSourceNameFilters
  expandBufAdd1(reply, false);   // 1.6: canGetConstantPool
  expandBufAdd1(reply, false);   // 1.6: canForceEarlyReturn

  // Fill in reserved22 through reserved32; note count started at 1.
  for (size_t i = 22; i <= 32; ++i) {
    expandBufAdd1(reply, false);
  }
  return ERR_NONE;
}

static JdwpError VM_AllClassesImpl(ExpandBuf* pReply, bool descriptor_and_status, bool generic)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::vector<JDWP::RefTypeId> classes;
  Dbg::GetClassList(classes);

  expandBufAdd4BE(pReply, classes.size());

  for (size_t i = 0; i < classes.size(); ++i) {
    static const char genericSignature[1] = "";
    JDWP::JdwpTypeTag type_tag;
    std::string descriptor;
    uint32_t class_status;
    JDWP::JdwpError status = Dbg::GetClassInfo(classes[i], &type_tag, &class_status, &descriptor);
    if (status != ERR_NONE) {
      return status;
    }

    expandBufAdd1(pReply, type_tag);
    expandBufAddRefTypeId(pReply, classes[i]);
    if (descriptor_and_status) {
      expandBufAddUtf8String(pReply, descriptor);
      if (generic) {
        expandBufAddUtf8String(pReply, genericSignature);
      }
      expandBufAdd4BE(pReply, class_status);
    }
  }

  return ERR_NONE;
}

static JdwpError VM_AllClasses(JdwpState*, Request&, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return VM_AllClassesImpl(pReply, true, false);
}

static JdwpError VM_AllClassesWithGeneric(JdwpState*, Request&, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return VM_AllClassesImpl(pReply, true, true);
}

static JdwpError VM_InstanceCounts(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  int32_t class_count = request.ReadSigned32("class count");
  if (class_count < 0) {
    return ERR_ILLEGAL_ARGUMENT;
  }
  std::vector<RefTypeId> class_ids;
  for (int32_t i = 0; i < class_count; ++i) {
    class_ids.push_back(request.ReadRefTypeId());
  }

  std::vector<uint64_t> counts;
  JdwpError rc = Dbg::GetInstanceCounts(class_ids, counts);
  if (rc != ERR_NONE) {
    return rc;
  }

  expandBufAdd4BE(pReply, counts.size());
  for (size_t i = 0; i < counts.size(); ++i) {
    expandBufAdd8BE(pReply, counts[i]);
  }
  return ERR_NONE;
}

static JdwpError RT_Modifiers(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  return Dbg::GetModifiers(refTypeId, pReply);
}

/*
 * Get values from static fields in a reference type.
 */
static JdwpError RT_GetValues(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  int32_t field_count = request.ReadSigned32("field count");
  expandBufAdd4BE(pReply, field_count);
  for (int32_t i = 0; i < field_count; ++i) {
    FieldId fieldId = request.ReadFieldId();
    JdwpError status = Dbg::GetStaticFieldValue(refTypeId, fieldId, pReply);
    if (status != ERR_NONE) {
      return status;
    }
  }
  return ERR_NONE;
}

/*
 * Get the name of the source file in which a reference type was declared.
 */
static JdwpError RT_SourceFile(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  std::string source_file;
  JdwpError status = Dbg::GetSourceFile(refTypeId, source_file);
  if (status != ERR_NONE) {
    return status;
  }
  expandBufAddUtf8String(pReply, source_file);
  return ERR_NONE;
}

/*
 * Return the current status of the reference type.
 */
static JdwpError RT_Status(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  JDWP::JdwpTypeTag type_tag;
  uint32_t class_status;
  JDWP::JdwpError status = Dbg::GetClassInfo(refTypeId, &type_tag, &class_status, NULL);
  if (status != ERR_NONE) {
    return status;
  }
  expandBufAdd4BE(pReply, class_status);
  return ERR_NONE;
}

/*
 * Return interfaces implemented directly by this class.
 */
static JdwpError RT_Interfaces(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  return Dbg::OutputDeclaredInterfaces(refTypeId, pReply);
}

/*
 * Return the class object corresponding to this type.
 */
static JdwpError RT_ClassObject(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  ObjectId class_object_id;
  JdwpError status = Dbg::GetClassObject(refTypeId, class_object_id);
  if (status != ERR_NONE) {
    return status;
  }
  VLOG(jdwp) << StringPrintf("    --> ObjectId %#llx", class_object_id);
  expandBufAddObjectId(pReply, class_object_id);
  return ERR_NONE;
}

/*
 * Returns the value of the SourceDebugExtension attribute.
 *
 * JDB seems interested, but DEX files don't currently support this.
 */
static JdwpError RT_SourceDebugExtension(JdwpState*, Request&, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  /* referenceTypeId in, string out */
  return ERR_ABSENT_INFORMATION;
}

static JdwpError RT_Signature(JdwpState*, Request& request, ExpandBuf* pReply, bool with_generic)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();

  std::string signature;
  JdwpError status = Dbg::GetSignature(refTypeId, signature);
  if (status != ERR_NONE) {
    return status;
  }
  expandBufAddUtf8String(pReply, signature);
  if (with_generic) {
    expandBufAddUtf8String(pReply, "");
  }
  return ERR_NONE;
}

static JdwpError RT_Signature(JdwpState* state, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return RT_Signature(state, request, pReply, false);
}

static JdwpError RT_SignatureWithGeneric(JdwpState* state, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return RT_Signature(state, request, pReply, true);
}

/*
 * Return the instance of java.lang.ClassLoader that loaded the specified
 * reference type, or null if it was loaded by the system loader.
 */
static JdwpError RT_ClassLoader(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  return Dbg::GetClassLoader(refTypeId, pReply);
}

/*
 * Given a referenceTypeId, return a block of stuff that describes the
 * fields declared by a class.
 */
static JdwpError RT_FieldsWithGeneric(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  return Dbg::OutputDeclaredFields(refTypeId, true, pReply);
}

// Obsolete equivalent of FieldsWithGeneric, without the generic type information.
static JdwpError RT_Fields(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  return Dbg::OutputDeclaredFields(refTypeId, false, pReply);
}

/*
 * Given a referenceTypeID, return a block of goodies describing the
 * methods declared by a class.
 */
static JdwpError RT_MethodsWithGeneric(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  return Dbg::OutputDeclaredMethods(refTypeId, true, pReply);
}

// Obsolete equivalent of MethodsWithGeneric, without the generic type information.
static JdwpError RT_Methods(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  return Dbg::OutputDeclaredMethods(refTypeId, false, pReply);
}

static JdwpError RT_Instances(JdwpState*, Request& request, ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId class_id = request.ReadRefTypeId();
  int32_t max_count = request.ReadSigned32("max count");
  if (max_count < 0) {
    return ERR_ILLEGAL_ARGUMENT;
  }

  std::vector<ObjectId> instances;
  JdwpError rc = Dbg::GetInstances(class_id, max_count, instances);
  if (rc != ERR_NONE) {
    return rc;
  }

  return WriteTaggedObjectList(reply, instances);
}

/*
 * Return the immediate superclass of a class.
 */
static JdwpError CT_Superclass(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId class_id = request.ReadRefTypeId();
  RefTypeId superClassId;
  JdwpError status = Dbg::GetSuperclass(class_id, superClassId);
  if (status != ERR_NONE) {
    return status;
  }
  expandBufAddRefTypeId(pReply, superClassId);
  return ERR_NONE;
}

/*
 * Set static class values.
 */
static JdwpError CT_SetValues(JdwpState* , Request& request, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId class_id = request.ReadRefTypeId();
  int32_t values_count = request.ReadSigned32("values count");

  UNUSED(class_id);

  for (int32_t i = 0; i < values_count; ++i) {
    FieldId fieldId = request.ReadFieldId();
    JDWP::JdwpTag fieldTag = Dbg::GetStaticFieldBasicTag(fieldId);
    size_t width = Dbg::GetTagWidth(fieldTag);
    uint64_t value = request.ReadValue(width);

    VLOG(jdwp) << "    --> field=" << fieldId << " tag=" << fieldTag << " --> " << value;
    JdwpError status = Dbg::SetStaticFieldValue(fieldId, value, width);
    if (status != ERR_NONE) {
      return status;
    }
  }

  return ERR_NONE;
}

/*
 * Invoke a static method.
 *
 * Example: Eclipse sometimes uses java/lang/Class.forName(String s) on
 * values in the "variables" display.
 */
static JdwpError CT_InvokeMethod(JdwpState* state, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId class_id = request.ReadRefTypeId();
  ObjectId thread_id = request.ReadThreadId();
  MethodId method_id = request.ReadMethodId();

  return FinishInvoke(state, request, pReply, thread_id, 0, class_id, method_id, false);
}

/*
 * Create a new object of the requested type, and invoke the specified
 * constructor.
 *
 * Example: in IntelliJ, create a watch on "new String(myByteArray)" to
 * see the contents of a byte[] as a string.
 */
static JdwpError CT_NewInstance(JdwpState* state, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId class_id = request.ReadRefTypeId();
  ObjectId thread_id = request.ReadThreadId();
  MethodId method_id = request.ReadMethodId();

  ObjectId object_id;
  JdwpError status = Dbg::CreateObject(class_id, object_id);
  if (status != ERR_NONE) {
    return status;
  }
  if (object_id == 0) {
    return ERR_OUT_OF_MEMORY;
  }
  return FinishInvoke(state, request, pReply, thread_id, object_id, class_id, method_id, true);
}

/*
 * Create a new array object of the requested type and length.
 */
static JdwpError AT_newInstance(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId arrayTypeId = request.ReadRefTypeId();
  int32_t length = request.ReadSigned32("length");

  ObjectId object_id;
  JdwpError status = Dbg::CreateArrayObject(arrayTypeId, length, object_id);
  if (status != ERR_NONE) {
    return status;
  }
  if (object_id == 0) {
    return ERR_OUT_OF_MEMORY;
  }
  expandBufAdd1(pReply, JT_ARRAY);
  expandBufAddObjectId(pReply, object_id);
  return ERR_NONE;
}

/*
 * Return line number information for the method, if present.
 */
static JdwpError M_LineTable(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId refTypeId = request.ReadRefTypeId();
  MethodId method_id = request.ReadMethodId();

  Dbg::OutputLineTable(refTypeId, method_id, pReply);

  return ERR_NONE;
}

static JdwpError M_VariableTable(JdwpState*, Request& request, ExpandBuf* pReply,
                                 bool generic)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId class_id = request.ReadRefTypeId();
  MethodId method_id = request.ReadMethodId();

  // We could return ERR_ABSENT_INFORMATION here if the DEX file was built without local variable
  // information. That will cause Eclipse to make a best-effort attempt at displaying local
  // variables anonymously. However, the attempt isn't very good, so we're probably better off just
  // not showing anything.
  Dbg::OutputVariableTable(class_id, method_id, generic, pReply);
  return ERR_NONE;
}

static JdwpError M_VariableTable(JdwpState* state, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return M_VariableTable(state, request, pReply, false);
}

static JdwpError M_VariableTableWithGeneric(JdwpState* state, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return M_VariableTable(state, request, pReply, true);
}

static JdwpError M_Bytecodes(JdwpState*, Request& request, ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId class_id = request.ReadRefTypeId();
  MethodId method_id = request.ReadMethodId();

  std::vector<uint8_t> bytecodes;
  JdwpError rc = Dbg::GetBytecodes(class_id, method_id, bytecodes);
  if (rc != ERR_NONE) {
    return rc;
  }

  expandBufAdd4BE(reply, bytecodes.size());
  for (size_t i = 0; i < bytecodes.size(); ++i) {
    expandBufAdd1(reply, bytecodes[i]);
  }

  return ERR_NONE;
}

/*
 * Given an object reference, return the runtime type of the object
 * (class or array).
 *
 * This can get called on different things, e.g. thread_id gets
 * passed in here.
 */
static JdwpError OR_ReferenceType(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId object_id = request.ReadObjectId();
  return Dbg::GetReferenceType(object_id, pReply);
}

/*
 * Get values from the fields of an object.
 */
static JdwpError OR_GetValues(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId object_id = request.ReadObjectId();
  int32_t field_count = request.ReadSigned32("field count");

  expandBufAdd4BE(pReply, field_count);
  for (int32_t i = 0; i < field_count; ++i) {
    FieldId fieldId = request.ReadFieldId();
    JdwpError status = Dbg::GetFieldValue(object_id, fieldId, pReply);
    if (status != ERR_NONE) {
      return status;
    }
  }

  return ERR_NONE;
}

/*
 * Set values in the fields of an object.
 */
static JdwpError OR_SetValues(JdwpState*, Request& request, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId object_id = request.ReadObjectId();
  int32_t field_count = request.ReadSigned32("field count");

  for (int32_t i = 0; i < field_count; ++i) {
    FieldId fieldId = request.ReadFieldId();

    JDWP::JdwpTag fieldTag = Dbg::GetFieldBasicTag(fieldId);
    size_t width = Dbg::GetTagWidth(fieldTag);
    uint64_t value = request.ReadValue(width);

    VLOG(jdwp) << "    --> fieldId=" << fieldId << " tag=" << fieldTag << "(" << width << ") value=" << value;
    JdwpError status = Dbg::SetFieldValue(object_id, fieldId, value, width);
    if (status != ERR_NONE) {
      return status;
    }
  }

  return ERR_NONE;
}

static JdwpError OR_MonitorInfo(JdwpState*, Request& request, ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId object_id = request.ReadObjectId();
  return Dbg::GetMonitorInfo(object_id, reply);
}

/*
 * Invoke an instance method.  The invocation must occur in the specified
 * thread, which must have been suspended by an event.
 *
 * The call is synchronous.  All threads in the VM are resumed, unless the
 * SINGLE_THREADED flag is set.
 *
 * If you ask Eclipse to "inspect" an object (or ask JDB to "print" an
 * object), it will try to invoke the object's toString() function.  This
 * feature becomes crucial when examining ArrayLists with Eclipse.
 */
static JdwpError OR_InvokeMethod(JdwpState* state, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId object_id = request.ReadObjectId();
  ObjectId thread_id = request.ReadThreadId();
  RefTypeId class_id = request.ReadRefTypeId();
  MethodId method_id = request.ReadMethodId();

  return FinishInvoke(state, request, pReply, thread_id, object_id, class_id, method_id, false);
}

static JdwpError OR_DisableCollection(JdwpState*, Request& request, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId object_id = request.ReadObjectId();
  return Dbg::DisableCollection(object_id);
}

static JdwpError OR_EnableCollection(JdwpState*, Request& request, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId object_id = request.ReadObjectId();
  return Dbg::EnableCollection(object_id);
}

static JdwpError OR_IsCollected(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId object_id = request.ReadObjectId();
  bool is_collected;
  JdwpError rc = Dbg::IsCollected(object_id, is_collected);
  expandBufAdd1(pReply, is_collected ? 1 : 0);
  return rc;
}

static JdwpError OR_ReferringObjects(JdwpState*, Request& request, ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId object_id = request.ReadObjectId();
  int32_t max_count = request.ReadSigned32("max count");
  if (max_count < 0) {
    return ERR_ILLEGAL_ARGUMENT;
  }

  std::vector<ObjectId> referring_objects;
  JdwpError rc = Dbg::GetReferringObjects(object_id, max_count, referring_objects);
  if (rc != ERR_NONE) {
    return rc;
  }

  return WriteTaggedObjectList(reply, referring_objects);
}

/*
 * Return the string value in a string object.
 */
static JdwpError SR_Value(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId stringObject = request.ReadObjectId();
  std::string str(Dbg::StringToUtf8(stringObject));

  VLOG(jdwp) << StringPrintf("    --> %s", PrintableString(str).c_str());

  expandBufAddUtf8String(pReply, str);

  return ERR_NONE;
}

/*
 * Return a thread's name.
 */
static JdwpError TR_Name(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();

  std::string name;
  JdwpError error = Dbg::GetThreadName(thread_id, name);
  if (error != ERR_NONE) {
    return error;
  }
  VLOG(jdwp) << StringPrintf("  Name of thread %#llx is \"%s\"", thread_id, name.c_str());
  expandBufAddUtf8String(pReply, name);

  return ERR_NONE;
}

/*
 * Suspend the specified thread.
 *
 * It's supposed to remain suspended even if interpreted code wants to
 * resume it; only the JDI is allowed to resume it.
 */
static JdwpError TR_Suspend(JdwpState*, Request& request, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();

  if (thread_id == Dbg::GetThreadSelfId()) {
    LOG(INFO) << "  Warning: ignoring request to suspend self";
    return ERR_THREAD_NOT_SUSPENDED;
  }

  Thread* self = Thread::Current();
  self->TransitionFromRunnableToSuspended(kWaitingForDebuggerSend);
  JdwpError result = Dbg::SuspendThread(thread_id);
  self->TransitionFromSuspendedToRunnable();
  return result;
}

/*
 * Resume the specified thread.
 */
static JdwpError TR_Resume(JdwpState*, Request& request, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();

  if (thread_id == Dbg::GetThreadSelfId()) {
    LOG(INFO) << "  Warning: ignoring request to resume self";
    return ERR_NONE;
  }

  Dbg::ResumeThread(thread_id);
  return ERR_NONE;
}

/*
 * Return status of specified thread.
 */
static JdwpError TR_Status(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();

  JDWP::JdwpThreadStatus threadStatus;
  JDWP::JdwpSuspendStatus suspendStatus;
  JdwpError error = Dbg::GetThreadStatus(thread_id, &threadStatus, &suspendStatus);
  if (error != ERR_NONE) {
    return error;
  }

  VLOG(jdwp) << "    --> " << threadStatus << ", " << suspendStatus;

  expandBufAdd4BE(pReply, threadStatus);
  expandBufAdd4BE(pReply, suspendStatus);

  return ERR_NONE;
}

/*
 * Return the thread group that the specified thread is a member of.
 */
static JdwpError TR_ThreadGroup(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();
  return Dbg::GetThreadGroup(thread_id, pReply);
}

/*
 * Return the current call stack of a suspended thread.
 *
 * If the thread isn't suspended, the error code isn't defined, but should
 * be THREAD_NOT_SUSPENDED.
 */
static JdwpError TR_Frames(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();
  uint32_t start_frame = request.ReadUnsigned32("start frame");
  uint32_t length = request.ReadUnsigned32("length");

  size_t actual_frame_count;
  JdwpError error = Dbg::GetThreadFrameCount(thread_id, actual_frame_count);
  if (error != ERR_NONE) {
    return error;
  }

  if (actual_frame_count <= 0) {
    return ERR_THREAD_NOT_SUSPENDED;  // 0 means no managed frames (which means "in native").
  }

  if (start_frame > actual_frame_count) {
    return ERR_INVALID_INDEX;
  }
  if (length == static_cast<uint32_t>(-1)) {
    length = actual_frame_count - start_frame;
  }
  if (start_frame + length > actual_frame_count) {
    return ERR_INVALID_LENGTH;
  }

  return Dbg::GetThreadFrames(thread_id, start_frame, length, pReply);
}

/*
 * Returns the #of frames on the specified thread, which must be suspended.
 */
static JdwpError TR_FrameCount(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();

  size_t frame_count;
  JdwpError rc = Dbg::GetThreadFrameCount(thread_id, frame_count);
  if (rc != ERR_NONE) {
    return rc;
  }
  expandBufAdd4BE(pReply, static_cast<uint32_t>(frame_count));

  return ERR_NONE;
}

static JdwpError TR_OwnedMonitors(Request& request, ExpandBuf* reply, bool with_stack_depths)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();

  std::vector<ObjectId> monitors;
  std::vector<uint32_t> stack_depths;
  JdwpError rc = Dbg::GetOwnedMonitors(thread_id, monitors, stack_depths);
  if (rc != ERR_NONE) {
    return rc;
  }

  expandBufAdd4BE(reply, monitors.size());
  for (size_t i = 0; i < monitors.size(); ++i) {
    rc = WriteTaggedObject(reply, monitors[i]);
    if (rc != ERR_NONE) {
      return rc;
    }
    if (with_stack_depths) {
      expandBufAdd4BE(reply, stack_depths[i]);
    }
  }
  return ERR_NONE;
}

static JdwpError TR_OwnedMonitors(JdwpState*, Request& request, ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return TR_OwnedMonitors(request, reply, false);
}

static JdwpError TR_OwnedMonitorsStackDepthInfo(JdwpState*, Request& request, ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return TR_OwnedMonitors(request, reply, true);
}

static JdwpError TR_CurrentContendedMonitor(JdwpState*, Request& request, ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();

  ObjectId contended_monitor;
  JdwpError rc = Dbg::GetContendedMonitor(thread_id, contended_monitor);
  if (rc != ERR_NONE) {
    return rc;
  }
  return WriteTaggedObject(reply, contended_monitor);
}

static JdwpError TR_Interrupt(JdwpState*, Request& request, ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();
  return Dbg::Interrupt(thread_id);
}

/*
 * Return the debug suspend count for the specified thread.
 *
 * (The thread *might* still be running -- it might not have examined
 * its suspend count recently.)
 */
static JdwpError TR_DebugSuspendCount(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();
  return Dbg::GetThreadDebugSuspendCount(thread_id, pReply);
}

/*
 * Return the name of a thread group.
 *
 * The Eclipse debugger recognizes "main" and "system" as special.
 */
static JdwpError TGR_Name(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_group_id = request.ReadThreadGroupId();

  expandBufAddUtf8String(pReply, Dbg::GetThreadGroupName(thread_group_id));

  return ERR_NONE;
}

/*
 * Returns the thread group -- if any -- that contains the specified
 * thread group.
 */
static JdwpError TGR_Parent(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_group_id = request.ReadThreadGroupId();

  ObjectId parentGroup = Dbg::GetThreadGroupParent(thread_group_id);
  expandBufAddObjectId(pReply, parentGroup);

  return ERR_NONE;
}

/*
 * Return the active threads and thread groups that are part of the
 * specified thread group.
 */
static JdwpError TGR_Children(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_group_id = request.ReadThreadGroupId();

  std::vector<ObjectId> thread_ids;
  Dbg::GetThreads(thread_group_id, thread_ids);
  expandBufAdd4BE(pReply, thread_ids.size());
  for (uint32_t i = 0; i < thread_ids.size(); ++i) {
    expandBufAddObjectId(pReply, thread_ids[i]);
  }

  std::vector<ObjectId> child_thread_groups_ids;
  Dbg::GetChildThreadGroups(thread_group_id, child_thread_groups_ids);
  expandBufAdd4BE(pReply, child_thread_groups_ids.size());
  for (uint32_t i = 0; i < child_thread_groups_ids.size(); ++i) {
    expandBufAddObjectId(pReply, child_thread_groups_ids[i]);
  }

  return ERR_NONE;
}

/*
 * Return the #of components in the array.
 */
static JdwpError AR_Length(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId array_id = request.ReadArrayId();

  int length;
  JdwpError status = Dbg::GetArrayLength(array_id, length);
  if (status != ERR_NONE) {
    return status;
  }
  VLOG(jdwp) << "    --> " << length;

  expandBufAdd4BE(pReply, length);

  return ERR_NONE;
}

/*
 * Return the values from an array.
 */
static JdwpError AR_GetValues(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId array_id = request.ReadArrayId();
  uint32_t offset = request.ReadUnsigned32("offset");
  uint32_t length = request.ReadUnsigned32("length");
  return Dbg::OutputArray(array_id, offset, length, pReply);
}

/*
 * Set values in an array.
 */
static JdwpError AR_SetValues(JdwpState*, Request& request, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId array_id = request.ReadArrayId();
  uint32_t offset = request.ReadUnsigned32("offset");
  uint32_t count = request.ReadUnsigned32("count");
  return Dbg::SetArrayElements(array_id, offset, count, request);
}

static JdwpError CLR_VisibleClasses(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  request.ReadObjectId();  // classLoaderObject
  // TODO: we should only return classes which have the given class loader as a defining or
  // initiating loader. The former would be easy; the latter is hard, because we don't have
  // any such notion.
  return VM_AllClassesImpl(pReply, false, false);
}

/*
 * Set an event trigger.
 *
 * Reply with a requestID.
 */
static JdwpError ER_Set(JdwpState* state, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  JdwpEventKind event_kind = request.ReadEnum1<JdwpEventKind>("event kind");
  JdwpSuspendPolicy suspend_policy = request.ReadEnum1<JdwpSuspendPolicy>("suspend policy");
  int32_t modifier_count = request.ReadSigned32("modifier count");

  CHECK_LT(modifier_count, 256);    /* reasonableness check */

  JdwpEvent* pEvent = EventAlloc(modifier_count);
  pEvent->eventKind = event_kind;
  pEvent->suspend_policy = suspend_policy;
  pEvent->modCount = modifier_count;

  /*
   * Read modifiers.  Ordering may be significant (see explanation of Count
   * mods in JDWP doc).
   */
  for (int32_t i = 0; i < modifier_count; ++i) {
    JdwpEventMod& mod = pEvent->mods[i];
    mod.modKind = request.ReadModKind();
    switch (mod.modKind) {
    case MK_COUNT:
      {
        // Report once, when "--count" reaches 0.
        uint32_t count = request.ReadUnsigned32("count");
        if (count == 0) {
          return ERR_INVALID_COUNT;
        }
        mod.count.count = count;
      }
      break;
    case MK_CONDITIONAL:
      {
        // Conditional on expression.
        uint32_t exprId = request.ReadUnsigned32("expr id");
        mod.conditional.exprId = exprId;
      }
      break;
    case MK_THREAD_ONLY:
      {
        // Only report events in specified thread.
        ObjectId thread_id = request.ReadThreadId();
        mod.threadOnly.threadId = thread_id;
      }
      break;
    case MK_CLASS_ONLY:
      {
        // For ClassPrepare, MethodEntry.
        RefTypeId class_id = request.ReadRefTypeId();
        mod.classOnly.refTypeId = class_id;
      }
      break;
    case MK_CLASS_MATCH:
      {
        // Restrict events to matching classes.
        // pattern is "java.foo.*", we want "java/foo/*".
        std::string pattern(request.ReadUtf8String());
        std::replace(pattern.begin(), pattern.end(), '.', '/');
        mod.classMatch.classPattern = strdup(pattern.c_str());
      }
      break;
    case MK_CLASS_EXCLUDE:
      {
        // Restrict events to non-matching classes.
        // pattern is "java.foo.*", we want "java/foo/*".
        std::string pattern(request.ReadUtf8String());
        std::replace(pattern.begin(), pattern.end(), '.', '/');
        mod.classExclude.classPattern = strdup(pattern.c_str());
      }
      break;
    case MK_LOCATION_ONLY:
      {
        // Restrict certain events based on location.
        JdwpLocation location = request.ReadLocation();
        mod.locationOnly.loc = location;
      }
      break;
    case MK_EXCEPTION_ONLY:
      {
        // Modifies EK_EXCEPTION events,
        mod.exceptionOnly.refTypeId = request.ReadRefTypeId();  // null => all exceptions.
        mod.exceptionOnly.caught = request.ReadEnum1<uint8_t>("caught");
        mod.exceptionOnly.uncaught = request.ReadEnum1<uint8_t>("uncaught");
      }
      break;
    case MK_FIELD_ONLY:
      {
        // For field access/modification events.
        RefTypeId declaring = request.ReadRefTypeId();
        FieldId fieldId = request.ReadFieldId();
        mod.fieldOnly.refTypeId = declaring;
        mod.fieldOnly.fieldId = fieldId;
      }
      break;
    case MK_STEP:
      {
        // For use with EK_SINGLE_STEP.
        ObjectId thread_id = request.ReadThreadId();
        uint32_t size = request.ReadUnsigned32("step size");
        uint32_t depth = request.ReadUnsigned32("step depth");
        VLOG(jdwp) << StringPrintf("    Step: thread=%#llx", thread_id)
                     << " size=" << JdwpStepSize(size) << " depth=" << JdwpStepDepth(depth);

        mod.step.threadId = thread_id;
        mod.step.size = size;
        mod.step.depth = depth;
      }
      break;
    case MK_INSTANCE_ONLY:
      {
        // Report events related to a specific object.
        ObjectId instance = request.ReadObjectId();
        mod.instanceOnly.objectId = instance;
      }
      break;
    default:
      LOG(WARNING) << "GLITCH: unsupported modKind=" << mod.modKind;
      break;
    }
  }

  /*
   * We reply with an integer "requestID".
   */
  uint32_t requestId = state->NextEventSerial();
  expandBufAdd4BE(pReply, requestId);

  pEvent->requestId = requestId;

  VLOG(jdwp) << StringPrintf("    --> event requestId=%#x", requestId);

  /* add it to the list */
  JdwpError err = state->RegisterEvent(pEvent);
  if (err != ERR_NONE) {
    /* registration failed, probably because event is bogus */
    EventFree(pEvent);
    LOG(WARNING) << "WARNING: event request rejected";
  }
  return err;
}

static JdwpError ER_Clear(JdwpState* state, Request& request, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  request.ReadEnum1<JdwpEventKind>("event kind");
  uint32_t requestId = request.ReadUnsigned32("request id");

  // Failure to find an event with a matching ID is a no-op
  // and does not return an error.
  state->UnregisterEventById(requestId);
  return ERR_NONE;
}

/*
 * Return the values of arguments and local variables.
 */
static JdwpError SF_GetValues(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();
  FrameId frame_id = request.ReadFrameId();
  int32_t slot_count = request.ReadSigned32("slot count");

  expandBufAdd4BE(pReply, slot_count);     /* "int values" */
  for (int32_t i = 0; i < slot_count; ++i) {
    uint32_t slot = request.ReadUnsigned32("slot");
    JDWP::JdwpTag reqSigByte = request.ReadTag();

    VLOG(jdwp) << "    --> slot " << slot << " " << reqSigByte;

    size_t width = Dbg::GetTagWidth(reqSigByte);
    uint8_t* ptr = expandBufAddSpace(pReply, width+1);
    Dbg::GetLocalValue(thread_id, frame_id, slot, reqSigByte, ptr, width);
  }

  return ERR_NONE;
}

/*
 * Set the values of arguments and local variables.
 */
static JdwpError SF_SetValues(JdwpState*, Request& request, ExpandBuf*)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();
  FrameId frame_id = request.ReadFrameId();
  int32_t slot_count = request.ReadSigned32("slot count");

  for (int32_t i = 0; i < slot_count; ++i) {
    uint32_t slot = request.ReadUnsigned32("slot");
    JDWP::JdwpTag sigByte = request.ReadTag();
    size_t width = Dbg::GetTagWidth(sigByte);
    uint64_t value = request.ReadValue(width);

    VLOG(jdwp) << "    --> slot " << slot << " " << sigByte << " " << value;
    Dbg::SetLocalValue(thread_id, frame_id, slot, sigByte, value, width);
  }

  return ERR_NONE;
}

static JdwpError SF_ThisObject(JdwpState*, Request& request, ExpandBuf* reply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectId thread_id = request.ReadThreadId();
  FrameId frame_id = request.ReadFrameId();

  ObjectId object_id;
  JdwpError rc = Dbg::GetThisObject(thread_id, frame_id, &object_id);
  if (rc != ERR_NONE) {
    return rc;
  }

  return WriteTaggedObject(reply, object_id);
}

/*
 * Return the reference type reflected by this class object.
 *
 * This appears to be required because ReferenceTypeId values are NEVER
 * reused, whereas ClassIds can be recycled like any other object.  (Either
 * that, or I have no idea what this is for.)
 */
static JdwpError COR_ReflectedType(JdwpState*, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  RefTypeId class_object_id = request.ReadRefTypeId();
  return Dbg::GetReflectedType(class_object_id, pReply);
}

/*
 * Handle a DDM packet with a single chunk in it.
 */
static JdwpError DDM_Chunk(JdwpState* state, Request& request, ExpandBuf* pReply)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  state->NotifyDdmsActive();
  uint8_t* replyBuf = NULL;
  int replyLen = -1;
  if (Dbg::DdmHandlePacket(request, &replyBuf, &replyLen)) {
    // If they want to send something back, we copy it into the buffer.
    // TODO: consider altering the JDWP stuff to hold the packet header
    // in a separate buffer.  That would allow us to writev() DDM traffic
    // instead of copying it into the expanding buffer.  The reduction in
    // heap requirements is probably more valuable than the efficiency.
    CHECK_GT(replyLen, 0);
    memcpy(expandBufAddSpace(pReply, replyLen), replyBuf, replyLen);
    free(replyBuf);
  }
  return ERR_NONE;
}

/*
 * Handler map decl.
 */
typedef JdwpError (*JdwpRequestHandler)(JdwpState* state, Request& request, ExpandBuf* reply);

struct JdwpHandlerMap {
  uint8_t cmdSet;
  uint8_t cmd;
  JdwpRequestHandler func;
  const char* name;
};

/*
 * Map commands to functions.
 *
 * Command sets 0-63 are incoming requests, 64-127 are outbound requests,
 * and 128-256 are vendor-defined.
 */
static const JdwpHandlerMap gHandlers[] = {
  /* VirtualMachine command set (1) */
  { 1,    1,  VM_Version,               "VirtualMachine.Version" },
  { 1,    2,  VM_ClassesBySignature,    "VirtualMachine.ClassesBySignature" },
  { 1,    3,  VM_AllClasses,            "VirtualMachine.AllClasses" },
  { 1,    4,  VM_AllThreads,            "VirtualMachine.AllThreads" },
  { 1,    5,  VM_TopLevelThreadGroups,  "VirtualMachine.TopLevelThreadGroups" },
  { 1,    6,  VM_Dispose,               "VirtualMachine.Dispose" },
  { 1,    7,  VM_IDSizes,               "VirtualMachine.IDSizes" },
  { 1,    8,  VM_Suspend,               "VirtualMachine.Suspend" },
  { 1,    9,  VM_Resume,                "VirtualMachine.Resume" },
  { 1,    10, VM_Exit,                  "VirtualMachine.Exit" },
  { 1,    11, VM_CreateString,          "VirtualMachine.CreateString" },
  { 1,    12, VM_Capabilities,          "VirtualMachine.Capabilities" },
  { 1,    13, VM_ClassPaths,            "VirtualMachine.ClassPaths" },
  { 1,    14, VM_DisposeObjects,        "VirtualMachine.DisposeObjects" },
  { 1,    15, NULL,                     "VirtualMachine.HoldEvents" },
  { 1,    16, NULL,                     "VirtualMachine.ReleaseEvents" },
  { 1,    17, VM_CapabilitiesNew,       "VirtualMachine.CapabilitiesNew" },
  { 1,    18, NULL,                     "VirtualMachine.RedefineClasses" },
  { 1,    19, NULL,                     "VirtualMachine.SetDefaultStratum" },
  { 1,    20, VM_AllClassesWithGeneric, "VirtualMachine.AllClassesWithGeneric" },
  { 1,    21, VM_InstanceCounts,        "VirtualMachine.InstanceCounts" },

  /* ReferenceType command set (2) */
  { 2,    1,  RT_Signature,            "ReferenceType.Signature" },
  { 2,    2,  RT_ClassLoader,          "ReferenceType.ClassLoader" },
  { 2,    3,  RT_Modifiers,            "ReferenceType.Modifiers" },
  { 2,    4,  RT_Fields,               "ReferenceType.Fields" },
  { 2,    5,  RT_Methods,              "ReferenceType.Methods" },
  { 2,    6,  RT_GetValues,            "ReferenceType.GetValues" },
  { 2,    7,  RT_SourceFile,           "ReferenceType.SourceFile" },
  { 2,    8,  NULL,                    "ReferenceType.NestedTypes" },
  { 2,    9,  RT_Status,               "ReferenceType.Status" },
  { 2,    10, RT_Interfaces,           "ReferenceType.Interfaces" },
  { 2,    11, RT_ClassObject,          "ReferenceType.ClassObject" },
  { 2,    12, RT_SourceDebugExtension, "ReferenceType.SourceDebugExtension" },
  { 2,    13, RT_SignatureWithGeneric, "ReferenceType.SignatureWithGeneric" },
  { 2,    14, RT_FieldsWithGeneric,    "ReferenceType.FieldsWithGeneric" },
  { 2,    15, RT_MethodsWithGeneric,   "ReferenceType.MethodsWithGeneric" },
  { 2,    16, RT_Instances,            "ReferenceType.Instances" },
  { 2,    17, NULL,                    "ReferenceType.ClassFileVersion" },
  { 2,    18, NULL,                    "ReferenceType.ConstantPool" },

  /* ClassType command set (3) */
  { 3,    1,  CT_Superclass,    "ClassType.Superclass" },
  { 3,    2,  CT_SetValues,     "ClassType.SetValues" },
  { 3,    3,  CT_InvokeMethod,  "ClassType.InvokeMethod" },
  { 3,    4,  CT_NewInstance,   "ClassType.NewInstance" },

  /* ArrayType command set (4) */
  { 4,    1,  AT_newInstance,   "ArrayType.NewInstance" },

  /* InterfaceType command set (5) */

  /* Method command set (6) */
  { 6,    1,  M_LineTable,                "Method.LineTable" },
  { 6,    2,  M_VariableTable,            "Method.VariableTable" },
  { 6,    3,  M_Bytecodes,                "Method.Bytecodes" },
  { 6,    4,  NULL,                       "Method.IsObsolete" },
  { 6,    5,  M_VariableTableWithGeneric, "Method.VariableTableWithGeneric" },

  /* Field command set (8) */

  /* ObjectReference command set (9) */
  { 9,    1,  OR_ReferenceType,     "ObjectReference.ReferenceType" },
  { 9,    2,  OR_GetValues,         "ObjectReference.GetValues" },
  { 9,    3,  OR_SetValues,         "ObjectReference.SetValues" },
  { 9,    4,  NULL,                 "ObjectReference.UNUSED" },
  { 9,    5,  OR_MonitorInfo,       "ObjectReference.MonitorInfo" },
  { 9,    6,  OR_InvokeMethod,      "ObjectReference.InvokeMethod" },
  { 9,    7,  OR_DisableCollection, "ObjectReference.DisableCollection" },
  { 9,    8,  OR_EnableCollection,  "ObjectReference.EnableCollection" },
  { 9,    9,  OR_IsCollected,       "ObjectReference.IsCollected" },
  { 9,    10, OR_ReferringObjects,  "ObjectReference.ReferringObjects" },

  /* StringReference command set (10) */
  { 10,   1,  SR_Value,         "StringReference.Value" },

  /* ThreadReference command set (11) */
  { 11,   1,  TR_Name,                        "ThreadReference.Name" },
  { 11,   2,  TR_Suspend,                     "ThreadReference.Suspend" },
  { 11,   3,  TR_Resume,                      "ThreadReference.Resume" },
  { 11,   4,  TR_Status,                      "ThreadReference.Status" },
  { 11,   5,  TR_ThreadGroup,                 "ThreadReference.ThreadGroup" },
  { 11,   6,  TR_Frames,                      "ThreadReference.Frames" },
  { 11,   7,  TR_FrameCount,                  "ThreadReference.FrameCount" },
  { 11,   8,  TR_OwnedMonitors,               "ThreadReference.OwnedMonitors" },
  { 11,   9,  TR_CurrentContendedMonitor,     "ThreadReference.CurrentContendedMonitor" },
  { 11,   10, NULL,                           "ThreadReference.Stop" },
  { 11,   11, TR_Interrupt,                   "ThreadReference.Interrupt" },
  { 11,   12, TR_DebugSuspendCount,           "ThreadReference.SuspendCount" },
  { 11,   13, TR_OwnedMonitorsStackDepthInfo, "ThreadReference.OwnedMonitorsStackDepthInfo" },
  { 11,   14, NULL,                           "ThreadReference.ForceEarlyReturn" },

  /* ThreadGroupReference command set (12) */
  { 12,   1,  TGR_Name,         "ThreadGroupReference.Name" },
  { 12,   2,  TGR_Parent,       "ThreadGroupReference.Parent" },
  { 12,   3,  TGR_Children,     "ThreadGroupReference.Children" },

  /* ArrayReference command set (13) */
  { 13,   1,  AR_Length,        "ArrayReference.Length" },
  { 13,   2,  AR_GetValues,     "ArrayReference.GetValues" },
  { 13,   3,  AR_SetValues,     "ArrayReference.SetValues" },

  /* ClassLoaderReference command set (14) */
  { 14,   1,  CLR_VisibleClasses, "ClassLoaderReference.VisibleClasses" },

  /* EventRequest command set (15) */
  { 15,   1,  ER_Set,           "EventRequest.Set" },
  { 15,   2,  ER_Clear,         "EventRequest.Clear" },
  { 15,   3,  NULL,             "EventRequest.ClearAllBreakpoints" },

  /* StackFrame command set (16) */
  { 16,   1,  SF_GetValues,     "StackFrame.GetValues" },
  { 16,   2,  SF_SetValues,     "StackFrame.SetValues" },
  { 16,   3,  SF_ThisObject,    "StackFrame.ThisObject" },
  { 16,   4,  NULL,             "StackFrame.PopFrames" },

  /* ClassObjectReference command set (17) */
  { 17,   1,  COR_ReflectedType, "ClassObjectReference.ReflectedType" },

  /* Event command set (64) */
  { 64, 100,  NULL, "Event.Composite" },  // sent from VM to debugger, never received by VM

  { 199,  1,  DDM_Chunk,        "DDM.Chunk" },
};

static const char* GetCommandName(Request& request) {
  for (size_t i = 0; i < arraysize(gHandlers); ++i) {
    if (gHandlers[i].cmdSet == request.GetCommandSet() && gHandlers[i].cmd == request.GetCommand()) {
      return gHandlers[i].name;
    }
  }
  return "?UNKNOWN?";
}

static std::string DescribeCommand(Request& request) {
  std::string result;
  result += "REQUEST: ";
  result += GetCommandName(request);
  result += StringPrintf(" (length=%d id=0x%06x)", request.GetLength(), request.GetId());
  return result;
}

/*
 * Process a request from the debugger.
 *
 * On entry, the JDWP thread is in VMWAIT.
 */
void JdwpState::ProcessRequest(Request& request, ExpandBuf* pReply) {
  JdwpError result = ERR_NONE;

  if (request.GetCommandSet() != kJDWPDdmCmdSet) {
    /*
     * Activity from a debugger, not merely ddms.  Mark us as having an
     * active debugger session, and zero out the last-activity timestamp
     * so waitForDebugger() doesn't return if we stall for a bit here.
     */
    Dbg::GoActive();
    QuasiAtomic::Write64(&last_activity_time_ms_, 0);
  }

  /*
   * If a debugger event has fired in another thread, wait until the
   * initiating thread has suspended itself before processing messages
   * from the debugger.  Otherwise we (the JDWP thread) could be told to
   * resume the thread before it has suspended.
   *
   * We call with an argument of zero to wait for the current event
   * thread to finish, and then clear the block.  Depending on the thread
   * suspend policy, this may allow events in other threads to fire,
   * but those events have no bearing on what the debugger has sent us
   * in the current request.
   *
   * Note that we MUST clear the event token before waking the event
   * thread up, or risk waiting for the thread to suspend after we've
   * told it to resume.
   */
  SetWaitForEventThread(0);

  /*
   * Tell the VM that we're running and shouldn't be interrupted by GC.
   * Do this after anything that can stall indefinitely.
   */
  Thread* self = Thread::Current();
  ThreadState old_state = self->TransitionFromSuspendedToRunnable();

  expandBufAddSpace(pReply, kJDWPHeaderLen);

  size_t i;
  for (i = 0; i < arraysize(gHandlers); ++i) {
    if (gHandlers[i].cmdSet == request.GetCommandSet() && gHandlers[i].cmd == request.GetCommand() && gHandlers[i].func != NULL) {
      VLOG(jdwp) << DescribeCommand(request);
      result = (*gHandlers[i].func)(this, request, pReply);
      if (result == ERR_NONE) {
        request.CheckConsumed();
      }
      break;
    }
  }
  if (i == arraysize(gHandlers)) {
    LOG(ERROR) << "Command not implemented: " << DescribeCommand(request);
    LOG(ERROR) << HexDump(request.data(), request.size());
    result = ERR_NOT_IMPLEMENTED;
  }

  /*
   * Set up the reply header.
   *
   * If we encountered an error, only send the header back.
   */
  uint8_t* replyBuf = expandBufGetBuffer(pReply);
  Set4BE(replyBuf + 4, request.GetId());
  Set1(replyBuf + 8, kJDWPFlagReply);
  Set2BE(replyBuf + 9, result);
  if (result == ERR_NONE) {
    Set4BE(replyBuf + 0, expandBufGetLength(pReply));
  } else {
    Set4BE(replyBuf + 0, kJDWPHeaderLen);
  }

  CHECK_GT(expandBufGetLength(pReply), 0U) << GetCommandName(request) << " " << request.GetId();

  size_t respLen = expandBufGetLength(pReply) - kJDWPHeaderLen;
  VLOG(jdwp) << "REPLY: " << GetCommandName(request) << " " << result << " (length=" << respLen << ")";
  if (false) {
    VLOG(jdwp) << HexDump(expandBufGetBuffer(pReply) + kJDWPHeaderLen, respLen);
  }

  VLOG(jdwp) << "----------";

  /*
   * Update last-activity timestamp.  We really only need this during
   * the initial setup.  Only update if this is a non-DDMS packet.
   */
  if (request.GetCommandSet() != kJDWPDdmCmdSet) {
    QuasiAtomic::Write64(&last_activity_time_ms_, MilliTime());
  }

  /* tell the VM that GC is okay again */
  self->TransitionFromRunnableToSuspended(old_state);
}

}  // namespace JDWP

}  // namespace art
