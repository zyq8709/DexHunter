/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include <sys/mman.h>  /* for PROT_* */

#include "Dalvik.h"
#include "alloc/HeapBitmap.h"
#include "alloc/HeapBitmapInlines.h"
#include "alloc/HeapSource.h"
#include "alloc/Visit.h"

/*
 * Maintain a card table from the the write barrier. All writes of
 * non-NULL values to heap addresses should go through an entry in
 * WriteBarrier, and from there to here.
 *
 * The heap is divided into "cards" of GC_CARD_SIZE bytes, as
 * determined by GC_CARD_SHIFT. The card table contains one byte of
 * data per card, to be used by the GC. The value of the byte will be
 * one of GC_CARD_CLEAN or GC_CARD_DIRTY.
 *
 * After any store of a non-NULL object pointer into a heap object,
 * code is obliged to mark the card dirty. The setters in
 * ObjectInlines.h [such as dvmSetFieldObject] do this for you. The
 * JIT and fast interpreters also contain code to mark cards as dirty.
 *
 * The card table's base [the "biased card table"] gets set to a
 * rather strange value.  In order to keep the JIT from having to
 * fabricate or load GC_DIRTY_CARD to store into the card table,
 * biased base is within the mmap allocation at a point where it's low
 * byte is equal to GC_DIRTY_CARD. See dvmCardTableStartup for details.
 */

/*
 * Initializes the card table; must be called before any other
 * dvmCardTable*() functions.
 */
bool dvmCardTableStartup(size_t heapMaximumSize, size_t growthLimit)
{
    size_t length;
    void *allocBase;
    u1 *biasedBase;
    GcHeap *gcHeap = gDvm.gcHeap;
    int offset;
    void *heapBase = dvmHeapSourceGetBase();
    assert(gcHeap != NULL);
    assert(heapBase != NULL);
    /* All zeros is the correct initial value; all clean. */
    assert(GC_CARD_CLEAN == 0);

    /* Set up the card table */
    length = heapMaximumSize / GC_CARD_SIZE;
    /* Allocate an extra 256 bytes to allow fixed low-byte of base */
    allocBase = dvmAllocRegion(length + 0x100, PROT_READ | PROT_WRITE,
                            "dalvik-card-table");
    if (allocBase == NULL) {
        return false;
    }
    gcHeap->cardTableBase = (u1*)allocBase;
    gcHeap->cardTableLength = growthLimit / GC_CARD_SIZE;
    gcHeap->cardTableMaxLength = length;
    biasedBase = (u1 *)((uintptr_t)allocBase -
                       ((uintptr_t)heapBase >> GC_CARD_SHIFT));
    offset = GC_CARD_DIRTY - ((uintptr_t)biasedBase & 0xff);
    gcHeap->cardTableOffset = offset + (offset < 0 ? 0x100 : 0);
    biasedBase += gcHeap->cardTableOffset;
    assert(((uintptr_t)biasedBase & 0xff) == GC_CARD_DIRTY);
    gDvm.biasedCardTableBase = biasedBase;

    return true;
}

/*
 * Tears down the entire CardTable.
 */
void dvmCardTableShutdown()
{
    gDvm.biasedCardTableBase = NULL;
    munmap(gDvm.gcHeap->cardTableBase, gDvm.gcHeap->cardTableLength);
}

void dvmClearCardTable()
{
    /*
     * The goal is to zero out some mmap-allocated pages.  We can accomplish
     * this with memset() or madvise(MADV_DONTNEED).  The latter has some
     * useful properties, notably that the pages are returned to the system,
     * so cards for parts of the heap we haven't expanded into won't be
     * allocated physical pages.  On the other hand, if we un-map the card
     * area, we'll have to fault it back in as we resume dirtying objects,
     * which reduces performance.
     *
     * We don't cause any correctness issues by failing to clear cards; we
     * just take a performance hit during the second pause of the concurrent
     * collection.  The "advisory" nature of madvise() isn't a big problem.
     *
     * What we really want to do is:
     * (1) zero out all cards that were touched
     * (2) use madvise() to release any pages that won't be used in the near
     *     future
     *
     * For #1, we don't really know which cards were touched, but we can
     * approximate it with the "live bits max" value, which tells us the
     * highest start address at which an object was allocated.  This may
     * leave vestigial nonzero entries at the end if temporary objects are
     * created during a concurrent GC, but that should be harmless.  (We
     * can round up to the end of the card table page to reduce this.)
     *
     * For #2, we don't know which pages will be used in the future.  Some
     * simple experiments suggested that a "typical" app will touch about
     * 60KB of pages while initializing, but drops down to 20-24KB while
     * idle.  We can save a few hundred KB system-wide with aggressive
     * use of madvise().  The cost of mapping those pages back in is paid
     * outside of the GC pause, which reduces the impact.  (We might be
     * able to get the benefits by only doing this occasionally, e.g. if
     * the heap shrinks a lot or we somehow notice that we've been idle.)
     *
     * Note that cardTableLength is initially set to the growth limit, and
     * on request will be expanded to the heap maximum.
     */
    assert(gDvm.gcHeap->cardTableBase != NULL);

    if (gDvm.lowMemoryMode) {
      // zero out cards with madvise(), discarding all pages in the card table
      madvise(gDvm.gcHeap->cardTableBase, gDvm.gcHeap->cardTableLength, MADV_DONTNEED);
    } else {
      // zero out cards with memset(), using liveBits as an estimate
      const HeapBitmap* liveBits = dvmHeapSourceGetLiveBits();
      size_t maxLiveCard = (liveBits->max - liveBits->base) / GC_CARD_SIZE;
      maxLiveCard = ALIGN_UP_TO_PAGE_SIZE(maxLiveCard);
      if (maxLiveCard > gDvm.gcHeap->cardTableLength) {
          maxLiveCard = gDvm.gcHeap->cardTableLength;
      }

      memset(gDvm.gcHeap->cardTableBase, GC_CARD_CLEAN, maxLiveCard);
    }
}

/*
 * Returns true iff the address is within the bounds of the card table.
 */
bool dvmIsValidCard(const u1 *cardAddr)
{
    GcHeap *h = gDvm.gcHeap;
    u1* begin = h->cardTableBase + h->cardTableOffset;
    u1* end = &begin[h->cardTableLength];
    return cardAddr >= begin && cardAddr < end;
}

/*
 * Returns the address of the relevant byte in the card table, given
 * an address on the heap.
 */
u1 *dvmCardFromAddr(const void *addr)
{
    u1 *biasedBase = gDvm.biasedCardTableBase;
    u1 *cardAddr = biasedBase + ((uintptr_t)addr >> GC_CARD_SHIFT);
    assert(dvmIsValidCard(cardAddr));
    return cardAddr;
}

/*
 * Returns the first address in the heap which maps to this card.
 */
void *dvmAddrFromCard(const u1 *cardAddr)
{
    assert(dvmIsValidCard(cardAddr));
    uintptr_t offset = cardAddr - gDvm.biasedCardTableBase;
    return (void *)(offset << GC_CARD_SHIFT);
}

/*
 * Dirties the card for the given address.
 */
void dvmMarkCard(const void *addr)
{
    u1 *cardAddr = dvmCardFromAddr(addr);
    *cardAddr = GC_CARD_DIRTY;
}

/*
 * Returns true if the object is on a dirty card.
 */
static bool isObjectDirty(const Object *obj)
{
    assert(obj != NULL);
    assert(dvmIsValidObject(obj));
    u1 *card = dvmCardFromAddr(obj);
    return *card == GC_CARD_DIRTY;
}

/*
 * Context structure for verifying the card table.
 */
struct WhiteReferenceCounter {
    HeapBitmap *markBits;
    size_t whiteRefs;
};

/*
 * Visitor that counts white referents.
 */
static void countWhiteReferenceVisitor(void *addr, void *arg)
{
    WhiteReferenceCounter *ctx;
    Object *obj;

    assert(addr != NULL);
    assert(arg != NULL);
    obj = *(Object **)addr;
    if (obj == NULL) {
        return;
    }
    assert(dvmIsValidObject(obj));
    ctx = (WhiteReferenceCounter *)arg;
    if (dvmHeapBitmapIsObjectBitSet(ctx->markBits, obj)) {
        return;
    }
    ctx->whiteRefs += 1;
}

/*
 * Visitor that logs white references.
 */
static void dumpWhiteReferenceVisitor(void *addr, void *arg)
{
    WhiteReferenceCounter *ctx;
    Object *obj;

    assert(addr != NULL);
    assert(arg != NULL);
    obj = *(Object **)addr;
    if (obj == NULL) {
        return;
    }
    assert(dvmIsValidObject(obj));
    ctx = (WhiteReferenceCounter*)arg;
    if (dvmHeapBitmapIsObjectBitSet(ctx->markBits, obj)) {
        return;
    }
    ALOGE("object %p is white", obj);
}

/*
 * Visitor that signals the caller when a matching reference is found.
 */
static void dumpReferencesVisitor(void *pObj, void *arg)
{
    Object *obj = *(Object **)pObj;
    Object *lookingFor = *(Object **)arg;
    if (lookingFor != NULL && lookingFor == obj) {
        *(Object **)arg = NULL;
    }
}

static void dumpReferencesCallback(Object *obj, void *arg)
{
    if (obj == (Object *)arg) {
        return;
    }
    dvmVisitObject(dumpReferencesVisitor, obj, &arg);
    if (arg == NULL) {
        ALOGD("Found %p in the heap @ %p", arg, obj);
        dvmDumpObject(obj);
    }
}

/*
 * Root visitor that looks for matching references.
 */
static void dumpReferencesRootVisitor(void *ptr, u4 threadId,
                                      RootType type, void *arg)
{
    Object *obj = *(Object **)ptr;
    Object *lookingFor = *(Object **)arg;
    if (obj == lookingFor) {
        ALOGD("Found %p in a root @ %p", arg, ptr);
    }
}

/*
 * Invokes visitors to search for references to an object.
 */
static void dumpReferences(const Object *obj)
{
    HeapBitmap *bitmap = dvmHeapSourceGetLiveBits();
    void *arg = (void *)obj;
    dvmVisitRoots(dumpReferencesRootVisitor, arg);
    dvmHeapBitmapWalk(bitmap, dumpReferencesCallback, arg);
}

/*
 * Returns true if the given object is a reference object and the
 * just the referent is unmarked.
 */
static bool isReferentUnmarked(const Object *obj,
                               const WhiteReferenceCounter* ctx)
{
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    assert(ctx != NULL);
    if (ctx->whiteRefs != 1) {
        return false;
    } else if (IS_CLASS_FLAG_SET(obj->clazz, CLASS_ISREFERENCE)) {
        size_t offset = gDvm.offJavaLangRefReference_referent;
        const Object *referent = dvmGetFieldObject(obj, offset);
        return !dvmHeapBitmapIsObjectBitSet(ctx->markBits, referent);
    } else {
        return false;
    }
}

/*
 * Returns true if the given object is a string and has been interned
 * by the user.
 */
static bool isWeakInternedString(const Object *obj)
{
    assert(obj != NULL);
    if (obj->clazz == gDvm.classJavaLangString) {
        return dvmIsWeakInternedString((StringObject *)obj);
    } else {
        return false;
    }
}

/*
 * Returns true if the given object has been pushed on the mark stack
 * by root marking.
 */
static bool isPushedOnMarkStack(const Object *obj)
{
    GcMarkStack *stack = &gDvm.gcHeap->markContext.stack;
    for (const Object **ptr = stack->base; ptr < stack->top; ++ptr) {
        if (*ptr == obj) {
            return true;
        }
    }
    return false;
}

/*
 * Callback applied to marked objects.  If the object is gray and on
 * an unmarked card an error is logged and the VM is aborted.  Card
 * table verification occurs between root marking and weak reference
 * processing.  We treat objects marked from the roots and weak
 * references specially as it is permissible for these objects to be
 * gray and on an unmarked card.
 */
static void verifyCardTableCallback(Object *obj, void *arg)
{
    WhiteReferenceCounter ctx = { (HeapBitmap *)arg, 0 };

    dvmVisitObject(countWhiteReferenceVisitor, obj, &ctx);
    if (ctx.whiteRefs == 0) {
        return;
    } else if (isObjectDirty(obj)) {
        return;
    } else if (isReferentUnmarked(obj, &ctx)) {
        return;
    } else if (isWeakInternedString(obj)) {
        return;
    } else if (isPushedOnMarkStack(obj)) {
        return;
    } else {
        ALOGE("Verify failed, object %p is gray and on an unmarked card", obj);
        dvmDumpObject(obj);
        dvmVisitObject(dumpWhiteReferenceVisitor, obj, &ctx);
        dumpReferences(obj);
        dvmAbort();
    }
}

/*
 * Verifies that gray objects are on a dirty card.
 */
void dvmVerifyCardTable()
{
    HeapBitmap *markBits = gDvm.gcHeap->markContext.bitmap;
    dvmHeapBitmapWalk(markBits, verifyCardTableCallback, markBits);
}
