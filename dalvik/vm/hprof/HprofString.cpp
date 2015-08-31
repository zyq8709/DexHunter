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
 * Common string pool for the profiler
 */
#include "Hprof.h"

static HashTable *gStringHashTable;

int hprofStartup_String()
{
    gStringHashTable = dvmHashTableCreate(512, free);
    if (gStringHashTable == NULL) {
        return UNIQUE_ERROR();
    }
    return 0;
}

int hprofShutdown_String()
{
    dvmHashTableFree(gStringHashTable);
    return 0;
}

static u4 computeUtf8Hash(const char *str)
{
    u4 hash = 0;
    const char *cp;
    char c;

    cp = str;
    while ((c = *cp++) != '\0') {
        hash = hash * 31 + c;
    }

    return hash;
}

hprof_string_id hprofLookupStringId(const char *str)
{
    void *val;
    u4 hashValue;

    dvmHashTableLock(gStringHashTable);

    hashValue = computeUtf8Hash(str);
    val = dvmHashTableLookup(gStringHashTable, hashValue, (void *)str,
            (HashCompareFunc)strcmp, false);
    if (val == NULL) {
        const char *newStr;

        newStr = strdup(str);
        val = dvmHashTableLookup(gStringHashTable, hashValue, (void *)newStr,
                (HashCompareFunc)strcmp, true);
        assert(val != NULL);
    }

    dvmHashTableUnlock(gStringHashTable);

    return (hprof_string_id)val;
}

int hprofDumpStrings(hprof_context_t *ctx)
{
    HashIter iter;
    hprof_record_t *rec = &ctx->curRec;
    int err;

    dvmHashTableLock(gStringHashTable);

    for (err = 0, dvmHashIterBegin(gStringHashTable, &iter);
         err == 0 && !dvmHashIterDone(&iter);
         dvmHashIterNext(&iter))
    {
        err = hprofStartNewRecord(ctx, HPROF_TAG_STRING, HPROF_TIME);
        if (err == 0) {
            const char *str;

            str = (const char *)dvmHashIterData(&iter);
            assert(str != NULL);

            /* STRING format:
             *
             * ID:     ID for this string
             * [u1]*:  UTF8 characters for string (NOT NULL terminated)
             *         (the record format encodes the length)
             *
             * We use the address of the string data as its ID.
             */
            err = hprofAddU4ToRecord(rec, (u4)str);
            if (err == 0) {
                err = hprofAddUtf8StringToRecord(rec, str);
            }
        }
    }

    dvmHashTableUnlock(gStringHashTable);

    return err;
}
