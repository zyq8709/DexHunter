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
 * JNI method invocation.  This is used to call a C/C++ JNI method.  The
 * argument list has to be pushed onto the native stack according to
 * local calling conventions.
 *
 * This version supports the MIPS O32 ABI.
 */

/* TODO: this is candidate for consolidation of similar code from ARM. */

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
 * This function produces mips-specific hints - specifically a description
 * of padding required to keep all 64-bit parameters properly aligned.
 *
 * MIPS JNI hint format(Same as ARM)
 *
 *       LLLL FFFFFFFF FFFFFFFF FFFFFFFF
 *
 *   L - number of double-words of storage required on the stack (0-30 words)
 *   F - pad flag -- if set, the stack increases 8 bytes, else the stack increases 4 bytes
 *                   after copying 32 bits args into stack. (little different from ARM)
 *
 * If there are too many arguments to construct valid hints, this function will
 * return a result with the S bit set.
 */
u4 dvmPlatformInvokeHints(const DexProto* proto)
{

    const char* sig = dexProtoGetShorty(proto);
    int padFlags, jniHints;
    char sigByte;
    int stackOffset, padMask, hints;

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
        /*
         * StackOffset includes the space for a2/a3. However we have reserved
         * 16 bytes on stack in CallO32.S, so we should subtract 2 from stackOffset.
         */
        stackOffset -= 2;
        if (stackOffset < 0)
            stackOffset = 0;
        jniHints |= ((stackOffset+1) / 2) << DALVIK_JNI_COUNT_SHIFT;
        jniHints |= padFlags;
    }

    return jniHints;
}
