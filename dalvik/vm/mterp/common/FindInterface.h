/*
 * Copyright (C) 2009 The Android Open Source Project
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

/* common includes */
#include "Dalvik.h"

extern "C" {

/*
 * Look up an interface on a class using the cache.
 *
 * This function used to be defined in mterp/c/header.c, but it is now used by
 * the JIT compiler as well so it is separated into its own header file to
 * avoid potential out-of-sync changes in the future.
 */
INLINE Method* dvmFindInterfaceMethodInCache(ClassObject* thisClass,
    u4 methodIdx, const Method* method, DvmDex* methodClassDex)
{
#define ATOMIC_CACHE_CALC \
    dvmInterpFindInterfaceMethod(thisClass, methodIdx, method, methodClassDex)
#define ATOMIC_CACHE_NULL_ALLOWED false

    return (Method*) ATOMIC_CACHE_LOOKUP(methodClassDex->pInterfaceCache,
                DEX_INTERFACE_CACHE_SIZE, thisClass, methodIdx);

#undef ATOMIC_CACHE_CALC
}

}
