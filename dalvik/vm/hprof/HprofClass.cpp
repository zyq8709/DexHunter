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
 * Class object pool
 */

#include "Hprof.h"

static HashTable *gClassHashTable;

int hprofStartup_Class()
{
    gClassHashTable = dvmHashTableCreate(128, NULL);
    if (gClassHashTable == NULL) {
        return UNIQUE_ERROR();
    }
    return 0;
}

int hprofShutdown_Class()
{
    dvmHashTableFree(gClassHashTable);

    return 0;
}

static u4 computeClassHash(const ClassObject *clazz)
{
    u4 hash;
    const char *cp;
    char c;

    cp = clazz->descriptor;
    hash = (u4)clazz->classLoader;
    while ((c = *cp++) != '\0') {
        hash = hash * 31 + c;
    }

    return hash;
}

static int classCmp(const void *v1, const void *v2)
{
    const ClassObject *c1 = (const ClassObject *)v1;
    const ClassObject *c2 = (const ClassObject *)v2;
    intptr_t diff;

    diff = (uintptr_t)c1->classLoader - (uintptr_t)c2->classLoader;
    if (diff == 0) {
        return strcmp(c1->descriptor, c2->descriptor);
    }
    return diff;
}

static int getPrettyClassNameId(const char *descriptor) {
    std::string name(dvmHumanReadableDescriptor(descriptor));
    return hprofLookupStringId(name.c_str());
}

hprof_class_object_id hprofLookupClassId(const ClassObject *clazz)
{
    void *val;

    if (clazz == NULL) {
        /* Someone's probably looking up the superclass
         * of java.lang.Object or of a primitive class.
         */
        return (hprof_class_object_id)0;
    }

    dvmHashTableLock(gClassHashTable);

    /* We're using the hash table as a list.
     * TODO: replace the hash table with a more suitable structure
     */
    val = dvmHashTableLookup(gClassHashTable, computeClassHash(clazz),
            (void *)clazz, classCmp, true);
    assert(val != NULL);

    dvmHashTableUnlock(gClassHashTable);

    /* Make sure that the class's name is in the string table.
     * This is a bunch of extra work that we only have to do
     * because of the order of tables in the output file
     * (strings need to be dumped before classes).
     */
    getPrettyClassNameId(clazz->descriptor);

    return (hprof_class_object_id)clazz;
}

int hprofDumpClasses(hprof_context_t *ctx)
{
    HashIter iter;
    hprof_record_t *rec = &ctx->curRec;
    int err;

    dvmHashTableLock(gClassHashTable);

    for (err = 0, dvmHashIterBegin(gClassHashTable, &iter);
         err == 0 && !dvmHashIterDone(&iter);
         dvmHashIterNext(&iter))
    {
        err = hprofStartNewRecord(ctx, HPROF_TAG_LOAD_CLASS, HPROF_TIME);
        if (err == 0) {
            const ClassObject *clazz;

            clazz = (const ClassObject *)dvmHashIterData(&iter);
            assert(clazz != NULL);

            /* LOAD CLASS format:
             *
             * u4:     class serial number (always > 0)
             * ID:     class object ID
             * u4:     stack trace serial number
             * ID:     class name string ID
             *
             * We use the address of the class object structure as its ID.
             */
            hprofAddU4ToRecord(rec, clazz->serialNumber);
            hprofAddIdToRecord(rec, (hprof_class_object_id)clazz);
            hprofAddU4ToRecord(rec, HPROF_NULL_STACK_TRACE);
            hprofAddIdToRecord(rec, getPrettyClassNameId(clazz->descriptor));
        }
    }

    dvmHashTableUnlock(gClassHashTable);

    return err;
}
