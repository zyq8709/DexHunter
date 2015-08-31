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

/*
 * Validate and manipulate MUTF-8 encoded string data.
 */

#include "DexUtf.h"

/* Compare two '\0'-terminated modified UTF-8 strings, using Unicode
 * code point values for comparison. This treats different encodings
 * for the same code point as equivalent, except that only a real '\0'
 * byte is considered the string terminator. The return value is as
 * for strcmp(). */
int dexUtf8Cmp(const char* s1, const char* s2) {
    for (;;) {
        if (*s1 == '\0') {
            if (*s2 == '\0') {
                return 0;
            }
            return -1;
        } else if (*s2 == '\0') {
            return 1;
        }

        int utf1 = dexGetUtf16FromUtf8(&s1);
        int utf2 = dexGetUtf16FromUtf8(&s2);
        int diff = utf1 - utf2;

        if (diff != 0) {
            return diff;
        }
    }
}

/* for dexIsValidMemberNameUtf8(), a bit vector indicating valid low ascii */
u4 DEX_MEMBER_VALID_LOW_ASCII[4] = {
    0x00000000, // 00..1f low control characters; nothing valid
    0x03ff2010, // 20..3f digits and symbols; valid: '0'..'9', '$', '-'
    0x87fffffe, // 40..5f uppercase etc.; valid: 'A'..'Z', '_'
    0x07fffffe  // 60..7f lowercase etc.; valid: 'a'..'z'
};

/* Helper for dexIsValidMemberNameUtf8(); do not call directly. */
bool dexIsValidMemberNameUtf8_0(const char** pUtf8Ptr) {
    /*
     * It's a multibyte encoded character. Decode it and analyze. We
     * accept anything that isn't (a) an improperly encoded low value,
     * (b) an improper surrogate pair, (c) an encoded '\0', (d) a high
     * control character, or (e) a high space, layout, or special
     * character (U+00a0, U+2000..U+200f, U+2028..U+202f,
     * U+fff0..U+ffff). This is all specified in the dex format
     * document.
     */

    u2 utf16 = dexGetUtf16FromUtf8(pUtf8Ptr);

    // Perform follow-up tests based on the high 8 bits.
    switch (utf16 >> 8) {
        case 0x00: {
            // It's only valid if it's above the ISO-8859-1 high space (0xa0).
            return (utf16 > 0x00a0);
        }
        case 0xd8:
        case 0xd9:
        case 0xda:
        case 0xdb: {
            /*
             * It's a leading surrogate. Check to see that a trailing
             * surrogate follows.
             */
            utf16 = dexGetUtf16FromUtf8(pUtf8Ptr);
            return (utf16 >= 0xdc00) && (utf16 <= 0xdfff);
        }
        case 0xdc:
        case 0xdd:
        case 0xde:
        case 0xdf: {
            // It's a trailing surrogate, which is not valid at this point.
            return false;
        }
        case 0x20:
        case 0xff: {
            // It's in the range that has spaces, controls, and specials.
            switch (utf16 & 0xfff8) {
                case 0x2000:
                case 0x2008:
                case 0x2028:
                case 0xfff0:
                case 0xfff8: {
                    return false;
                }
            }
            break;
        }
    }

    return true;
}

/* Return whether the given string is a valid field or method name. */
bool dexIsValidMemberName(const char* s) {
    bool angleName = false;

    switch (*s) {
        case '\0': {
            // The empty string is not a valid name.
            return false;
        }
        case '<': {
            /*
             * '<' is allowed only at the start of a name, and if present,
             * means that the name must end with '>'.
             */
            angleName = true;
            s++;
            break;
        }
    }

    for (;;) {
        switch (*s) {
            case '\0': {
                return !angleName;
            }
            case '>': {
                return angleName && s[1] == '\0';
            }
        }
        if (!dexIsValidMemberNameUtf8(&s)) {
            return false;
        }
    }
}

/* Helper for validating type descriptors and class names, which is parametric
 * with respect to type vs. class and dot vs. slash. */
static bool isValidTypeDescriptorOrClassName(const char* s, bool isClassName,
        bool dotSeparator) {
    int arrayCount = 0;

    while (*s == '[') {
        arrayCount++;
        s++;
    }

    if (arrayCount > 255) {
        // Arrays may have no more than 255 dimensions.
        return false;
    }

    if (arrayCount != 0) {
        /*
         * If we're looking at an array of some sort, then it doesn't
         * matter if what is being asked for is a class name; the
         * format looks the same as a type descriptor in that case, so
         * treat it as such.
         */
        isClassName = false;
    }

    if (!isClassName) {
        /*
         * We are looking for a descriptor. Either validate it as a
         * single-character primitive type, or continue on to check the
         * embedded class name (bracketed by "L" and ";").
         */
        switch (*(s++)) {
            case 'B':
            case 'C':
            case 'D':
            case 'F':
            case 'I':
            case 'J':
            case 'S':
            case 'Z': {
                // These are all single-character descriptors for primitive types.
                return (*s == '\0');
            }
            case 'V': {
                // Non-array void is valid, but you can't have an array of void.
                return (arrayCount == 0) && (*s == '\0');
            }
            case 'L': {
                // Class name: Break out and continue below.
                break;
            }
            default: {
                // Oddball descriptor character.
                return false;
            }
        }
    }

    /*
     * We just consumed the 'L' that introduces a class name as part
     * of a type descriptor, or we are looking for an unadorned class
     * name.
     */

    bool sepOrFirst = true; // first character or just encountered a separator.
    for (;;) {
        u1 c = (u1) *s;
        switch (c) {
            case '\0': {
                /*
                 * Premature end for a type descriptor, but valid for
                 * a class name as long as we haven't encountered an
                 * empty component (including the degenerate case of
                 * the empty string "").
                 */
                return isClassName && !sepOrFirst;
            }
            case ';': {
                /*
                 * Invalid character for a class name, but the
                 * legitimate end of a type descriptor. In the latter
                 * case, make sure that this is the end of the string
                 * and that it doesn't end with an empty component
                 * (including the degenerate case of "L;").
                 */
                return !isClassName && !sepOrFirst && (s[1] == '\0');
            }
            case '/':
            case '.': {
                if (dotSeparator != (c == '.')) {
                    // The wrong separator character.
                    return false;
                }
                if (sepOrFirst) {
                    // Separator at start or two separators in a row.
                    return false;
                }
                sepOrFirst = true;
                s++;
                break;
            }
            default: {
                if (!dexIsValidMemberNameUtf8(&s)) {
                    return false;
                }
                sepOrFirst = false;
                break;
            }
        }
    }
}

/* Return whether the given string is a valid type descriptor. */
bool dexIsValidTypeDescriptor(const char* s) {
    return isValidTypeDescriptorOrClassName(s, false, false);
}

/* (documented in header) */
bool dexIsValidClassName(const char* s, bool dotSeparator) {
    return isValidTypeDescriptorOrClassName(s, true, dotSeparator);
}

/* Return whether the given string is a valid reference descriptor. This
 * is true if dexIsValidTypeDescriptor() returns true and the descriptor
 * is for a class or array and not a primitive type. */
bool dexIsReferenceDescriptor(const char* s) {
    if (!dexIsValidTypeDescriptor(s)) {
        return false;
    }

    return (s[0] == 'L') || (s[0] == '[');
}

/* Return whether the given string is a valid class descriptor. This
 * is true if dexIsValidTypeDescriptor() returns true and the descriptor
 * is for a class and not an array or primitive type. */
bool dexIsClassDescriptor(const char* s) {
    if (!dexIsValidTypeDescriptor(s)) {
        return false;
    }

    return s[0] == 'L';
}

/* Return whether the given string is a valid field type descriptor. This
 * is true if dexIsValidTypeDescriptor() returns true and the descriptor
 * is for anything but "void". */
bool dexIsFieldDescriptor(const char* s) {
    if (!dexIsValidTypeDescriptor(s)) {
        return false;
    }

    return s[0] != 'V';
}

