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
 *    H - target-specific hints (see below for details)
 *
 * This function produces x86-specific hints for the standard 32-bit 386 ABI.
 * Note that the JNI requirements are very close to the 386 runtime model.  In
 * particular, natural datatype alignments do not apply to passed arguments.
 * All arguments have 32-bit alignment.  As a result, we don't have to worry
 * about padding - just total size.  The only tricky bit is that floating point
 * return values come back on the FP stack.
 *
 *
 * 386 ABI JNI hint format
 *
 *       ZZZZ ZZZZZZZZ AAAAAAAA AAAAAAAA
 *
 *   Z - reserved, must be 0
 *   A - size of variable argument block in 32-bit words (note - does not
 *       include JNIEnv or clazz)
 *
 * For the 386 ABI, valid hints should always be generated.
 */
u4 dvmPlatformInvokeHints( const DexProto* proto)
{
    const char* sig = dexProtoGetShorty(proto);
    unsigned int jniHints, wordCount;
    char sigByte;

    wordCount = 0;
    while (true) {
        sigByte = *(sig++);

        if (sigByte == '\0')
            break;

        wordCount++;

        if (sigByte == 'D' || sigByte == 'J') {
            wordCount++;
        }
    }

    if (wordCount > 0xFFFF) {
        /* Invalid - Dex file limitation */
        jniHints = DALVIK_JNI_NO_ARG_INFO;
    } else {
        jniHints = wordCount;
    }

    return jniHints;
}
