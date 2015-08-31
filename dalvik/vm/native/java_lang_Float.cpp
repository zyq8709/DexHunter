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

static void Float_floatToIntBits(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangFloat_floatToIntBits);
}

static void Float_floatToRawIntBits(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangFloat_floatToRawIntBits);
}

static void Float_intBitsToFloat(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangFloat_intBitsToFloat);
}

const DalvikNativeMethod dvm_java_lang_Float[] = {
    { "floatToIntBits",    "(F)I", Float_floatToIntBits },
    { "floatToRawIntBits", "(F)I", Float_floatToRawIntBits },
    { "intBitsToFloat",    "(I)F", Float_intBitsToFloat },
    { NULL, NULL, NULL },
};
