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
 * String interning.
 */
#include "Dalvik.h"

#include <stddef.h>

/*
 * Prep string interning.
 */
bool dvmStringInternStartup()
{
    dvmInitMutex(&gDvm.internLock);
    gDvm.internedStrings = dvmHashTableCreate(256, NULL);
    if (gDvm.internedStrings == NULL)
        return false;
    gDvm.literalStrings = dvmHashTableCreate(256, NULL);
    if (gDvm.literalStrings == NULL)
        return false;
    return true;
}

/*
 * Chuck the intern list.
 *
 * The contents of the list are StringObjects that live on the GC heap.
 */
void dvmStringInternShutdown()
{
    if (gDvm.internedStrings != NULL || gDvm.literalStrings != NULL) {
        dvmDestroyMutex(&gDvm.internLock);
    }
    dvmHashTableFree(gDvm.internedStrings);
    gDvm.internedStrings = NULL;
    dvmHashTableFree(gDvm.literalStrings);
    gDvm.literalStrings = NULL;
}

static StringObject* lookupString(HashTable* table, u4 key, StringObject* value)
{
    void* entry = dvmHashTableLookup(table, key, (void*)value,
                                     dvmHashcmpStrings, false);
    return (StringObject*)entry;
}

static StringObject* insertString(HashTable* table, u4 key, StringObject* value)
{
    if (dvmIsNonMovingObject(value) == false) {
        value = (StringObject*)dvmCloneObject(value, ALLOC_NON_MOVING);
    }
    void* entry = dvmHashTableLookup(table, key, (void*)value,
                                     dvmHashcmpStrings, true);
    return (StringObject*)entry;
}

static StringObject* lookupInternedString(StringObject* strObj, bool isLiteral)
{
    StringObject* found;

    assert(strObj != NULL);
    u4 key = dvmComputeStringHash(strObj);
    dvmLockMutex(&gDvm.internLock);
    if (isLiteral) {
        /*
         * Check the literal table for a match.
         */
        StringObject* literal = lookupString(gDvm.literalStrings, key, strObj);
        if (literal != NULL) {
            /*
             * A match was found in the literal table, the easy case.
             */
            found = literal;
        } else {
            /*
             * There is no match in the literal table, check the
             * interned string table.
             */
            StringObject* interned = lookupString(gDvm.internedStrings, key, strObj);
            if (interned != NULL) {
                /*
                 * A match was found in the interned table.  Move the
                 * matching string to the literal table.
                 */
                dvmHashTableRemove(gDvm.internedStrings, key, interned);
                found = insertString(gDvm.literalStrings, key, interned);
                assert(found == interned);
            } else {
                /*
                 * No match in the literal table or the interned
                 * table.  Insert into the literal table.
                 */
                found = insertString(gDvm.literalStrings, key, strObj);
                assert(found == strObj);
            }
        }
    } else {
        /*
         * Check the literal table for a match.
         */
        found = lookupString(gDvm.literalStrings, key, strObj);
        if (found == NULL) {
            /*
             * No match was found in the literal table.  Insert into
             * the intern table if it does not already exist.
             */
            found = insertString(gDvm.internedStrings, key, strObj);
        }
    }
    assert(found != NULL);
    dvmUnlockMutex(&gDvm.internLock);
    return found;
}

/*
 * Find an entry in the interned string table.
 *
 * If the string doesn't already exist, the StringObject is added to
 * the table.  Otherwise, the existing entry is returned.
 */
StringObject* dvmLookupInternedString(StringObject* strObj)
{
    return lookupInternedString(strObj, false);
}

/*
 * Same as dvmLookupInternedString(), but guarantees that the
 * returned string is a literal.
 */
StringObject* dvmLookupImmortalInternedString(StringObject* strObj)
{
    return lookupInternedString(strObj, true);
}

/*
 * Returns true if the object is a weak interned string.  Any string
 * interned by the user is weak.
 */
bool dvmIsWeakInternedString(StringObject* strObj)
{
    assert(strObj != NULL);
    if (gDvm.internedStrings == NULL) {
        return false;
    }
    dvmLockMutex(&gDvm.internLock);
    u4 key = dvmComputeStringHash(strObj);
    StringObject* found = lookupString(gDvm.internedStrings, key, strObj);
    dvmUnlockMutex(&gDvm.internLock);
    return found == strObj;
}

/*
 * Clear white references from the intern table.
 */
void dvmGcDetachDeadInternedStrings(int (*isUnmarkedObject)(void *))
{
    /* It's possible for a GC to happen before dvmStringInternStartup()
     * is called.
     */
    if (gDvm.internedStrings != NULL) {
        dvmLockMutex(&gDvm.internLock);
        dvmHashForeachRemove(gDvm.internedStrings, isUnmarkedObject);
        dvmUnlockMutex(&gDvm.internLock);
    }
}
