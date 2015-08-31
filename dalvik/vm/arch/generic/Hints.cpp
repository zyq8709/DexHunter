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
 * Target-specific optimization and run-time hints
 */


#include "Dalvik.h"
#include "libdex/DexClass.h"

#include <stdlib.h>
#include <stddef.h>
#include <sys/stat.h>


/*
 * The class loader will associate with each method a 32-bit info word
 * (jniArgInfo) to support JNI calls.  The high order 4 bits of this word
 * are the same for all targets, while the lower 28 are used for hints to
 * allow accelerated JNI bridge transfers.
 *
 * jniArgInfo (32-bit int) layout:
 *
 *    SRRRHHHH HHHHHHHH HHHHHHHH HHHHHHHH
 *
 *    S - if set, ignore the hints and do things the hard way (scan signature)
 *    R - return-type enumeration
 *    H - target-specific hints
 *
 * This function is a placeholder/template and should be duplicated in the
 * appropriate arch/<target>/ directory for new target ports.  The hints
 * field should be defined and constructed in conjunction with
 * dvmPlatformInvoke.

 * If valid hints can't be constructed, this function should return a negative
 * value.  In that case, the caller will set the S bit in the jniArgInfo word
 * and convert the arguments the slow way.
 */
u4 dvmPlatformInvokeHints( const DexProto* proto)
{
    /* No hints for generic target - force argument walk at run-time */
    return DALVIK_JNI_NO_ARG_INFO;
}
