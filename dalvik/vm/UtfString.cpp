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
 * UTF-8 and Unicode string manipulation, plus java/lang/String convenience
 * functions.
 *
 * In most cases we populate the fields in the String object directly,
 * rather than going through an instance field lookup.
 */
#include "Dalvik.h"
#include <stdlib.h>

/*
 * Allocate a new instance of the class String, performing first-use
 * initialization of the class if necessary. Upon success, the
 * returned value will have all its fields except hashCode already
 * filled in, including a reference to a newly-allocated char[] for
 * the contents, sized as given. Additionally, a reference to the
 * chars array is stored to the pChars pointer. Callers must
 * subsequently call dvmReleaseTrackedAlloc() on the result pointer.
 * This function returns NULL on failure.
 */
static StringObject* makeStringObject(u4 charsLength, ArrayObject** pChars)
{
    /*
     * The String class should have already gotten found (but not
     * necessarily initialized) before making it here. We assert it
     * explicitly, since historically speaking, we have had bugs with
     * regard to when the class String gets set up. The assert helps
     * make any regressions easier to diagnose.
     */
    assert(gDvm.classJavaLangString != NULL);

    if (!dvmIsClassInitialized(gDvm.classJavaLangString)) {
        /* Perform first-time use initialization of the class. */
        if (!dvmInitClass(gDvm.classJavaLangString)) {
            ALOGE("FATAL: Could not initialize class String");
            dvmAbort();
        }
    }

    Object* result = dvmAllocObject(gDvm.classJavaLangString, ALLOC_DEFAULT);
    if (result == NULL) {
        return NULL;
    }

    ArrayObject* chars = dvmAllocPrimitiveArray('C', charsLength, ALLOC_DEFAULT);
    if (chars == NULL) {
        dvmReleaseTrackedAlloc(result, NULL);
        return NULL;
    }

    dvmSetFieldInt(result, STRING_FIELDOFF_COUNT, charsLength);
    dvmSetFieldObject(result, STRING_FIELDOFF_VALUE, (Object*) chars);
    dvmReleaseTrackedAlloc((Object*) chars, NULL);
    /* Leave offset and hashCode set to zero. */

    *pChars = chars;
    return (StringObject*) result;
}

/*
 * Compute a hash code on a UTF-8 string, for use with internal hash tables.
 *
 * This may or may not yield the same results as the java/lang/String
 * computeHashCode() function.  (To make sure this doesn't get abused,
 * I'm initializing the hash code to 1 so they *don't* match up.)
 *
 * It would be more correct to invoke dexGetUtf16FromUtf8() here and compute
 * the hash with the result.  That way, if something encoded the same
 * character in two different ways, the hash value would be the same.  For
 * our purposes that isn't necessary.
 */
u4 dvmComputeUtf8Hash(const char* utf8Str)
{
    u4 hash = 1;

    while (*utf8Str != '\0')
        hash = hash * 31 + *utf8Str++;

    return hash;
}

/*
 * Like "strlen", but for strings encoded with "modified" UTF-8.
 *
 * The value returned is the number of characters, which may or may not
 * be the same as the number of bytes.
 *
 * (If this needs optimizing, try: mask against 0xa0, shift right 5,
 * get increment {1-3} from table of 8 values.)
 */
size_t dvmUtf8Len(const char* utf8Str)
{
    size_t len = 0;
    int ic;

    while ((ic = *utf8Str++) != '\0') {
        len++;
        if ((ic & 0x80) != 0) {
            /* two- or three-byte encoding */
            utf8Str++;
            if ((ic & 0x20) != 0) {
                /* three-byte encoding */
                utf8Str++;
            }
        }
    }

    return len;
}

/*
 * Convert a "modified" UTF-8 string to UTF-16.
 */
void dvmConvertUtf8ToUtf16(u2* utf16Str, const char* utf8Str)
{
    while (*utf8Str != '\0')
        *utf16Str++ = dexGetUtf16FromUtf8(&utf8Str);
}

/*
 * Given a UTF-16 string, compute the length of the corresponding UTF-8
 * string in bytes.
 */
static int utf16_utf8ByteLen(const u2* utf16Str, int len)
{
    int utf8Len = 0;

    while (len--) {
        unsigned int uic = *utf16Str++;

        /*
         * The most common case is (uic > 0 && uic <= 0x7f).
         */
        if (uic == 0 || uic > 0x7f) {
            if (uic > 0x07ff)
                utf8Len += 3;
            else /*(uic > 0x7f || uic == 0) */
                utf8Len += 2;
        } else
            utf8Len++;
    }
    return utf8Len;
}

/*
 * Convert a UTF-16 string to UTF-8.
 *
 * Make sure you allocate "utf8Str" with the result of utf16_utf8ByteLen(),
 * not just "len".
 */
static void convertUtf16ToUtf8(char* utf8Str, const u2* utf16Str, int len)
{
    assert(len >= 0);

    while (len--) {
        unsigned int uic = *utf16Str++;

        /*
         * The most common case is (uic > 0 && uic <= 0x7f).
         */
        if (uic == 0 || uic > 0x7f) {
            if (uic > 0x07ff) {
                *utf8Str++ = (uic >> 12) | 0xe0;
                *utf8Str++ = ((uic >> 6) & 0x3f) | 0x80;
                *utf8Str++ = (uic & 0x3f) | 0x80;
            } else /*(uic > 0x7f || uic == 0)*/ {
                *utf8Str++ = (uic >> 6) | 0xc0;
                *utf8Str++ = (uic & 0x3f) | 0x80;
            }
        } else {
            *utf8Str++ = uic;
        }
    }

    *utf8Str = '\0';
}

/*
 * Use the java/lang/String.computeHashCode() algorithm.
 */
static inline u4 computeUtf16Hash(const u2* utf16Str, size_t len)
{
    u4 hash = 0;

    while (len--)
        hash = hash * 31 + *utf16Str++;

    return hash;
}

u4 dvmComputeStringHash(StringObject* strObj) {
    int hashCode = dvmGetFieldInt(strObj, STRING_FIELDOFF_HASHCODE);
    if (hashCode != 0) {
      return hashCode;
    }
    int len = dvmGetFieldInt(strObj, STRING_FIELDOFF_COUNT);
    int offset = dvmGetFieldInt(strObj, STRING_FIELDOFF_OFFSET);
    ArrayObject* chars =
            (ArrayObject*) dvmGetFieldObject(strObj, STRING_FIELDOFF_VALUE);
    hashCode = computeUtf16Hash((u2*)(void*)chars->contents + offset, len);
    dvmSetFieldInt(strObj, STRING_FIELDOFF_HASHCODE, hashCode);
    return hashCode;
}

StringObject* dvmCreateStringFromCstr(const char* utf8Str) {
    assert(utf8Str != NULL);
    return dvmCreateStringFromCstrAndLength(utf8Str, dvmUtf8Len(utf8Str));
}

StringObject* dvmCreateStringFromCstr(const std::string& utf8Str) {
    return dvmCreateStringFromCstr(utf8Str.c_str());
}

/*
 * Create a java/lang/String from a C string, given its UTF-16 length
 * (number of UTF-16 code points).
 *
 * The caller must call dvmReleaseTrackedAlloc() on the return value.
 *
 * Returns NULL and throws an exception on failure.
 */
StringObject* dvmCreateStringFromCstrAndLength(const char* utf8Str,
    size_t utf16Length)
{
    assert(utf8Str != NULL);

    ArrayObject* chars;
    StringObject* newObj = makeStringObject(utf16Length, &chars);
    if (newObj == NULL) {
        return NULL;
    }

    dvmConvertUtf8ToUtf16((u2*)(void*)chars->contents, utf8Str);

    u4 hashCode = computeUtf16Hash((u2*)(void*)chars->contents, utf16Length);
    dvmSetFieldInt((Object*) newObj, STRING_FIELDOFF_HASHCODE, hashCode);

    return newObj;
}

/*
 * Create a new java/lang/String object, using the given Unicode data.
 */
StringObject* dvmCreateStringFromUnicode(const u2* unichars, int len)
{
    /* We allow a NULL pointer if the length is zero. */
    assert(len == 0 || unichars != NULL);

    ArrayObject* chars;
    StringObject* newObj = makeStringObject(len, &chars);
    if (newObj == NULL) {
        return NULL;
    }

    if (len > 0) memcpy(chars->contents, unichars, len * sizeof(u2));

    u4 hashCode = computeUtf16Hash((u2*)(void*)chars->contents, len);
    dvmSetFieldInt((Object*)newObj, STRING_FIELDOFF_HASHCODE, hashCode);

    return newObj;
}

/*
 * Create a new C string from a java/lang/String object.
 *
 * Returns NULL if the object is NULL.
 */
char* dvmCreateCstrFromString(const StringObject* jstr)
{
    assert(gDvm.classJavaLangString != NULL);
    if (jstr == NULL) {
        return NULL;
    }

    int len = dvmGetFieldInt(jstr, STRING_FIELDOFF_COUNT);
    int offset = dvmGetFieldInt(jstr, STRING_FIELDOFF_OFFSET);
    ArrayObject* chars =
            (ArrayObject*) dvmGetFieldObject(jstr, STRING_FIELDOFF_VALUE);
    const u2* data = (const u2*)(void*)chars->contents + offset;
    assert(offset + len <= (int) chars->length);

    int byteLen = utf16_utf8ByteLen(data, len);
    char* newStr = (char*) malloc(byteLen+1);
    if (newStr == NULL) {
        return NULL;
    }
    convertUtf16ToUtf8(newStr, data, len);

    return newStr;
}

void dvmGetStringUtfRegion(const StringObject* jstr,
        int start, int len, char* buf)
{
    const u2* data = jstr->chars() + start;
    convertUtf16ToUtf8(buf, data, len);
}

int StringObject::utfLength() const
{
    assert(gDvm.classJavaLangString != NULL);

    int len = dvmGetFieldInt(this, STRING_FIELDOFF_COUNT);
    int offset = dvmGetFieldInt(this, STRING_FIELDOFF_OFFSET);
    ArrayObject* chars =
            (ArrayObject*) dvmGetFieldObject(this, STRING_FIELDOFF_VALUE);
    const u2* data = (const u2*)(void*)chars->contents + offset;
    assert(offset + len <= (int) chars->length);

    return utf16_utf8ByteLen(data, len);
}

int StringObject::length() const
{
    return dvmGetFieldInt(this, STRING_FIELDOFF_COUNT);
}

ArrayObject* StringObject::array() const
{
    return (ArrayObject*) dvmGetFieldObject(this, STRING_FIELDOFF_VALUE);
}

const u2* StringObject::chars() const
{
    int offset = dvmGetFieldInt(this, STRING_FIELDOFF_OFFSET);
    ArrayObject* chars =
            (ArrayObject*) dvmGetFieldObject(this, STRING_FIELDOFF_VALUE);
    return (const u2*)(void*)chars->contents + offset;
}


/*
 * Compare two String objects.
 *
 * This is a dvmHashTableLookup() callback.  The function has already
 * compared their hash values; we need to do a full compare to ensure
 * that the strings really match.
 */
int dvmHashcmpStrings(const void* vstrObj1, const void* vstrObj2)
{
    const StringObject* strObj1 = (const StringObject*) vstrObj1;
    const StringObject* strObj2 = (const StringObject*) vstrObj2;

    assert(gDvm.classJavaLangString != NULL);

    /* get offset and length into char array; all values are in 16-bit units */
    int len1 = dvmGetFieldInt(strObj1, STRING_FIELDOFF_COUNT);
    int offset1 = dvmGetFieldInt(strObj1, STRING_FIELDOFF_OFFSET);
    int len2 = dvmGetFieldInt(strObj2, STRING_FIELDOFF_COUNT);
    int offset2 = dvmGetFieldInt(strObj2, STRING_FIELDOFF_OFFSET);
    if (len1 != len2) {
        return len1 - len2;
    }

    ArrayObject* chars1 =
            (ArrayObject*) dvmGetFieldObject(strObj1, STRING_FIELDOFF_VALUE);
    ArrayObject* chars2 =
            (ArrayObject*) dvmGetFieldObject(strObj2, STRING_FIELDOFF_VALUE);

    /* damage here actually indicates a broken java/lang/String */
    assert(offset1 + len1 <= (int) chars1->length);
    assert(offset2 + len2 <= (int) chars2->length);

    return memcmp((const u2*)(void*)chars1->contents + offset1,
                  (const u2*)(void*)chars2->contents + offset2,
                  len1 * sizeof(u2));
}

ArrayObject* dvmCreateStringArray(const std::vector<std::string>& strings) {
    Thread* self = dvmThreadSelf();

    // Allocate an array to hold the String objects.
    ClassObject* elementClass = dvmFindArrayClassForElement(gDvm.classJavaLangString);
    ArrayObject* stringArray = dvmAllocArrayByClass(elementClass, strings.size(), ALLOC_DEFAULT);
    if (stringArray == NULL) {
        // Probably OOM.
        assert(dvmCheckException(self));
        return NULL;
    }

    // Create the individual String objects and add them to the array.
    for (size_t i = 0; i < strings.size(); i++) {
        Object* str = (Object*) dvmCreateStringFromCstr(strings[i]);
        if (str == NULL) {
            // Probably OOM; drop out now.
            assert(dvmCheckException(self));
            dvmReleaseTrackedAlloc((Object*) stringArray, self);
            return NULL;
        }
        dvmSetObjectArrayElement(stringArray, i, str);
        /* stored in tracked array, okay to release */
        dvmReleaseTrackedAlloc(str, self);
    }

    return stringArray;
}
