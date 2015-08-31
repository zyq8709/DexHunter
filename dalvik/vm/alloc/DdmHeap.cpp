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
 * DDM-related heap functions
 */
#include <sys/time.h>
#include <time.h>

#include "Dalvik.h"
#include "alloc/Heap.h"
#include "alloc/HeapInternal.h"
#include "alloc/DdmHeap.h"
#include "alloc/DlMalloc.h"
#include "alloc/HeapSource.h"

#define DEFAULT_HEAP_ID  1

enum HpifWhen {
    HPIF_WHEN_NEVER = 0,
    HPIF_WHEN_NOW = 1,
    HPIF_WHEN_NEXT_GC = 2,
    HPIF_WHEN_EVERY_GC = 3
};

/*
 * Chunk HPIF (client --> server)
 *
 * Heap Info. General information about the heap,
 * suitable for a summary display.
 *
 *   [u4]: number of heaps
 *
 *   For each heap:
 *     [u4]: heap ID
 *     [u8]: timestamp in ms since Unix epoch
 *     [u1]: capture reason (same as 'when' value from server)
 *     [u4]: max heap size in bytes (-Xmx)
 *     [u4]: current heap size in bytes
 *     [u4]: current number of bytes allocated
 *     [u4]: current number of objects allocated
 */
#define HPIF_SIZE(numHeaps) \
        (sizeof(u4) + (numHeaps) * (5 * sizeof(u4) + sizeof(u1) + sizeof(u8)))
void dvmDdmSendHeapInfo(int reason, bool shouldLock)
{
    struct timeval now;
    u8 nowMs;
    u1 *buf, *b;

    buf = (u1 *)malloc(HPIF_SIZE(1));
    if (buf == NULL) {
        return;
    }
    b = buf;

    /* If there's a one-shot 'when', reset it.
     */
    if (reason == gDvm.gcHeap->ddmHpifWhen) {
        if (shouldLock && ! dvmLockHeap()) {
            ALOGW("%s(): can't lock heap to clear when", __func__);
            goto skip_when;
        }
        if (reason == gDvm.gcHeap->ddmHpifWhen) {
            if (gDvm.gcHeap->ddmHpifWhen == HPIF_WHEN_NEXT_GC) {
                gDvm.gcHeap->ddmHpifWhen = HPIF_WHEN_NEVER;
            }
        }
        if (shouldLock) {
            dvmUnlockHeap();
        }
    }
skip_when:

    /* The current time, in milliseconds since 0:00 GMT, 1/1/70.
     */
    if (gettimeofday(&now, NULL) < 0) {
        nowMs = 0;
    } else {
        nowMs = (u8)now.tv_sec * 1000 + now.tv_usec / 1000;
    }

    /* number of heaps */
    set4BE(b, 1); b += 4;

    /* For each heap (of which there is one) */
    {
        /* heap ID */
        set4BE(b, DEFAULT_HEAP_ID); b += 4;

        /* timestamp */
        set8BE(b, nowMs); b += 8;

        /* 'when' value */
        *b++ = (u1)reason;

        /* max allowed heap size in bytes */
        set4BE(b, dvmHeapSourceGetMaximumSize()); b += 4;

        /* current heap size in bytes */
        set4BE(b, dvmHeapSourceGetValue(HS_FOOTPRINT, NULL, 0)); b += 4;

        /* number of bytes allocated */
        set4BE(b, dvmHeapSourceGetValue(HS_BYTES_ALLOCATED, NULL, 0)); b += 4;

        /* number of objects allocated */
        set4BE(b, dvmHeapSourceGetValue(HS_OBJECTS_ALLOCATED, NULL, 0)); b += 4;
    }
    assert((intptr_t)b == (intptr_t)buf + (intptr_t)HPIF_SIZE(1));

    dvmDbgDdmSendChunk(CHUNK_TYPE("HPIF"), b - buf, buf);
}

bool dvmDdmHandleHpifChunk(int when)
{
    switch (when) {
    case HPIF_WHEN_NOW:
        dvmDdmSendHeapInfo(when, true);
        break;
    case HPIF_WHEN_NEVER:
    case HPIF_WHEN_NEXT_GC:
    case HPIF_WHEN_EVERY_GC:
        if (dvmLockHeap()) {
            gDvm.gcHeap->ddmHpifWhen = when;
            dvmUnlockHeap();
        } else {
            ALOGI("%s(): can't lock heap to set when", __func__);
            return false;
        }
        break;
    default:
        ALOGI("%s(): bad when value 0x%08x", __func__, when);
        return false;
    }

    return true;
}

enum HpsgSolidity {
    SOLIDITY_FREE = 0,
    SOLIDITY_HARD = 1,
    SOLIDITY_SOFT = 2,
    SOLIDITY_WEAK = 3,
    SOLIDITY_PHANTOM = 4,
    SOLIDITY_FINALIZABLE = 5,
    SOLIDITY_SWEEP = 6,
};

enum HpsgKind {
    KIND_OBJECT = 0,
    KIND_CLASS_OBJECT = 1,
    KIND_ARRAY_1 = 2,
    KIND_ARRAY_2 = 3,
    KIND_ARRAY_4 = 4,
    KIND_ARRAY_8 = 5,
    KIND_UNKNOWN = 6,
    KIND_NATIVE = 7,
};

#define HPSG_PARTIAL (1<<7)
#define HPSG_STATE(solidity, kind) \
    ((u1)((((kind) & 0x7) << 3) | ((solidity) & 0x7)))

struct HeapChunkContext {
    void* startOfNextMemoryChunk;
    u1 *buf;
    u1 *p;
    u1 *pieceLenField;
    size_t bufLen;
    size_t totalAllocationUnits;
    int type;
    bool merge;
    bool needHeader;
};

#define ALLOCATION_UNIT_SIZE 8

static void flush_hpsg_chunk(HeapChunkContext *ctx)
{
    if (ctx->pieceLenField == NULL && ctx->needHeader) {
        /* Already flushed */
        return;
    }
    /* Patch the "length of piece" field.
     */
    assert(ctx->buf <= ctx->pieceLenField &&
            ctx->pieceLenField <= ctx->p);
    set4BE(ctx->pieceLenField, ctx->totalAllocationUnits);

    /* Send the chunk.
     */
    dvmDbgDdmSendChunk(ctx->type, ctx->p - ctx->buf, ctx->buf);

    /* Reset the context.
     */
    ctx->p = ctx->buf;
    ctx->totalAllocationUnits = 0;
    ctx->needHeader = true;
    ctx->pieceLenField = NULL;
}

static void append_chunk(HeapChunkContext *ctx, u1 state, void* ptr, size_t length) {
    /* Make sure there's enough room left in the buffer.
     * We need to use two bytes for every fractional 256
     * allocation units used by the chunk and 17 bytes for
     * any header.
     */
    {
        size_t needed = (((length/ALLOCATION_UNIT_SIZE + 255) / 256) * 2) + 17;
        size_t bytesLeft = ctx->bufLen - (size_t)(ctx->p - ctx->buf);
        if (bytesLeft < needed) {
            flush_hpsg_chunk(ctx);
        }
        bytesLeft = ctx->bufLen - (size_t)(ctx->p - ctx->buf);
        if (bytesLeft < needed) {
            ALOGW("chunk is too big to transmit (length=%zd, %zd bytes)",
                  length, needed);
            return;
        }
    }
    if (ctx->needHeader) {
        /*
         * Start a new HPSx chunk.
         */

        /* [u4]: heap ID */
        set4BE(ctx->p, DEFAULT_HEAP_ID); ctx->p += 4;

        /* [u1]: size of allocation unit, in bytes */
        *ctx->p++ = 8;

        /* [u4]: virtual address of segment start */
        set4BE(ctx->p, (uintptr_t)ptr); ctx->p += 4;

        /* [u4]: offset of this piece (relative to the virtual address) */
        set4BE(ctx->p, 0); ctx->p += 4;

        /* [u4]: length of piece, in allocation units
         * We won't know this until we're done, so save the offset
         * and stuff in a dummy value.
         */
        ctx->pieceLenField = ctx->p;
        set4BE(ctx->p, 0x55555555); ctx->p += 4;

        ctx->needHeader = false;
    }
    /* Write out the chunk description.
     */
    length /= ALLOCATION_UNIT_SIZE;   // convert to allocation units
    ctx->totalAllocationUnits += length;
    while (length > 256) {
        *ctx->p++ = state | HPSG_PARTIAL;
        *ctx->p++ = 255;     // length - 1
        length -= 256;
    }
    *ctx->p++ = state;
    *ctx->p++ = length - 1;
}

/*
 * Called by dlmalloc_inspect_all. If used_bytes != 0 then start is
 * the start of a malloc-ed piece of memory of size used_bytes. If
 * start is 0 then start is the beginning of any free space not
 * including dlmalloc's book keeping and end the start of the next
 * dlmalloc chunk. Regions purely containing book keeping don't
 * callback.
 */
static void heap_chunk_callback(void* start, void* end, size_t used_bytes,
                                void* arg)
{
    u1 state;
    HeapChunkContext *ctx = (HeapChunkContext *)arg;
    UNUSED_PARAMETER(end);

    if (used_bytes == 0) {
        if (start == NULL) {
            // Reset for start of new heap.
            ctx->startOfNextMemoryChunk = NULL;
            flush_hpsg_chunk(ctx);
        }
        // Only process in use memory so that free region information
        // also includes dlmalloc book keeping.
        return;
    }

    /* If we're looking at the native heap, we'll just return
     * (SOLIDITY_HARD, KIND_NATIVE) for all allocated chunks
     */
    bool native = ctx->type == CHUNK_TYPE("NHSG");

    if (ctx->startOfNextMemoryChunk != NULL) {
        // Transmit any pending free memory. Native free memory of
        // over kMaxFreeLen could be because of the use of mmaps, so
        // don't report. If not free memory then start a new segment.
        bool flush = true;
        if (start > ctx->startOfNextMemoryChunk) {
            const size_t kMaxFreeLen = 2 * SYSTEM_PAGE_SIZE;
            void* freeStart = ctx->startOfNextMemoryChunk;
            void* freeEnd = start;
            size_t freeLen = (char*)freeEnd - (char*)freeStart;
            if (!native || freeLen < kMaxFreeLen) {
                append_chunk(ctx, HPSG_STATE(SOLIDITY_FREE, 0),
                             freeStart, freeLen);
                flush = false;
            }
        }
        if (flush) {
            ctx->startOfNextMemoryChunk = NULL;
            flush_hpsg_chunk(ctx);
        }
    }
    const Object *obj = (const Object *)start;

    /* It's an allocated chunk.  Figure out what it is.
     */
//TODO: if ctx.merge, see if this chunk is different from the last chunk.
//      If it's the same, we should combine them.
    if (!native && dvmIsValidObject(obj)) {
        ClassObject *clazz = obj->clazz;
        if (clazz == NULL) {
            /* The object was probably just created
             * but hasn't been initialized yet.
             */
            state = HPSG_STATE(SOLIDITY_HARD, KIND_OBJECT);
        } else if (dvmIsTheClassClass(clazz)) {
            state = HPSG_STATE(SOLIDITY_HARD, KIND_CLASS_OBJECT);
        } else if (IS_CLASS_FLAG_SET(clazz, CLASS_ISARRAY)) {
            if (IS_CLASS_FLAG_SET(clazz, CLASS_ISOBJECTARRAY)) {
                state = HPSG_STATE(SOLIDITY_HARD, KIND_ARRAY_4);
            } else {
                switch (clazz->elementClass->primitiveType) {
                case PRIM_BOOLEAN:
                case PRIM_BYTE:
                    state = HPSG_STATE(SOLIDITY_HARD, KIND_ARRAY_1);
                    break;
                case PRIM_CHAR:
                case PRIM_SHORT:
                    state = HPSG_STATE(SOLIDITY_HARD, KIND_ARRAY_2);
                    break;
                case PRIM_INT:
                case PRIM_FLOAT:
                    state = HPSG_STATE(SOLIDITY_HARD, KIND_ARRAY_4);
                    break;
                case PRIM_DOUBLE:
                case PRIM_LONG:
                    state = HPSG_STATE(SOLIDITY_HARD, KIND_ARRAY_8);
                    break;
                default:
                    assert(!"Unknown GC heap object type");
                    state = HPSG_STATE(SOLIDITY_HARD, KIND_UNKNOWN);
                    break;
                }
            }
        } else {
            state = HPSG_STATE(SOLIDITY_HARD, KIND_OBJECT);
        }
    } else {
        obj = NULL; // it's not actually an object
        state = HPSG_STATE(SOLIDITY_HARD, KIND_NATIVE);
    }
    append_chunk(ctx, state, start, used_bytes + HEAP_SOURCE_CHUNK_OVERHEAD);
    ctx->startOfNextMemoryChunk =
        (char*)start + used_bytes + HEAP_SOURCE_CHUNK_OVERHEAD;
}

enum HpsgWhen {
    HPSG_WHEN_NEVER = 0,
    HPSG_WHEN_EVERY_GC = 1,
};
enum HpsgWhat {
    HPSG_WHAT_MERGED_OBJECTS = 0,
    HPSG_WHAT_DISTINCT_OBJECTS = 1,
};

/*
 * Maximum chunk size.  Obtain this from the formula:
 *
 * (((maximum_heap_size / ALLOCATION_UNIT_SIZE) + 255) / 256) * 2
 */
#define HPSx_CHUNK_SIZE (16384 - 16)

static void walkHeap(bool merge, bool native)
{
    HeapChunkContext ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.bufLen = HPSx_CHUNK_SIZE;
    ctx.buf = (u1 *)malloc(ctx.bufLen);
    if (ctx.buf == NULL) {
        return;
    }

    ctx.merge = merge;
    if (native) {
        ctx.type = CHUNK_TYPE("NHSG");
    } else {
        if (ctx.merge) {
            ctx.type = CHUNK_TYPE("HPSG");
        } else {
            ctx.type = CHUNK_TYPE("HPSO");
        }
    }

    ctx.p = ctx.buf;
    ctx.needHeader = true;
    if (native) {
        dlmalloc_inspect_all(heap_chunk_callback, (void*)&ctx);
    } else {
        dvmHeapSourceWalk(heap_chunk_callback, (void *)&ctx);
    }
    if (ctx.p > ctx.buf) {
        flush_hpsg_chunk(&ctx);
    }

    free(ctx.buf);
}

void dvmDdmSendHeapSegments(bool shouldLock, bool native)
{
    u1 heapId[sizeof(u4)];
    GcHeap *gcHeap = gDvm.gcHeap;
    int when, what;
    bool merge;

    /* Don't even grab the lock if there's nothing to do when we're called.
     */
    if (!native) {
        when = gcHeap->ddmHpsgWhen;
        what = gcHeap->ddmHpsgWhat;
        if (when == HPSG_WHEN_NEVER) {
            return;
        }
    } else {
        when = gcHeap->ddmNhsgWhen;
        what = gcHeap->ddmNhsgWhat;
        if (when == HPSG_WHEN_NEVER) {
            return;
        }
    }
    if (shouldLock && !dvmLockHeap()) {
        ALOGW("Can't lock heap for DDM HPSx dump");
        return;
    }

    /* Figure out what kind of chunks we'll be sending.
     */
    if (what == HPSG_WHAT_MERGED_OBJECTS) {
        merge = true;
    } else if (what == HPSG_WHAT_DISTINCT_OBJECTS) {
        merge = false;
    } else {
        assert(!"bad HPSG.what value");
        return;
    }

    /* First, send a heap start chunk.
     */
    set4BE(heapId, DEFAULT_HEAP_ID);
    dvmDbgDdmSendChunk(native ? CHUNK_TYPE("NHST") : CHUNK_TYPE("HPST"),
        sizeof(u4), heapId);

    /* Send a series of heap segment chunks.
     */
    walkHeap(merge, native);

    /* Finally, send a heap end chunk.
     */
    dvmDbgDdmSendChunk(native ? CHUNK_TYPE("NHEN") : CHUNK_TYPE("HPEN"),
        sizeof(u4), heapId);

    if (shouldLock) {
        dvmUnlockHeap();
    }
}

bool dvmDdmHandleHpsgNhsgChunk(int when, int what, bool native)
{
    ALOGI("dvmDdmHandleHpsgChunk(when %d, what %d, heap %d)", when, what,
         native);
    switch (when) {
    case HPSG_WHEN_NEVER:
    case HPSG_WHEN_EVERY_GC:
        break;
    default:
        ALOGI("%s(): bad when value 0x%08x", __func__, when);
        return false;
    }

    switch (what) {
    case HPSG_WHAT_MERGED_OBJECTS:
    case HPSG_WHAT_DISTINCT_OBJECTS:
        break;
    default:
        ALOGI("%s(): bad what value 0x%08x", __func__, what);
        return false;
    }

    if (dvmLockHeap()) {
        if (!native) {
            gDvm.gcHeap->ddmHpsgWhen = when;
            gDvm.gcHeap->ddmHpsgWhat = what;
        } else {
            gDvm.gcHeap->ddmNhsgWhen = when;
            gDvm.gcHeap->ddmNhsgWhat = what;
        }
//TODO: if what says we should dump immediately, signal (or do) it from here
        dvmUnlockHeap();
    } else {
        ALOGI("%s(): can't lock heap to set when/what", __func__);
        return false;
    }

    return true;
}
