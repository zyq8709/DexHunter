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
 * java.lang.Throwable
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"


/*
 * private static Object nativeFillInStackTrace()
 */
static void Dalvik_java_lang_Throwable_nativeFillInStackTrace(const u4* args,
    JValue* pResult)
{
    Object* stackState = NULL;

    UNUSED_PARAMETER(args);

    stackState = dvmFillInStackTrace(dvmThreadSelf());
    RETURN_PTR(stackState);
}

/*
 * private static StackTraceElement[] nativeGetStackTrace(Object stackState)
 *
 * The "stackState" argument must be the value returned by an earlier call to
 * nativeFillInStackTrace().
 */
static void Dalvik_java_lang_Throwable_nativeGetStackTrace(const u4* args,
    JValue* pResult)
{
    Object* stackState = (Object*) args[0];
    ArrayObject* elements = NULL;

    if (stackState == NULL) {
        ALOGW("getStackTrace() called but no trace available");
        RETURN_PTR(NULL);   /* could throw NPE; currently caller will do so */
    }

    elements = dvmGetStackTrace(stackState);
    RETURN_PTR(elements);
}

const DalvikNativeMethod dvm_java_lang_Throwable[] = {
    { "nativeFillInStackTrace", "()Ljava/lang/Object;",
        Dalvik_java_lang_Throwable_nativeFillInStackTrace },
    { "nativeGetStackTrace",    "(Ljava/lang/Object;)[Ljava/lang/StackTraceElement;",
        Dalvik_java_lang_Throwable_nativeGetStackTrace },
    { NULL, NULL, NULL },
};
