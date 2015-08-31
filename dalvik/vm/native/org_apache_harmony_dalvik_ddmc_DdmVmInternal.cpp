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
 * org.apache.harmony.dalvik.ddmc.DdmVmInternal
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"


/*
 * public static void threadNotify(boolean enable)
 *
 * Enable DDM thread notifications.
 */
static void Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_threadNotify(
    const u4* args, JValue* pResult)
{
    bool enable = (args[0] != 0);

    //ALOGI("ddmThreadNotification: %d", enable);
    dvmDdmSetThreadNotification(enable);
    RETURN_VOID();
}

/*
 * public static byte[] getThreadStats()
 *
 * Get a buffer full of thread info.
 */
static void Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_getThreadStats(
    const u4* args, JValue* pResult)
{
    UNUSED_PARAMETER(args);

    ArrayObject* result = dvmDdmGenerateThreadStats();
    dvmReleaseTrackedAlloc((Object*) result, NULL);
    RETURN_PTR(result);
}

/*
 * public static int heapInfoNotify(int what)
 *
 * Enable DDM heap notifications.
 */
static void Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_heapInfoNotify(
    const u4* args, JValue* pResult)
{
    int when = args[0];
    bool ret;

    ret = dvmDdmHandleHpifChunk(when);
    RETURN_BOOLEAN(ret);
}

/*
 * public static boolean heapSegmentNotify(int when, int what, bool native)
 *
 * Enable DDM heap notifications.
 */
static void
    Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_heapSegmentNotify(
    const u4* args, JValue* pResult)
{
    int  when   = args[0];        // 0=never (off), 1=during GC
    int  what   = args[1];        // 0=merged objects, 1=distinct objects
    bool native = (args[2] != 0); // false=virtual heap, true=native heap
    bool ret;

    ret = dvmDdmHandleHpsgNhsgChunk(when, what, native);
    RETURN_BOOLEAN(ret);
}

/*
 * public static StackTraceElement[] getStackTraceById(int threadId)
 *
 * Get a stack trace as an array of StackTraceElement objects.  Returns
 * NULL on failure, e.g. if the threadId couldn't be found.
 */
static void
    Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_getStackTraceById(
    const u4* args, JValue* pResult)
{
    u4 threadId = args[0];
    ArrayObject* trace;

    trace = dvmDdmGetStackTraceById(threadId);
    RETURN_PTR(trace);
}

/*
 * public static void enableRecentAllocations(boolean enable)
 *
 * Enable or disable recent allocation tracking.
 */
static void
    Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_enableRecentAllocations(
    const u4* args, JValue* pResult)
{
    bool enable = (args[0] != 0);

    if (enable)
        (void) dvmEnableAllocTracker();
    else
        (void) dvmDisableAllocTracker();
    RETURN_VOID();
}

/*
 * public static boolean getRecentAllocationStatus()
 *
 * Returns "true" if allocation tracking is enabled.
 */
static void
    Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_getRecentAllocationStatus(
    const u4* args, JValue* pResult)
{
    UNUSED_PARAMETER(args);
    RETURN_BOOLEAN(gDvm.allocRecords != NULL);
}

/*
 * public static byte[] getRecentAllocations()
 *
 * Fill a buffer with data on recent heap allocations.
 */
static void
    Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_getRecentAllocations(
    const u4* args, JValue* pResult)
{
    ArrayObject* data;

    data = dvmDdmGetRecentAllocations();
    dvmReleaseTrackedAlloc((Object*) data, NULL);
    RETURN_PTR(data);
}

const DalvikNativeMethod dvm_org_apache_harmony_dalvik_ddmc_DdmVmInternal[] = {
    { "threadNotify",       "(Z)V",
      Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_threadNotify },
    { "getThreadStats",     "()[B",
      Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_getThreadStats },
    { "heapInfoNotify",     "(I)Z",
      Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_heapInfoNotify },
    { "heapSegmentNotify",  "(IIZ)Z",
      Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_heapSegmentNotify },
    { "getStackTraceById",  "(I)[Ljava/lang/StackTraceElement;",
      Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_getStackTraceById },
    { "enableRecentAllocations", "(Z)V",
      Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_enableRecentAllocations },
    { "getRecentAllocationStatus", "()Z",
      Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_getRecentAllocationStatus },
    { "getRecentAllocations", "()[B",
      Dalvik_org_apache_harmony_dalvik_ddmc_DdmVmInternal_getRecentAllocations },
    { NULL, NULL, NULL },
};
