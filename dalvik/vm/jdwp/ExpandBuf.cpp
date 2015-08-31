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
 * Implementation of an expandable byte buffer.  Designed for serializing
 * primitive values, e.g. JDWP replies.
 */
#include "jdwp/ExpandBuf.h"
#include "Bits.h"
#include "Common.h"

#include <stdlib.h>
#include <string.h>

/*
 * Data structure used to track buffer use.
 */
struct ExpandBuf {
    u1*     storage;
    int     curLen;
    int     maxLen;
};

#define kInitialStorage 64

/*
 * Allocate a JdwpBuf and some initial storage.
 */
ExpandBuf* expandBufAlloc()
{
    ExpandBuf* newBuf;

    newBuf = (ExpandBuf*) malloc(sizeof(*newBuf));
    newBuf->storage = (u1*) malloc(kInitialStorage);
    newBuf->curLen = 0;
    newBuf->maxLen = kInitialStorage;

    return newBuf;
}

/*
 * Free a JdwpBuf and associated storage.
 */
void expandBufFree(ExpandBuf* pBuf)
{
    if (pBuf == NULL)
        return;

    free(pBuf->storage);
    free(pBuf);
}

/*
 * Get a pointer to the start of the buffer.
 */
u1* expandBufGetBuffer(ExpandBuf* pBuf)
{
    return pBuf->storage;
}

/*
 * Get the amount of data currently in the buffer.
 */
size_t expandBufGetLength(ExpandBuf* pBuf)
{
    return pBuf->curLen;
}


/*
 * Ensure that the buffer has enough space to hold incoming data.  If it
 * doesn't, resize the buffer.
 */
static void ensureSpace(ExpandBuf* pBuf, int newCount)
{
    u1* newPtr;

    if (pBuf->curLen + newCount <= pBuf->maxLen)
        return;

    while (pBuf->curLen + newCount > pBuf->maxLen)
        pBuf->maxLen *= 2;

    newPtr = (u1*) realloc(pBuf->storage, pBuf->maxLen);
    if (newPtr == NULL) {
        ALOGE("realloc(%d) failed", pBuf->maxLen);
        abort();
    }

    pBuf->storage = newPtr;
}

/*
 * Allocate some space in the buffer.
 */
u1* expandBufAddSpace(ExpandBuf* pBuf, int gapSize)
{
    u1* gapStart;

    ensureSpace(pBuf, gapSize);
    gapStart = pBuf->storage + pBuf->curLen;
    /* do we want to garbage-fill the gap for debugging? */
    pBuf->curLen += gapSize;

    return gapStart;
}

/*
 * Append a byte.
 */
void expandBufAdd1(ExpandBuf* pBuf, u1 val)
{
    ensureSpace(pBuf, sizeof(val));
    *(pBuf->storage + pBuf->curLen) = val;
    pBuf->curLen++;
}

/*
 * Append two big-endian bytes.
 */
void expandBufAdd2BE(ExpandBuf* pBuf, u2 val)
{
    ensureSpace(pBuf, sizeof(val));
    set2BE(pBuf->storage + pBuf->curLen, val);
    pBuf->curLen += sizeof(val);
}

/*
 * Append four big-endian bytes.
 */
void expandBufAdd4BE(ExpandBuf* pBuf, u4 val)
{
    ensureSpace(pBuf, sizeof(val));
    set4BE(pBuf->storage + pBuf->curLen, val);
    pBuf->curLen += sizeof(val);
}

/*
 * Append eight big-endian bytes.
 */
void expandBufAdd8BE(ExpandBuf* pBuf, u8 val)
{
    ensureSpace(pBuf, sizeof(val));
    set8BE(pBuf->storage + pBuf->curLen, val);
    pBuf->curLen += sizeof(val);
}

/*
 * Add a UTF8 string as a 4-byte length followed by a non-NULL-terminated
 * string.
 *
 * Because these strings are coming out of the VM, it's safe to assume that
 * they can be null-terminated (either they don't have null bytes or they
 * have stored null bytes in a multi-byte encoding).
 */
void expandBufAddUtf8String(ExpandBuf* pBuf, const u1* str)
{
    int strLen = strlen((const char*)str);

    ensureSpace(pBuf, sizeof(u4) + strLen);
    setUtf8String(pBuf->storage + pBuf->curLen, str);
    pBuf->curLen += sizeof(u4) + strLen;
}
