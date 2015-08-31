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
 * Maintain an expanding set of unique pointer values.
 */
#include "Dalvik.h"

/*
 * Sorted, expanding list of pointers.
 */
struct PointerSet {
    u2          alloc;
    u2          count;
    const void** list;
};

/*
 * Verify that the set is in sorted order.
 */
#ifndef NDEBUG
static bool verifySorted(PointerSet* pSet)
{
    const void* last = NULL;
    int i;

    for (i = 0; i < pSet->count; i++) {
        const void* cur = pSet->list[i];
        if (cur < last)
            return false;
        last = cur;
    }

    return true;
}
#endif

/*
 * Allocate a new PointerSet.
 *
 * Returns NULL on failure.
 */
PointerSet* dvmPointerSetAlloc(int initialSize)
{
    PointerSet* pSet = (PointerSet*)calloc(1, sizeof(PointerSet));
    if (pSet != NULL) {
        if (initialSize > 0) {
            pSet->list = (const void**)malloc(sizeof(void*) * initialSize);
            if (pSet->list == NULL) {
                free(pSet);
                return NULL;
            }
            pSet->alloc = initialSize;
        }
    }

    return pSet;
}

/*
 * Free up a PointerSet.
 */
void dvmPointerSetFree(PointerSet* pSet)
{
    if (pSet == NULL)
        return;

    if (pSet->list != NULL) {
        free(pSet->list);
        pSet->list = NULL;
    }
    free(pSet);
}

/*
 * Clear the contents of a pointer set.
 */
void dvmPointerSetClear(PointerSet* pSet)
{
    pSet->count = 0;
}

/*
 * Get the number of pointers currently stored in the list.
 */
int dvmPointerSetGetCount(const PointerSet* pSet)
{
    return pSet->count;
}

/*
 * Get the Nth entry from the list.
 */
const void* dvmPointerSetGetEntry(const PointerSet* pSet, int i)
{
    return pSet->list[i];
}

/*
 * Insert a new entry into the list.  If it already exists, this returns
 * without doing anything.
 *
 * Returns "true" if the value was added.
 */
bool dvmPointerSetAddEntry(PointerSet* pSet, const void* ptr)
{
    int nearby;

    if (dvmPointerSetHas(pSet, ptr, &nearby))
        return false;

    /* ensure we have space to add one more */
    if (pSet->count == pSet->alloc) {
        /* time to expand */
        const void** newList;

        if (pSet->alloc == 0)
            pSet->alloc = 4;
        else
            pSet->alloc *= 2;
        LOGVV("expanding %p to %d", pSet, pSet->alloc);
        newList = (const void**)realloc(pSet->list, pSet->alloc * sizeof(void*));
        if (newList == NULL) {
            ALOGE("Failed expanding ptr set (alloc=%d)", pSet->alloc);
            dvmAbort();
        }
        pSet->list = newList;
    }

    if (pSet->count == 0) {
        /* empty list */
        assert(nearby == 0);
    } else {
        /*
         * Determine the insertion index.  The binary search might have
         * terminated "above" or "below" the value.
         */
        if (nearby != 0 && ptr < pSet->list[nearby-1]) {
            //ALOGD("nearby-1=%d %p, inserting %p at -1",
            //    nearby-1, pSet->list[nearby-1], ptr);
            nearby--;
        } else if (ptr < pSet->list[nearby]) {
            //ALOGD("nearby=%d %p, inserting %p at +0",
            //    nearby, pSet->list[nearby], ptr);
        } else {
            //ALOGD("nearby+1=%d %p, inserting %p at +1",
            //    nearby+1, pSet->list[nearby+1], ptr);
            nearby++;
        }

        /*
         * Move existing values, if necessary.
         */
        if (nearby != pSet->count) {
            /* shift up */
            memmove(&pSet->list[nearby+1], &pSet->list[nearby],
                (pSet->count - nearby) * sizeof(pSet->list[0]));
        }
    }

    pSet->list[nearby] = ptr;
    pSet->count++;

    assert(verifySorted(pSet));
    return true;
}

/*
 * Returns "true" if the element was successfully removed.
 */
bool dvmPointerSetRemoveEntry(PointerSet* pSet, const void* ptr)
{
    int where;

    if (!dvmPointerSetHas(pSet, ptr, &where))
        return false;

    if (where != pSet->count-1) {
        /* shift down */
        memmove(&pSet->list[where], &pSet->list[where+1],
            (pSet->count-1 - where) * sizeof(pSet->list[0]));
    }

    pSet->count--;
    pSet->list[pSet->count] = (const void*) 0xdecadead;     // debug
    return true;
}

/*
 * Returns the index if "ptr" appears in the list.  If it doesn't appear,
 * this returns a negative index for a nearby element.
 */
bool dvmPointerSetHas(const PointerSet* pSet, const void* ptr, int* pIndex)
{
    int hi, lo, mid;

    lo = mid = 0;
    hi = pSet->count-1;

    /* array is sorted, use a binary search */
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        const void* listVal = pSet->list[mid];

        if (ptr > listVal) {
            lo = mid + 1;
        } else if (ptr < listVal) {
            hi = mid - 1;
        } else /* listVal == ptr */ {
            if (pIndex != NULL)
                *pIndex = mid;
            return true;
        }
    }

    if (pIndex != NULL)
        *pIndex = mid;
    return false;
}

/*
 * Compute the intersection of the set and the array of pointers passed in.
 *
 * Any pointer in "pSet" that does not appear in "ptrArray" is removed.
 */
void dvmPointerSetIntersect(PointerSet* pSet, const void** ptrArray, int count)
{
    int i, j;

    for (i = 0; i < pSet->count; i++) {
        for (j = 0; j < count; j++) {
            if (pSet->list[i] == ptrArray[j]) {
                /* match, keep this one */
                break;
            }
        }

        if (j == count) {
            /* no match, remove entry */
            if (i != pSet->count-1) {
                /* shift down */
                memmove(&pSet->list[i], &pSet->list[i+1],
                    (pSet->count-1 - i) * sizeof(pSet->list[0]));
            }

            pSet->count--;
            pSet->list[pSet->count] = (const void*) 0xdecadead;     // debug
            i--;        /* adjust loop counter */
        }
    }
}

/*
 * Print the list contents to stdout.  For debugging.
 */
void dvmPointerSetDump(const PointerSet* pSet)
{
    ALOGI("PointerSet %p", pSet);
    int i;
    for (i = 0; i < pSet->count; i++)
        ALOGI(" %2d: %p", i, pSet->list[i]);
}
