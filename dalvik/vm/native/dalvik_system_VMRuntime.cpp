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
 * dalvik.system.VMRuntime
 */
#include "Dalvik.h"
#include "ScopedPthreadMutexLock.h"
#include "UniquePtr.h"
#include "alloc/HeapSource.h"
#include "alloc/Visit.h"
#include "libdex/DexClass.h"
#include "native/InternalNativePriv.h"

#include <limits.h>

#include <map>

/*
 * public native float getTargetHeapUtilization()
 *
 * Gets the current ideal heap utilization, represented as a number
 * between zero and one.
 */
static void Dalvik_dalvik_system_VMRuntime_getTargetHeapUtilization(
    const u4* args, JValue* pResult)
{
    UNUSED_PARAMETER(args);

    RETURN_FLOAT(dvmGetTargetHeapUtilization());
}

/*
 * native float nativeSetTargetHeapUtilization()
 *
 * Sets the current ideal heap utilization, represented as a number
 * between zero and one.  Returns the old utilization.
 *
 * Note that this is NOT static.
 */
static void Dalvik_dalvik_system_VMRuntime_nativeSetTargetHeapUtilization(
    const u4* args, JValue* pResult)
{
    dvmSetTargetHeapUtilization(dvmU4ToFloat(args[1]));

    RETURN_VOID();
}

/*
 * public native void startJitCompilation()
 *
 * Callback function from the framework to indicate that an app has gone
 * through the startup phase and it is time to enable the JIT compiler.
 */
static void Dalvik_dalvik_system_VMRuntime_startJitCompilation(const u4* args,
    JValue* pResult)
{
#if defined(WITH_JIT)
    if (gDvm.executionMode == kExecutionModeJit && gDvmJit.disableJit == false) {
        ScopedPthreadMutexLock lock(&gDvmJit.compilerLock);
        gDvmJit.alreadyEnabledViaFramework = true;
        pthread_cond_signal(&gDvmJit.compilerQueueActivity);
    }
#endif
    RETURN_VOID();
}

/*
 * public native void disableJitCompilation()
 *
 * Callback function from the framework to indicate that a VM instance wants to
 * permanently disable the JIT compiler. Currently only the system server uses
 * this interface when it detects system-wide safe mode is enabled.
 */
static void Dalvik_dalvik_system_VMRuntime_disableJitCompilation(const u4* args,
    JValue* pResult)
{
#if defined(WITH_JIT)
    if (gDvm.executionMode == kExecutionModeJit) {
        gDvmJit.disableJit = true;
    }
#endif
    RETURN_VOID();
}

static void Dalvik_dalvik_system_VMRuntime_newNonMovableArray(const u4* args,
    JValue* pResult)
{
    ClassObject* elementClass = (ClassObject*) args[1];
    int length = args[2];

    if (elementClass == NULL) {
        dvmThrowNullPointerException("elementClass == null");
        RETURN_VOID();
    }
    if (length < 0) {
        dvmThrowNegativeArraySizeException(length);
        RETURN_VOID();
    }

    // TODO: right now, we don't have a copying collector, so there's no need
    // to do anything special here, but we ought to pass the non-movability
    // through to the allocator.
    ClassObject* arrayClass = dvmFindArrayClassForElement(elementClass);
    ArrayObject* newArray = dvmAllocArrayByClass(arrayClass,
                                                 length,
                                                 ALLOC_NON_MOVING);
    if (newArray == NULL) {
        assert(dvmCheckException(dvmThreadSelf()));
        RETURN_VOID();
    }
    dvmReleaseTrackedAlloc((Object*) newArray, NULL);

    RETURN_PTR(newArray);
}

static void Dalvik_dalvik_system_VMRuntime_addressOf(const u4* args,
    JValue* pResult)
{
    ArrayObject* array = (ArrayObject*) args[1];
    if (!dvmIsArray(array)) {
        dvmThrowIllegalArgumentException(NULL);
        RETURN_VOID();
    }
    // TODO: we should also check that this is a non-movable array.
    s8 result = (uintptr_t) array->contents;
    RETURN_LONG(result);
}

static void Dalvik_dalvik_system_VMRuntime_clearGrowthLimit(const u4* args,
    JValue* pResult)
{
    dvmClearGrowthLimit();
    RETURN_VOID();
}

static void Dalvik_dalvik_system_VMRuntime_isDebuggerActive(
    const u4* args, JValue* pResult)
{
    RETURN_BOOLEAN(gDvm.debuggerActive || gDvm.nativeDebuggerActive);
}

static void Dalvik_dalvik_system_VMRuntime_properties(const u4* args,
    JValue* pResult)
{
    ArrayObject* result = dvmCreateStringArray(*gDvm.properties);
    dvmReleaseTrackedAlloc((Object*) result, dvmThreadSelf());
    RETURN_PTR(result);
}

static void returnCString(JValue* pResult, const char* s)
{
    Object* result = (Object*) dvmCreateStringFromCstr(s);
    dvmReleaseTrackedAlloc(result, dvmThreadSelf());
    RETURN_PTR(result);
}

static void Dalvik_dalvik_system_VMRuntime_bootClassPath(const u4* args,
    JValue* pResult)
{
    returnCString(pResult, gDvm.bootClassPathStr);
}

static void Dalvik_dalvik_system_VMRuntime_classPath(const u4* args,
    JValue* pResult)
{
    returnCString(pResult, gDvm.classPathStr);
}

static void Dalvik_dalvik_system_VMRuntime_vmVersion(const u4* args,
    JValue* pResult)
{
    char buf[64];
    sprintf(buf, "%d.%d.%d",
            DALVIK_MAJOR_VERSION, DALVIK_MINOR_VERSION, DALVIK_BUG_VERSION);
    returnCString(pResult, buf);
}

static void Dalvik_dalvik_system_VMRuntime_vmLibrary(const u4* args,
    JValue* pResult)
{
    returnCString(pResult, "libdvm.so");
}

static void Dalvik_dalvik_system_VMRuntime_setTargetSdkVersion(const u4* args,
    JValue* pResult)
{
    // This is the target SDK version of the app we're about to run.
    // Note that this value may be CUR_DEVELOPMENT (10000).
    // Note that this value may be 0, meaning "current".
    int targetSdkVersion = args[1];
    if (targetSdkVersion > 0 && targetSdkVersion <= 13 /* honeycomb-mr2 */) {
        if (gDvmJni.useCheckJni) {
            ALOGI("CheckJNI enabled: not enabling JNI app bug workarounds.");
        } else {
            ALOGI("Enabling JNI app bug workarounds for target SDK version %i...",
                  targetSdkVersion);
            gDvmJni.workAroundAppJniBugs = true;
        }
    }
    RETURN_VOID();
}

static void Dalvik_dalvik_system_VMRuntime_registerNativeAllocation(const u4* args,
                                                                    JValue* pResult)
{
  int bytes = args[1];
  if (bytes < 0) {
    dvmThrowRuntimeException("allocation size negative");
  } else {
    dvmHeapSourceRegisterNativeAllocation(bytes);
  }
  RETURN_VOID();
}

static void Dalvik_dalvik_system_VMRuntime_registerNativeFree(const u4* args,
                                                              JValue* pResult)
{
  int bytes = args[1];
  if (bytes < 0) {
    dvmThrowRuntimeException("allocation size negative");
  } else {
    dvmHeapSourceRegisterNativeFree(bytes);
  }
  RETURN_VOID();
}

static DvmDex* getDvmDexFromClassPathEntry(ClassPathEntry* cpe) {
    if (cpe->kind == kCpeDex) {
        return ((RawDexFile*) cpe->ptr)->pDvmDex;
    }
    if (cpe->kind == kCpeJar) {
        return ((JarFile*) cpe->ptr)->pDvmDex;
    }
    LOG_ALWAYS_FATAL("Unknown cpe->kind=%d", cpe->kind);
}

typedef std::map<std::string, StringObject*> StringTable;

static void preloadDexCachesStringsVisitor(void* addr, u4 threadId, RootType type, void* arg) {
    StringTable& table = *(StringTable*) arg;
    StringObject* strObj = *(StringObject**) addr;
    LOG_FATAL_IF(strObj->clazz != gDvm.classJavaLangString, "Unknown class for supposed string");
    char* newStr = dvmCreateCstrFromString(strObj);
    // ALOGI("VMRuntime.preloadDexCaches interned=%s", newStr);
    table[newStr] = strObj;
    free(newStr);
}

// Based on dvmResolveString.
static void preloadDexCachesResolveString(DvmDex* pDvmDex,
                                          uint32_t stringIdx,
                                          StringTable& strings) {
    StringObject* string = dvmDexGetResolvedString(pDvmDex, stringIdx);
    if (string != NULL) {
        return;
    }
    const DexFile* pDexFile = pDvmDex->pDexFile;
    uint32_t utf16Size;
    const char* utf8 = dexStringAndSizeById(pDexFile, stringIdx, &utf16Size);
    string = strings[utf8];
    if (string == NULL) {
        return;
    }
    // ALOGI("VMRuntime.preloadDexCaches found string=%s", utf8);
    dvmDexSetResolvedString(pDvmDex, stringIdx, string);
}

// Based on dvmResolveClass.
static void preloadDexCachesResolveType(DvmDex* pDvmDex, uint32_t typeIdx) {
    ClassObject* clazz = dvmDexGetResolvedClass(pDvmDex, typeIdx);
    if (clazz != NULL) {
        return;
    }
    const DexFile* pDexFile = pDvmDex->pDexFile;
    const char* className = dexStringByTypeIdx(pDexFile, typeIdx);
    if (className[0] != '\0' && className[1] == '\0') {
        /* primitive type */
        clazz = dvmFindPrimitiveClass(className[0]);
    } else {
        clazz = dvmLookupClass(className, NULL, true);
    }
    if (clazz == NULL) {
        return;
    }
    // Skip uninitialized classes because filled cache entry implies it is initialized.
    if (!dvmIsClassInitialized(clazz)) {
        // ALOGI("VMRuntime.preloadDexCaches uninitialized clazz=%s", className);
        return;
    }
    // ALOGI("VMRuntime.preloadDexCaches found clazz=%s", className);
    dvmDexSetResolvedClass(pDvmDex, typeIdx, clazz);
}

// Based on dvmResolveInstField/dvmResolveStaticField.
static void preloadDexCachesResolveField(DvmDex* pDvmDex, uint32_t fieldIdx, bool instance) {
    Field* field = dvmDexGetResolvedField(pDvmDex, fieldIdx);
    if (field != NULL) {
        return;
    }
    const DexFile* pDexFile = pDvmDex->pDexFile;
    const DexFieldId* pFieldId = dexGetFieldId(pDexFile, fieldIdx);
    ClassObject* clazz = dvmDexGetResolvedClass(pDvmDex, pFieldId->classIdx);
    if (clazz == NULL) {
        return;
    }
    // Skip static fields for uninitialized classes because a filled
    // cache entry implies the class is initialized.
    if (!instance && !dvmIsClassInitialized(clazz)) {
        return;
    }
    const char* fieldName = dexStringById(pDexFile, pFieldId->nameIdx);
    const char* signature = dexStringByTypeIdx(pDexFile, pFieldId->typeIdx);
    if (instance) {
        field = dvmFindInstanceFieldHier(clazz, fieldName, signature);
    } else {
        field = dvmFindStaticFieldHier(clazz, fieldName, signature);
    }
    if (field == NULL) {
        return;
    }
    // ALOGI("VMRuntime.preloadDexCaches found field %s %s.%s",
    //       signature, clazz->descriptor, fieldName);
    dvmDexSetResolvedField(pDvmDex, fieldIdx, field);
}

// Based on dvmResolveMethod.
static void preloadDexCachesResolveMethod(DvmDex* pDvmDex,
                                          uint32_t methodIdx,
                                          MethodType methodType) {
    Method* method = dvmDexGetResolvedMethod(pDvmDex, methodIdx);
    if (method != NULL) {
        return;
    }
    const DexFile* pDexFile = pDvmDex->pDexFile;
    const DexMethodId* pMethodId = dexGetMethodId(pDexFile, methodIdx);
    ClassObject* clazz = dvmDexGetResolvedClass(pDvmDex, pMethodId->classIdx);
    if (clazz == NULL) {
        return;
    }
    // Skip static methods for uninitialized classes because a filled
    // cache entry implies the class is initialized.
    if ((methodType == METHOD_STATIC) && !dvmIsClassInitialized(clazz)) {
        return;
    }
    const char* methodName = dexStringById(pDexFile, pMethodId->nameIdx);
    DexProto proto;
    dexProtoSetFromMethodId(&proto, pDexFile, pMethodId);

    if (methodType == METHOD_DIRECT) {
        method = dvmFindDirectMethod(clazz, methodName, &proto);
    } else if (methodType == METHOD_STATIC) {
        method = dvmFindDirectMethodHier(clazz, methodName, &proto);
    } else {
        method = dvmFindVirtualMethodHier(clazz, methodName, &proto);
    }
    if (method == NULL) {
        return;
    }
    // ALOGI("VMRuntime.preloadDexCaches found method %s.%s",
    //        clazz->descriptor, methodName);
    dvmDexSetResolvedMethod(pDvmDex, methodIdx, method);
}

struct DexCacheStats {
    uint32_t numStrings;
    uint32_t numTypes;
    uint32_t numFields;
    uint32_t numMethods;
    DexCacheStats() : numStrings(0), numTypes(0), numFields(0), numMethods(0) {};
};

static const bool kPreloadDexCachesEnabled = true;

// Disabled because it takes a long time (extra half second) but
// gives almost no benefit in terms of saving private dirty pages.
static const bool kPreloadDexCachesStrings = false;

static const bool kPreloadDexCachesTypes = true;
static const bool kPreloadDexCachesFieldsAndMethods = true;

static const bool kPreloadDexCachesCollectStats = false;

static void preloadDexCachesStatsTotal(DexCacheStats* total) {
    if (!kPreloadDexCachesCollectStats) {
        return;
    }

    for (ClassPathEntry* cpe = gDvm.bootClassPath; cpe->kind != kCpeLastEntry; cpe++) {
        DvmDex* pDvmDex = getDvmDexFromClassPathEntry(cpe);
        const DexHeader* pHeader = pDvmDex->pHeader;
        total->numStrings += pHeader->stringIdsSize;
        total->numFields += pHeader->fieldIdsSize;
        total->numMethods += pHeader->methodIdsSize;
        total->numTypes += pHeader->typeIdsSize;
    }
}

static void preloadDexCachesStatsFilled(DexCacheStats* filled) {
    if (!kPreloadDexCachesCollectStats) {
        return;
    }
    for (ClassPathEntry* cpe = gDvm.bootClassPath; cpe->kind != kCpeLastEntry; cpe++) {
        DvmDex* pDvmDex = getDvmDexFromClassPathEntry(cpe);
        const DexHeader* pHeader = pDvmDex->pHeader;
        for (size_t i = 0; i < pHeader->stringIdsSize; i++) {
            StringObject* string = dvmDexGetResolvedString(pDvmDex, i);
            if (string != NULL) {
                filled->numStrings++;
            }
        }
        for (size_t i = 0; i < pHeader->typeIdsSize; i++) {
            ClassObject* clazz = dvmDexGetResolvedClass(pDvmDex, i);
            if (clazz != NULL) {
                filled->numTypes++;
            }
        }
        for (size_t i = 0; i < pHeader->fieldIdsSize; i++) {
            Field* field = dvmDexGetResolvedField(pDvmDex, i);
            if (field != NULL) {
                filled->numFields++;
            }
        }
        for (size_t i = 0; i < pHeader->methodIdsSize; i++) {
            Method* method = dvmDexGetResolvedMethod(pDvmDex, i);
            if (method != NULL) {
                filled->numMethods++;
            }
        }
    }
}

static void Dalvik_dalvik_system_VMRuntime_preloadDexCaches(const u4* args, JValue* pResult)
{
    if (!kPreloadDexCachesEnabled) {
        return;
    }

    DexCacheStats total;
    DexCacheStats before;
    if (kPreloadDexCachesCollectStats) {
        ALOGI("VMRuntime.preloadDexCaches starting");
        preloadDexCachesStatsTotal(&total);
        preloadDexCachesStatsFilled(&before);
    }

    // We use a std::map to avoid heap allocating StringObjects to lookup in gDvm.literalStrings
    StringTable strings;
    if (kPreloadDexCachesStrings) {
        dvmLockMutex(&gDvm.internLock);
        dvmHashTableLock(gDvm.literalStrings);
        for (int i = 0; i < gDvm.literalStrings->tableSize; ++i) {
            HashEntry *entry = &gDvm.literalStrings->pEntries[i];
            if (entry->data != NULL && entry->data != HASH_TOMBSTONE) {
                preloadDexCachesStringsVisitor(&entry->data, 0, ROOT_INTERNED_STRING, &strings);
            }
        }
        dvmHashTableUnlock(gDvm.literalStrings);
        dvmUnlockMutex(&gDvm.internLock);
    }

    for (ClassPathEntry* cpe = gDvm.bootClassPath; cpe->kind != kCpeLastEntry; cpe++) {
        DvmDex* pDvmDex = getDvmDexFromClassPathEntry(cpe);
        const DexHeader* pHeader = pDvmDex->pHeader;
        const DexFile* pDexFile = pDvmDex->pDexFile;

        if (kPreloadDexCachesStrings) {
            for (size_t i = 0; i < pHeader->stringIdsSize; i++) {
                preloadDexCachesResolveString(pDvmDex, i, strings);
            }
        }

        if (kPreloadDexCachesTypes) {
            for (size_t i = 0; i < pHeader->typeIdsSize; i++) {
                preloadDexCachesResolveType(pDvmDex, i);
            }
        }

        if (kPreloadDexCachesFieldsAndMethods) {
            for (size_t classDefIndex = 0;
                 classDefIndex < pHeader->classDefsSize;
                 classDefIndex++) {
                const DexClassDef* pClassDef = dexGetClassDef(pDexFile, classDefIndex);
                const u1* pEncodedData = dexGetClassData(pDexFile, pClassDef);
                UniquePtr<DexClassData> pClassData(dexReadAndVerifyClassData(&pEncodedData, NULL));
                if (pClassData.get() == NULL) {
                    continue;
                }
                for (uint32_t fieldIndex = 0;
                     fieldIndex < pClassData->header.staticFieldsSize;
                     fieldIndex++) {
                    const DexField* pField = &pClassData->staticFields[fieldIndex];
                    preloadDexCachesResolveField(pDvmDex, pField->fieldIdx, false);
                }
                for (uint32_t fieldIndex = 0;
                     fieldIndex < pClassData->header.instanceFieldsSize;
                     fieldIndex++) {
                    const DexField* pField = &pClassData->instanceFields[fieldIndex];
                    preloadDexCachesResolveField(pDvmDex, pField->fieldIdx, true);
                }
                for (uint32_t methodIndex = 0;
                     methodIndex < pClassData->header.directMethodsSize;
                     methodIndex++) {
                    const DexMethod* pDexMethod = &pClassData->directMethods[methodIndex];
                    MethodType methodType = (((pDexMethod->accessFlags & ACC_STATIC) != 0) ?
                                             METHOD_STATIC :
                                             METHOD_DIRECT);
                    preloadDexCachesResolveMethod(pDvmDex, pDexMethod->methodIdx, methodType);
                }
                for (uint32_t methodIndex = 0;
                     methodIndex < pClassData->header.virtualMethodsSize;
                     methodIndex++) {
                    const DexMethod* pDexMethod = &pClassData->virtualMethods[methodIndex];
                    preloadDexCachesResolveMethod(pDvmDex, pDexMethod->methodIdx, METHOD_VIRTUAL);
                }
            }
        }
    }

    if (kPreloadDexCachesCollectStats) {
        DexCacheStats after;
        preloadDexCachesStatsFilled(&after);
        ALOGI("VMRuntime.preloadDexCaches strings total=%d before=%d after=%d",
              total.numStrings, before.numStrings, after.numStrings);
        ALOGI("VMRuntime.preloadDexCaches types total=%d before=%d after=%d",
              total.numTypes, before.numTypes, after.numTypes);
        ALOGI("VMRuntime.preloadDexCaches fields total=%d before=%d after=%d",
              total.numFields, before.numFields, after.numFields);
        ALOGI("VMRuntime.preloadDexCaches methods total=%d before=%d after=%d",
              total.numMethods, before.numMethods, after.numMethods);
        ALOGI("VMRuntime.preloadDexCaches finished");
    }

    RETURN_VOID();
}

const DalvikNativeMethod dvm_dalvik_system_VMRuntime[] = {
    { "addressOf", "(Ljava/lang/Object;)J",
        Dalvik_dalvik_system_VMRuntime_addressOf },
    { "bootClassPath", "()Ljava/lang/String;",
        Dalvik_dalvik_system_VMRuntime_bootClassPath },
    { "classPath", "()Ljava/lang/String;",
        Dalvik_dalvik_system_VMRuntime_classPath },
    { "clearGrowthLimit", "()V",
        Dalvik_dalvik_system_VMRuntime_clearGrowthLimit },
    { "disableJitCompilation", "()V",
        Dalvik_dalvik_system_VMRuntime_disableJitCompilation },
    { "isDebuggerActive", "()Z",
        Dalvik_dalvik_system_VMRuntime_isDebuggerActive },
    { "getTargetHeapUtilization", "()F",
        Dalvik_dalvik_system_VMRuntime_getTargetHeapUtilization },
    { "nativeSetTargetHeapUtilization", "(F)V",
        Dalvik_dalvik_system_VMRuntime_nativeSetTargetHeapUtilization },
    { "newNonMovableArray", "(Ljava/lang/Class;I)Ljava/lang/Object;",
        Dalvik_dalvik_system_VMRuntime_newNonMovableArray },
    { "properties", "()[Ljava/lang/String;",
        Dalvik_dalvik_system_VMRuntime_properties },
    { "setTargetSdkVersion", "(I)V",
        Dalvik_dalvik_system_VMRuntime_setTargetSdkVersion },
    { "startJitCompilation", "()V",
        Dalvik_dalvik_system_VMRuntime_startJitCompilation },
    { "vmVersion", "()Ljava/lang/String;",
        Dalvik_dalvik_system_VMRuntime_vmVersion },
    { "vmLibrary", "()Ljava/lang/String;",
        Dalvik_dalvik_system_VMRuntime_vmLibrary },
    { "registerNativeAllocation", "(I)V",
        Dalvik_dalvik_system_VMRuntime_registerNativeAllocation },
    { "registerNativeFree", "(I)V",
        Dalvik_dalvik_system_VMRuntime_registerNativeFree },
    { "preloadDexCaches", "()V",
        Dalvik_dalvik_system_VMRuntime_preloadDexCaches },
    { NULL, NULL, NULL },
};
