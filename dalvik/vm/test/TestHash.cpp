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
 * Test the hash table functions.
 */
#include "Dalvik.h"

#include <stdlib.h>

#ifndef NDEBUG

#define kNumTestEntries 14

/*
 * Test foreach.
 */
static int printFunc(void* data, void* arg)
{
    //printf("  '%s'\n", (const char*) data);
    // (should verify strings)

    int* count = (int*) arg;
    (*count)++;
    return 0;
}
static void dumpForeach(HashTable* pTab)
{
    int count = 0;

    //printf("Print from foreach:\n");
    dvmHashForeach(pTab, printFunc, &count);
    if (count != kNumTestEntries) {
        ALOGE("TestHash foreach test failed");
        assert(false);
    }
}

/*
 * Test iterator.
 */
static void dumpIterator(HashTable* pTab)
{
    int count = 0;

    //printf("Print from iterator:\n");
    HashIter iter;
    for (dvmHashIterBegin(pTab, &iter); !dvmHashIterDone(&iter);
        dvmHashIterNext(&iter))
    {
        //const char* str = (const char*) dvmHashIterData(&iter);
        //printf("  '%s'\n", str);
        // (should verify strings)
        count++;
    }
    if (count != kNumTestEntries) {
        ALOGE("TestHash iterator test failed");
        assert(false);
    }
}

/*
 * Some quick hash table tests.
 */
bool dvmTestHash()
{
    HashTable* pTab;
    char tmpStr[64];
    const char* str;
    u4 hash;
    int i;

    ALOGV("TestHash BEGIN");

    pTab = dvmHashTableCreate(dvmHashSize(12), free);
    if (pTab == NULL)
        return false;

    dvmHashTableLock(pTab);

    /* add some entries */
    for (i = 0; i < kNumTestEntries; i++) {
        sprintf(tmpStr, "entry %d", i);
        hash = dvmComputeUtf8Hash(tmpStr);
        dvmHashTableLookup(pTab, hash, strdup(tmpStr),
            (HashCompareFunc) strcmp, true);
    }

    dvmHashTableUnlock(pTab);

    /* make sure we can find all entries */
    for (i = 0; i < kNumTestEntries; i++) {
        sprintf(tmpStr, "entry %d", i);
        hash = dvmComputeUtf8Hash(tmpStr);
        str = (const char*) dvmHashTableLookup(pTab, hash, tmpStr,
                (HashCompareFunc) strcmp, false);
        if (str == NULL) {
            ALOGE("TestHash: failure: could not find '%s'", tmpStr);
            /* return false */
        }
    }

    /* make sure it behaves correctly when entry not found and !doAdd */
    sprintf(tmpStr, "entry %d", 17);
    hash = dvmComputeUtf8Hash(tmpStr);
    str = (const char*) dvmHashTableLookup(pTab, hash, tmpStr,
            (HashCompareFunc) strcmp, false);
    if (str == NULL) {
        /* good */
    } else {
        ALOGE("TestHash found nonexistent string (improper add?)");
    }

    dumpForeach(pTab);
    dumpIterator(pTab);

    /* make sure they all get freed */
    dvmHashTableFree(pTab);


    /*
     * Round 2: verify probing & tombstones.
     */
    pTab = dvmHashTableCreate(dvmHashSize(2), free);
    if (pTab == NULL)
        return false;

    hash = 0;

    /* two entries, same hash, different values */
    const char* str1;
    str1 = (char*) dvmHashTableLookup(pTab, hash, strdup("one"),
            (HashCompareFunc) strcmp, true);
    assert(str1 != NULL);
    str = (const char*) dvmHashTableLookup(pTab, hash, strdup("two"),
            (HashCompareFunc) strcmp, true);

    /* remove the first one */
    if (!dvmHashTableRemove(pTab, hash, (void*)str1))
        ALOGE("TestHash failed to delete item");
    else
        free((void*)str1);     // "Remove" doesn't call the free func

    /* make sure iterator doesn't included deleted entries */
    int count = 0;
    HashIter iter;
    for (dvmHashIterBegin(pTab, &iter); !dvmHashIterDone(&iter);
        dvmHashIterNext(&iter))
    {
        count++;
    }
    if (count != 1) {
        ALOGE("TestHash wrong number of entries (%d)", count);
    }

    /* see if we can find them */
    str = (const char*) dvmHashTableLookup(pTab, hash, (void*)"one",
            (HashCompareFunc) strcmp,false);
    if (str != NULL)
        ALOGE("TestHash deleted entry has returned!");
    str = (const char*) dvmHashTableLookup(pTab, hash, (void*)"two",
            (HashCompareFunc) strcmp,false);
    if (str == NULL)
        ALOGE("TestHash entry vanished");

    /* force a table realloc to exercise tombstone removal */
    for (i = 0; i < 20; i++) {
        sprintf(tmpStr, "entry %d", i);
        str = (const char*) dvmHashTableLookup(pTab, hash, strdup(tmpStr),
                (HashCompareFunc) strcmp, true);
        assert(str != NULL);
    }

    dvmHashTableFree(pTab);
    ALOGV("TestHash END");

    return true;
}

#endif /*NDEBUG*/
