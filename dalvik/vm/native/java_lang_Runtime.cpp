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
 * java.lang.Runtime
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"

#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>

/*
 * public void gc()
 *
 * Initiate a gc.
 */
static void Dalvik_java_lang_Runtime_gc(const u4* args, JValue* pResult)
{
    UNUSED_PARAMETER(args);

    dvmCollectGarbage();
    RETURN_VOID();
}

/*
 * private static void nativeExit(int code)
 *
 * Runtime.exit() calls this after doing shutdown processing.  Runtime.halt()
 * uses this as well.
 */
static void Dalvik_java_lang_Runtime_nativeExit(const u4* args,
    JValue* pResult)
{
    int status = args[0];
    if (gDvm.exitHook != NULL) {
        dvmChangeStatus(NULL, THREAD_NATIVE);
        (*gDvm.exitHook)(status);     // not expected to return
        dvmChangeStatus(NULL, THREAD_RUNNING);
        ALOGW("JNI exit hook returned");
    }
#if defined(WITH_JIT) && defined(WITH_JIT_TUNING)
    dvmCompilerDumpStats();
#endif
    ALOGD("Calling exit(%d)", status);
    exit(status);
}

/*
 * static String nativeLoad(String filename, ClassLoader loader, String ldLibraryPath)
 *
 * Load the specified full path as a dynamic library filled with
 * JNI-compatible methods. Returns null on success, or a failure
 * message on failure.
 */
static void Dalvik_java_lang_Runtime_nativeLoad(const u4* args,
    JValue* pResult)
{
    StringObject* fileNameObj = (StringObject*) args[0];
    Object* classLoader = (Object*) args[1];
    StringObject* ldLibraryPathObj = (StringObject*) args[2];

    assert(fileNameObj != NULL);
    char* fileName = dvmCreateCstrFromString(fileNameObj);

    if (ldLibraryPathObj != NULL) {
        char* ldLibraryPath = dvmCreateCstrFromString(ldLibraryPathObj);
        void* sym = dlsym(RTLD_DEFAULT, "android_update_LD_LIBRARY_PATH");
        if (sym != NULL) {
            typedef void (*Fn)(const char*);
            Fn android_update_LD_LIBRARY_PATH = reinterpret_cast<Fn>(sym);
            (*android_update_LD_LIBRARY_PATH)(ldLibraryPath);
        } else {
            ALOGE("android_update_LD_LIBRARY_PATH not found; .so dependencies will not work!");
        }
        free(ldLibraryPath);
    }

    StringObject* result = NULL;
    char* reason = NULL;
    bool success = dvmLoadNativeCode(fileName, classLoader, &reason);
    if (!success) {
        const char* msg = (reason != NULL) ? reason : "unknown failure";
        result = dvmCreateStringFromCstr(msg);
        dvmReleaseTrackedAlloc((Object*) result, NULL);
    }

    free(reason);
    free(fileName);
    RETURN_PTR(result);
}

/*
 * public long maxMemory()
 *
 * Returns GC heap max memory in bytes.
 */
static void Dalvik_java_lang_Runtime_maxMemory(const u4* args, JValue* pResult)
{
    RETURN_LONG(dvmGetHeapDebugInfo(kVirtualHeapMaximumSize));
}

/*
 * public long totalMemory()
 *
 * Returns GC heap total memory in bytes.
 */
static void Dalvik_java_lang_Runtime_totalMemory(const u4* args,
    JValue* pResult)
{
    RETURN_LONG(dvmGetHeapDebugInfo(kVirtualHeapSize));
}

/*
 * public long freeMemory()
 *
 * Returns GC heap free memory in bytes.
 */
static void Dalvik_java_lang_Runtime_freeMemory(const u4* args,
    JValue* pResult)
{
    size_t size = dvmGetHeapDebugInfo(kVirtualHeapSize);
    size_t allocated = dvmGetHeapDebugInfo(kVirtualHeapAllocated);
    long long result = size - allocated;
    if (result < 0) {
        result = 0;
    }
    RETURN_LONG(result);
}

const DalvikNativeMethod dvm_java_lang_Runtime[] = {
    { "freeMemory",          "()J",
        Dalvik_java_lang_Runtime_freeMemory },
    { "gc",                 "()V",
        Dalvik_java_lang_Runtime_gc },
    { "maxMemory",          "()J",
        Dalvik_java_lang_Runtime_maxMemory },
    { "nativeExit",         "(I)V",
        Dalvik_java_lang_Runtime_nativeExit },
    { "nativeLoad",         "(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;)Ljava/lang/String;",
        Dalvik_java_lang_Runtime_nativeLoad },
    { "totalMemory",          "()J",
        Dalvik_java_lang_Runtime_totalMemory },
    { NULL, NULL, NULL },
};
