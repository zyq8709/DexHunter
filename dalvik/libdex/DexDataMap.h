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

#ifndef LIBDEX_DEXDATAMAP_H_
#define LIBDEX_DEXDATAMAP_H_

#include "DexFile.h"

struct DexDataMap {
    u4 count;    /* number of items currently in the map */
    u4 max;      /* maximum number of items that may be held */
    u4* offsets; /* array of item offsets */
    u2* types;   /* corresponding array of item types */
};

/*
 * Allocate and initialize a DexDataMap. Returns NULL on failure.
 */
DexDataMap* dexDataMapAlloc(u4 maxCount);

/*
 * Free a DexDataMap.
 */
void dexDataMapFree(DexDataMap* map);

/*
 * Add a new element to the map. The offset must be greater than the
 * all previously added offsets.
 */
void dexDataMapAdd(DexDataMap* map, u4 offset, u2 type);

/*
 * Get the type associated with the given offset. This returns -1 if
 * there is no entry for the given offset.
 */
int dexDataMapGet(DexDataMap* map, u4 offset);

/*
 * Verify that there is an entry in the map, mapping the given offset to
 * the given type. This will return true if such an entry exists and
 * return false as well as log an error if not.
 */
bool dexDataMapVerify(DexDataMap* map, u4 offset, u2 type);

/*
 * Like dexDataMapVerify(), but also accept a 0 offset as valid.
 */
DEX_INLINE bool dexDataMapVerify0Ok(DexDataMap* map, u4 offset, u2 type) {
    if (offset == 0) {
        return true;
    }

    return dexDataMapVerify(map, offset, type);
}

#endif  // LIBDEX_DEXDATAMAP_H_
