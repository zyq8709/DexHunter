/*
 * Copyright (C) 2009 The Android Open Source Project
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
 * Indirect reference table management.
 */
#include "Dalvik.h"

static void abortMaybe() {
    // If CheckJNI is on, it'll give a more detailed error before aborting.
    // Otherwise, we want to abort rather than hand back a bad reference.
    if (!gDvmJni.useCheckJni) {
        dvmAbort();
    }
}

bool IndirectRefTable::init(size_t initialCount,
        size_t maxCount, IndirectRefKind desiredKind)
{
    assert(initialCount > 0);
    assert(initialCount <= maxCount);
    assert(desiredKind != kIndirectKindInvalid);

    table_ = (IndirectRefSlot*) malloc(initialCount * sizeof(IndirectRefSlot));
    if (table_ == NULL) {
        return false;
    }
    memset(table_, 0xd1, initialCount * sizeof(IndirectRefSlot));

    segmentState.all = IRT_FIRST_SEGMENT;
    alloc_entries_ = initialCount;
    max_entries_ = maxCount;
    kind_ = desiredKind;

    return true;
}

/*
 * Clears out the contents of a IndirectRefTable, freeing allocated storage.
 */
void IndirectRefTable::destroy()
{
    free(table_);
    table_ = NULL;
    alloc_entries_ = max_entries_ = -1;
}

IndirectRef IndirectRefTable::add(u4 cookie, Object* obj)
{
    IRTSegmentState prevState;
    prevState.all = cookie;
    size_t topIndex = segmentState.parts.topIndex;

    assert(obj != NULL);
    assert(dvmIsHeapAddress(obj));
    assert(table_ != NULL);
    assert(alloc_entries_ <= max_entries_);
    assert(segmentState.parts.numHoles >= prevState.parts.numHoles);

    /*
     * We know there's enough room in the table.  Now we just need to find
     * the right spot.  If there's a hole, find it and fill it; otherwise,
     * add to the end of the list.
     */
    IndirectRef result;
    IndirectRefSlot* slot;
    int numHoles = segmentState.parts.numHoles - prevState.parts.numHoles;
    if (numHoles > 0) {
        assert(topIndex > 1);
        /* find the first hole; likely to be near the end of the list,
         * we know the item at the topIndex is not a hole */
        slot = &table_[topIndex - 1];
        assert(slot->obj != NULL);
        while ((--slot)->obj != NULL) {
            assert(slot >= table_ + prevState.parts.topIndex);
        }
        segmentState.parts.numHoles--;
    } else {
        /* add to the end, grow if needed */
        if (topIndex == alloc_entries_) {
            /* reached end of allocated space; did we hit buffer max? */
            if (topIndex == max_entries_) {
                ALOGE("JNI ERROR (app bug): %s reference table overflow (max=%d)",
                        indirectRefKindToString(kind_), max_entries_);
                return NULL;
            }

            size_t newSize = alloc_entries_ * 2;
            if (newSize > max_entries_) {
                newSize = max_entries_;
            }
            assert(newSize > alloc_entries_);

            IndirectRefSlot* newTable =
                    (IndirectRefSlot*) realloc(table_, newSize * sizeof(IndirectRefSlot));
            if (table_ == NULL) {
                ALOGE("JNI ERROR (app bug): unable to expand %s reference table "
                        "(from %d to %d, max=%d)",
                        indirectRefKindToString(kind_),
                        alloc_entries_, newSize, max_entries_);
                return NULL;
            }

            memset(newTable + alloc_entries_, 0xd1,
                   (newSize - alloc_entries_) * sizeof(IndirectRefSlot));

            alloc_entries_ = newSize;
            table_ = newTable;
        }
        slot = &table_[topIndex++];
        segmentState.parts.topIndex = topIndex;
    }

    slot->obj = obj;
    slot->serial = nextSerial(slot->serial);
    result = toIndirectRef(slot - table_, slot->serial, kind_);

    assert(result != NULL);
    return result;
}

/*
 * Get the referent of an indirect ref from the table.
 *
 * Returns kInvalidIndirectRefObject if iref is invalid.
 */
Object* IndirectRefTable::get(IndirectRef iref) const {
    IndirectRefKind kind = indirectRefKind(iref);
    if (kind != kind_) {
        if (iref == NULL) {
            ALOGW("Attempt to look up NULL %s reference", indirectRefKindToString(kind_));
            return kInvalidIndirectRefObject;
        }
        if (kind == kIndirectKindInvalid) {
            ALOGE("JNI ERROR (app bug): invalid %s reference %p",
                    indirectRefKindToString(kind_), iref);
            abortMaybe();
            return kInvalidIndirectRefObject;
        }
        // References of the requested kind cannot appear within this table.
        return kInvalidIndirectRefObject;
    }

    u4 topIndex = segmentState.parts.topIndex;
    u4 index = extractIndex(iref);
    if (index >= topIndex) {
        /* bad -- stale reference? */
        ALOGE("JNI ERROR (app bug): accessed stale %s reference %p (index %d in a table of size %d)",
                indirectRefKindToString(kind_), iref, index, topIndex);
        abortMaybe();
        return kInvalidIndirectRefObject;
    }

    Object* obj = table_[index].obj;
    if (obj == NULL) {
        ALOGI("JNI ERROR (app bug): accessed deleted %s reference %p",
                indirectRefKindToString(kind_), iref);
        abortMaybe();
        return kInvalidIndirectRefObject;
    }

    u4 serial = extractSerial(iref);
    if (serial != table_[index].serial) {
        ALOGE("JNI ERROR (app bug): attempt to use stale %s reference %p",
                indirectRefKindToString(kind_), iref);
        abortMaybe();
        return kInvalidIndirectRefObject;
    }

    return obj;
}

static int findObject(const Object* obj, int bottomIndex, int topIndex,
        const IndirectRefSlot* table) {
    for (int i = bottomIndex; i < topIndex; ++i) {
        if (table[i].obj == obj) {
            return i;
        }
    }
    return -1;
}

bool IndirectRefTable::contains(const Object* obj) const {
    return findObject(obj, 0, segmentState.parts.topIndex, table_) >= 0;
}

/*
 * Remove "obj" from "pRef".  We extract the table offset bits from "iref"
 * and zap the corresponding entry, leaving a hole if it's not at the top.
 *
 * If the entry is not between the current top index and the bottom index
 * specified by the cookie, we don't remove anything.  This is the behavior
 * required by JNI's DeleteLocalRef function.
 *
 * Note this is NOT called when a local frame is popped.  This is only used
 * for explicit single removals.
 *
 * Returns "false" if nothing was removed.
 */
bool IndirectRefTable::remove(u4 cookie, IndirectRef iref)
{
    IRTSegmentState prevState;
    prevState.all = cookie;
    u4 topIndex = segmentState.parts.topIndex;
    u4 bottomIndex = prevState.parts.topIndex;

    assert(table_ != NULL);
    assert(alloc_entries_ <= max_entries_);
    assert(segmentState.parts.numHoles >= prevState.parts.numHoles);

    IndirectRefKind kind = indirectRefKind(iref);
    u4 index;
    if (kind == kind_) {
        index = extractIndex(iref);
        if (index < bottomIndex) {
            /* wrong segment */
            ALOGV("Attempt to remove index outside index area (%ud vs %ud-%ud)",
                    index, bottomIndex, topIndex);
            return false;
        }
        if (index >= topIndex) {
            /* bad -- stale reference? */
            ALOGD("Attempt to remove invalid index %ud (bottom=%ud top=%ud)",
                    index, bottomIndex, topIndex);
            return false;
        }
        if (table_[index].obj == NULL) {
            ALOGD("Attempt to remove cleared %s reference %p",
                    indirectRefKindToString(kind_), iref);
            return false;
        }
        u4 serial = extractSerial(iref);
        if (table_[index].serial != serial) {
            ALOGD("Attempt to remove stale %s reference %p",
                    indirectRefKindToString(kind_), iref);
            return false;
        }
    } else if (kind == kIndirectKindInvalid && gDvmJni.workAroundAppJniBugs) {
        // reference looks like a pointer, scan the table to find the index
        int i = findObject(reinterpret_cast<Object*>(iref), bottomIndex, topIndex, table_);
        if (i < 0) {
            ALOGW("trying to work around app JNI bugs, but didn't find %p in table!", iref);
            return false;
        }
        index = i;
    } else {
        // References of the requested kind cannot appear within this table.
        return false;
    }

    if (index == topIndex - 1) {
        // Top-most entry.  Scan up and consume holes.
        int numHoles = segmentState.parts.numHoles - prevState.parts.numHoles;
        if (numHoles != 0) {
            while (--topIndex > bottomIndex && numHoles != 0) {
                ALOGV("+++ checking for hole at %d (cookie=0x%08x) val=%p",
                    topIndex-1, cookie, table_[topIndex-1].obj);
                if (table_[topIndex-1].obj != NULL) {
                    break;
                }
                ALOGV("+++ ate hole at %d", topIndex-1);
                numHoles--;
            }
            segmentState.parts.numHoles = numHoles + prevState.parts.numHoles;
            segmentState.parts.topIndex = topIndex;
        } else {
            segmentState.parts.topIndex = topIndex-1;
            ALOGV("+++ ate last entry %d", topIndex-1);
        }
    } else {
        /*
         * Not the top-most entry.  This creates a hole.  We NULL out the
         * entry to prevent somebody from deleting it twice and screwing up
         * the hole count.
         */
        table_[index].obj = NULL;
        segmentState.parts.numHoles++;
        ALOGV("+++ left hole at %d, holes=%d", index, segmentState.parts.numHoles);
    }

    return true;
}

const char* indirectRefKindToString(IndirectRefKind kind)
{
    switch (kind) {
    case kIndirectKindInvalid:      return "invalid";
    case kIndirectKindLocal:        return "local";
    case kIndirectKindGlobal:       return "global";
    case kIndirectKindWeakGlobal:   return "weak global";
    default:                        return "UNKNOWN";
    }
}

void IndirectRefTable::dump(const char* descr) const
{
    size_t count = capacity();
    Object** copy = new Object*[count];
    for (size_t i = 0; i < count; i++) {
        copy[i] = table_[i].obj;
    }
    dvmDumpReferenceTableContents(copy, count, descr);
    delete[] copy;
}
