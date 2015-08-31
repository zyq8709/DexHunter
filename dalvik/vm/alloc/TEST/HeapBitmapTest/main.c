#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#define __attribute(x) /* disable inlining */
#include "HeapBitmap.h"
#undef __attribute

#define PAGE_SIZE 4096
#define HEAP_BASE ((void *)0x10000)
#define HEAP_SIZE (5 * PAGE_SIZE + 888)

#define VERBOSE 1
#if VERBOSE
#define TRACE(...) printf(__VA_ARGS__)
#else
#define TRACE(...) /**/
#endif

void
test_init()
{
    HeapBitmap hb;
    bool ok;

    memset(&hb, 0x55, sizeof(hb));

    ok = dvmHeapBitmapInit(&hb, HEAP_BASE, HEAP_SIZE, "test");
    assert(ok);

    assert(hb.bits != NULL);
    assert(hb.bitsLen >= HB_OFFSET_TO_INDEX(HEAP_SIZE));
    assert(hb.base == (uintptr_t)HEAP_BASE);
    assert(hb.max < hb.base);

    /* Make sure hb.bits is mapped.
     */
    *hb.bits = 0x55;
    assert(*hb.bits = 0x55);
    *hb.bits = 0;

#define TEST_UNMAP 0
#if TEST_UNMAP
    /* Hold onto this to make sure it's unmapped later.
     */
    unsigned long int *bits = hb.bits;
#endif

    dvmHeapBitmapDelete(&hb);

    assert(hb.bits == NULL);
    assert(hb.bitsLen == 0);
    assert(hb.base == 0);
    assert(hb.max == 0);

#if TEST_UNMAP
    /* This pointer shouldn't be mapped anymore.
     */
    *bits = 0x55;
    assert(!"Should have segfaulted");
#endif
}

bool is_zeroed(const HeapBitmap *hb)
{
    int i;

    for (i = 0; i < hb->bitsLen / sizeof (*hb->bits); i++) {
        if (hb->bits[i] != 0L) {
            return false;
        }
    }
    return true;
}

void assert_empty(const HeapBitmap *hb)
{
    assert(hb->bits != NULL);
    assert(hb->bitsLen >= HB_OFFSET_TO_INDEX(HEAP_SIZE));
    assert(hb->base == (uintptr_t)HEAP_BASE);
    assert(hb->max < hb->base);

    assert(is_zeroed(hb));

    assert(!dvmHeapBitmapMayContainObject(hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapMayContainObject(hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapMayContainObject(hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapMayContainObject(hb,
            HEAP_BASE + HEAP_SIZE));

    assert(!dvmHeapBitmapIsObjectBitSet(hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapIsObjectBitSet(hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapIsObjectBitSet(hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));
}

void
test_bits()
{
    HeapBitmap hb;
    bool ok;

    ok = dvmHeapBitmapInit(&hb, HEAP_BASE, HEAP_SIZE, "test");
    assert(ok);

    assert_empty(&hb);

    /* Set the lowest address.
     */
    dvmHeapBitmapSetObjectBit(&hb, HEAP_BASE);
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE));

    assert(dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));

    /* Set the highest address.
     */
    dvmHeapBitmapSetObjectBit(&hb, HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT);
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE));
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE));

    assert(dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));

    /* Clear the lowest address.
     */
    dvmHeapBitmapClearObjectBit(&hb, HEAP_BASE);
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));
    assert(!is_zeroed(&hb));

    /* Clear the highest address.
     */
    dvmHeapBitmapClearObjectBit(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT);
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));
    assert(is_zeroed(&hb));

    /* Clean up.
     */
    dvmHeapBitmapDelete(&hb);
}

void
test_clear()
{
    HeapBitmap hb;
    bool ok;

    ok = dvmHeapBitmapInit(&hb, HEAP_BASE, HEAP_SIZE, "test");
    assert(ok);
    assert_empty(&hb);

    /* Set the highest address.
     */
    dvmHeapBitmapSetObjectBit(&hb, HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT);
    assert(!is_zeroed(&hb));

    /* Clear the bitmap.
     */
    dvmHeapBitmapZero(&hb);
    assert_empty(&hb);

    /* Clean up.
     */
    dvmHeapBitmapDelete(&hb);
}

void
test_modify()
{
    HeapBitmap hb;
    bool ok;
    unsigned long bit;

    ok = dvmHeapBitmapInit(&hb, HEAP_BASE, HEAP_SIZE, "test");
    assert(ok);
    assert_empty(&hb);

    /* Set the lowest address.
     */
    bit = dvmHeapBitmapSetAndReturnObjectBit(&hb, HEAP_BASE);
    assert(bit == 0);
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE));

    assert(dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));

    /* Set the lowest address again.
     */
    bit = dvmHeapBitmapSetAndReturnObjectBit(&hb, HEAP_BASE);
    assert(bit != 0);
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE));

    assert(dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));

    /* Set the highest address.
     */
    bit = dvmHeapBitmapSetAndReturnObjectBit(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT);
    assert(bit == 0);
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE));
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE));

    assert(dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));

    /* Set the highest address again.
     */
    bit = dvmHeapBitmapSetAndReturnObjectBit(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT);
    assert(bit != 0);
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE));
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));
    assert(!dvmHeapBitmapMayContainObject(&hb,
            HEAP_BASE + HEAP_SIZE));

    assert(dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE));
    assert(!dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HB_OBJECT_ALIGNMENT));
    assert(dvmHeapBitmapIsObjectBitSet(&hb,
            HEAP_BASE + HEAP_SIZE - HB_OBJECT_ALIGNMENT));

    /* Clean up.
     */
    dvmHeapBitmapDelete(&hb);
}

/*
 * xor test support functions
 */

static void *gCallbackArg = NULL;

#define NUM_XOR_PTRS  128
static size_t gNumPtrs;
static void *gXorPtrs[NUM_XOR_PTRS];
static bool gClearedPtrs[NUM_XOR_PTRS];
static bool gSeenPtrs[NUM_XOR_PTRS];

bool
xorCallback(size_t numPtrs, void **ptrs, const void *finger, void *arg)
{
    assert(numPtrs > 0);
    assert(ptrs != NULL);
    assert(arg == gCallbackArg);

size_t i;
    for (i = 0; i < numPtrs; i++) {
        assert(ptrs[i] < finger);
        printf("callback: 0x%08x ( < 0x%08x )\n",
                (uintptr_t)ptrs[i], (uintptr_t)finger);
    }

    return true;
}

bool
seenAndClearedMatch()
{
    size_t i;
    for (i = 0; i < gNumPtrs; i++) {
        if (gClearedPtrs[i] != gSeenPtrs[i]) {
            return false;
        }
    }
    return true;
}

void
run_xor(ssize_t offset, size_t step)
{
    assert(step != 0);
    assert(step < HEAP_SIZE);

    /* Figure out the range.
     */
uintptr_t base;
uintptr_t top;
    if (offset >= 0) {
        base = (uintptr_t)HEAP_BASE + offset;
    } else {
        base = (uintptr_t)HEAP_BASE + (uintptr_t)HEAP_SIZE + offset;
    }
    if (base < (uintptr_t)HEAP_BASE) {
        base = (uintptr_t)HEAP_BASE;
    } else if (base > (uintptr_t)(HEAP_BASE + HEAP_SIZE)) {
        base = (uintptr_t)(HEAP_BASE + HEAP_SIZE);
    } else {
        base = (base + HB_OBJECT_ALIGNMENT - 1) & ~(HB_OBJECT_ALIGNMENT - 1);
    }
    step *= HB_OBJECT_ALIGNMENT;
    top = base + step * NUM_XOR_PTRS;
    if (top > (uintptr_t)(HEAP_BASE + HEAP_SIZE)) {
        top = (uintptr_t)(HEAP_BASE + HEAP_SIZE);
    }

    /* Create the pointers.
     */
    gNumPtrs = 0;
    memset(gXorPtrs, 0, sizeof(gXorPtrs));
    memset(gClearedPtrs, 0, sizeof(gClearedPtrs));
    memset(gSeenPtrs, 0, sizeof(gSeenPtrs));

uintptr_t addr;
void **p = gXorPtrs;
    for (addr = base; addr < top; addr += step) {
        *p++ = (void *)addr;
        gNumPtrs++;
    }
    assert(seenAndClearedMatch());

    /* Set up the bitmaps.
     */
HeapBitmap hb1, hb2;
bool ok;

    ok = dvmHeapBitmapInit(&hb1, HEAP_BASE, HEAP_SIZE, "test1");
    assert(ok);
    ok = dvmHeapBitmapInitFromTemplate(&hb2, &hb1, "test2");
    assert(ok);

    /* Walk two empty bitmaps.
     */
TRACE("walk 0\n");
    ok = dvmHeapBitmapXorWalk(&hb1, &hb2, xorCallback, gCallbackArg);
    assert(ok);
    assert(seenAndClearedMatch());

    /* Walk one empty bitmap.
     */
TRACE("walk 1\n");
    dvmHeapBitmapSetObjectBit(&hb1, (void *)base);
    ok = dvmHeapBitmapXorWalk(&hb1, &hb2, xorCallback, gCallbackArg);
    assert(ok);

    /* Make the bitmaps match.
     */
TRACE("walk 2\n");
    dvmHeapBitmapSetObjectBit(&hb2, (void *)base);
    ok = dvmHeapBitmapXorWalk(&hb1, &hb2, xorCallback, gCallbackArg);
    assert(ok);

    /* Clear the bitmaps.
     */
    dvmHeapBitmapZero(&hb1);
    assert_empty(&hb1);
    dvmHeapBitmapZero(&hb2);
    assert_empty(&hb2);

    /* Set the pointers we created in one of the bitmaps,
     * then visit them.
     */
size_t i;
    for (i = 0; i < gNumPtrs; i++) {
        dvmHeapBitmapSetObjectBit(&hb1, gXorPtrs[i]);
    }
TRACE("walk 3\n");
    ok = dvmHeapBitmapXorWalk(&hb1, &hb2, xorCallback, gCallbackArg);
    assert(ok);

    /* Set every third pointer in the other bitmap, and visit again.
     */
    for (i = 0; i < gNumPtrs; i += 3) {
        dvmHeapBitmapSetObjectBit(&hb2, gXorPtrs[i]);
    }
TRACE("walk 4\n");
    ok = dvmHeapBitmapXorWalk(&hb1, &hb2, xorCallback, gCallbackArg);
    assert(ok);

    /* Set every other pointer in the other bitmap, and visit again.
     */
    for (i = 0; i < gNumPtrs; i += 2) {
        dvmHeapBitmapSetObjectBit(&hb2, gXorPtrs[i]);
    }
TRACE("walk 5\n");
    ok = dvmHeapBitmapXorWalk(&hb1, &hb2, xorCallback, gCallbackArg);
    assert(ok);

    /* Walk just one bitmap.
     */
TRACE("walk 6\n");
    ok = dvmHeapBitmapWalk(&hb2, xorCallback, gCallbackArg);
    assert(ok);

//xxx build an expect list for the callback
//xxx test where max points to beginning, middle, and end of a word

    /* Clean up.
     */
    dvmHeapBitmapDelete(&hb1);
    dvmHeapBitmapDelete(&hb2);
}

void
test_xor()
{
    run_xor(0, 1);
    run_xor(100, 34);
}

int main(int argc, char *argv[])
{
    printf("test_init...\n");
    test_init();

    printf("test_bits...\n");
    test_bits();

    printf("test_clear...\n");
    test_clear();

    printf("test_modify...\n");
    test_modify();

    printf("test_xor...\n");
    test_xor();

    printf("done.\n");
    return 0;
}
