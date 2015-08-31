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
 * Linear memory allocation, tied to class loaders.
 */
#include "Dalvik.h"

#include <sys/mman.h>
#include <limits.h>
#include <errno.h>

//#define DISABLE_LINEAR_ALLOC

// Use ashmem to name the LinearAlloc section
#define USE_ASHMEM 1

#ifdef USE_ASHMEM
#include <cutils/ashmem.h>
#endif /* USE_ASHMEM */

/*
Overview

This is intended to be a simple, fast allocator for "write-once" storage.
The expectation is that this will hold small allocations that don't change,
such as parts of classes (vtables, fields, methods, interfaces).  Because
the lifetime of these items is tied to classes, which in turn are tied
to class loaders, we associate the storage with a ClassLoader object.

[ We don't yet support class unloading, and our ClassLoader implementation
is in flux, so for now we just have a single global region and the
"classLoader" argument is ignored. ]

By storing the data here, rather than on the system heap, we reduce heap
clutter, speed class loading, reduce the memory footprint (reduced heap
structure overhead), and most importantly we increase the number of pages
that remain shared between processes launched in "Zygote mode".

The 4 bytes preceding each block contain the block length.  This allows us
to support "free" and "realloc" calls in a limited way.  We don't free
storage once it has been allocated, but in some circumstances it could be
useful to erase storage to garbage values after a "free" or "realloc".
(Bad idea if we're trying to share pages.)  We need to align to 8-byte
boundaries for some architectures, so we have a 50-50 chance of getting
this for free in a given block.

A NULL value for the "classLoader" argument refers to the bootstrap class
loader, which is never unloaded (until the VM shuts down).

Because the memory is not expected to be updated, we can use mprotect to
guard the pages on debug builds.  Handy when tracking down corruption.
*/

/* alignment for allocations; must be power of 2, and currently >= hdr_xtra */
#define BLOCK_ALIGN         8

/* default length of memory segment (worst case is probably "dexopt") */
#define DEFAULT_MAX_LENGTH  (16*1024*1024)

/* leave enough space for a length word */
#define HEADER_EXTRA        4

/* overload the length word */
#define LENGTHFLAG_FREE    0x80000000
#define LENGTHFLAG_RW      0x40000000
#define LENGTHFLAG_MASK    (~(LENGTHFLAG_FREE|LENGTHFLAG_RW))


/* fwd */
static void checkAllFree(Object* classLoader);


/*
 * Someday, retrieve the linear alloc struct associated with a particular
 * class loader.  For now, always use the boostrap loader's instance.
 */
static inline LinearAllocHdr* getHeader(Object* classLoader)
{
    return gDvm.pBootLoaderAlloc;
}

/*
 * Convert a pointer to memory to a pointer to the block header (which is
 * currently just a length word).
 */
static inline u4* getBlockHeader(void* mem)
{
    return ((u4*) mem) -1;
}

/*
 * Create a new linear allocation block.
 */
LinearAllocHdr* dvmLinearAllocCreate(Object* classLoader)
{
#ifdef DISABLE_LINEAR_ALLOC
    return (LinearAllocHdr*) 0x12345;
#endif
    LinearAllocHdr* pHdr;

    pHdr = (LinearAllocHdr*) malloc(sizeof(*pHdr));


    /*
     * "curOffset" points to the location of the next pre-block header,
     * which means we have to advance to the next BLOCK_ALIGN address and
     * back up.
     *
     * Note we leave the first page empty (see below), and start the
     * first entry on the second page at an offset that ensures the next
     * chunk of data will be properly aligned.
     */
    assert(BLOCK_ALIGN >= HEADER_EXTRA);
    pHdr->curOffset = pHdr->firstOffset =
        (BLOCK_ALIGN-HEADER_EXTRA) + SYSTEM_PAGE_SIZE;
    pHdr->mapLength = DEFAULT_MAX_LENGTH;

#ifdef USE_ASHMEM
    int fd;

    fd = ashmem_create_region("dalvik-LinearAlloc", DEFAULT_MAX_LENGTH);
    if (fd < 0) {
        ALOGE("ashmem LinearAlloc failed %s", strerror(errno));
        free(pHdr);
        return NULL;
    }

    pHdr->mapAddr = (char*)mmap(NULL, pHdr->mapLength, PROT_READ | PROT_WRITE,
        MAP_PRIVATE, fd, 0);
    if (pHdr->mapAddr == MAP_FAILED) {
        ALOGE("LinearAlloc mmap(%d) failed: %s", pHdr->mapLength,
            strerror(errno));
        free(pHdr);
        close(fd);
        return NULL;
    }

    close(fd);
#else /*USE_ASHMEM*/
    // MAP_ANON is listed as "deprecated" on Linux,
    // but MAP_ANONYMOUS is not defined under Mac OS X.
    pHdr->mapAddr = mmap(NULL, pHdr->mapLength, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (pHdr->mapAddr == MAP_FAILED) {
        ALOGE("LinearAlloc mmap(%d) failed: %s", pHdr->mapLength,
            strerror(errno));
        free(pHdr);
        return NULL;
    }
#endif /*USE_ASHMEM*/

    /* region expected to begin on a page boundary */
    assert(((int) pHdr->mapAddr & (SYSTEM_PAGE_SIZE-1)) == 0);

    /* the system should initialize newly-mapped memory to zero */
    assert(*(u4*) (pHdr->mapAddr + pHdr->curOffset) == 0);

    /*
     * Disable access to all except starting page.  We will enable pages
     * as we use them.  This helps prevent bad pointers from working.  The
     * pages start out PROT_NONE, become read/write while we access them,
     * then go to read-only after we finish our changes.
     *
     * We have to make the first page readable because we have 4 pad bytes,
     * followed by 4 length bytes, giving an initial offset of 8.  The
     * generic code below assumes that there could have been a previous
     * allocation that wrote into those 4 pad bytes, therefore the page
     * must have been marked readable by the previous allocation.
     *
     * We insert an extra page in here to force a break in the memory map
     * so we can see ourselves more easily in "showmap".  Otherwise this
     * stuff blends into the neighboring pages.  [TODO: do we still need
     * the extra page now that we have ashmem?]
     */
    if (mprotect(pHdr->mapAddr, pHdr->mapLength, PROT_NONE) != 0) {
        ALOGW("LinearAlloc init mprotect failed: %s", strerror(errno));
        free(pHdr);
        return NULL;
    }
    if (mprotect(pHdr->mapAddr + SYSTEM_PAGE_SIZE, SYSTEM_PAGE_SIZE,
            ENFORCE_READ_ONLY ? PROT_READ : PROT_READ|PROT_WRITE) != 0)
    {
        ALOGW("LinearAlloc init mprotect #2 failed: %s", strerror(errno));
        free(pHdr);
        return NULL;
    }

    if (ENFORCE_READ_ONLY) {
        /* allocate the per-page ref count */
        int numPages = (pHdr->mapLength+SYSTEM_PAGE_SIZE-1) / SYSTEM_PAGE_SIZE;
        pHdr->writeRefCount = (short*)calloc(numPages, sizeof(short));
        if (pHdr->writeRefCount == NULL) {
            free(pHdr);
            return NULL;
        }
    }

    dvmInitMutex(&pHdr->lock);

    ALOGV("LinearAlloc: created region at %p-%p",
        pHdr->mapAddr, pHdr->mapAddr + pHdr->mapLength-1);

    return pHdr;
}

/*
 * Destroy a linear allocation area.
 *
 * We do a trivial "has everything been freed?" check before unmapping the
 * memory and freeing the LinearAllocHdr.
 */
void dvmLinearAllocDestroy(Object* classLoader)
{
#ifdef DISABLE_LINEAR_ALLOC
    return;
#endif
    LinearAllocHdr* pHdr = getHeader(classLoader);
    if (pHdr == NULL)
        return;

    checkAllFree(classLoader);

    //dvmLinearAllocDump(classLoader);

    if (gDvm.verboseShutdown) {
        ALOGV("Unmapping linear allocator base=%p", pHdr->mapAddr);
        ALOGD("LinearAlloc %p used %d of %d (%d%%)",
            classLoader, pHdr->curOffset, pHdr->mapLength,
            (pHdr->curOffset * 100) / pHdr->mapLength);
    }

    if (munmap(pHdr->mapAddr, pHdr->mapLength) != 0) {
        ALOGW("LinearAlloc munmap(%p, %d) failed: %s",
            pHdr->mapAddr, pHdr->mapLength, strerror(errno));
    }
    free(pHdr);
}

/*
 * Allocate "size" bytes of storage, associated with a particular class
 * loader.
 *
 * It's okay for size to be zero.
 *
 * We always leave "curOffset" pointing at the next place where we will
 * store the header that precedes the returned storage.
 *
 * This aborts the VM on failure, so it's not necessary to check for a
 * NULL return value.
 */
void* dvmLinearAlloc(Object* classLoader, size_t size)
{
    LinearAllocHdr* pHdr = getHeader(classLoader);
    int startOffset, nextOffset;
    int lastGoodOff, firstWriteOff, lastWriteOff;

#ifdef DISABLE_LINEAR_ALLOC
    return calloc(1, size);
#endif

    LOGVV("--- LinearAlloc(%p, %d)", classLoader, size);

    /*
     * What we'd like to do is just determine the new end-of-alloc size
     * and atomic-swap the updated value in.  The trouble is that, the
     * first time we reach a new page, we need to call mprotect() to
     * make the page available, and we don't want to call mprotect() on
     * every allocation.  The troubled situation is:
     *  - thread A allocs across a page boundary, but gets preempted
     *    before mprotect() completes
     *  - thread B allocs within the new page, and doesn't call mprotect()
     */
    dvmLockMutex(&pHdr->lock);

    startOffset = pHdr->curOffset;
    assert(((startOffset + HEADER_EXTRA) & (BLOCK_ALIGN-1)) == 0);

    /*
     * Compute the new offset.  The old offset points at the address where
     * we will store the hidden block header, so we advance past that,
     * add the size of data they want, add another header's worth so we
     * know we have room for that, and round up to BLOCK_ALIGN.  That's
     * the next location where we'll put user data.  We then subtract the
     * chunk header size off so we're back to the header pointer.
     *
     * Examples:
     *   old=12 size=3 new=((12+(4*2)+3+7) & ~7)-4 = 24-4 --> 20
     *   old=12 size=5 new=((12+(4*2)+5+7) & ~7)-4 = 32-4 --> 28
     */
    nextOffset = ((startOffset + HEADER_EXTRA*2 + size + (BLOCK_ALIGN-1))
                    & ~(BLOCK_ALIGN-1)) - HEADER_EXTRA;
    LOGVV("--- old=%d size=%d new=%d", startOffset, size, nextOffset);

    if (nextOffset > pHdr->mapLength) {
        /*
         * We don't have to abort here.  We could fall back on the system
         * malloc(), and have our "free" call figure out what to do.  Only
         * works if the users of these functions actually free everything
         * they allocate.
         */
        ALOGE("LinearAlloc exceeded capacity (%d), last=%d",
            pHdr->mapLength, (int) size);
        dvmAbort();
    }

    /*
     * Round up "size" to encompass the entire region, including the 0-7
     * pad bytes before the next chunk header.  This way we get maximum
     * utility out of "realloc", and when we're doing ENFORCE_READ_ONLY
     * stuff we always treat the full extent.
     */
    size = nextOffset - (startOffset + HEADER_EXTRA);
    LOGVV("--- (size now %d)", size);

    /*
     * See if we are starting on or have crossed into a new page.  If so,
     * call mprotect on the page(s) we're about to write to.  We have to
     * page-align the start address, but don't have to make the length a
     * SYSTEM_PAGE_SIZE multiple (but we do it anyway).
     *
     * Note that "startOffset" is not the last *allocated* byte, but rather
     * the offset of the first *unallocated* byte (which we are about to
     * write the chunk header to).  "nextOffset" is similar.
     *
     * If ENFORCE_READ_ONLY is enabled, we have to call mprotect even if
     * we've written to this page before, because it might be read-only.
     */
    lastGoodOff = (startOffset-1) & ~(SYSTEM_PAGE_SIZE-1);
    firstWriteOff = startOffset & ~(SYSTEM_PAGE_SIZE-1);
    lastWriteOff = (nextOffset-1) & ~(SYSTEM_PAGE_SIZE-1);
    LOGVV("---  lastGood=0x%04x firstWrite=0x%04x lastWrite=0x%04x",
        lastGoodOff, firstWriteOff, lastWriteOff);
    if (lastGoodOff != lastWriteOff || ENFORCE_READ_ONLY) {
        int cc, start, len;

        start = firstWriteOff;
        assert(start <= nextOffset);
        len = (lastWriteOff - firstWriteOff) + SYSTEM_PAGE_SIZE;

        LOGVV("---    calling mprotect(start=%d len=%d RW)", start, len);
        cc = mprotect(pHdr->mapAddr + start, len, PROT_READ | PROT_WRITE);
        if (cc != 0) {
            ALOGE("LinearAlloc mprotect (+%d %d) failed: %s",
                start, len, strerror(errno));
            /* we're going to fail soon, might as do it now */
            dvmAbort();
        }
    }

    /* update the ref counts on the now-writable pages */
    if (ENFORCE_READ_ONLY) {
        int i, start, end;

        start = firstWriteOff / SYSTEM_PAGE_SIZE;
        end = lastWriteOff / SYSTEM_PAGE_SIZE;

        LOGVV("---  marking pages %d-%d RW (alloc %d at %p)",
            start, end, size, pHdr->mapAddr + startOffset + HEADER_EXTRA);
        for (i = start; i <= end; i++)
            pHdr->writeRefCount[i]++;
    }

    /* stow the size in the header */
    if (ENFORCE_READ_ONLY)
        *(u4*)(pHdr->mapAddr + startOffset) = size | LENGTHFLAG_RW;
    else
        *(u4*)(pHdr->mapAddr + startOffset) = size;

    /*
     * Update data structure.
     */
    pHdr->curOffset = nextOffset;

    dvmUnlockMutex(&pHdr->lock);
    return pHdr->mapAddr + startOffset + HEADER_EXTRA;
}

/*
 * Helper function, replaces strdup().
 */
char* dvmLinearStrdup(Object* classLoader, const char* str)
{
#ifdef DISABLE_LINEAR_ALLOC
    return strdup(str);
#endif
    int len = strlen(str);
    void* mem = dvmLinearAlloc(classLoader, len+1);
    memcpy(mem, str, len+1);
    if (ENFORCE_READ_ONLY)
        dvmLinearSetReadOnly(classLoader, mem);
    return (char*) mem;
}

/*
 * "Reallocate" a piece of memory.
 *
 * If the new size is <= the old size, we return the original pointer
 * without doing anything.
 *
 * If the new size is > the old size, we allocate new storage, copy the
 * old stuff over, and mark the new stuff as free.
 */
void* dvmLinearRealloc(Object* classLoader, void* mem, size_t newSize)
{
#ifdef DISABLE_LINEAR_ALLOC
    return realloc(mem, newSize);
#endif
    /* make sure we have the right region (and mem != NULL) */
    assert(mem != NULL);
    assert(mem >= (void*) getHeader(classLoader)->mapAddr &&
           mem < (void*) (getHeader(classLoader)->mapAddr +
                          getHeader(classLoader)->curOffset));

    const u4* pLen = getBlockHeader(mem);
    ALOGV("--- LinearRealloc(%d) old=%d", newSize, *pLen);

    /* handle size reduction case */
    if (*pLen >= newSize) {
        if (ENFORCE_READ_ONLY)
            dvmLinearSetReadWrite(classLoader, mem);
        return mem;
    }

    void* newMem;

    newMem = dvmLinearAlloc(classLoader, newSize);
    assert(newMem != NULL);
    memcpy(newMem, mem, *pLen);
    dvmLinearFree(classLoader, mem);

    return newMem;
}


/*
 * Update the read/write status of one or more pages.
 */
static void updatePages(Object* classLoader, void* mem, int direction)
{
    LinearAllocHdr* pHdr = getHeader(classLoader);
    dvmLockMutex(&pHdr->lock);

    /* make sure we have the right region */
    assert(mem >= (void*) pHdr->mapAddr &&
           mem < (void*) (pHdr->mapAddr + pHdr->curOffset));

    u4* pLen = getBlockHeader(mem);
    u4 len = *pLen & LENGTHFLAG_MASK;
    int firstPage, lastPage;

    firstPage = ((u1*)pLen - (u1*)pHdr->mapAddr) / SYSTEM_PAGE_SIZE;
    lastPage = ((u1*)mem - (u1*)pHdr->mapAddr + (len-1)) / SYSTEM_PAGE_SIZE;
    LOGVV("--- updating pages %d-%d (%d)", firstPage, lastPage, direction);

    int i, cc;

    /*
     * Update individual pages.  We could do some sort of "lazy update" to
     * combine mprotect calls, but that's almost certainly more trouble
     * than it's worth.
     */
    for (i = firstPage; i <= lastPage; i++) {
        if (direction < 0) {
            /*
             * Trying to mark read-only.
             */
            if (i == firstPage) {
                if ((*pLen & LENGTHFLAG_RW) == 0) {
                    ALOGW("Double RO on %p", mem);
                    dvmAbort();
                } else
                    *pLen &= ~LENGTHFLAG_RW;
            }

            if (pHdr->writeRefCount[i] == 0) {
                ALOGE("Can't make page %d any less writable", i);
                dvmAbort();
            }
            pHdr->writeRefCount[i]--;
            if (pHdr->writeRefCount[i] == 0) {
                LOGVV("---  prot page %d RO", i);
                cc = mprotect(pHdr->mapAddr + SYSTEM_PAGE_SIZE * i,
                        SYSTEM_PAGE_SIZE, PROT_READ);
                assert(cc == 0);
            }
        } else {
            /*
             * Trying to mark writable.
             */
            if (pHdr->writeRefCount[i] >= 32767) {
                ALOGE("Can't make page %d any more writable", i);
                dvmAbort();
            }
            if (pHdr->writeRefCount[i] == 0) {
                LOGVV("---  prot page %d RW", i);
                cc = mprotect(pHdr->mapAddr + SYSTEM_PAGE_SIZE * i,
                        SYSTEM_PAGE_SIZE, PROT_READ | PROT_WRITE);
                assert(cc == 0);
            }
            pHdr->writeRefCount[i]++;

            if (i == firstPage) {
                if ((*pLen & LENGTHFLAG_RW) != 0) {
                    ALOGW("Double RW on %p", mem);
                    dvmAbort();
                } else
                    *pLen |= LENGTHFLAG_RW;
            }
        }
    }

    dvmUnlockMutex(&pHdr->lock);
}

/*
 * Try to mark the pages in which a chunk of memory lives as read-only.
 * Whether or not the pages actually change state depends on how many
 * others are trying to access the same pages.
 *
 * Only call here if ENFORCE_READ_ONLY is true.
 */
void dvmLinearSetReadOnly(Object* classLoader, void* mem)
{
#ifdef DISABLE_LINEAR_ALLOC
    return;
#endif
    updatePages(classLoader, mem, -1);
}

/*
 * Make the pages on which "mem" sits read-write.
 *
 * This covers the header as well as the data itself.  (We could add a
 * "header-only" mode for dvmLinearFree.)
 *
 * Only call here if ENFORCE_READ_ONLY is true.
 */
void dvmLinearSetReadWrite(Object* classLoader, void* mem)
{
#ifdef DISABLE_LINEAR_ALLOC
    return;
#endif
    updatePages(classLoader, mem, 1);
}

/*
 * Mark an allocation as free.
 */
void dvmLinearFree(Object* classLoader, void* mem)
{
#ifdef DISABLE_LINEAR_ALLOC
    free(mem);
    return;
#endif
    if (mem == NULL)
        return;

    /* make sure we have the right region */
    assert(mem >= (void*) getHeader(classLoader)->mapAddr &&
           mem < (void*) (getHeader(classLoader)->mapAddr +
                          getHeader(classLoader)->curOffset));

    if (ENFORCE_READ_ONLY)
        dvmLinearSetReadWrite(classLoader, mem);

    u4* pLen = getBlockHeader(mem);
    *pLen |= LENGTHFLAG_FREE;

    if (ENFORCE_READ_ONLY)
        dvmLinearSetReadOnly(classLoader, mem);
}

/*
 * For debugging, dump the contents of a linear alloc area.
 *
 * We grab the lock so that the header contents and list output are
 * consistent.
 */
void dvmLinearAllocDump(Object* classLoader)
{
#ifdef DISABLE_LINEAR_ALLOC
    return;
#endif
    LinearAllocHdr* pHdr = getHeader(classLoader);

    dvmLockMutex(&pHdr->lock);

    ALOGI("LinearAlloc classLoader=%p", classLoader);
    ALOGI("  mapAddr=%p mapLength=%d firstOffset=%d",
        pHdr->mapAddr, pHdr->mapLength, pHdr->firstOffset);
    ALOGI("  curOffset=%d", pHdr->curOffset);

    int off = pHdr->firstOffset;
    u4 rawLen, fullLen;

    while (off < pHdr->curOffset) {
        rawLen = *(u4*) (pHdr->mapAddr + off);
        fullLen = ((HEADER_EXTRA*2 + (rawLen & LENGTHFLAG_MASK))
                    & ~(BLOCK_ALIGN-1));

        ALOGI("  %p (%3d): %clen=%d%s", pHdr->mapAddr + off + HEADER_EXTRA,
            (int) ((off + HEADER_EXTRA) / SYSTEM_PAGE_SIZE),
            (rawLen & LENGTHFLAG_FREE) != 0 ? '*' : ' ',
            rawLen & LENGTHFLAG_MASK,
            (rawLen & LENGTHFLAG_RW) != 0 ? " [RW]" : "");

        off += fullLen;
    }

    if (ENFORCE_READ_ONLY) {
        ALOGI("writeRefCount map:");

        int numPages = (pHdr->mapLength+SYSTEM_PAGE_SIZE-1) / SYSTEM_PAGE_SIZE;
        int zstart = 0;
        int i;

        for (i = 0; i < numPages; i++) {
            int count = pHdr->writeRefCount[i];

            if (count != 0) {
                if (zstart < i-1)
                    printf(" %d-%d: zero\n", zstart, i-1);
                else if (zstart == i-1)
                    printf(" %d: zero\n", zstart);
                zstart = i+1;
                printf(" %d: %d\n", i, count);
            }
        }
        if (zstart < i)
            printf(" %d-%d: zero\n", zstart, i-1);
    }

    ALOGD("LinearAlloc %p using %d of %d (%d%%)",
        classLoader, pHdr->curOffset, pHdr->mapLength,
        (pHdr->curOffset * 100) / pHdr->mapLength);

    dvmUnlockMutex(&pHdr->lock);
}

/*
 * Verify that all blocks are freed.
 *
 * This should only be done as we're shutting down, but there could be a
 * daemon thread that's still trying to do something, so we grab the locks.
 */
static void checkAllFree(Object* classLoader)
{
#ifdef DISABLE_LINEAR_ALLOC
    return;
#endif
    LinearAllocHdr* pHdr = getHeader(classLoader);

    dvmLockMutex(&pHdr->lock);

    int off = pHdr->firstOffset;
    u4 rawLen, fullLen;

    while (off < pHdr->curOffset) {
        rawLen = *(u4*) (pHdr->mapAddr + off);
        fullLen = ((HEADER_EXTRA*2 + (rawLen & LENGTHFLAG_MASK))
                    & ~(BLOCK_ALIGN-1));

        if ((rawLen & LENGTHFLAG_FREE) == 0) {
            ALOGW("LinearAlloc %p not freed: %p len=%d", classLoader,
                pHdr->mapAddr + off + HEADER_EXTRA, rawLen & LENGTHFLAG_MASK);
        }

        off += fullLen;
    }

    dvmUnlockMutex(&pHdr->lock);
}

/*
 * Determine if [start, start+length) is contained in the in-use area of
 * a single LinearAlloc.  The full set of linear allocators is scanned.
 *
 * [ Since we currently only have one region, this is pretty simple.  In
 * the future we'll need to traverse a table of class loaders. ]
 */
bool dvmLinearAllocContains(const void* start, size_t length)
{
    LinearAllocHdr* pHdr = getHeader(NULL);

    if (pHdr == NULL)
        return false;

    return (char*) start >= pHdr->mapAddr &&
           ((char*)start + length) <= (pHdr->mapAddr + pHdr->curOffset);
}
