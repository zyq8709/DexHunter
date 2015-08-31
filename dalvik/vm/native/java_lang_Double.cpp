/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "Dalvik.h"
#include "native/InternalNativePriv.h"

static void Double_doubleToLongBits(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangDouble_doubleToLongBits);
}

static void Double_doubleToRawLongBits(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangDouble_doubleToRawLongBits);
}

static void Double_longBitsToDouble(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangDouble_longBitsToDouble);
}

const DalvikNativeMethod dvm_java_lang_Double[] = {
    { "doubleToLongBits",    "(D)J", Double_doubleToLongBits },
    { "doubleToRawLongBits", "(D)J", Double_doubleToRawLongBits },
    { "longBitsToDouble",    "(J)D", Double_longBitsToDouble },
    { NULL, NULL, NULL },
};
