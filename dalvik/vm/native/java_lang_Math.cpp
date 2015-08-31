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

static void Math_absD(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangMath_abs_double);
}

static void Math_absF(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangMath_abs_float);
}

static void Math_absI(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangMath_abs_int);
}

static void Math_absJ(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangMath_abs_long);
}

static void Math_cos(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangMath_cos);
}

static void Math_maxI(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangMath_max_int);
}

static void Math_minI(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangMath_min_int);
}

static void Math_sin(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangMath_sin);
}

static void Math_sqrt(const u4* args, JValue* pResult)
{
    MAKE_INTRINSIC_TRAMPOLINE(javaLangMath_sqrt);
}

const DalvikNativeMethod dvm_java_lang_Math[] = {
    { "abs",  "(D)D",  Math_absD },
    { "abs",  "(F)F",  Math_absF },
    { "abs",  "(I)I",  Math_absI },
    { "abs",  "(J)J",  Math_absJ },
    { "cos",  "(D)D",  Math_cos },
    { "max",  "(II)I", Math_maxI },
    { "min",  "(II)I", Math_minI },
    { "sin",  "(D)D",  Math_sin },
    { "sqrt", "(D)D",  Math_sqrt },
    { NULL, NULL, NULL },
};
