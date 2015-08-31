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
 * org.apache.harmony.dalvik.NativeTestTarget
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"


/*
 * public static void emptyInternalStaticMethod()
 *
 * For benchmarks, a do-nothing internal method with no arguments.
 */
static void Dalvik_org_apache_harmony_dalvik_NativeTestTarget_emptyInternalMethod(
    const u4* args, JValue* pResult)
{
    UNUSED_PARAMETER(args);

    RETURN_VOID();
}

const DalvikNativeMethod dvm_org_apache_harmony_dalvik_NativeTestTarget[] =
{
    { "emptyInternalStaticMethod", "()V",
        Dalvik_org_apache_harmony_dalvik_NativeTestTarget_emptyInternalMethod },
    { NULL, NULL, NULL },
};
