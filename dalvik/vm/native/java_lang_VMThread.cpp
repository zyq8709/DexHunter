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
 * java.lang.VMThread
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"


/*
 * static void create(Thread t, long stacksize)
 *
 * This is eventually called as a result of Thread.start().
 *
 * Throws an exception on failure.
 */
static void Dalvik_java_lang_VMThread_create(const u4* args, JValue* pResult)
{
    Object* threadObj = (Object*) args[0];
    s8 stackSize = GET_ARG_LONG(args, 1);

    /* copying collector will pin threadObj for us since it was an argument */
    dvmCreateInterpThread(threadObj, (int) stackSize);
    RETURN_VOID();
}

/*
 * static Thread currentThread()
 */
static void Dalvik_java_lang_VMThread_currentThread(const u4* args,
    JValue* pResult)
{
    UNUSED_PARAMETER(args);

    RETURN_PTR(dvmThreadSelf()->threadObj);
}

/*
 * void getStatus()
 *
 * Gets the Thread status. Result is in VM terms, has to be mapped to
 * Thread.State by interpreted code.
 */
static void Dalvik_java_lang_VMThread_getStatus(const u4* args, JValue* pResult)
{
    Object* thisPtr = (Object*) args[0];
    Thread* thread;
    int result;

    dvmLockThreadList(NULL);
    thread = dvmGetThreadFromThreadObject(thisPtr);
    if (thread != NULL)
        result = thread->status;
    else
        result = THREAD_ZOMBIE;     // assume it used to exist and is now gone
    dvmUnlockThreadList();

    RETURN_INT(result);
}

/*
 * boolean holdsLock(Object object)
 *
 * Returns whether the current thread has a monitor lock on the specific
 * object.
 */
static void Dalvik_java_lang_VMThread_holdsLock(const u4* args, JValue* pResult)
{
    Object* thisPtr = (Object*) args[0];
    Object* object = (Object*) args[1];
    Thread* thread;

    if (object == NULL) {
        dvmThrowNullPointerException("object == null");
        RETURN_VOID();
    }

    dvmLockThreadList(NULL);
    thread = dvmGetThreadFromThreadObject(thisPtr);
    int result = dvmHoldsLock(thread, object);
    dvmUnlockThreadList();

    RETURN_BOOLEAN(result);
}

/*
 * void interrupt()
 *
 * Interrupt a thread that is waiting (or is about to wait) on a monitor.
 */
static void Dalvik_java_lang_VMThread_interrupt(const u4* args, JValue* pResult)
{
    Object* thisPtr = (Object*) args[0];
    Thread* thread;

    dvmLockThreadList(NULL);
    thread = dvmGetThreadFromThreadObject(thisPtr);
    if (thread != NULL)
        dvmThreadInterrupt(thread);
    dvmUnlockThreadList();
    RETURN_VOID();
}

/*
 * static boolean interrupted()
 *
 * Determine if the current thread has been interrupted.  Clears the flag.
 */
static void Dalvik_java_lang_VMThread_interrupted(const u4* args,
    JValue* pResult)
{
    Thread* self = dvmThreadSelf();
    bool interrupted;

    UNUSED_PARAMETER(args);

    interrupted = self->interrupted;
    self->interrupted = false;
    RETURN_BOOLEAN(interrupted);
}

/*
 * boolean isInterrupted()
 *
 * Determine if the specified thread has been interrupted.  Does not clear
 * the flag.
 */
static void Dalvik_java_lang_VMThread_isInterrupted(const u4* args,
    JValue* pResult)
{
    Object* thisPtr = (Object*) args[0];
    Thread* thread;
    bool interrupted;

    dvmLockThreadList(NULL);
    thread = dvmGetThreadFromThreadObject(thisPtr);
    if (thread != NULL)
        interrupted = thread->interrupted;
    else
        interrupted = false;
    dvmUnlockThreadList();

    RETURN_BOOLEAN(interrupted);
}

/*
 * void nameChanged(String newName)
 *
 * The name of the target thread has changed.  We may need to alert DDMS.
 */
static void Dalvik_java_lang_VMThread_nameChanged(const u4* args,
    JValue* pResult)
{
    Object* thisPtr = (Object*) args[0];
    StringObject* nameStr = (StringObject*) args[1];
    Thread* thread;
    int threadId = -1;

    /* get the thread's ID */
    dvmLockThreadList(NULL);
    thread = dvmGetThreadFromThreadObject(thisPtr);
    if (thread != NULL)
        threadId = thread->threadId;
    dvmUnlockThreadList();

    dvmDdmSendThreadNameChange(threadId, nameStr);
    //char* str = dvmCreateCstrFromString(nameStr);
    //ALOGI("UPDATE: threadid=%d now '%s'", threadId, str);
    //free(str);

    RETURN_VOID();
}

/*
 * void setPriority(int newPriority)
 *
 * Alter the priority of the specified thread.  "newPriority" will range
 * from Thread.MIN_PRIORITY to Thread.MAX_PRIORITY (1-10), with "normal"
 * threads at Thread.NORM_PRIORITY (5).
 */
static void Dalvik_java_lang_VMThread_setPriority(const u4* args,
    JValue* pResult)
{
    Object* thisPtr = (Object*) args[0];
    int newPriority = args[1];
    Thread* thread;

    dvmLockThreadList(NULL);
    thread = dvmGetThreadFromThreadObject(thisPtr);
    if (thread != NULL)
        dvmChangeThreadPriority(thread, newPriority);
    //dvmDumpAllThreads(false);
    dvmUnlockThreadList();

    RETURN_VOID();
}

/*
 * static void sleep(long msec, int nsec)
 */
static void Dalvik_java_lang_VMThread_sleep(const u4* args, JValue* pResult)
{
    dvmThreadSleep(GET_ARG_LONG(args,0), args[2]);
    RETURN_VOID();
}

/*
 * public void yield()
 *
 * Causes the thread to temporarily pause and allow other threads to execute.
 *
 * The exact behavior is poorly defined.  Some discussion here:
 *   http://www.cs.umd.edu/~pugh/java/memoryModel/archive/0944.html
 */
static void Dalvik_java_lang_VMThread_yield(const u4* args, JValue* pResult)
{
    UNUSED_PARAMETER(args);

    sched_yield();

    RETURN_VOID();
}

const DalvikNativeMethod dvm_java_lang_VMThread[] = {
    { "create",         "(Ljava/lang/Thread;J)V",
        Dalvik_java_lang_VMThread_create },
    { "currentThread",  "()Ljava/lang/Thread;",
        Dalvik_java_lang_VMThread_currentThread },
    { "getStatus",      "()I",
        Dalvik_java_lang_VMThread_getStatus },
    { "holdsLock",      "(Ljava/lang/Object;)Z",
        Dalvik_java_lang_VMThread_holdsLock },
    { "interrupt",      "()V",
        Dalvik_java_lang_VMThread_interrupt },
    { "interrupted",    "()Z",
        Dalvik_java_lang_VMThread_interrupted },
    { "isInterrupted",  "()Z",
        Dalvik_java_lang_VMThread_isInterrupted },
    { "nameChanged",    "(Ljava/lang/String;)V",
        Dalvik_java_lang_VMThread_nameChanged },
    { "setPriority",    "(I)V",
        Dalvik_java_lang_VMThread_setPriority },
    { "sleep",          "(JI)V",
        Dalvik_java_lang_VMThread_sleep },
    { "yield",          "()V",
        Dalvik_java_lang_VMThread_yield },
    { NULL, NULL, NULL },
};
