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
 * java.lang.Object
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"


/*
 * private Object internalClone()
 *
 * Implements most of Object.clone().
 */
static void Dalvik_java_lang_Object_internalClone(const u4* args,
    JValue* pResult)
{
    Object* thisPtr = (Object*) args[0];
    Object* clone = dvmCloneObject(thisPtr, ALLOC_DONT_TRACK);

    RETURN_PTR(clone);
}

/*
 * public int hashCode()
 */
static void Dalvik_java_lang_Object_hashCode(const u4* args, JValue* pResult)
{
    Object* thisPtr = (Object*) args[0];
    RETURN_INT(dvmIdentityHashCode(thisPtr));
}

/*
 * public Class getClass()
 */
static void Dalvik_java_lang_Object_getClass(const u4* args, JValue* pResult)
{
    Object* thisPtr = (Object*) args[0];

    RETURN_PTR(thisPtr->clazz);
}

/*
 * public void notify()
 *
 * NOTE: we declare this as a full DalvikBridgeFunc, rather than a
 * DalvikNativeFunc, because we really want to avoid the "self" lookup.
 */
static void Dalvik_java_lang_Object_notify(const u4* args, JValue* pResult,
    const Method* method, Thread* self)
{
    Object* thisPtr = (Object*) args[0];

    dvmObjectNotify(self, thisPtr);
    RETURN_VOID();
}

/*
 * public void notifyAll()
 */
static void Dalvik_java_lang_Object_notifyAll(const u4* args, JValue* pResult,
    const Method* method, Thread* self)
{
    Object* thisPtr = (Object*) args[0];

    dvmObjectNotifyAll(self, thisPtr);
    RETURN_VOID();
}

/*
 * public void wait(long ms, int ns) throws InterruptedException
 */
static void Dalvik_java_lang_Object_wait(const u4* args, JValue* pResult,
    const Method* method, Thread* self)
{
    Object* thisPtr = (Object*) args[0];

    dvmObjectWait(self, thisPtr, GET_ARG_LONG(args,1), (s4)args[3], true);
    RETURN_VOID();
}

const DalvikNativeMethod dvm_java_lang_Object[] = {
    { "internalClone",  "(Ljava/lang/Cloneable;)Ljava/lang/Object;",
        Dalvik_java_lang_Object_internalClone },
    { "hashCode",       "()I",
        Dalvik_java_lang_Object_hashCode },
    { "notify",         "()V",
        (DalvikNativeFunc) Dalvik_java_lang_Object_notify },
    { "notifyAll",      "()V",
        (DalvikNativeFunc) Dalvik_java_lang_Object_notifyAll },
    { "wait",           "(JI)V",
        (DalvikNativeFunc) Dalvik_java_lang_Object_wait },
    { "getClass",       "()Ljava/lang/Class;",
        Dalvik_java_lang_Object_getClass },
    { NULL, NULL, NULL },
};
