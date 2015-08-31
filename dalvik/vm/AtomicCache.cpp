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
 * Mutex-free cache.  Each entry has two 32-bit keys, one 32-bit value,
 * and a 32-bit version.
 */
#include "Dalvik.h"

#include <stdlib.h>

/*
 * I think modern C mandates that the results of a boolean expression are
 * 0 or 1.  If not, or we suddenly turn into C++ and bool != int, use this.
 */
#define BOOL_TO_INT(x)  (x)
//#define BOOL_TO_INT(x)  ((x) ? 1 : 0)

#define CPU_CACHE_WIDTH         32
#define CPU_CACHE_WIDTH_1       (CPU_CACHE_WIDTH-1)

#define ATOMIC_LOCK_FLAG        (1 << 31)

/*
 * Allocate cache.
 */
AtomicCache* dvmAllocAtomicCache(int numEntries)
{
    AtomicCache* newCache;

    newCache = (AtomicCache*) calloc(1, sizeof(AtomicCache));
    if (newCache == NULL)
        return NULL;

    newCache->numEntries = numEntries;

    newCache->entryAlloc = calloc(1,
        sizeof(AtomicCacheEntry) * numEntries + CPU_CACHE_WIDTH);
    if (newCache->entryAlloc == NULL) {
        free(newCache);
        return NULL;
    }

    /*
     * Adjust storage to align on a 32-byte boundary.  Each entry is 16 bytes
     * wide.  This ensures that each cache entry sits on a single CPU cache
     * line.
     */
    assert(sizeof(AtomicCacheEntry) == 16);
    newCache->entries = (AtomicCacheEntry*)
        (((int) newCache->entryAlloc + CPU_CACHE_WIDTH_1) & ~CPU_CACHE_WIDTH_1);

    return newCache;
}

/*
 * Free cache.
 */
void dvmFreeAtomicCache(AtomicCache* cache)
{
    if (cache != NULL) {
        free(cache->entryAlloc);
        free(cache);
    }
}


/*
 * Update a cache entry.
 *
 * In the event of a collision with another thread, the update may be skipped.
 *
 * We only need "pCache" for stats.
 */
void dvmUpdateAtomicCache(u4 key1, u4 key2, u4 value, AtomicCacheEntry* pEntry,
    u4 firstVersion
#if CALC_CACHE_STATS > 0
    , AtomicCache* pCache
#endif
    )
{
    /*
     * The fields don't match, so we want to update them.  There is a risk
     * that another thread is also trying to update them, so we grab an
     * ownership flag to lock out other threads.
     *
     * If the lock flag was already set in "firstVersion", somebody else
     * was in mid-update, and we don't want to continue here.  (This means
     * that using "firstVersion" as the "before" argument to the CAS would
     * succeed when it shouldn't and vice-versa -- we could also just pass
     * in (firstVersion & ~ATOMIC_LOCK_FLAG) as the first argument.)
     *
     * NOTE: we don't deal with the situation where we overflow the version
     * counter and trample the ATOMIC_LOCK_FLAG (at 2^31).  Probably not
     * a real concern.
     */
    if ((firstVersion & ATOMIC_LOCK_FLAG) != 0 ||
        android_atomic_release_cas(
                firstVersion, firstVersion | ATOMIC_LOCK_FLAG,
                (volatile s4*) &pEntry->version) != 0)
    {
        /*
         * We couldn't get the write lock.  Return without updating the table.
         */
#if CALC_CACHE_STATS > 0
        pCache->fail++;
#endif
        return;
    }

    /* must be even-valued on entry */
    assert((firstVersion & 0x01) == 0);

#if CALC_CACHE_STATS > 0
    /* for stats, assume a key value of zero indicates an empty entry */
    if (pEntry->key1 == 0)
        pCache->fills++;
    else
        pCache->misses++;
#endif

    /*
     * We have the write lock, but somebody could be reading this entry
     * while we work.  We use memory barriers to ensure that the state
     * is always consistent when the version number is even.
     */
    u4 newVersion = (firstVersion | ATOMIC_LOCK_FLAG) + 1;
    assert((newVersion & 0x01) == 1);

    pEntry->version = newVersion;

    android_atomic_release_store(key1, (int32_t*) &pEntry->key1);
    pEntry->key2 = key2;
    pEntry->value = value;

    newVersion++;
    android_atomic_release_store(newVersion, (int32_t*) &pEntry->version);

    /*
     * Clear the lock flag.  Nobody else should have been able to modify
     * pEntry->version, so if this fails the world is broken.
     */
    assert(newVersion == ((firstVersion + 2) | ATOMIC_LOCK_FLAG));
    if (android_atomic_release_cas(
            newVersion, newVersion & ~ATOMIC_LOCK_FLAG,
            (volatile s4*) &pEntry->version) != 0)
    {
        //ALOGE("unable to reset the instanceof cache ownership");
        dvmAbort();
    }
}


/*
 * Dump the "instanceof" cache stats.
 */
void dvmDumpAtomicCacheStats(const AtomicCache* pCache)
{
    if (pCache == NULL)
        return;
    dvmFprintf(stdout,
        "Cache stats: trv=%d fai=%d hit=%d mis=%d fil=%d %d%% (size=%d)\n",
        pCache->trivial, pCache->fail, pCache->hits,
        pCache->misses, pCache->fills,
        (pCache->hits == 0) ? 0 :
            pCache->hits * 100 /
                (pCache->fail + pCache->hits + pCache->misses + pCache->fills),
        pCache->numEntries);
}
