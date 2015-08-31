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
 * Reference table management.
 */
#include "Dalvik.h"

/*
 * Initialize a ReferenceTable structure.
 */
bool dvmInitReferenceTable(ReferenceTable* pRef, int initialCount,
    int maxCount)
{
    assert(initialCount > 0);
    assert(initialCount <= maxCount);

    pRef->table = (Object**) malloc(initialCount * sizeof(Object*));
    if (pRef->table == NULL)
        return false;
#ifndef NDEBUG
    memset(pRef->table, 0xdd, initialCount * sizeof(Object*));
#endif
    pRef->nextEntry = pRef->table;
    pRef->allocEntries = initialCount;
    pRef->maxEntries = maxCount;

    return true;
}

/*
 * Clears out the contents of a ReferenceTable, freeing allocated storage.
 */
void dvmClearReferenceTable(ReferenceTable* pRef)
{
    free(pRef->table);
    pRef->table = pRef->nextEntry = NULL;
    pRef->allocEntries = pRef->maxEntries = -1;
}

/*
 * Add "obj" to "pRef".
 */
bool dvmAddToReferenceTable(ReferenceTable* pRef, Object* obj)
{
    assert(obj != NULL);
    assert(dvmIsHeapAddress(obj));
    assert(pRef->table != NULL);
    assert(pRef->allocEntries <= pRef->maxEntries);

    if (pRef->nextEntry == pRef->table + pRef->allocEntries) {
        /* reached end of allocated space; did we hit buffer max? */
        if (pRef->nextEntry == pRef->table + pRef->maxEntries) {
            ALOGW("ReferenceTable overflow (max=%d)", pRef->maxEntries);
            return false;
        }

        Object** newTable;
        int newSize;

        newSize = pRef->allocEntries * 2;
        if (newSize > pRef->maxEntries)
            newSize = pRef->maxEntries;
        assert(newSize > pRef->allocEntries);

        newTable = (Object**) realloc(pRef->table, newSize * sizeof(Object*));
        if (newTable == NULL) {
            ALOGE("Unable to expand ref table (from %d to %d %d-byte entries)",
                pRef->allocEntries, newSize, sizeof(Object*));
            return false;
        }
        LOGVV("Growing %p from %d to %d", pRef, pRef->allocEntries, newSize);

        /* update entries; adjust "nextEntry" in case memory moved */
        pRef->nextEntry = newTable + (pRef->nextEntry - pRef->table);
        pRef->table = newTable;
        pRef->allocEntries = newSize;
    }

    *pRef->nextEntry++ = obj;
    return true;
}

/*
 * Returns NULL if not found.
 */
Object** dvmFindInReferenceTable(const ReferenceTable* pRef, Object** bottom,
    Object* obj)
{
    Object** ptr;

    ptr = pRef->nextEntry;
    while (--ptr >= bottom) {
        if (*ptr == obj)
            return ptr;
    }
    return NULL;
}

/*
 * Remove "obj" from "pRef".  We start at the end of the list (where the
 * most-recently-added element is), and stop searching for a match after
 * examining the element at "bottom".
 *
 * Most of the time "obj" is at or near the end of the list.  If not, we
 * compact it down.
 */
bool dvmRemoveFromReferenceTable(ReferenceTable* pRef, Object** bottom,
    Object* obj)
{
    Object** ptr;

    assert(pRef->table != NULL);

    /*
     * Scan from the most-recently-added entry up to the bottom entry for
     * this frame.
     */
    ptr = dvmFindInReferenceTable(pRef, bottom, obj);
    if (ptr == NULL)
        return false;

    /*
     * Delete the entry.
     */
    pRef->nextEntry--;
    int moveCount = pRef->nextEntry - ptr;
    if (moveCount != 0) {
        /* remove from middle, slide the rest down */
        memmove(ptr, ptr+1, moveCount * sizeof(Object*));
        //ALOGV("LREF delete %p, shift %d down", obj, moveCount);
    } else {
        /* last entry, falls off the end */
        //ALOGV("LREF delete %p from end", obj);
    }

    return true;
}

/*
 * If "obj" is an array, return the number of elements in the array.
 * Otherwise, return zero.
 */
static size_t getElementCount(const Object* obj)
{
    const ArrayObject* arrayObj = (ArrayObject*) obj;
    if (arrayObj == NULL || arrayObj == kClearedJniWeakGlobal ||
            arrayObj->clazz == NULL || !dvmIsArray(arrayObj)) {
        return 0;
    }
    return arrayObj->length;
}

/*
 * This is a qsort() callback.  We sort Object* by class, allocation size,
 * and then by the Object* itself.
 */
static int compareObject(const void* vobj1, const void* vobj2)
{
    const Object* obj1 = *((Object* const*) vobj1);
    const Object* obj2 = *((Object* const*) vobj2);

    // Ensure null references and cleared jweaks appear at the end.
    if (obj1 == NULL) {
        if (obj2 == NULL) {
            return 0;
        } else {
            return 1;
        }
    } else if (obj2 == NULL) {
        return -1;
    }
    if (obj1 == kClearedJniWeakGlobal) {
        if (obj2 == kClearedJniWeakGlobal) {
            return 0;
        } else {
            return 1;
        }
    } else if (obj2 == kClearedJniWeakGlobal) {
        return -1;
    }

    if (obj1->clazz != obj2->clazz) {
        return (u1*)obj1->clazz - (u1*)obj2->clazz;
    } else {
        size_t count1 = getElementCount(obj1);
        size_t count2 = getElementCount(obj2);
        if (count1 != count2) {
            return count1 - count2;
        } else {
            return (u1*)obj1 - (u1*)obj2;
        }
    }
}

/*
 * Log an object with some additional info.
 *
 * Pass in the number of elements in the array (or 0 if this is not an
 * array object), and the number of additional objects that are identical
 * or equivalent to the original.
 */
static void logSummaryLine(const Object* obj, size_t elems, int identical, int equiv)
{
    if (obj == NULL) {
        ALOGW("    NULL reference (count=%d)", equiv);
        return;
    }
    if (obj == kClearedJniWeakGlobal) {
        ALOGW("    cleared jweak (count=%d)", equiv);
        return;
    }

    std::string className(dvmHumanReadableType(obj));
    if (obj->clazz == gDvm.classJavaLangClass) {
        // We're summarizing multiple instances, so using the exemplar
        // Class' type parameter here would be misleading.
        className = "java.lang.Class";
    }
    if (elems != 0) {
        StringAppendF(&className, " (%zd elements)", elems);
    }

    size_t total = identical + equiv + 1;
    std::string msg(StringPrintf("%5d of %s", total, className.c_str()));
    if (identical + equiv != 0) {
        StringAppendF(&msg, " (%d unique instances)", equiv + 1);
    }
    ALOGW("    %s", msg.c_str());
}

/*
 * Dump a summary of an array of references to the log file.
 *
 * This is used to dump the contents of ReferenceTable and IndirectRefTable
 * structs.
 */
void dvmDumpReferenceTableContents(Object* const* refs, size_t count,
    const char* descr)
{
    ALOGW("%s reference table (%p) dump:", descr, refs);

    if (count == 0) {
        ALOGW("  (empty)");
        return;
    }

    // Dump the most recent N entries.
    const size_t kLast = 10;
    int first = count - kLast;
    if (first < 0) {
        first = 0;
    }
    ALOGW("  Last %d entries (of %d):", (count - first), count);
    for (int idx = count - 1; idx >= first; --idx) {
        const Object* ref = refs[idx];
        if (ref == NULL) {
            continue;
        }
        if (ref == kClearedJniWeakGlobal) {
            ALOGW("    %5d: cleared jweak", idx);
            continue;
        }
        if (ref->clazz == NULL) {
            // should only be possible right after a plain dvmMalloc().
            size_t size = dvmObjectSizeInHeap(ref);
            ALOGW("    %5d: %p (raw) (%zd bytes)", idx, ref, size);
            continue;
        }

        std::string className(dvmHumanReadableType(ref));

        std::string extras;
        size_t elems = getElementCount(ref);
        if (elems != 0) {
            StringAppendF(&extras, " (%zd elements)", elems);
        } else if (ref->clazz == gDvm.classJavaLangString) {
            const StringObject* str =
                    reinterpret_cast<const StringObject*>(ref);
            extras += " \"";
            size_t count = 0;
            char* s = dvmCreateCstrFromString(str);
            char* p = s;
            for (; *p && count < 16; ++p, ++count) {
                extras += *p;
            }
            if (*p == 0) {
                extras += "\"";
            } else {
                StringAppendF(&extras, "... (%d chars)", str->length());
            }
            free(s);
        }
        ALOGW("    %5d: %p %s%s", idx, ref, className.c_str(), extras.c_str());
    }

    // Make a copy of the table, and sort it.
    Object** tableCopy = (Object**)malloc(sizeof(Object*) * count);
    if (tableCopy == NULL) {
        ALOGE("Unable to copy table with %d elements", count);
        return;
    }
    memcpy(tableCopy, refs, sizeof(Object*) * count);
    qsort(tableCopy, count, sizeof(Object*), compareObject);
    refs = tableCopy;       // use sorted list

    // Remove any uninteresting stuff from the list. The sort moved them all to the end.
    while (count > 0 && refs[count-1] == NULL) {
        --count;
    }
    while (count > 0 && refs[count-1] == kClearedJniWeakGlobal) {
        --count;
    }
    if (count == 0) {
        return;
    }

    // Dump a summary of the whole table.
    ALOGW("  Summary:");
    size_t equiv, identical;
    equiv = identical = 0;
    size_t idx;
    size_t elems;
    for (idx = 1; idx < count; idx++) {
        elems = getElementCount(refs[idx-1]);

        if (refs[idx] == refs[idx-1]) {
            // same reference, added more than once.
            identical++;
        } else if (refs[idx]->clazz == refs[idx-1]->clazz &&
            getElementCount(refs[idx]) == elems)
        {
            // same class / element count, different object.
            equiv++;
        } else {
            // different class.
            logSummaryLine(refs[idx-1], elems, identical, equiv);
            equiv = identical = 0;
        }
    }

    // Handle the last entry (everything above outputs refs[i-1]).
    elems = getElementCount(refs[idx-1]);
    logSummaryLine(refs[count-1], elems, identical, equiv);

    free(tableCopy);
}

/*
 * Dump the contents of a ReferenceTable to the log.
 */
void dvmDumpReferenceTable(const ReferenceTable* pRef, const char* descr)
{
    dvmDumpReferenceTableContents(pRef->table, dvmReferenceTableEntries(pRef),
        descr);
}
