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
 * dalvik.system.VMStack
 */
#include "Dalvik.h"
#include "UniquePtr.h"
#include "native/InternalNativePriv.h"

/*
 * public static ClassLoader getCallingClassLoader()
 *
 * Return the defining class loader of the caller's caller.
 */
static void Dalvik_dalvik_system_VMStack_getCallingClassLoader(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz =
        dvmGetCaller2Class(dvmThreadSelf()->interpSave.curFrame);

    UNUSED_PARAMETER(args);

    if (clazz == NULL)
        RETURN_PTR(NULL);
    RETURN_PTR(clazz->classLoader);
}

/*
 * public static Class<?> getStackClass2()
 *
 * Returns the class of the caller's caller's caller.
 */
static void Dalvik_dalvik_system_VMStack_getStackClass2(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz =
        dvmGetCaller3Class(dvmThreadSelf()->interpSave.curFrame);

    UNUSED_PARAMETER(args);

    RETURN_PTR(clazz);
}

/*
 * public static Class<?>[] getClasses(int maxDepth)
 *
 * Create an array of classes for the methods on the stack, skipping the
 * first two and all reflection methods.  If "stopAtPrivileged" is set,
 * stop shortly after we encounter a privileged class.
 */
static void Dalvik_dalvik_system_VMStack_getClasses(const u4* args,
    JValue* pResult)
{
    /* note "maxSize" is unsigned, so -1 turns into a very large value */
    size_t maxSize = args[0];
    size_t size = 0;
    const size_t kSkip = 2;

    /*
     * Get an array with the stack trace in it.
     */
    void *fp = dvmThreadSelf()->interpSave.curFrame;
    size_t depth = dvmComputeExactFrameDepth(fp);
    UniquePtr<const Method*[]> methods(new const Method*[depth]);
    dvmFillStackTraceArray(fp, methods.get(), depth);

    /*
     * Run through the array and count up how many elements there are.
     */
    for (size_t i = kSkip; i < depth && size < maxSize; ++i) {
        const Method* meth = methods[i];

        if (dvmIsReflectionMethod(meth))
            continue;

        size++;
    }

    /*
     * Create an array object to hold the classes.
     * TODO: can use gDvm.classJavaLangClassArray here?
     */
    ClassObject* classArrayClass = dvmFindArrayClass("[Ljava/lang/Class;",
                                                     NULL);
    if (classArrayClass == NULL) {
        ALOGW("Unable to find java.lang.Class array class");
        return;
    }
    ArrayObject* classes = dvmAllocArrayByClass(classArrayClass,
                                                size,
                                                ALLOC_DEFAULT);
    if (classes == NULL) {
        ALOGW("Unable to allocate class array of %zd elements", size);
        return;
    }

    /*
     * Fill in the array.
     */
    size_t objCount = 0;
    for (size_t i = kSkip; i < depth; ++i) {
        if (dvmIsReflectionMethod(methods[i])) {
            continue;
        }
        Object* klass = (Object *)methods[i]->clazz;
        dvmSetObjectArrayElement(classes, objCount, klass);
        objCount++;
    }
    assert(objCount == classes->length);

    dvmReleaseTrackedAlloc((Object*)classes, NULL);
    RETURN_PTR(classes);
}

/*
 * Return a trace buffer for the specified thread or NULL if the
 * thread is not still alive. *depth is set to the length of a
 * non-NULL trace buffer. Caller is responsible for freeing the trace
 * buffer.
 */
static int* getTraceBuf(Object* targetThreadObj, size_t* pStackDepth)
{
    Thread* self = dvmThreadSelf();
    Thread* thread;
    int* traceBuf;

    assert(targetThreadObj != NULL);

    dvmLockThreadList(self);

    /*
     * Make sure the thread is still alive and in the list.
     */
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (thread->threadObj == targetThreadObj)
            break;
    }
    if (thread == NULL) {
        ALOGI("VMStack.getTraceBuf: threadObj %p not active",
            targetThreadObj);
        dvmUnlockThreadList();
        return NULL;
    }

    /*
     * Suspend the thread, pull out the stack trace, then resume the thread
     * and release the thread list lock.  If we're being asked to examine
     * our own stack trace, skip the suspend/resume.
     */
    if (thread != self)
        dvmSuspendThread(thread);
    traceBuf = dvmFillInStackTraceRaw(thread, pStackDepth);
    if (thread != self)
        dvmResumeThread(thread);
    dvmUnlockThreadList();

    return traceBuf;
}

/*
 * public static StackTraceElement[] getThreadStackTrace(Thread t)
 *
 * Retrieve the stack trace of the specified thread and return it as an
 * array of StackTraceElement.  Returns NULL on failure.
 */
static void Dalvik_dalvik_system_VMStack_getThreadStackTrace(const u4* args,
    JValue* pResult)
{
    Object* targetThreadObj = (Object*) args[0];
    size_t stackDepth;
    int* traceBuf = getTraceBuf(targetThreadObj, &stackDepth);

    if (traceBuf == NULL)
        RETURN_PTR(NULL);

    /*
     * Convert the raw buffer into an array of StackTraceElement.
     */
    ArrayObject* trace = dvmGetStackTraceRaw(traceBuf, stackDepth);
    free(traceBuf);
    RETURN_PTR(trace);
}

/*
 * public static int fillStackTraceElements(Thread t, StackTraceElement[] stackTraceElements)
 *
 * Retrieve a partial stack trace of the specified thread and return
 * the number of frames filled.  Returns 0 on failure.
 */
static void Dalvik_dalvik_system_VMStack_fillStackTraceElements(const u4* args,
    JValue* pResult)
{
    Object* targetThreadObj = (Object*) args[0];
    ArrayObject* steArray = (ArrayObject*) args[1];
    size_t stackDepth;
    int* traceBuf = getTraceBuf(targetThreadObj, &stackDepth);

    if (traceBuf == NULL)
        RETURN_PTR(NULL);

    /*
     * Set the raw buffer into an array of StackTraceElement.
     */
    if (stackDepth > steArray->length) {
        stackDepth = steArray->length;
    }
    dvmFillStackTraceElements(traceBuf, stackDepth, steArray);
    free(traceBuf);
    RETURN_INT(stackDepth);
}

const DalvikNativeMethod dvm_dalvik_system_VMStack[] = {
    { "getCallingClassLoader",  "()Ljava/lang/ClassLoader;",
        Dalvik_dalvik_system_VMStack_getCallingClassLoader },
    { "getStackClass2",         "()Ljava/lang/Class;",
        Dalvik_dalvik_system_VMStack_getStackClass2 },
    { "getClasses",             "(I)[Ljava/lang/Class;",
        Dalvik_dalvik_system_VMStack_getClasses },
    { "getThreadStackTrace",    "(Ljava/lang/Thread;)[Ljava/lang/StackTraceElement;",
        Dalvik_dalvik_system_VMStack_getThreadStackTrace },
    { "fillStackTraceElements", "(Ljava/lang/Thread;[Ljava/lang/StackTraceElement;)I",
        Dalvik_dalvik_system_VMStack_fillStackTraceElements },
    { NULL, NULL, NULL },
};
