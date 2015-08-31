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
 * java.lang.reflect.Constructor
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"


/*
 * public int constructNative(Object[] args, Class declaringClass,
 *     Class[] parameterTypes, int slot, boolean noAccessCheck)
 *
 * We get here through Constructor.newInstance().  The Constructor object
 * would not be available if the constructor weren't public (per the
 * definition of Class.getConstructor), so we can skip the method access
 * check.  We can also safely assume the constructor isn't associated
 * with an interface, array, or primitive class.
 */
static void Dalvik_java_lang_reflect_Constructor_constructNative(
    const u4* args, JValue* pResult)
{
    // ignore thisPtr in args[0]
    ArrayObject* argList = (ArrayObject*) args[1];
    ClassObject* declaringClass = (ClassObject*) args[2];
    ArrayObject* params = (ArrayObject*) args[3];
    int slot = args[4];
    bool noAccessCheck = (args[5] != 0);
    Object* newObj;
    Method* meth;

    if (dvmIsAbstractClass(declaringClass)) {
        dvmThrowInstantiationException(declaringClass, NULL);
        RETURN_VOID();
    }

    /* initialize the class if it hasn't been already */
    if (!dvmIsClassInitialized(declaringClass)) {
        if (!dvmInitClass(declaringClass)) {
            ALOGW("Class init failed in Constructor.constructNative (%s)",
                declaringClass->descriptor);
            assert(dvmCheckException(dvmThreadSelf()));
            RETURN_VOID();
        }
    }

    newObj = dvmAllocObject(declaringClass, ALLOC_DEFAULT);
    if (newObj == NULL)
        RETURN_PTR(NULL);

    meth = dvmSlotToMethod(declaringClass, slot);
    assert(meth != NULL);

    (void) dvmInvokeMethod(newObj, meth, argList, params, NULL, noAccessCheck);
    dvmReleaseTrackedAlloc(newObj, NULL);
    RETURN_PTR(newObj);
}

const DalvikNativeMethod dvm_java_lang_reflect_Constructor[] = {
    { "constructNative",    "([Ljava/lang/Object;Ljava/lang/Class;[Ljava/lang/Class;IZ)Ljava/lang/Object;",
        Dalvik_java_lang_reflect_Constructor_constructNative },
    { NULL, NULL, NULL },
};
