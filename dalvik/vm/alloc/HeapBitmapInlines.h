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

#ifndef DALVIK_HEAP_BITMAPINLINES_H_
#define DALVIK_HEAP_BITMAPINLINES_H_

static unsigned long dvmHeapBitmapSetAndReturnObjectBit(HeapBitmap *hb, const void *obj) __attribute__((used));
static void dvmHeapBitmapSetObjectBit(HeapBitmap *hb, const void *obj) __attribute__((used));
static void dvmHeapBitmapClearObjectBit(HeapBitmap *hb, const void *obj) __attribute__((used));

/*
 * Internal function; do not call directly.
 */
static unsigned long _heapBitmapModifyObjectBit(HeapBitmap *hb, const void *obj,
                                                bool setBit, bool returnOld)
{
    const uintptr_t offset = (uintptr_t)obj - hb->base;
    const size_t index = HB_OFFSET_TO_INDEX(offset);
    const unsigned long mask = HB_OFFSET_TO_MASK(offset);

    assert(hb->bits != NULL);
    assert((uintptr_t)obj >= hb->base);
    assert(index < hb->bitsLen / sizeof(*hb->bits));
    if (setBit) {
        if ((uintptr_t)obj > hb->max) {
            hb->max = (uintptr_t)obj;
        }
        if (returnOld) {
            unsigned long *p = hb->bits + index;
            const unsigned long word = *p;
            *p |= mask;
            return word & mask;
        } else {
            hb->bits[index] |= mask;
        }
    } else {
        hb->bits[index] &= ~mask;
    }
    return false;
}

/*
 * Sets the bit corresponding to <obj>, and returns the previous value
 * of that bit (as zero or non-zero). Does no range checking to see if
 * <obj> is outside of the coverage of the bitmap.
 *
 * NOTE: casting this value to a bool is dangerous, because higher
 * set bits will be lost.
 */
static unsigned long dvmHeapBitmapSetAndReturnObjectBit(HeapBitmap *hb,
                                                        const void *obj)
{
    return _heapBitmapModifyObjectBit(hb, obj, true, true);
}

/*
 * Sets the bit corresponding to <obj>, and widens the range of seen
 * pointers if necessary.  Does no range checking.
 */
static void dvmHeapBitmapSetObjectBit(HeapBitmap *hb, const void *obj)
{
    _heapBitmapModifyObjectBit(hb, obj, true, false);
}

/*
 * Clears the bit corresponding to <obj>.  Does no range checking.
 */
static void dvmHeapBitmapClearObjectBit(HeapBitmap *hb, const void *obj)
{
    _heapBitmapModifyObjectBit(hb, obj, false, false);
}

/*
 * Returns the current value of the bit corresponding to <obj>,
 * as zero or non-zero.  Does no range checking.
 *
 * NOTE: casting this value to a bool is dangerous, because higher
 * set bits will be lost.
 */
static unsigned long dvmHeapBitmapIsObjectBitSet(const HeapBitmap *hb,
                                                 const void *obj)
{
    assert(dvmHeapBitmapCoversAddress(hb, obj));
    assert(hb->bits != NULL);
    assert((uintptr_t)obj >= hb->base);
    if ((uintptr_t)obj <= hb->max) {
        const uintptr_t offset = (uintptr_t)obj - hb->base;
        return hb->bits[HB_OFFSET_TO_INDEX(offset)] & HB_OFFSET_TO_MASK(offset);
    } else {
        return 0;
    }
}

#endif  // DALVIK_HEAP_BITMAPINLINES_H_
