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
#ifndef DALVIK_HEAP_BITMAP_H_
#define DALVIK_HEAP_BITMAP_H_

#include <limits.h>
#include <stdint.h>

#define HB_OBJECT_ALIGNMENT 8
#define HB_BITS_PER_WORD (sizeof(unsigned long) * CHAR_BIT)

/* <offset> is the difference from .base to a pointer address.
 * <index> is the index of .bits that contains the bit representing
 *         <offset>.
 */
#define HB_OFFSET_TO_INDEX(offset_) \
    ((uintptr_t)(offset_) / HB_OBJECT_ALIGNMENT / HB_BITS_PER_WORD)
#define HB_INDEX_TO_OFFSET(index_) \
    ((uintptr_t)(index_) * HB_OBJECT_ALIGNMENT * HB_BITS_PER_WORD)

#define HB_OFFSET_TO_BYTE_INDEX(offset_) \
  (HB_OFFSET_TO_INDEX(offset_) * sizeof(*((HeapBitmap *)0)->bits))

/* Pack the bits in backwards so they come out in address order
 * when using CLZ.
 */
#define HB_OFFSET_TO_MASK(offset_) \
    (1 << \
        (31-(((uintptr_t)(offset_) / HB_OBJECT_ALIGNMENT) % HB_BITS_PER_WORD)))

struct HeapBitmap {
    /* The bitmap data, which points to an mmap()ed area of zeroed
     * anonymous memory.
     */
    unsigned long *bits;

    /* The size of the used memory pointed to by bits, in bytes.  This
     * value changes when the bitmap is shrunk.
     */
    size_t bitsLen;

    /* The real size of the memory pointed to by bits.  This is the
     * number of bytes we requested from the allocator and does not
     * change.
     */
    size_t allocLen;

    /* The base address, which corresponds to the first bit in
     * the bitmap.
     */
    uintptr_t base;

    /* The highest pointer value ever returned by an allocation
     * from this heap.  I.e., the highest address that may correspond
     * to a set bit.  If there are no bits set, (max < base).
     */
    uintptr_t max;
};

/*
 * Callback types used by the walking routines.
 */
typedef void BitmapCallback(Object *obj, void *arg);
typedef void BitmapScanCallback(Object *obj, void *finger, void *arg);
typedef void BitmapSweepCallback(size_t numPtrs, void **ptrs, void *arg);

/*
 * Initialize a HeapBitmap so that it points to a bitmap large
 * enough to cover a heap at <base> of <maxSize> bytes, where
 * objects are guaranteed to be HB_OBJECT_ALIGNMENT-aligned.
 */
bool dvmHeapBitmapInit(HeapBitmap *hb, const void *base, size_t maxSize,
        const char *name);

/*
 * Clean up any resources associated with the bitmap.
 */
void dvmHeapBitmapDelete(HeapBitmap *hb);

/*
 * Fill the bitmap with zeroes.  Returns the bitmap's memory to
 * the system as a side-effect.
 */
void dvmHeapBitmapZero(HeapBitmap *hb);

/*
 * Returns true if the address range of the bitmap covers the object
 * address.
 */
bool dvmHeapBitmapCoversAddress(const HeapBitmap *hb, const void *obj);

/*
 * Applies the callback function to each set address in the bitmap.
 */
void dvmHeapBitmapWalk(const HeapBitmap *bitmap,
                       BitmapCallback *callback, void *callbackArg);

/*
 * Like dvmHeapBitmapWalk but takes a callback function with a finger
 * address.
 */
void dvmHeapBitmapScanWalk(HeapBitmap *bitmap,
                           BitmapScanCallback *callback, void *arg);

/*
 * Walk through the bitmaps in increasing address order, and find the
 * object pointers that correspond to garbage objects.  Call
 * <callback> zero or more times with lists of these object pointers.
 *
 * The callback is not permitted to increase the max of either bitmap.
 */
void dvmHeapBitmapSweepWalk(const HeapBitmap *liveHb, const HeapBitmap *markHb,
                            uintptr_t base, uintptr_t max,
                            BitmapSweepCallback *callback, void *callbackArg);

#endif  // DALVIK_HEAP_BITMAP_H_
