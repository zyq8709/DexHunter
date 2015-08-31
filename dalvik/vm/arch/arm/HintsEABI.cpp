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
 * This function produces arm-specific hints - specifically a description
 * of padding required to keep all 64-bit parameters properly aligned.
 *
 * ARM JNI hint format
 *
 *       LLLL FFFFFFFF FFFFFFFF FFFFFFFF
 *
 *   L - number of double-words of storage required on the stack (0-30 words)
 *   F - pad flag -- if set, write a pad word to the stack before copying
 *       the next 32 bits
 *
 * If there are too many arguments to construct valid hints, this function will
 * return a result with the S bit set.
 */
u4 dvmPlatformInvokeHints(const DexProto* proto)
{
    const char* sig = dexProtoGetShorty(proto);
    int padFlags, jniHints;
    char sigByte;
    int stackOffset, padMask;

    stackOffset = padFlags = 0;
    padMask = 0x00000001;

    /* Skip past the return type */
    sig++;

    while (true) {
        sigByte = *(sig++);

        if (sigByte == '\0')
            break;

        if (sigByte == 'D' || sigByte == 'J') {
            if ((stackOffset & 1) != 0) {
                padFlags |= padMask;
                stackOffset++;
                padMask <<= 1;
            }
            stackOffset += 2;
            padMask <<= 2;
        } else {
            stackOffset++;
            padMask <<= 1;
        }
    }

    jniHints = 0;

    if (stackOffset > DALVIK_JNI_COUNT_SHIFT) {
        /* too big for "fast" version */
        jniHints = DALVIK_JNI_NO_ARG_INFO;
    } else {
        assert((padFlags & (0xffffffff << DALVIK_JNI_COUNT_SHIFT)) == 0);
        stackOffset -= 2;           // r2/r3 holds first two items
        if (stackOffset < 0)
            stackOffset = 0;
        jniHints |= ((stackOffset+1) / 2) << DALVIK_JNI_COUNT_SHIFT;
        jniHints |= padFlags;
    }

    return jniHints;
}
