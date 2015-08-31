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

#include "jdwp/jdwp_expand_buf.h"

#include <stdlib.h>
#include <string.h>

#include "base/logging.h"
#include "jdwp/jdwp.h"
#include "jdwp/jdwp_bits.h"

namespace art {

namespace JDWP {

/*
 * Data structure used to track buffer use.
 */
struct ExpandBuf {
  uint8_t*     storage;
  int     curLen;
  int     maxLen;
};

#define kInitialStorage 64

/*
 * Allocate a JdwpBuf and some initial storage.
 */
ExpandBuf* expandBufAlloc() {
  ExpandBuf* newBuf = new ExpandBuf;
  newBuf->storage = reinterpret_cast<uint8_t*>(malloc(kInitialStorage));
  newBuf->curLen = 0;
  newBuf->maxLen = kInitialStorage;
  return newBuf;
}

/*
 * Free a JdwpBuf and associated storage.
 */
void expandBufFree(ExpandBuf* pBuf) {
  if (pBuf == NULL) {
    return;
  }

  free(pBuf->storage);
  delete pBuf;
}

/*
 * Get a pointer to the start of the buffer.
 */
uint8_t* expandBufGetBuffer(ExpandBuf* pBuf) {
  return pBuf->storage;
}

/*
 * Get the amount of data currently in the buffer.
 */
size_t expandBufGetLength(ExpandBuf* pBuf) {
  return pBuf->curLen;
}

/*
 * Ensure that the buffer has enough space to hold incoming data.  If it
 * doesn't, resize the buffer.
 */
static void ensureSpace(ExpandBuf* pBuf, int newCount) {
  if (pBuf->curLen + newCount <= pBuf->maxLen) {
    return;
  }

  while (pBuf->curLen + newCount > pBuf->maxLen) {
    pBuf->maxLen *= 2;
  }

  uint8_t* newPtr = reinterpret_cast<uint8_t*>(realloc(pBuf->storage, pBuf->maxLen));
  if (newPtr == NULL) {
    LOG(FATAL) << "realloc(" << pBuf->maxLen << ") failed";
  }

  pBuf->storage = newPtr;
}

/*
 * Allocate some space in the buffer.
 */
uint8_t* expandBufAddSpace(ExpandBuf* pBuf, int gapSize) {
  uint8_t* gapStart;

  ensureSpace(pBuf, gapSize);
  gapStart = pBuf->storage + pBuf->curLen;
  /* do we want to garbage-fill the gap for debugging? */
  pBuf->curLen += gapSize;

  return gapStart;
}

/*
 * Append a byte.
 */
void expandBufAdd1(ExpandBuf* pBuf, uint8_t val) {
  ensureSpace(pBuf, sizeof(val));
  *(pBuf->storage + pBuf->curLen) = val;
  pBuf->curLen++;
}

/*
 * Append two big-endian bytes.
 */
void expandBufAdd2BE(ExpandBuf* pBuf, uint16_t val) {
  ensureSpace(pBuf, sizeof(val));
  Set2BE(pBuf->storage + pBuf->curLen, val);
  pBuf->curLen += sizeof(val);
}

/*
 * Append four big-endian bytes.
 */
void expandBufAdd4BE(ExpandBuf* pBuf, uint32_t val) {
  ensureSpace(pBuf, sizeof(val));
  Set4BE(pBuf->storage + pBuf->curLen, val);
  pBuf->curLen += sizeof(val);
}

/*
 * Append eight big-endian bytes.
 */
void expandBufAdd8BE(ExpandBuf* pBuf, uint64_t val) {
  ensureSpace(pBuf, sizeof(val));
  Set8BE(pBuf->storage + pBuf->curLen, val);
  pBuf->curLen += sizeof(val);
}

static void SetUtf8String(uint8_t* buf, const char* str, size_t strLen) {
  Set4BE(buf, strLen);
  memcpy(buf + sizeof(uint32_t), str, strLen);
}

/*
 * Add a UTF8 string as a 4-byte length followed by a non-NULL-terminated
 * string.
 *
 * Because these strings are coming out of the VM, it's safe to assume that
 * they can be null-terminated (either they don't have null bytes or they
 * have stored null bytes in a multi-byte encoding).
 */
void expandBufAddUtf8String(ExpandBuf* pBuf, const char* s) {
  int strLen = strlen(s);
  ensureSpace(pBuf, sizeof(uint32_t) + strLen);
  SetUtf8String(pBuf->storage + pBuf->curLen, s, strLen);
  pBuf->curLen += sizeof(uint32_t) + strLen;
}

void expandBufAddUtf8String(ExpandBuf* pBuf, const std::string& s) {
  ensureSpace(pBuf, sizeof(uint32_t) + s.size());
  SetUtf8String(pBuf->storage + pBuf->curLen, s.data(), s.size());
  pBuf->curLen += sizeof(uint32_t) + s.size();
}

void expandBufAddLocation(ExpandBuf* buf, const JdwpLocation& location) {
  expandBufAdd1(buf, location.type_tag);
  expandBufAddObjectId(buf, location.class_id);
  expandBufAddMethodId(buf, location.method_id);
  expandBufAdd8BE(buf, location.dex_pc);
}

}  // namespace JDWP

}  // namespace art
