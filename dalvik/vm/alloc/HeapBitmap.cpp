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

#include "Dalvik.h"
#include "HeapBitmap.h"
#include <sys/mman.h>   /* for PROT_* */

/*
 * Initialize a HeapBitmap so that it points to a bitmap large
 * enough to cover a heap at <base> of <maxSize> bytes, where
 * objects are guaranteed to be HB_OBJECT_ALIGNMENT-aligned.
 */
bool dvmHeapBitmapInit(HeapBitmap *hb, const void *base, size_t maxSize,
                       const char *name)
{
    void *bits;
    size_t bitsLen;

    assert(hb != NULL);
    assert(name != NULL);
    bitsLen = HB_OFFSET_TO_INDEX(maxSize) * sizeof(*hb->bits);
    bits = dvmAllocRegion(bitsLen, PROT_READ | PROT_WRITE, name);
    if (bits == NULL) {
        ALOGE("Could not mmap %zd-byte ashmem region '%s'", bitsLen, name);
        return false;
    }
    hb->bits = (unsigned long *)bits;
    hb->bitsLen = hb->allocLen = bitsLen;
    hb->base = (uintptr_t)base;
    hb->max = hb->base - 1;
    return true;
}

/*
 * Clean up any resources associated with the bitmap.
 */
void dvmHeapBitmapDelete(HeapBitmap *hb)
{
    assert(hb != NULL);

    if (hb->bits != NULL) {
        munmap((char *)hb->bits, hb->allocLen);
    }
    memset(hb, 0, sizeof(*hb));
}

/*
 * Fill the bitmap with zeroes.  Returns the bitmap's memory to
 * the system as a side-effect.
 */
void dvmHeapBitmapZero(HeapBitmap *hb)
{
    assert(hb != NULL);

    if (hb->bits != NULL) {
        /* This returns the memory to the system.
         * Successive page faults will return zeroed memory.
         */
        madvise(hb->bits, hb->bitsLen, MADV_DONTNEED);
        hb->max = hb->base - 1;
    }
}

/*
 * Return true iff <obj> is within the range of pointers that this
 * bitmap could potentially cover, even if a bit has not been set
 * for it.
 */
bool dvmHeapBitmapCoversAddress(const HeapBitmap *hb, const void *obj)
{
    assert(hb != NULL);
    if (obj != NULL) {
        const uintptr_t offset = (uintptr_t)obj - hb->base;
        const size_t index = HB_OFFSET_TO_INDEX(offset);
        return index < hb->bitsLen / sizeof(*hb->bits);
    }
    return false;
}

/*
 * Visits set bits in address order.  The callback is not permitted to
 * change the bitmap bits or max during the traversal.
 */
void dvmHeapBitmapWalk(const HeapBitmap *bitmap, BitmapCallback *callback,
                       void *arg)
{
    assert(bitmap != NULL);
    assert(bitmap->bits != NULL);
    assert(callback != NULL);
    uintptr_t end = HB_OFFSET_TO_INDEX(bitmap->max - bitmap->base);
    for (uintptr_t i = 0; i <= end; ++i) {
        unsigned long word = bitmap->bits[i];
        if (UNLIKELY(word != 0)) {
            unsigned long highBit = 1 << (HB_BITS_PER_WORD - 1);
            uintptr_t ptrBase = HB_INDEX_TO_OFFSET(i) + bitmap->base;
            while (word != 0) {
                const int shift = CLZ(word);
                Object* obj = (Object *)(ptrBase + shift * HB_OBJECT_ALIGNMENT);
                (*callback)(obj, arg);
                word &= ~(highBit >> shift);
            }
        }
    }
}

/*
 * Similar to dvmHeapBitmapWalk but the callback routine is permitted
 * to change the bitmap bits and max during traversal.  Used by the
 * the root marking scan exclusively.
 *
 * The callback is invoked with a finger argument.  The finger is a
 * pointer to an address not yet visited by the traversal.  If the
 * callback sets a bit for an address at or above the finger, this
 * address will be visited by the traversal.  If the callback sets a
 * bit for an address below the finger, this address will not be
 * visited.
 */
void dvmHeapBitmapScanWalk(HeapBitmap *bitmap,
                           BitmapScanCallback *callback, void *arg)
{
    assert(bitmap != NULL);
    assert(bitmap->bits != NULL);
    assert(callback != NULL);
    uintptr_t end = HB_OFFSET_TO_INDEX(bitmap->max - bitmap->base);
    uintptr_t i;
    for (i = 0; i <= end; ++i) {
        unsigned long word = bitmap->bits[i];
        if (UNLIKELY(word != 0)) {
            unsigned long highBit = 1 << (HB_BITS_PER_WORD - 1);
            uintptr_t ptrBase = HB_INDEX_TO_OFFSET(i) + bitmap->base;
            void *finger = (void *)(HB_INDEX_TO_OFFSET(i + 1) + bitmap->base);
            while (word != 0) {
                const int shift = CLZ(word);
                Object *obj = (Object *)(ptrBase + shift * HB_OBJECT_ALIGNMENT);
                (*callback)(obj, finger, arg);
                word &= ~(highBit >> shift);
            }
            end = HB_OFFSET_TO_INDEX(bitmap->max - bitmap->base);
        }
    }
}

/*
 * Walk through the bitmaps in increasing address order, and find the
 * object pointers that correspond to garbage objects.  Call
 * <callback> zero or more times with lists of these object pointers.
 *
 * The callback is not permitted to increase the max of either bitmap.
 */
void dvmHeapBitmapSweepWalk(const HeapBitmap *liveHb, const HeapBitmap *markHb,
                            uintptr_t base, uintptr_t max,
                            BitmapSweepCallback *callback, void *callbackArg)
{
    assert(liveHb != NULL);
    assert(liveHb->bits != NULL);
    assert(markHb != NULL);
    assert(markHb->bits != NULL);
    assert(liveHb->base == markHb->base);
    assert(liveHb->bitsLen == markHb->bitsLen);
    assert(callback != NULL);
    assert(base <= max);
    assert(base >= liveHb->base);
    assert(max <= liveHb->max);
    if (liveHb->max < liveHb->base) {
        /* Easy case; both are obviously empty.
         */
        return;
    }
    void *pointerBuf[4 * HB_BITS_PER_WORD];
    void **pb = pointerBuf;
    size_t start = HB_OFFSET_TO_INDEX(base - liveHb->base);
    size_t end = HB_OFFSET_TO_INDEX(max - liveHb->base);
    unsigned long *live = liveHb->bits;
    unsigned long *mark = markHb->bits;
    for (size_t i = start; i <= end; i++) {
        unsigned long garbage = live[i] & ~mark[i];
        if (UNLIKELY(garbage != 0)) {
            unsigned long highBit = 1 << (HB_BITS_PER_WORD - 1);
            uintptr_t ptrBase = HB_INDEX_TO_OFFSET(i) + liveHb->base;
            while (garbage != 0) {
                int shift = CLZ(garbage);
                garbage &= ~(highBit >> shift);
                *pb++ = (void *)(ptrBase + shift * HB_OBJECT_ALIGNMENT);
            }
            /* Make sure that there are always enough slots available */
            /* for an entire word of 1s. */
            if (pb >= &pointerBuf[NELEM(pointerBuf) - HB_BITS_PER_WORD]) {
                (*callback)(pb - pointerBuf, pointerBuf, callbackArg);
                pb = pointerBuf;
            }
        }
    }
    if (pb > pointerBuf) {
        (*callback)(pb - pointerBuf, pointerBuf, callbackArg);
    }
}
