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
 * Verification-time map of data section items
 */

#include "DexDataMap.h"
#include <safe_iop.h>
#include <stdlib.h>

/*
 * Allocate and initialize a DexDataMap. Returns NULL on failure.
 */
DexDataMap* dexDataMapAlloc(u4 maxCount) {
    /*
     * Allocate a single chunk for the DexDataMap per se as well as the
     * two arrays.
     */
    size_t size = 0;
    DexDataMap* map = NULL;

    /*
     * Avoiding pulling in safe_iop for safe_iopf.
     */
    if (!safe_mul(&size, maxCount, sizeof(u4) + sizeof(u2)) ||
        !safe_add(&size, size, sizeof(DexDataMap))) {
      return NULL;
    }

    map = (DexDataMap*) malloc(size);

    if (map == NULL) {
        return NULL;
    }

    map->count = 0;
    map->max = maxCount;
    map->offsets = (u4*) (map + 1);
    map->types = (u2*) (map->offsets + maxCount);

    return map;
}

/*
 * Free a DexDataMap.
 */
void dexDataMapFree(DexDataMap* map) {
    /*
     * Since everything got allocated together, everything can be freed
     * in one fell swoop. Also, free(NULL) is a nop (per spec), so we
     * don't have to worry about an explicit test for that.
     */
    free(map);
}

/*
 * Add a new element to the map. The offset must be greater than the
 * all previously added offsets.
 */
void dexDataMapAdd(DexDataMap* map, u4 offset, u2 type) {
    assert(map != NULL);
    assert(map->count < map->max);

    if ((map->count != 0) &&
            (map->offsets[map->count - 1] >= offset)) {
        ALOGE("Out-of-order data map offset: %#x then %#x",
                map->offsets[map->count - 1], offset);
        return;
    }

    map->offsets[map->count] = offset;
    map->types[map->count] = type;
    map->count++;
}

/*
 * Get the type associated with the given offset. This returns -1 if
 * there is no entry for the given offset.
 */
int dexDataMapGet(DexDataMap* map, u4 offset) {
    assert(map != NULL);

    // Note: Signed type is important for max and min.
    int min = 0;
    int max = map->count - 1;
    u4* offsets = map->offsets;

    while (max >= min) {
        int guessIdx = (min + max) >> 1;
        u4 guess = offsets[guessIdx];

        if (offset < guess) {
            max = guessIdx - 1;
        } else if (offset > guess) {
            min = guessIdx + 1;
        } else {
            // We have a winner!
            return map->types[guessIdx];
        }
    }

    // No match.
    return -1;
}

/*
 * Verify that there is an entry in the map, mapping the given offset to
 * the given type. This will return true if such an entry exists and
 * return false as well as log an error if not.
 */
bool dexDataMapVerify(DexDataMap* map, u4 offset, u2 type) {
    int found = dexDataMapGet(map, offset);

    if (found == type) {
        return true;
    }

    if (found < 0) {
        ALOGE("No data map entry found @ %#x; expected %x",
                offset, type);
    } else {
        ALOGE("Unexpected data map entry @ %#x: expected %x, found %x",
                offset, type, found);
    }

    return false;
}
