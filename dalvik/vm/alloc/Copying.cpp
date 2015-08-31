/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include <errno.h>
#include <limits.h>
#include <sys/mman.h>

#include "Dalvik.h"
#include "alloc/Heap.h"
#include "alloc/HeapBitmap.h"
#include "alloc/HeapInternal.h"
#include "alloc/HeapSource.h"
#include "alloc/Verify.h"

/*
 * A "mostly copying", generational, garbage collector.
 *
 * TODO: we allocate our own contiguous tract of page frames to back
 * object allocations.  To cooperate with other heaps active in the
 * virtual machine we need to move the responsibility of allocating
 * pages someplace outside of this code.
 *
 * The other major data structures that maintain the state of the heap
 * are the block space table and the block queue.
 *
 * The block space table records the state of a block.  We must track
 * whether a block is:
 *
 * - Free or allocated in some space.
 *
 * - If the block holds part of a large object allocation, whether the
 *   block is the initial or a continued block of the allocation.
 *
 * - Whether the block is pinned, that is to say whether at least one
 *   object in the block must remain stationary.  Only needed during a
 *   GC.
 *
 * - Which space the object belongs to.  At present this means
 *   from-space or to-space.
 *
 * The block queue is used during garbage collection.  Unlike Cheney's
 * algorithm, from-space and to-space are not contiguous.  Therefore,
 * one cannot maintain the state of the copy with just two pointers.
 * The block queue exists to thread lists of blocks from the various
 * spaces together.
 *
 * Additionally, we record the free space frontier of the heap, as
 * well as the address of the first object within a block, which is
 * required to copy objects following a large object (not currently
 * implemented).  This is stored in the heap source structure.  This
 * should be moved elsewhere to support in-line allocations from Java
 * threads.
 *
 * Allocation requests are satisfied by reserving storage from one or
 * more contiguous blocks.  Objects that are small enough to fit
 * inside a block are packed together within a block.  Objects that
 * are larger than a block are allocated from contiguous sequences of
 * blocks.  When half the available blocks are filled, a garbage
 * collection occurs.  We "flip" spaces (exchange from- and to-space),
 * copy live objects into to space, and perform pointer adjustment.
 *
 * Copying is made more complicated by the requirement that some
 * objects must not be moved.  This property is known as "pinning".
 * These objects must be dealt with specially.  We use Bartlett's
 * scheme; blocks containing such objects are grayed (promoted) at the
 * start of a garbage collection.  By virtue of this trick, tracing
 * from the roots proceeds as usual but all objects on those pages are
 * considered promoted and therefore not moved.
 *
 * TODO: there is sufficient information within the garbage collector
 * to implement Attardi's scheme for evacuating unpinned objects from
 * a page that is otherwise pinned.  This would eliminate false
 * retention caused by the large pinning granularity.
 *
 * We need a scheme for medium and large objects.  Ignore that for
 * now, we can return to this later.
 *
 * Eventually we need to worry about promoting objects out of the
 * copy-collected heap (tenuring) into a less volatile space.  Copying
 * may not always be the best policy for such spaces.  We should
 * consider a variant of mark, sweep, compact.
 *
 * The block scheme allows us to use VM page faults to maintain a
 * write barrier.  Consider having a special leaf state for a page.
 *
 * Bibliography:
 *
 * C. J. Cheney. 1970. A non-recursive list compacting
 * algorithm. CACM. 13-11 pp677--678.
 *
 * Joel F. Bartlett. 1988. Compacting Garbage Collection with
 * Ambiguous Roots. Digital Equipment Corporation.
 *
 * Joel F. Bartlett. 1989. Mostly-Copying Garbage Collection Picks Up
 * Generations and C++. Digital Equipment Corporation.
 *
 * G. May Yip. 1991. Incremental, Generational Mostly-Copying Garbage
 * Collection in Uncooperative Environments. Digital Equipment
 * Corporation.
 *
 * Giuseppe Attardi, Tito Flagella. 1994. A Customisable Memory
 * Management Framework. TR-94-010
 *
 * Giuseppe Attardi, Tito Flagella, Pietro Iglio. 1998. A customisable
 * memory management framework for C++. Software -- Practice and
 * Experience. 28(11), 1143-1183.
 *
 */

#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))

#if 0
#define LOG_ALLOC ALOGI
#define LOG_PIN ALOGI
#define LOG_PROM ALOGI
#define LOG_REF ALOGI
#define LOG_SCAV ALOGI
#define LOG_TRAN ALOGI
#define LOG_VER ALOGI
#else
#define LOG_ALLOC(...) ((void)0)
#define LOG_PIN(...) ((void)0)
#define LOG_PROM(...) ((void)0)
#define LOG_REF(...) ((void)0)
#define LOG_SCAV(...) ((void)0)
#define LOG_TRAN(...) ((void)0)
#define LOG_VER(...) ((void)0)
#endif

static void enqueueBlock(HeapSource *heapSource, size_t block);
static void scavengeReference(Object **obj);
static bool toSpaceContains(const void *addr);
static bool fromSpaceContains(const void *addr);
static size_t sumHeapBitmap(const HeapBitmap *bitmap);
static size_t objectSize(const Object *obj);
static void scavengeDataObject(Object *obj);
static void scavengeBlockQueue();

/*
 * We use 512-byte blocks.
 */
enum { BLOCK_SHIFT = 9 };
enum { BLOCK_SIZE = 1 << BLOCK_SHIFT };

/*
 * Space identifiers, stored into the blockSpace array.
 */
enum {
    BLOCK_FREE = 0,
    BLOCK_FROM_SPACE = 1,
    BLOCK_TO_SPACE = 2,
    BLOCK_CONTINUED = 7
};

/*
 * Alignment for all allocations, in bytes.
 */
enum { ALLOC_ALIGNMENT = 8 };

/*
 * Sentinel value for the queue end.
 */
#define QUEUE_TAIL (~(size_t)0)

struct HeapSource {

    /* The base address of backing store. */
    u1 *blockBase;

    /* Total number of blocks available for allocation. */
    size_t totalBlocks;
    size_t allocBlocks;

    /*
     * The scavenger work queue.  Implemented as an array of index
     * values into the queue.
     */
    size_t *blockQueue;

    /*
     * Base and limit blocks.  Basically the shifted start address of
     * the block.  We convert blocks to a relative number when
     * indexing in the block queue.  TODO: make the block queue base
     * relative rather than the index into the block queue.
     */
    size_t baseBlock, limitBlock;

    size_t queueHead;
    size_t queueTail;
    size_t queueSize;

    /* The space of the current block 0 (free), 1 or 2. */
    char *blockSpace;

    /* Start of free space in the current block. */
    u1 *allocPtr;
    /* Exclusive limit of free space in the current block. */
    u1 *allocLimit;

    HeapBitmap allocBits;

    /*
     * The starting size of the heap.  This value is the same as the
     * value provided to the -Xms flag.
     */
    size_t minimumSize;

    /*
     * The maximum size of the heap.  This value is the same as the
     * -Xmx flag.
     */
    size_t maximumSize;

    /*
     * The current, committed size of the heap.  At present, this is
     * equivalent to the maximumSize.
     */
    size_t currentSize;

    size_t bytesAllocated;
};

static unsigned long alignDown(unsigned long x, unsigned long n)
{
    return x & -n;
}

static unsigned long alignUp(unsigned long x, unsigned long n)
{
    return alignDown(x + (n - 1), n);
}

static void describeBlocks(const HeapSource *heapSource)
{
    for (size_t i = 0; i < heapSource->totalBlocks; ++i) {
        if ((i % 32) == 0) putchar('\n');
        printf("%d ", heapSource->blockSpace[i]);
    }
    putchar('\n');
}

/*
 * Virtual memory interface.
 */

static void *virtualAlloc(size_t length)
{
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    int prot = PROT_READ | PROT_WRITE;
    void *addr = mmap(NULL, length, prot, flags, -1, 0);
    if (addr == MAP_FAILED) {
        LOGE_HEAP("mmap: %s", strerror(errno));
        addr = NULL;
    }
    return addr;
}

static void virtualFree(void *addr, size_t length)
{
    assert(addr != NULL);
    assert((uintptr_t)addr % SYSTEM_PAGE_SIZE == 0);
    int res = munmap(addr, length);
    if (res == -1) {
        LOGE_HEAP("munmap: %s", strerror(errno));
    }
}

#ifndef NDEBUG
static int isValidAddress(const HeapSource *heapSource, const u1 *addr)
{
    size_t block;

    block = (uintptr_t)addr >> BLOCK_SHIFT;
    return heapSource->baseBlock <= block &&
           heapSource->limitBlock > block;
}
#endif

/*
 * Iterate over the block map looking for a contiguous run of free
 * blocks.
 */
static void *allocateBlocks(HeapSource *heapSource, size_t blocks)
{
    size_t allocBlocks = heapSource->allocBlocks;
    size_t totalBlocks = heapSource->totalBlocks;
    /* Check underflow. */
    assert(blocks != 0);
    /* Check overflow. */
    if (allocBlocks + blocks > totalBlocks / 2) {
        return NULL;
    }
    /* Scan block map. */
    for (size_t i = 0; i < totalBlocks; ++i) {
        /* Check fit. */
        for (size_t j = 0; j < blocks; ++j) { /* runs over totalBlocks */
            if (heapSource->blockSpace[i+j] != BLOCK_FREE) {
                break;
            }
        }
        /* No fit? */
        if (j != blocks) {
            i += j;
            continue;
        }
        /* Fit, allocate. */
        heapSource->blockSpace[i] = BLOCK_TO_SPACE; /* why to-space? */
        for (size_t j = 1; j < blocks; ++j) {
            heapSource->blockSpace[i+j] = BLOCK_CONTINUED;
        }
        heapSource->allocBlocks += blocks;
        void *addr = &heapSource->blockBase[i*BLOCK_SIZE];
        memset(addr, 0, blocks*BLOCK_SIZE);
        /* Collecting? */
        if (heapSource->queueHead != QUEUE_TAIL) {
            LOG_ALLOC("allocateBlocks allocBlocks=%zu,block#=%zu", heapSource->allocBlocks, i);
            /*
             * This allocated was on behalf of the transporter when it
             * shaded a white object gray.  We enqueue the block so
             * the scavenger can further shade the gray objects black.
             */
            enqueueBlock(heapSource, i);
        }

        return addr;
    }
    /* Insufficient space, fail. */
    ALOGE("Insufficient space, %zu blocks, %zu blocks allocated and %zu bytes allocated",
         heapSource->totalBlocks,
         heapSource->allocBlocks,
         heapSource->bytesAllocated);
    return NULL;
}

/* Converts an absolute address to a relative block number. */
static size_t addressToBlock(const HeapSource *heapSource, const void *addr)
{
    assert(heapSource != NULL);
    assert(isValidAddress(heapSource, addr));
    return (((uintptr_t)addr) >> BLOCK_SHIFT) - heapSource->baseBlock;
}

/* Converts a relative block number to an absolute address. */
static u1 *blockToAddress(const HeapSource *heapSource, size_t block)
{
    u1 *addr;

    addr = (u1 *) (((uintptr_t) heapSource->baseBlock + block) * BLOCK_SIZE);
    assert(isValidAddress(heapSource, addr));
    return addr;
}

static void clearBlock(HeapSource *heapSource, size_t block)
{
    assert(heapSource != NULL);
    assert(block < heapSource->totalBlocks);
    u1 *addr = heapSource->blockBase + block*BLOCK_SIZE;
    memset(addr, 0xCC, BLOCK_SIZE);
    for (size_t i = 0; i < BLOCK_SIZE; i += 8) {
        dvmHeapBitmapClearObjectBit(&heapSource->allocBits, addr + i);
    }
}

static void clearFromSpace(HeapSource *heapSource)
{
    assert(heapSource != NULL);
    size_t i = 0;
    size_t count = 0;
    while (i < heapSource->totalBlocks) {
        if (heapSource->blockSpace[i] != BLOCK_FROM_SPACE) {
            ++i;
            continue;
        }
        heapSource->blockSpace[i] = BLOCK_FREE;
        clearBlock(heapSource, i);
        ++i;
        ++count;
        while (i < heapSource->totalBlocks &&
               heapSource->blockSpace[i] == BLOCK_CONTINUED) {
            heapSource->blockSpace[i] = BLOCK_FREE;
            clearBlock(heapSource, i);
            ++i;
            ++count;
        }
    }
    LOG_SCAV("freed %zu blocks (%zu bytes)", count, count*BLOCK_SIZE);
}

/*
 * Appends the given block to the block queue.  The block queue is
 * processed in-order by the scavenger.
 */
static void enqueueBlock(HeapSource *heapSource, size_t block)
{
    assert(heapSource != NULL);
    assert(block < heapSource->totalBlocks);
    if (heapSource->queueHead != QUEUE_TAIL) {
        heapSource->blockQueue[heapSource->queueTail] = block;
    } else {
        heapSource->queueHead = block;
    }
    heapSource->blockQueue[block] = QUEUE_TAIL;
    heapSource->queueTail = block;
    ++heapSource->queueSize;
}

/*
 * Grays all objects within the block corresponding to the given
 * address.
 */
static void promoteBlockByAddr(HeapSource *heapSource, const void *addr)
{
    size_t block;

    block = addressToBlock(heapSource, (const u1 *)addr);
    if (heapSource->blockSpace[block] != BLOCK_TO_SPACE) {
        // LOG_PROM("promoting block %zu %d @ %p", block, heapSource->blockSpace[block], obj);
        heapSource->blockSpace[block] = BLOCK_TO_SPACE;
        enqueueBlock(heapSource, block);
        /* TODO(cshapiro): count continued blocks?*/
        heapSource->allocBlocks += 1;
    } else {
        // LOG_PROM("NOT promoting block %zu %d @ %p", block, heapSource->blockSpace[block], obj);
    }
}

GcHeap *dvmHeapSourceStartup(size_t startSize, size_t absoluteMaxSize)
{
    GcHeap* gcHeap;
    HeapSource *heapSource;

    assert(startSize <= absoluteMaxSize);

    heapSource = calloc(1, sizeof(*heapSource));
    assert(heapSource != NULL);

    heapSource->minimumSize = alignUp(startSize, BLOCK_SIZE);
    heapSource->maximumSize = alignUp(absoluteMaxSize, BLOCK_SIZE);

    heapSource->currentSize = heapSource->maximumSize;

    /* Allocate underlying storage for blocks. */
    heapSource->blockBase = virtualAlloc(heapSource->maximumSize);
    assert(heapSource->blockBase != NULL);
    heapSource->baseBlock = (uintptr_t) heapSource->blockBase >> BLOCK_SHIFT;
    heapSource->limitBlock = ((uintptr_t) heapSource->blockBase + heapSource->maximumSize) >> BLOCK_SHIFT;

    heapSource->allocBlocks = 0;
    heapSource->totalBlocks = (heapSource->limitBlock - heapSource->baseBlock);

    assert(heapSource->totalBlocks = heapSource->maximumSize / BLOCK_SIZE);

    {
        size_t size = sizeof(heapSource->blockQueue[0]);
        heapSource->blockQueue = malloc(heapSource->totalBlocks*size);
        assert(heapSource->blockQueue != NULL);
        memset(heapSource->blockQueue, 0xCC, heapSource->totalBlocks*size);
        heapSource->queueHead = QUEUE_TAIL;
    }

    /* Byte indicating space residence or free status of block. */
    {
        size_t size = sizeof(heapSource->blockSpace[0]);
        heapSource->blockSpace = calloc(1, heapSource->totalBlocks*size);
        assert(heapSource->blockSpace != NULL);
    }

    dvmHeapBitmapInit(&heapSource->allocBits,
                      heapSource->blockBase,
                      heapSource->maximumSize,
                      "blockBase");

    /* Initialize allocation pointers. */
    heapSource->allocPtr = allocateBlocks(heapSource, 1);
    heapSource->allocLimit = heapSource->allocPtr + BLOCK_SIZE;

    gcHeap = calloc(1, sizeof(*gcHeap));
    assert(gcHeap != NULL);
    gcHeap->heapSource = heapSource;

    return gcHeap;
}

/*
 * Perform any required heap initializations after forking from the
 * zygote process.  This is a no-op for the time being.  Eventually
 * this will demarcate the shared region of the heap.
 */
bool dvmHeapSourceStartupAfterZygote()
{
    return true;
}

bool dvmHeapSourceStartupBeforeFork()
{
    assert(!"implemented");
    return false;
}

void dvmHeapSourceShutdown(GcHeap **gcHeap)
{
    if (*gcHeap == NULL || (*gcHeap)->heapSource == NULL)
        return;
    free((*gcHeap)->heapSource->blockQueue);
    free((*gcHeap)->heapSource->blockSpace);
    virtualFree((*gcHeap)->heapSource->blockBase,
                (*gcHeap)->heapSource->maximumSize);
    free((*gcHeap)->heapSource);
    (*gcHeap)->heapSource = NULL;
    free(*gcHeap);
    *gcHeap = NULL;
}

size_t dvmHeapSourceGetValue(HeapSourceValueSpec spec,
                             size_t perHeapStats[],
                             size_t arrayLen)
{
    HeapSource *heapSource;
    size_t value;

    heapSource = gDvm.gcHeap->heapSource;
    switch (spec) {
    case HS_FOOTPRINT:
        value = heapSource->maximumSize;
        break;
    case HS_ALLOWED_FOOTPRINT:
        value = heapSource->maximumSize;
        break;
    case HS_BYTES_ALLOCATED:
        value = heapSource->bytesAllocated;
        break;
    case HS_OBJECTS_ALLOCATED:
        value = sumHeapBitmap(&heapSource->allocBits);
        break;
    default:
        assert(!"implemented");
        value = 0;
    }
    if (perHeapStats) {
        *perHeapStats = value;
    }
    return value;
}

/*
 * Performs a shallow copy of the allocation bitmap into the given
 * vector of heap bitmaps.
 */
void dvmHeapSourceGetObjectBitmaps(HeapBitmap objBits[], HeapBitmap markBits[],
                                   size_t numHeaps)
{
    assert(!"implemented");
}

HeapBitmap *dvmHeapSourceGetLiveBits()
{
    return &gDvm.gcHeap->heapSource->allocBits;
}

/*
 * Allocate the specified number of bytes from the heap.  The
 * allocation cursor points into a block of free storage.  If the
 * given allocation fits in the remaining space of the block, we
 * advance the cursor and return a pointer to the free storage.  If
 * the allocation cannot fit in the current block but is smaller than
 * a block we request a new block and allocate from it instead.  If
 * the allocation is larger than a block we must allocate from a span
 * of contiguous blocks.
 */
void *dvmHeapSourceAlloc(size_t length)
{
    HeapSource *heapSource;
    unsigned char *addr;
    size_t aligned, available, blocks;

    heapSource = gDvm.gcHeap->heapSource;
    assert(heapSource != NULL);
    assert(heapSource->allocPtr != NULL);
    assert(heapSource->allocLimit != NULL);

    aligned = alignUp(length, ALLOC_ALIGNMENT);
    available = heapSource->allocLimit - heapSource->allocPtr;

    /* Try allocating inside the current block. */
    if (aligned <= available) {
        addr = heapSource->allocPtr;
        heapSource->allocPtr += aligned;
        heapSource->bytesAllocated += aligned;
        dvmHeapBitmapSetObjectBit(&heapSource->allocBits, addr);
        return addr;
    }

    /* Try allocating in a new block. */
    if (aligned <= BLOCK_SIZE) {
        addr =  allocateBlocks(heapSource, 1);
        if (addr != NULL) {
            heapSource->allocLimit = addr + BLOCK_SIZE;
            heapSource->allocPtr = addr + aligned;
            heapSource->bytesAllocated += aligned;
            dvmHeapBitmapSetObjectBit(&heapSource->allocBits, addr);
            /* TODO(cshapiro): pad out the current block. */
        }
        return addr;
    }

    /* Try allocating in a span of blocks. */
    blocks = alignUp(aligned, BLOCK_SIZE) / BLOCK_SIZE;

    addr = allocateBlocks(heapSource, blocks);
    /* Propagate failure upward. */
    if (addr != NULL) {
        heapSource->bytesAllocated += aligned;
        dvmHeapBitmapSetObjectBit(&heapSource->allocBits, addr);
        /* TODO(cshapiro): pad out free space in the last block. */
    }
    return addr;
}

void *dvmHeapSourceAllocAndGrow(size_t size)
{
    return dvmHeapSourceAlloc(size);
}

/* TODO: refactor along with dvmHeapSourceAlloc */
void *allocateGray(size_t size)
{
    HeapSource *heapSource;
    void *addr;
    size_t block;

    /* TODO: add a check that we are in a GC. */
    heapSource = gDvm.gcHeap->heapSource;
    addr = dvmHeapSourceAlloc(size);
    assert(addr != NULL);
    block = addressToBlock(heapSource, (const u1 *)addr);
    if (heapSource->queueHead == QUEUE_TAIL) {
        /*
         * Forcibly append the underlying block to the queue.  This
         * condition occurs when referents are transported following
         * the initial trace.
         */
        enqueueBlock(heapSource, block);
        LOG_PROM("forced promoting block %zu %d @ %p", block, heapSource->blockSpace[block], addr);
    }
    return addr;
}

bool dvmHeapSourceContainsAddress(const void *ptr)
{
    HeapSource *heapSource = gDvm.gcHeap->heapSource;
    return dvmHeapBitmapCoversAddress(&heapSource->allocBits, ptr);
}

/*
 * Returns true if the given address is within the heap and points to
 * the header of a live object.
 */
bool dvmHeapSourceContains(const void *addr)
{
    HeapSource *heapSource;
    HeapBitmap *bitmap;

    heapSource = gDvm.gcHeap->heapSource;
    bitmap = &heapSource->allocBits;
    if (!dvmHeapBitmapCoversAddress(bitmap, addr)) {
        return false;
    } else {
        return dvmHeapBitmapIsObjectBitSet(bitmap, addr);
    }
}

bool dvmHeapSourceGetPtrFlag(const void *ptr, HeapSourcePtrFlag flag)
{
    assert(!"implemented");
    return false;
}

size_t dvmHeapSourceChunkSize(const void *ptr)
{
    assert(!"implemented");
    return 0;
}

size_t dvmHeapSourceFootprint()
{
    assert(!"implemented");
    return 0;
}

/*
 * Returns the "ideal footprint" which appears to be the number of
 * bytes currently committed to the heap.  This starts out at the
 * start size of the heap and grows toward the maximum size.
 */
size_t dvmHeapSourceGetIdealFootprint()
{
    return gDvm.gcHeap->heapSource->currentSize;
}

float dvmGetTargetHeapUtilization()
{
    return 0.5f;
}

void dvmSetTargetHeapUtilization(float newTarget)
{
    assert(newTarget > 0.0f && newTarget < 1.0f);
}

/*
 * Expands the size of the heap after a collection.  At present we
 * commit the pages for maximum size of the heap so this routine is
 * just a no-op.  Eventually, we will either allocate or commit pages
 * on an as-need basis.
 */
void dvmHeapSourceGrowForUtilization()
{
    /* do nothing */
}

void dvmHeapSourceWalk(void(*callback)(void* start, void* end,
                                       size_t used_bytes, void* arg),
                       void *arg)
{
    assert(!"implemented");
}

size_t dvmHeapSourceGetNumHeaps()
{
    return 1;
}

bool dvmTrackExternalAllocation(size_t n)
{
    /* do nothing */
    return true;
}

void dvmTrackExternalFree(size_t n)
{
    /* do nothing */
}

size_t dvmGetExternalBytesAllocated()
{
    assert(!"implemented");
    return 0;
}

void dvmHeapSourceFlip()
{
    HeapSource *heapSource = gDvm.gcHeap->heapSource;

    /* Reset the block queue. */
    heapSource->allocBlocks = 0;
    heapSource->queueSize = 0;
    heapSource->queueHead = QUEUE_TAIL;

    /* TODO(cshapiro): pad the current (prev) block. */

    heapSource->allocPtr = NULL;
    heapSource->allocLimit = NULL;

    /* Whiten all allocated blocks. */
    for (size_t i = 0; i < heapSource->totalBlocks; ++i) {
        if (heapSource->blockSpace[i] == BLOCK_TO_SPACE) {
            heapSource->blockSpace[i] = BLOCK_FROM_SPACE;
        }
    }
}

static void room(size_t *alloc, size_t *avail, size_t *total)
{
    HeapSource *heapSource = gDvm.gcHeap->heapSource;
    *total = heapSource->totalBlocks*BLOCK_SIZE;
    *alloc = heapSource->allocBlocks*BLOCK_SIZE;
    *avail = *total - *alloc;
}

static bool isSpaceInternal(u1 *addr, int space)
{
    HeapSource *heapSource;
    u1 *base, *limit;
    size_t offset;
    char space2;

    heapSource = gDvm.gcHeap->heapSource;
    base = heapSource->blockBase;
    assert(addr >= base);
    limit = heapSource->blockBase + heapSource->maximumSize;
    assert(addr < limit);
    offset = addr - base;
    space2 = heapSource->blockSpace[offset >> BLOCK_SHIFT];
    return space == space2;
}

static bool fromSpaceContains(const void *addr)
{
    return isSpaceInternal((u1 *)addr, BLOCK_FROM_SPACE);
}

static bool toSpaceContains(const void *addr)
{
    return isSpaceInternal((u1 *)addr, BLOCK_TO_SPACE);
}

/*
 * Notifies the collector that the object at the given address must
 * remain stationary during the current collection.
 */
static void pinObject(const Object *obj)
{
    promoteBlockByAddr(gDvm.gcHeap->heapSource, obj);
}

static size_t sumHeapBitmap(const HeapBitmap *bitmap)
{
    size_t sum = 0;
    for (size_t i = 0; i < bitmap->bitsLen >> 2; ++i) {
        sum += CLZ(bitmap->bits[i]);
    }
    return sum;
}

/*
 * Miscellaneous functionality.
 */

static int isForward(const void *addr)
{
    return (uintptr_t)addr & 0x1;
}

static void setForward(const void *toObj, void *fromObj)
{
    *(unsigned long *)fromObj = (uintptr_t)toObj | 0x1;
}

static void* getForward(const void *fromObj)
{
    return (void *)((uintptr_t)fromObj & ~0x1);
}

/* Beware, uses the same encoding as a forwarding pointers! */
static int isPermanentString(const StringObject *obj) {
    return (uintptr_t)obj & 0x1;
}

static void* getPermanentString(const StringObject *obj)
{
    return (void *)((uintptr_t)obj & ~0x1);
}


/*
 * Scavenging and transporting routines follow.  A transporter grays
 * an object.  A scavenger blackens an object.  We define these
 * routines for each fundamental object type.  Dispatch is performed
 * in scavengeObject.
 */

/*
 * Class object scavenging.
 */
static void scavengeClassObject(ClassObject *obj)
{
    LOG_SCAV("scavengeClassObject(obj=%p)", obj);
    assert(obj != NULL);
    assert(obj->obj.clazz != NULL);
    assert(obj->obj.clazz->descriptor != NULL);
    assert(!strcmp(obj->obj.clazz->descriptor, "Ljava/lang/Class;"));
    assert(obj->descriptor != NULL);
    LOG_SCAV("scavengeClassObject: descriptor='%s',vtableCount=%zu",
             obj->descriptor, obj->vtableCount);
    /* Delegate class object and instance field scavenging. */
    scavengeDataObject((Object *)obj);
    /* Scavenge the array element class object. */
    if (IS_CLASS_FLAG_SET(obj, CLASS_ISARRAY)) {
        scavengeReference((Object **)(void *)&obj->elementClass);
    }
    /* Scavenge the superclass. */
    scavengeReference((Object **)(void *)&obj->super);
    /* Scavenge the class loader. */
    scavengeReference(&obj->classLoader);
    /* Scavenge static fields. */
    for (int i = 0; i < obj->sfieldCount; ++i) {
        char ch = obj->sfields[i].field.signature[0];
        if (ch == '[' || ch == 'L') {
            scavengeReference((Object **)(void *)&obj->sfields[i].value.l);
        }
    }
    /* Scavenge interface class objects. */
    for (int i = 0; i < obj->interfaceCount; ++i) {
        scavengeReference((Object **) &obj->interfaces[i]);
    }
}

/*
 * Array object scavenging.
 */
static size_t scavengeArrayObject(ArrayObject *array)
{
    LOG_SCAV("scavengeArrayObject(array=%p)", array);
    /* Scavenge the class object. */
    assert(toSpaceContains(array));
    assert(array != NULL);
    assert(array->obj.clazz != NULL);
    scavengeReference((Object **) array);
    size_t length = dvmArrayObjectSize(array);
    /* Scavenge the array contents. */
    if (IS_CLASS_FLAG_SET(array->obj.clazz, CLASS_ISOBJECTARRAY)) {
        Object **contents = (Object **)array->contents;
        for (size_t i = 0; i < array->length; ++i) {
            scavengeReference(&contents[i]);
        }
    }
    return length;
}

/*
 * Reference object scavenging.
 */

static int getReferenceFlags(const Object *obj)
{
    int flags;

    flags = CLASS_ISREFERENCE |
            CLASS_ISWEAKREFERENCE |
            CLASS_ISPHANTOMREFERENCE;
    return GET_CLASS_FLAG_GROUP(obj->clazz, flags);
}

static int isSoftReference(const Object *obj)
{
    return getReferenceFlags(obj) == CLASS_ISREFERENCE;
}

static int isWeakReference(const Object *obj)
{
    return getReferenceFlags(obj) & CLASS_ISWEAKREFERENCE;
}

#ifndef NDEBUG
static bool isPhantomReference(const Object *obj)
{
    return getReferenceFlags(obj) & CLASS_ISPHANTOMREFERENCE;
}
#endif

/*
 * Returns true if the reference was registered with a reference queue
 * but has not yet been appended to it.
 */
static bool isReferenceEnqueuable(const Object *ref)
{
    Object *queue, *queueNext;

    queue = dvmGetFieldObject(ref, gDvm.offJavaLangRefReference_queue);
    queueNext = dvmGetFieldObject(ref, gDvm.offJavaLangRefReference_queueNext);
    if (queue == NULL || queueNext != NULL) {
        /*
         * There is no queue, or the reference has already
         * been enqueued.  The Reference.enqueue() method
         * will do nothing even if we call it.
         */
        return false;
    }

    /*
     * We need to call enqueue(), but if we called it from
     * here we'd probably deadlock.  Schedule a call.
     */
    return true;
}

/*
 * Schedules a reference to be appended to its reference queue.
 */
static void enqueueReference(Object *ref)
{
    assert(ref != NULL);
    assert(dvmGetFieldObject(ref, gDvm.offJavaLangRefReference_queue) != NULL);
    assert(dvmGetFieldObject(ref, gDvm.offJavaLangRefReference_queueNext) == NULL);
    if (!dvmHeapAddRefToLargeTable(&gDvm.gcHeap->referenceOperations, ref)) {
        ALOGE("no room for any more reference operations");
        dvmAbort();
    }
}

/*
 * Sets the referent field of a reference object to NULL.
 */
static void clearReference(Object *obj)
{
    dvmSetFieldObject(obj, gDvm.offJavaLangRefReference_referent, NULL);
}

/*
 * Clears reference objects with white referents.
 */
void clearWhiteReferences(Object **list)
{
    size_t referentOffset, queueNextOffset;
    bool doSignal;

    queueNextOffset = gDvm.offJavaLangRefReference_queueNext;
    referentOffset = gDvm.offJavaLangRefReference_referent;
    doSignal = false;
    while (*list != NULL) {
        Object *ref = *list;
        JValue *field = dvmFieldPtr(ref, referentOffset);
        Object *referent = field->l;
        *list = dvmGetFieldObject(ref, queueNextOffset);
        dvmSetFieldObject(ref, queueNextOffset, NULL);
        assert(referent != NULL);
        if (isForward(referent->clazz)) {
            field->l = referent = getForward(referent->clazz);
            continue;
        }
        if (fromSpaceContains(referent)) {
            /* Referent is white, clear it. */
            clearReference(ref);
            if (isReferenceEnqueuable(ref)) {
                enqueueReference(ref);
                doSignal = true;
            }
        }
    }
    /*
     * If we cleared a reference with a reference queue we must notify
     * the heap worker to append the reference.
     */
    if (doSignal) {
        dvmSignalHeapWorker(false);
    }
    assert(*list == NULL);
}

/*
 * Blackens referents subject to the soft reference preservation
 * policy.
 */
void preserveSoftReferences(Object **list)
{
    Object *ref;
    Object *prev, *next;
    size_t referentOffset, queueNextOffset;
    unsigned counter;
    bool white;

    queueNextOffset = gDvm.offJavaLangRefReference_queueNext;
    referentOffset = gDvm.offJavaLangRefReference_referent;
    counter = 0;
    prev = next = NULL;
    ref = *list;
    while (ref != NULL) {
        JValue *field = dvmFieldPtr(ref, referentOffset);
        Object *referent = field->l;
        next = dvmGetFieldObject(ref, queueNextOffset);
        assert(referent != NULL);
        if (isForward(referent->clazz)) {
            /* Referent is black. */
            field->l = referent = getForward(referent->clazz);
            white = false;
        } else {
            white = fromSpaceContains(referent);
        }
        if (!white && ((++counter) & 1)) {
            /* Referent is white and biased toward saving, gray it. */
            scavengeReference((Object **)(void *)&field->l);
            white = true;
        }
        if (white) {
            /* Referent is black, unlink it. */
            if (prev != NULL) {
                dvmSetFieldObject(ref, queueNextOffset, NULL);
                dvmSetFieldObject(prev, queueNextOffset, next);
            }
        } else {
            /* Referent is white, skip over it. */
            prev = ref;
        }
        ref = next;
    }
    /*
     * Restart the trace with the newly gray references added to the
     * root set.
     */
    scavengeBlockQueue();
}

void processFinalizableReferences()
{
    HeapRefTable newPendingRefs;
    LargeHeapRefTable *finRefs = gDvm.gcHeap->finalizableRefs;
    Object **ref;
    Object **lastRef;
    size_t totalPendCount;

    /*
     * All strongly, reachable objects are black.
     * Any white finalizable objects need to be finalized.
     */

    /* Create a table that the new pending refs will
     * be added to.
     */
    if (!dvmHeapInitHeapRefTable(&newPendingRefs)) {
        //TODO: mark all finalizable refs and hope that
        //      we can schedule them next time.  Watch out,
        //      because we may be expecting to free up space
        //      by calling finalizers.
        LOG_REF("no room for pending finalizations");
        dvmAbort();
    }

    /*
     * Walk through finalizableRefs and move any white references to
     * the list of new pending refs.
     */
    totalPendCount = 0;
    while (finRefs != NULL) {
        Object **gapRef;
        size_t newPendCount = 0;

        gapRef = ref = finRefs->refs.table;
        lastRef = finRefs->refs.nextEntry;
        while (ref < lastRef) {
            if (fromSpaceContains(*ref)) {
                if (!dvmHeapAddToHeapRefTable(&newPendingRefs, *ref)) {
                    //TODO: add the current table and allocate
                    //      a new, smaller one.
                    LOG_REF("no room for any more pending finalizations: %zd",
                            dvmHeapNumHeapRefTableEntries(&newPendingRefs));
                    dvmAbort();
                }
                newPendCount++;
            } else {
                /* This ref is black, so will remain on finalizableRefs.
                 */
                if (newPendCount > 0) {
                    /* Copy it up to fill the holes.
                     */
                    *gapRef++ = *ref;
                } else {
                    /* No holes yet; don't bother copying.
                     */
                    gapRef++;
                }
            }
            ref++;
        }
        finRefs->refs.nextEntry = gapRef;
        //TODO: if the table is empty when we're done, free it.
        totalPendCount += newPendCount;
        finRefs = finRefs->next;
    }
    LOG_REF("%zd finalizers triggered.", totalPendCount);
    if (totalPendCount == 0) {
        /* No objects required finalization.
         * Free the empty temporary table.
         */
        dvmClearReferenceTable(&newPendingRefs);
        return;
    }

    /* Add the new pending refs to the main list.
     */
    if (!dvmHeapAddTableToLargeTable(&gDvm.gcHeap->pendingFinalizationRefs,
                &newPendingRefs))
    {
        LOG_REF("can't insert new pending finalizations");
        dvmAbort();
    }

    //TODO: try compacting the main list with a memcpy loop

    /* Blacken the refs we just moved; we don't want them or their
     * children to get swept yet.
     */
    ref = newPendingRefs.table;
    lastRef = newPendingRefs.nextEntry;
    assert(ref < lastRef);
    HPROF_SET_GC_SCAN_STATE(HPROF_ROOT_FINALIZING, 0);
    while (ref < lastRef) {
        scavengeReference(ref);
        ref++;
    }
    HPROF_CLEAR_GC_SCAN_STATE();
    scavengeBlockQueue();
    dvmSignalHeapWorker(false);
}

/*
 * If a reference points to from-space and has been forwarded, we snap
 * the pointer to its new to-space address.  If the reference points
 * to an unforwarded from-space address we must enqueue the reference
 * for later processing.  TODO: implement proper reference processing
 * and move the referent scavenging elsewhere.
 */
static void scavengeReferenceObject(Object *obj)
{
    Object *referent;
    Object **queue;
    size_t referentOffset, queueNextOffset;

    assert(obj != NULL);
    LOG_SCAV("scavengeReferenceObject(obj=%p),'%s'", obj, obj->clazz->descriptor);
    scavengeDataObject(obj);
    referentOffset = gDvm.offJavaLangRefReference_referent;
    referent = dvmGetFieldObject(obj, referentOffset);
    if (referent == NULL || toSpaceContains(referent)) {
        return;
    }
    if (isSoftReference(obj)) {
        queue = &gDvm.gcHeap->softReferences;
    } else if (isWeakReference(obj)) {
        queue = &gDvm.gcHeap->weakReferences;
    } else {
        assert(isPhantomReference(obj));
        queue = &gDvm.gcHeap->phantomReferences;
    }
    queueNextOffset = gDvm.offJavaLangRefReference_queueNext;
    dvmSetFieldObject(obj, queueNextOffset, *queue);
    *queue = obj;
    LOG_SCAV("scavengeReferenceObject: enqueueing %p", obj);
}

/*
 * Data object scavenging.
 */
static void scavengeDataObject(Object *obj)
{
    // LOG_SCAV("scavengeDataObject(obj=%p)", obj);
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    assert(obj->clazz->objectSize != 0);
    assert(toSpaceContains(obj));
    /* Scavenge the class object. */
    ClassObject *clazz = obj->clazz;
    scavengeReference((Object **) obj);
    /* Scavenge instance fields. */
    if (clazz->refOffsets != CLASS_WALK_SUPER) {
        size_t refOffsets = clazz->refOffsets;
        while (refOffsets != 0) {
            size_t rshift = CLZ(refOffsets);
            size_t offset = CLASS_OFFSET_FROM_CLZ(rshift);
            Object **ref = (Object **)((u1 *)obj + offset);
            scavengeReference(ref);
            refOffsets &= ~(CLASS_HIGH_BIT >> rshift);
        }
    } else {
        for (; clazz != NULL; clazz = clazz->super) {
            InstField *field = clazz->ifields;
            for (int i = 0; i < clazz->ifieldRefCount; ++i, ++field) {
                size_t offset = field->byteOffset;
                Object **ref = (Object **)((u1 *)obj + offset);
                scavengeReference(ref);
            }
        }
    }
}

static Object *transportObject(const Object *fromObj)
{
    Object *toObj;
    size_t allocSize, copySize;

    LOG_TRAN("transportObject(fromObj=%p) allocBlocks=%zu",
                  fromObj,
                  gDvm.gcHeap->heapSource->allocBlocks);
    assert(fromObj != NULL);
    assert(fromSpaceContains(fromObj));
    allocSize = copySize = objectSize(fromObj);
    if (LW_HASH_STATE(fromObj->lock) != LW_HASH_STATE_UNHASHED) {
        /*
         * The object has been hashed or hashed and moved.  We must
         * reserve an additional word for a hash code.
         */
        allocSize += sizeof(u4);
    }
    if (LW_HASH_STATE(fromObj->lock) == LW_HASH_STATE_HASHED_AND_MOVED) {
        /*
         * The object has its hash code allocated.  Ensure the hash
         * code is copied along with the instance data.
         */
        copySize += sizeof(u4);
    }
    /* TODO(cshapiro): don't copy, re-map large data objects. */
    assert(copySize <= allocSize);
    toObj = allocateGray(allocSize);
    assert(toObj != NULL);
    assert(toSpaceContains(toObj));
    memcpy(toObj, fromObj, copySize);
    if (LW_HASH_STATE(fromObj->lock) == LW_HASH_STATE_HASHED) {
        /*
         * The object has had its hash code exposed.  Append it to the
         * instance and set a bit so we know to look for it there.
         */
        *(u4 *)(((char *)toObj) + copySize) = (u4)fromObj >> 3;
        toObj->lock |= LW_HASH_STATE_HASHED_AND_MOVED << LW_HASH_STATE_SHIFT;
    }
    LOG_TRAN("transportObject: from %p/%zu to %p/%zu (%zu,%zu) %s",
             fromObj, addressToBlock(gDvm.gcHeap->heapSource,fromObj),
             toObj, addressToBlock(gDvm.gcHeap->heapSource,toObj),
             copySize, allocSize, copySize < allocSize ? "DIFFERENT" : "");
    return toObj;
}

/*
 * Generic reference scavenging.
 */

/*
 * Given a reference to an object, the scavenge routine will gray the
 * reference.  Any objects pointed to by the scavenger object will be
 * transported to new space and a forwarding pointer will be installed
 * in the header of the object.
 */

/*
 * Blacken the given pointer.  If the pointer is in from space, it is
 * transported to new space.  If the object has a forwarding pointer
 * installed it has already been transported and the referent is
 * snapped to the new address.
 */
static void scavengeReference(Object **obj)
{
    ClassObject *clazz;
    Object *fromObj, *toObj;

    assert(obj);

    if (*obj == NULL) return;

    assert(dvmIsValidObject(*obj));

    /* The entire block is black. */
    if (toSpaceContains(*obj)) {
        LOG_SCAV("scavengeReference skipping pinned object @ %p", *obj);
        return;
    }
    LOG_SCAV("scavengeReference(*obj=%p)", *obj);

    assert(fromSpaceContains(*obj));

    clazz = (*obj)->clazz;

    if (isForward(clazz)) {
        // LOG_SCAV("forwarding %p @ %p to %p", *obj, obj, (void *)((uintptr_t)clazz & ~0x1));
        *obj = (Object *)getForward(clazz);
        return;
    }
    fromObj = *obj;
    if (clazz == NULL) {
        // LOG_SCAV("scavangeReference %p has a NULL class object", fromObj);
        assert(!"implemented");
        toObj = NULL;
    } else {
        toObj = transportObject(fromObj);
    }
    setForward(toObj, fromObj);
    *obj = (Object *)toObj;
}

/*
 * Generic object scavenging.
 */
static void scavengeObject(Object *obj)
{
    ClassObject *clazz;

    assert(obj != NULL);
    assert(obj->clazz != NULL);
    assert(!((uintptr_t)obj->clazz & 0x1));
    clazz = obj->clazz;
    if (dvmIsTheClassClass(clazz)) {
        scavengeClassObject((ClassObject *)obj);
    } else if (IS_CLASS_FLAG_SET(clazz, CLASS_ISARRAY)) {
        scavengeArrayObject((ArrayObject *)obj);
    } else if (IS_CLASS_FLAG_SET(clazz, CLASS_ISREFERENCE)) {
        scavengeReferenceObject(obj);
    } else {
        scavengeDataObject(obj);
    }
}

/*
 * External root scavenging routines.
 */

static void pinHashTableEntries(HashTable *table)
{
    LOG_PIN(">>> pinHashTableEntries(table=%p)", table);
    if (table == NULL) {
        return;
    }
    dvmHashTableLock(table);
    for (int i = 0; i < table->tableSize; ++i) {
        HashEntry *entry = &table->pEntries[i];
        void *obj = entry->data;
        if (obj == NULL || obj == HASH_TOMBSTONE) {
            continue;
        }
        pinObject(entry->data);
    }
    dvmHashTableUnlock(table);
    LOG_PIN("<<< pinHashTableEntries(table=%p)", table);
}

static void pinPrimitiveClasses()
{
    size_t length = ARRAYSIZE(gDvm.primitiveClass);
    for (size_t i = 0; i < length; i++) {
        if (gDvm.primitiveClass[i] != NULL) {
            pinObject((Object *)gDvm.primitiveClass[i]);
        }
    }
}

/*
 * Scavenge interned strings.  Permanent interned strings will have
 * been pinned and are therefore ignored.  Non-permanent strings that
 * have been forwarded are snapped.  All other entries are removed.
 */
static void scavengeInternedStrings()
{
    HashTable *table = gDvm.internedStrings;
    if (table == NULL) {
        return;
    }
    dvmHashTableLock(table);
    for (int i = 0; i < table->tableSize; ++i) {
        HashEntry *entry = &table->pEntries[i];
        Object *obj = (Object *)entry->data;
        if (obj == NULL || obj == HASH_TOMBSTONE) {
            continue;
        } else if (!isPermanentString((StringObject *)obj)) {
            // LOG_SCAV("entry->data=%p", entry->data);
            LOG_SCAV(">>> string obj=%p", entry->data);
            /* TODO(cshapiro): detach white string objects */
            scavengeReference((Object **)(void *)&entry->data);
            LOG_SCAV("<<< string obj=%p", entry->data);
        }
    }
    dvmHashTableUnlock(table);
}

static void pinInternedStrings()
{
    HashTable *table = gDvm.internedStrings;
    if (table == NULL) {
        return;
    }
    dvmHashTableLock(table);
    for (int i = 0; i < table->tableSize; ++i) {
        HashEntry *entry = &table->pEntries[i];
        Object *obj = (Object *)entry->data;
        if (obj == NULL || obj == HASH_TOMBSTONE) {
            continue;
        } else if (isPermanentString((StringObject *)obj)) {
            obj = (Object *)getPermanentString((StringObject*)obj);
            LOG_PROM(">>> pin string obj=%p", obj);
            pinObject(obj);
            LOG_PROM("<<< pin string obj=%p", obj);
        }
     }
    dvmHashTableUnlock(table);
}

/*
 * At present, reference tables contain references that must not be
 * moved by the collector.  Instead of scavenging each reference in
 * the table we pin each referenced object.
 */
static void pinReferenceTable(const ReferenceTable *table)
{
    assert(table != NULL);
    assert(table->table != NULL);
    assert(table->nextEntry != NULL);
    for (Object **entry = table->table; entry < table->nextEntry; ++entry) {
        assert(entry != NULL);
        assert(!isForward(*entry));
        pinObject(*entry);
    }
}

static void scavengeLargeHeapRefTable(LargeHeapRefTable *table)
{
    for (; table != NULL; table = table->next) {
        Object **ref = table->refs.table;
        for (; ref < table->refs.nextEntry; ++ref) {
            scavengeReference(ref);
        }
    }
}

/* This code was copied from Thread.c */
static void scavengeThreadStack(Thread *thread)
{
    const u4 *framePtr;
#if WITH_EXTRA_GC_CHECKS > 1
    bool first = true;
#endif

    framePtr = (const u4 *)thread->interpSave.curFrame;
    while (framePtr != NULL) {
        const StackSaveArea *saveArea;
        const Method *method;

        saveArea = SAVEAREA_FROM_FP(framePtr);
        method = saveArea->method;
        if (method != NULL && !dvmIsNativeMethod(method)) {
#ifdef COUNT_PRECISE_METHODS
            /* the GC is running, so no lock required */
            if (dvmPointerSetAddEntry(gDvm.preciseMethods, method))
                LOG_SCAV("PGC: added %s.%s %p",
                             method->clazz->descriptor, method->name, method);
#endif
#if WITH_EXTRA_GC_CHECKS > 1
            /*
             * May also want to enable the memset() in the "invokeMethod"
             * goto target in the portable interpreter.  That sets the stack
             * to a pattern that makes referring to uninitialized data
             * very obvious.
             */

            if (first) {
                /*
                 * First frame, isn't native, check the "alternate" saved PC
                 * as a sanity check.
                 *
                 * It seems like we could check the second frame if the first
                 * is native, since the PCs should be the same.  It turns out
                 * this doesn't always work.  The problem is that we could
                 * have calls in the sequence:
                 *   interp method #2
                 *   native method
                 *   interp method #1
                 *
                 * and then GC while in the native method after returning
                 * from interp method #2.  The currentPc on the stack is
                 * for interp method #1, but thread->currentPc2 is still
                 * set for the last thing interp method #2 did.
                 *
                 * This can also happen in normal execution:
                 * - sget-object on not-yet-loaded class
                 * - class init updates currentPc2
                 * - static field init is handled by parsing annotations;
                 *   static String init requires creation of a String object,
                 *   which can cause a GC
                 *
                 * Essentially, any pattern that involves executing
                 * interpreted code and then causes an allocation without
                 * executing instructions in the original method will hit
                 * this.  These are rare enough that the test still has
                 * some value.
                 */
                if (saveArea->xtra.currentPc != thread->currentPc2) {
                    ALOGW("PGC: savedPC(%p) != current PC(%p), %s.%s ins=%p",
                        saveArea->xtra.currentPc, thread->currentPc2,
                        method->clazz->descriptor, method->name, method->insns);
                    if (saveArea->xtra.currentPc != NULL)
                        ALOGE("  pc inst = 0x%04x", *saveArea->xtra.currentPc);
                    if (thread->currentPc2 != NULL)
                        ALOGE("  pc2 inst = 0x%04x", *thread->currentPc2);
                    dvmDumpThread(thread, false);
                }
            } else {
                /*
                 * It's unusual, but not impossible, for a non-first frame
                 * to be at something other than a method invocation.  For
                 * example, if we do a new-instance on a nonexistent class,
                 * we'll have a lot of class loader activity on the stack
                 * above the frame with the "new" operation.  Could also
                 * happen while we initialize a Throwable when an instruction
                 * fails.
                 *
                 * So there's not much we can do here to verify the PC,
                 * except to verify that it's a GC point.
                 */
            }
            assert(saveArea->xtra.currentPc != NULL);
#endif

            const RegisterMap* pMap;
            const u1* regVector;

            Method* nonConstMethod = (Method*) method;  // quiet gcc
            pMap = dvmGetExpandedRegisterMap(nonConstMethod);

            //LOG_SCAV("PGC: %s.%s", method->clazz->descriptor, method->name);

            if (pMap != NULL) {
                /* found map, get registers for this address */
                int addr = saveArea->xtra.currentPc - method->insns;
                regVector = dvmRegisterMapGetLine(pMap, addr);
                /*
                if (regVector == NULL) {
                    LOG_SCAV("PGC: map but no entry for %s.%s addr=0x%04x",
                                 method->clazz->descriptor, method->name, addr);
                } else {
                    LOG_SCAV("PGC: found map for %s.%s 0x%04x (t=%d)",
                                 method->clazz->descriptor, method->name, addr,
                                 thread->threadId);
                }
                */
            } else {
                /*
                 * No map found.  If precise GC is disabled this is
                 * expected -- we don't create pointers to the map data even
                 * if it's present -- but if it's enabled it means we're
                 * unexpectedly falling back on a conservative scan, so it's
                 * worth yelling a little.
                 */
                if (gDvm.preciseGc) {
                    LOG_SCAV("PGC: no map for %s.%s", method->clazz->descriptor, method->name);
                }
                regVector = NULL;
            }
            if (regVector == NULL) {
                /*
                 * There are no roots to scavenge.  Skip over the entire frame.
                 */
                framePtr += method->registersSize;
            } else {
                /*
                 * Precise scan.  v0 is at the lowest address on the
                 * interpreted stack, and is the first bit in the register
                 * vector, so we can walk through the register map and
                 * memory in the same direction.
                 *
                 * A '1' bit indicates a live reference.
                 */
                u2 bits = 1 << 1;
                for (int i = method->registersSize - 1; i >= 0; i--) {
                    u4 rval = *framePtr;

                    bits >>= 1;
                    if (bits == 1) {
                        /* set bit 9 so we can tell when we're empty */
                        bits = *regVector++ | 0x0100;
                    }

                    if (rval != 0 && (bits & 0x01) != 0) {
                        /*
                         * Non-null, register marked as live reference.  This
                         * should always be a valid object.
                         */
#if WITH_EXTRA_GC_CHECKS > 0
                        if ((rval & 0x3) != 0 || !dvmIsValidObject((Object*) rval)) {
                            /* this is very bad */
                            ALOGE("PGC: invalid ref in reg %d: 0x%08x",
                                method->registersSize-1 - i, rval);
                        } else
#endif
                        {

                            // LOG_SCAV("stack reference %u@%p", *framePtr, framePtr);
                            /* dvmMarkObjectNonNull((Object *)rval); */
                            scavengeReference((Object **) framePtr);
                        }
                    } else {
                        /*
                         * Null or non-reference, do nothing at all.
                         */
#if WITH_EXTRA_GC_CHECKS > 1
                        if (dvmIsValidObject((Object*) rval)) {
                            /* this is normal, but we feel chatty */
                            ALOGD("PGC: ignoring valid ref in reg %d: 0x%08x",
                                 method->registersSize-1 - i, rval);
                        }
#endif
                    }
                    ++framePtr;
                }
                dvmReleaseRegisterMapLine(pMap, regVector);
            }
        }
        /* else this is a break frame and there is nothing to gray, or
         * this is a native method and the registers are just the "ins",
         * copied from various registers in the caller's set.
         */

#if WITH_EXTRA_GC_CHECKS > 1
        first = false;
#endif

        /* Don't fall into an infinite loop if things get corrupted.
         */
        assert((uintptr_t)saveArea->prevFrame > (uintptr_t)framePtr ||
               saveArea->prevFrame == NULL);
        framePtr = saveArea->prevFrame;
    }
}

static void scavengeThread(Thread *thread)
{
    // LOG_SCAV("scavengeThread(thread=%p)", thread);

    // LOG_SCAV("Scavenging threadObj=%p", thread->threadObj);
    scavengeReference(&thread->threadObj);

    // LOG_SCAV("Scavenging exception=%p", thread->exception);
    scavengeReference(&thread->exception);

    scavengeThreadStack(thread);
}

static void scavengeThreadList()
{
    Thread *thread;

    dvmLockThreadList(dvmThreadSelf());
    thread = gDvm.threadList;
    while (thread) {
        scavengeThread(thread);
        thread = thread->next;
    }
    dvmUnlockThreadList();
}

static void pinThreadStack(const Thread *thread)
{
    const u4 *framePtr;
    const StackSaveArea *saveArea;
    Method *method;
    const char *shorty;
    Object *obj;

    saveArea = NULL;
    framePtr = (const u4 *)thread->interpSave.curFrame;
    for (; framePtr != NULL; framePtr = saveArea->prevFrame) {
        saveArea = SAVEAREA_FROM_FP(framePtr);
        method = (Method *)saveArea->method;
        if (method != NULL && dvmIsNativeMethod(method)) {
            /*
             * This is native method, pin its arguments.
             *
             * For purposes of graying references, we don't need to do
             * anything here, because all of the native "ins" were copied
             * from registers in the caller's stack frame and won't be
             * changed (an interpreted method can freely use registers
             * with parameters like any other register, but natives don't
             * work that way).
             *
             * However, we need to ensure that references visible to
             * native methods don't move around.  We can do a precise scan
             * of the arguments by examining the method signature.
             */
            LOG_PIN("+++ native scan %s.%s",
                    method->clazz->descriptor, method->name);
            assert(method->registersSize == method->insSize);
            if (!dvmIsStaticMethod(method)) {
                /* grab the "this" pointer */
                obj = (Object *)*framePtr++;
                if (obj == NULL) {
                    /*
                     * This can happen for the "fake" entry frame inserted
                     * for threads created outside the VM.  There's no actual
                     * call so there's no object.  If we changed the fake
                     * entry method to be declared "static" then this
                     * situation should never occur.
                     */
                } else {
                    assert(dvmIsValidObject(obj));
                    pinObject(obj);
                }
            }
            shorty = method->shorty+1;      // skip return value
            for (int i = method->registersSize - 1; i >= 0; i--, framePtr++) {
                switch (*shorty++) {
                case 'L':
                    obj = (Object *)*framePtr;
                    if (obj != NULL) {
                        assert(dvmIsValidObject(obj));
                        pinObject(obj);
                    }
                    break;
                case 'D':
                case 'J':
                    framePtr++;
                    break;
                default:
                    /* 32-bit non-reference value */
                    obj = (Object *)*framePtr;          // debug, remove
                    if (dvmIsValidObject(obj)) {        // debug, remove
                        /* if we see a lot of these, our scan might be off */
                        LOG_PIN("+++ did NOT pin obj %p", obj);
                    }
                    break;
                }
            }
        } else if (method != NULL && !dvmIsNativeMethod(method)) {
            const RegisterMap* pMap = dvmGetExpandedRegisterMap(method);
            const u1* regVector = NULL;

            ALOGI("conservative : %s.%s", method->clazz->descriptor, method->name);

            if (pMap != NULL) {
                int addr = saveArea->xtra.currentPc - method->insns;
                regVector = dvmRegisterMapGetLine(pMap, addr);
            }
            if (regVector == NULL) {
                /*
                 * No register info for this frame, conservatively pin.
                 */
                for (int i = 0; i < method->registersSize; ++i) {
                    u4 regValue = framePtr[i];
                    if (regValue != 0 && (regValue & 0x3) == 0 && dvmIsValidObject((Object *)regValue)) {
                        pinObject((Object *)regValue);
                    }
                }
            }
        }
        /*
         * Don't fall into an infinite loop if things get corrupted.
         */
        assert((uintptr_t)saveArea->prevFrame > (uintptr_t)framePtr ||
               saveArea->prevFrame == NULL);
    }
}

static void pinThread(const Thread *thread)
{
    assert(thread != NULL);
    LOG_PIN("pinThread(thread=%p)", thread);

    LOG_PIN("Pin native method arguments");
    pinThreadStack(thread);

    LOG_PIN("Pin internalLocalRefTable");
    pinReferenceTable(&thread->internalLocalRefTable);

    LOG_PIN("Pin jniLocalRefTable");
    pinReferenceTable(&thread->jniLocalRefTable);

    /* Can the check be pushed into the promote routine? */
    if (thread->jniMonitorRefTable.table) {
        LOG_PIN("Pin jniMonitorRefTable");
        pinReferenceTable(&thread->jniMonitorRefTable);
    }
}

static void pinThreadList()
{
    Thread *thread;

    dvmLockThreadList(dvmThreadSelf());
    thread = gDvm.threadList;
    while (thread) {
        pinThread(thread);
        thread = thread->next;
    }
    dvmUnlockThreadList();
}

/*
 * Heap block scavenging.
 */

/*
 * Scavenge objects in the current block.  Scavenging terminates when
 * the pointer reaches the highest address in the block or when a run
 * of zero words that continues to the highest address is reached.
 */
static void scavengeBlock(HeapSource *heapSource, size_t block)
{
    u1 *cursor;
    u1 *end;
    size_t size;

    LOG_SCAV("scavengeBlock(heapSource=%p,block=%zu)", heapSource, block);

    assert(heapSource != NULL);
    assert(block < heapSource->totalBlocks);
    assert(heapSource->blockSpace[block] == BLOCK_TO_SPACE);

    cursor = blockToAddress(heapSource, block);
    end = cursor + BLOCK_SIZE;
    LOG_SCAV("scavengeBlock start=%p, end=%p", cursor, end);

    /* Parse and scavenge the current block. */
    size = 0;
    while (cursor < end) {
        u4 word = *(u4 *)cursor;
        if (word != 0) {
            scavengeObject((Object *)cursor);
            size = objectSize((Object *)cursor);
            size = alignUp(size, ALLOC_ALIGNMENT);
            cursor += size;
        } else {
            /* Check for padding. */
            while (*(u4 *)cursor == 0) {
                cursor += 4;
                if (cursor == end) break;
            }
            /* Punt if something went wrong. */
            assert(cursor == end);
        }
    }
}

static size_t objectSize(const Object *obj)
{
    size_t size;

    assert(obj != NULL);
    assert(obj->clazz != NULL);
    if (obj->clazz == gDvm.classJavaLangClass) {
        size = dvmClassObjectSize((ClassObject *)obj);
    } else if (IS_CLASS_FLAG_SET(obj->clazz, CLASS_ISARRAY)) {
        size = dvmArrayObjectSize((ArrayObject *)obj);
    } else {
        assert(obj->clazz->objectSize != 0);
        size = obj->clazz->objectSize;
    }
    if (LW_HASH_STATE(obj->lock) == LW_HASH_STATE_HASHED_AND_MOVED) {
        size += sizeof(u4);
    }
    return size;
}

static void verifyBlock(HeapSource *heapSource, size_t block)
{
    u1 *cursor;
    u1 *end;
    size_t size;

    // LOG_VER("verifyBlock(heapSource=%p,block=%zu)", heapSource, block);

    assert(heapSource != NULL);
    assert(block < heapSource->totalBlocks);
    assert(heapSource->blockSpace[block] == BLOCK_TO_SPACE);

    cursor = blockToAddress(heapSource, block);
    end = cursor + BLOCK_SIZE;
    // LOG_VER("verifyBlock start=%p, end=%p", cursor, end);

    /* Parse and scavenge the current block. */
    size = 0;
    while (cursor < end) {
        u4 word = *(u4 *)cursor;
        if (word != 0) {
            dvmVerifyObject((Object *)cursor);
            size = objectSize((Object *)cursor);
            size = alignUp(size, ALLOC_ALIGNMENT);
            cursor += size;
        } else {
            /* Check for padding. */
            while (*(unsigned long *)cursor == 0) {
                cursor += 4;
                if (cursor == end) break;
            }
            /* Punt if something went wrong. */
            assert(cursor == end);
        }
    }
}

static void describeBlockQueue(const HeapSource *heapSource)
{
    size_t block, count;
    char space;

    block = heapSource->queueHead;
    count = 0;
    LOG_SCAV(">>> describeBlockQueue(heapSource=%p)", heapSource);
    /* Count the number of blocks enqueued. */
    while (block != QUEUE_TAIL) {
        block = heapSource->blockQueue[block];
        ++count;
    }
    LOG_SCAV("blockQueue %zu elements, enqueued %zu",
                 count, heapSource->queueSize);
    block = heapSource->queueHead;
    while (block != QUEUE_TAIL) {
        space = heapSource->blockSpace[block];
        LOG_SCAV("block=%zu@%p,space=%zu", block, blockToAddress(heapSource,block), space);
        block = heapSource->blockQueue[block];
    }

    LOG_SCAV("<<< describeBlockQueue(heapSource=%p)", heapSource);
}

/*
 * Blackens promoted objects.
 */
static void scavengeBlockQueue()
{
    HeapSource *heapSource;
    size_t block;

    LOG_SCAV(">>> scavengeBlockQueue()");
    heapSource = gDvm.gcHeap->heapSource;
    describeBlockQueue(heapSource);
    while (heapSource->queueHead != QUEUE_TAIL) {
        block = heapSource->queueHead;
        LOG_SCAV("Dequeueing block %zu", block);
        scavengeBlock(heapSource, block);
        heapSource->queueHead = heapSource->blockQueue[block];
        LOG_SCAV("New queue head is %zu", heapSource->queueHead);
    }
    LOG_SCAV("<<< scavengeBlockQueue()");
}

/*
 * Scan the block list and verify all blocks that are marked as being
 * in new space.  This should be parametrized so we can invoke this
 * routine outside of the context of a collection.
 */
static void verifyNewSpace()
{
    HeapSource *heapSource = gDvm.gcHeap->heapSource;
    size_t c0 = 0, c1 = 0, c2 = 0, c7 = 0;
    for (size_t i = 0; i < heapSource->totalBlocks; ++i) {
        switch (heapSource->blockSpace[i]) {
        case BLOCK_FREE: ++c0; break;
        case BLOCK_TO_SPACE: ++c1; break;
        case BLOCK_FROM_SPACE: ++c2; break;
        case BLOCK_CONTINUED: ++c7; break;
        default: assert(!"reached");
        }
    }
    LOG_VER("Block Demographics: "
            "Free=%zu,ToSpace=%zu,FromSpace=%zu,Continued=%zu",
            c0, c1, c2, c7);
    for (size_t i = 0; i < heapSource->totalBlocks; ++i) {
        if (heapSource->blockSpace[i] != BLOCK_TO_SPACE) {
            continue;
        }
        verifyBlock(heapSource, i);
    }
}

void describeHeap()
{
    HeapSource *heapSource = gDvm.gcHeap->heapSource;
    describeBlocks(heapSource);
}

/*
 * The collection interface.  Collection has a few distinct phases.
 * The first is flipping AKA condemning AKA whitening the heap.  The
 * second is to promote all objects which are pointed to by pinned or
 * ambiguous references.  The third phase is tracing from the stacks,
 * registers and various globals.  Lastly, a verification of the heap
 * is performed.  The last phase should be optional.
 */
void dvmScavengeRoots()  /* Needs a new name badly */
{
    GcHeap *gcHeap;

    {
        size_t alloc, unused, total;

        room(&alloc, &unused, &total);
        LOG_SCAV("BEFORE GC: %zu alloc, %zu free, %zu total.",
                     alloc, unused, total);
    }

    gcHeap = gDvm.gcHeap;
    dvmHeapSourceFlip();

    /*
     * Promote blocks with stationary objects.
     */
    pinThreadList();
    pinReferenceTable(&gDvm.jniGlobalRefTable);
    pinReferenceTable(&gDvm.jniPinRefTable);
    pinHashTableEntries(gDvm.loadedClasses);
    pinHashTableEntries(gDvm.dbgRegistry);
    pinPrimitiveClasses();
    pinInternedStrings();

    // describeBlocks(gcHeap->heapSource);

    /*
     * Create first, open new-space page right here.
     */

    /* Reset allocation to an unallocated block. */
    gDvm.gcHeap->heapSource->allocPtr = allocateBlocks(gDvm.gcHeap->heapSource, 1);
    gDvm.gcHeap->heapSource->allocLimit = gDvm.gcHeap->heapSource->allocPtr + BLOCK_SIZE;
    /*
     * Hack: promote the empty block allocated above.  If the
     * promotions that occurred above did not actually gray any
     * objects, the block queue may be empty.  We must force a
     * promotion to be safe.
     */
    promoteBlockByAddr(gDvm.gcHeap->heapSource, gDvm.gcHeap->heapSource->allocPtr);

    /*
     * Scavenge blocks and relocate movable objects.
     */

    LOG_SCAV("Scavenging gDvm.threadList");
    scavengeThreadList();

    LOG_SCAV("Scavenging gDvm.gcHeap->referenceOperations");
    scavengeLargeHeapRefTable(gcHeap->referenceOperations);

    LOG_SCAV("Scavenging gDvm.gcHeap->pendingFinalizationRefs");
    scavengeLargeHeapRefTable(gcHeap->pendingFinalizationRefs);

    LOG_SCAV("Scavenging random global stuff");
    scavengeReference(&gDvm.outOfMemoryObj);
    scavengeReference(&gDvm.internalErrorObj);
    scavengeReference(&gDvm.noClassDefFoundErrorObj);

    // LOG_SCAV("Scavenging gDvm.internedString");
    scavengeInternedStrings();

    LOG_SCAV("Root scavenge has completed.");

    scavengeBlockQueue();

    // LOG_SCAV("Re-snap global class pointers.");
    // scavengeGlobals();

    LOG_SCAV("New space scavenge has completed.");

    /*
     * Process reference objects in strength order.
     */

    LOG_REF("Processing soft references...");
    preserveSoftReferences(&gDvm.gcHeap->softReferences);
    clearWhiteReferences(&gDvm.gcHeap->softReferences);

    LOG_REF("Processing weak references...");
    clearWhiteReferences(&gDvm.gcHeap->weakReferences);

    LOG_REF("Finding finalizations...");
    processFinalizableReferences();

    LOG_REF("Processing f-reachable soft references...");
    clearWhiteReferences(&gDvm.gcHeap->softReferences);

    LOG_REF("Processing f-reachable weak references...");
    clearWhiteReferences(&gDvm.gcHeap->weakReferences);

    LOG_REF("Processing phantom references...");
    clearWhiteReferences(&gDvm.gcHeap->phantomReferences);

    /*
     * Verify the stack and heap.
     */
    dvmVerifyRoots();
    verifyNewSpace();

    //describeBlocks(gcHeap->heapSource);

    clearFromSpace(gcHeap->heapSource);

    {
        size_t alloc, rem, total;

        room(&alloc, &rem, &total);
        LOG_SCAV("AFTER GC: %zu alloc, %zu free, %zu total.", alloc, rem, total);
    }
}

/*
 * Interface compatibility routines.
 */

void dvmClearWhiteRefs(Object **list)
{
    /* do nothing */
    assert(*list == NULL);
}

void dvmHandleSoftRefs(Object **list)
{
    /* do nothing */
    assert(*list == NULL);
}

bool dvmHeapBeginMarkStep(GcMode mode)
{
    /* do nothing */
    return true;
}

void dvmHeapFinishMarkStep()
{
    /* do nothing */
}

void dvmHeapMarkRootSet()
{
    /* do nothing */
}

void dvmHeapScanMarkedObjects()
{
    dvmScavengeRoots();
}

void dvmHeapScheduleFinalizations()
{
    /* do nothing */
}

void dvmHeapSweepUnmarkedObjects(GcMode mode, int *numFreed, size_t *sizeFreed)
{
    *numFreed = 0;
    *sizeFreed = 0;
    /* do nothing */
}

void dvmMarkDirtyObjects()
{
    assert(!"implemented");
}

void dvmHeapSourceThreadShutdown()
{
    /* do nothing */
}
