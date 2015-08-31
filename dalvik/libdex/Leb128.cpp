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
 * Functions for interpreting LEB128 (little endian base 128) values
 */

#include "Leb128.h"

/*
 * Reads an unsigned LEB128 value, updating the given pointer to point
 * just past the end of the read value and also indicating whether the
 * value was syntactically valid. The only syntactically *invalid*
 * values are ones that are five bytes long where the final byte has
 * any but the low-order four bits set. Additionally, if the limit is
 * passed as non-NULL and bytes would need to be read past the limit,
 * then the read is considered invalid.
 */
int readAndVerifyUnsignedLeb128(const u1** pStream, const u1* limit,
        bool* okay) {
    const u1* ptr = *pStream;
    int result = readUnsignedLeb128(pStream);

    if (((limit != NULL) && (*pStream > limit))
            || (((*pStream - ptr) == 5) && (ptr[4] > 0x0f))) {
        *okay = false;
    }

    return result;
}

/*
 * Reads a signed LEB128 value, updating the given pointer to point
 * just past the end of the read value and also indicating whether the
 * value was syntactically valid. The only syntactically *invalid*
 * values are ones that are five bytes long where the final byte has
 * any but the low-order four bits set. Additionally, if the limit is
 * passed as non-NULL and bytes would need to be read past the limit,
 * then the read is considered invalid.
 */
int readAndVerifySignedLeb128(const u1** pStream, const u1* limit,
        bool* okay) {
    const u1* ptr = *pStream;
    int result = readSignedLeb128(pStream);

    if (((limit != NULL) && (*pStream > limit))
            || (((*pStream - ptr) == 5) && (ptr[4] > 0x0f))) {
        *okay = false;
    }

    return result;
}
