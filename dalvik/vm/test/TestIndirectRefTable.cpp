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
 * Test the indirect reference table implementation.
 */
#include "Dalvik.h"

#include <stdlib.h>
#include <sys/time.h>

#ifndef NDEBUG

#define DBUG_MSG    ALOGI

class Stopwatch {
public:
    Stopwatch() {
        reset();
    }

    void reset() {
        start_ = now();
    }

    float elapsedSeconds() {
        return (now() - start_) * 0.000001f;
    }

private:
    u8 start_;

    static u8 now() {
#ifdef HAVE_POSIX_CLOCKS
        struct timespec tm;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tm);
        return tm.tv_sec * 1000000LL + tm.tv_nsec / 1000;
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000000LL + tv.tv_usec;
#endif
    }
};

/*
 * Basic add/get/delete tests in an unsegmented table.
 */
static bool basicTest()
{
    static const int kTableMax = 20;
    IndirectRefTable irt;
    IndirectRef iref0, iref1, iref2, iref3;
    IndirectRef manyRefs[kTableMax];
    ClassObject* clazz = dvmFindClass("Ljava/lang/Object;", NULL);
    Object* obj0 = dvmAllocObject(clazz, ALLOC_DONT_TRACK);
    Object* obj1 = dvmAllocObject(clazz, ALLOC_DONT_TRACK);
    Object* obj2 = dvmAllocObject(clazz, ALLOC_DONT_TRACK);
    Object* obj3 = dvmAllocObject(clazz, ALLOC_DONT_TRACK);
    const u4 cookie = IRT_FIRST_SEGMENT;
    bool result = false;

    if (!irt.init(kTableMax/2, kTableMax, kIndirectKindGlobal)) {
        return false;
    }

    iref0 = (IndirectRef) 0x11110;
    if (irt.remove(cookie, iref0)) {
        ALOGE("unexpectedly successful removal");
        goto bail;
    }

    /*
     * Add three, check, remove in the order in which they were added.
     */
    DBUG_MSG("+++ START fifo\n");
    iref0 = irt.add(cookie, obj0);
    iref1 = irt.add(cookie, obj1);
    iref2 = irt.add(cookie, obj2);
    if (iref0 == NULL || iref1 == NULL || iref2 == NULL) {
        ALOGE("trivial add1 failed");
        goto bail;
    }

    if (irt.get(iref0) != obj0 ||
            irt.get(iref1) != obj1 ||
            irt.get(iref2) != obj2) {
        ALOGE("objects don't match expected values %p %p %p vs. %p %p %p",
                irt.get(iref0), irt.get(iref1), irt.get(iref2),
                obj0, obj1, obj2);
        goto bail;
    } else {
        DBUG_MSG("+++ obj1=%p --> iref1=%p\n", obj1, iref1);
    }

    if (!irt.remove(cookie, iref0) ||
            !irt.remove(cookie, iref1) ||
            !irt.remove(cookie, iref2))
    {
        ALOGE("fifo deletion failed");
        goto bail;
    }

    /* table should be empty now */
    if (irt.capacity() != 0) {
        ALOGE("fifo del not empty");
        goto bail;
    }

    /* get invalid entry (off the end of the list) */
    if (irt.get(iref0) != kInvalidIndirectRefObject) {
        ALOGE("stale entry get succeeded unexpectedly");
        goto bail;
    }

    /*
     * Add three, remove in the opposite order.
     */
    DBUG_MSG("+++ START lifo\n");
    iref0 = irt.add(cookie, obj0);
    iref1 = irt.add(cookie, obj1);
    iref2 = irt.add(cookie, obj2);
    if (iref0 == NULL || iref1 == NULL || iref2 == NULL) {
        ALOGE("trivial add2 failed");
        goto bail;
    }

    if (!irt.remove(cookie, iref2) ||
            !irt.remove(cookie, iref1) ||
            !irt.remove(cookie, iref0))
    {
        ALOGE("lifo deletion failed");
        goto bail;
    }

    /* table should be empty now */
    if (irt.capacity() != 0) {
        ALOGE("lifo del not empty");
        goto bail;
    }

    /*
     * Add three, remove middle / middle / bottom / top.  (Second attempt
     * to remove middle should fail.)
     */
    DBUG_MSG("+++ START unorder\n");
    iref0 = irt.add(cookie, obj0);
    iref1 = irt.add(cookie, obj1);
    iref2 = irt.add(cookie, obj2);
    if (iref0 == NULL || iref1 == NULL || iref2 == NULL) {
        ALOGE("trivial add3 failed");
        goto bail;
    }

    if (irt.capacity() != 3) {
        ALOGE("expected 3 entries, found %d", irt.capacity());
        goto bail;
    }

    if (!irt.remove(cookie, iref1) || irt.remove(cookie, iref1)) {
        ALOGE("unorder deletion1 failed");
        goto bail;
    }

    /* get invalid entry (from hole) */
    if (irt.get(iref1) != kInvalidIndirectRefObject) {
        ALOGE("hole get succeeded unexpectedly");
        goto bail;
    }

    if (!irt.remove(cookie, iref2) || !irt.remove(cookie, iref0)) {
        ALOGE("unorder deletion2 failed");
        goto bail;
    }

    /* table should be empty now */
    if (irt.capacity() != 0) {
        ALOGE("unorder del not empty");
        goto bail;
    }

    /*
     * Add four entries.  Remove #1, add new entry, verify that table size
     * is still 4 (i.e. holes are getting filled).  Remove #1 and #3, verify
     * that we delete one and don't hole-compact the other.
     */
    DBUG_MSG("+++ START hole fill\n");
    iref0 = irt.add(cookie, obj0);
    iref1 = irt.add(cookie, obj1);
    iref2 = irt.add(cookie, obj2);
    iref3 = irt.add(cookie, obj3);
    if (iref0 == NULL || iref1 == NULL || iref2 == NULL || iref3 == NULL) {
        ALOGE("trivial add4 failed");
        goto bail;
    }
    if (!irt.remove(cookie, iref1)) {
        ALOGE("remove 1 of 4 failed");
        goto bail;
    }
    iref1 = irt.add(cookie, obj1);
    if (irt.capacity() != 4) {
        ALOGE("hole not filled");
        goto bail;
    }
    if (!irt.remove(cookie, iref1) || !irt.remove(cookie, iref3)) {
        ALOGE("remove 1/3 failed");
        goto bail;
    }
    if (irt.capacity() != 3) {
        ALOGE("should be 3 after two deletions");
        goto bail;
    }
    if (!irt.remove(cookie, iref2) || !irt.remove(cookie, iref0)) {
        ALOGE("remove 2/0 failed");
        goto bail;
    }
    if (irt.capacity() != 0) {
        ALOGE("not empty after split remove");
        goto bail;
    }

    /*
     * Add an entry, remove it, add a new entry, and try to use the original
     * iref.  They have the same slot number but are for different objects.
     * With the extended checks in place, this should fail.
     */
    DBUG_MSG("+++ START switched\n");
    iref0 = irt.add(cookie, obj0);
    irt.remove(cookie, iref0);
    iref1 = irt.add(cookie, obj1);
    if (irt.remove(cookie, iref0)) {
        ALOGE("mismatched del succeeded (%p vs %p)", iref0, iref1);
        goto bail;
    }
    if (!irt.remove(cookie, iref1)) {
        ALOGE("switched del failed");
        goto bail;
    }
    if (irt.capacity() != 0) {
        ALOGE("switching del not empty");
        goto bail;
    }

    /*
     * Same as above, but with the same object.  A more rigorous checker
     * (e.g. with slot serialization) will catch this.
     */
    DBUG_MSG("+++ START switched same object\n");
    iref0 = irt.add(cookie, obj0);
    irt.remove(cookie, iref0);
    iref1 = irt.add(cookie, obj0);
    if (iref0 != iref1) {
        /* try 0, should not work */
        if (irt.remove(cookie, iref0)) {
            ALOGE("temporal del succeeded (%p vs %p)", iref0, iref1);
            goto bail;
        }
    }
    if (!irt.remove(cookie, iref1)) {
        ALOGE("temporal cleanup failed");
        goto bail;
    }
    if (irt.capacity() != 0) {
        ALOGE("temporal del not empty");
        goto bail;
    }

    DBUG_MSG("+++ START null lookup\n");
    if (irt.get(NULL) != kInvalidIndirectRefObject) {
        ALOGE("null lookup succeeded");
        goto bail;
    }

    DBUG_MSG("+++ START stale lookup\n");
    iref0 = irt.add(cookie, obj0);
    irt.remove(cookie, iref0);
    if (irt.get(iref0) != kInvalidIndirectRefObject) {
        ALOGE("stale lookup succeeded");
        goto bail;
    }

    /*
     * Test table overflow.
     */
    DBUG_MSG("+++ START overflow\n");
    int i;
    for (i = 0; i < kTableMax; i++) {
        manyRefs[i] = irt.add(cookie, obj0);
        if (manyRefs[i] == NULL) {
            ALOGE("Failed adding %d of %d", i, kTableMax);
            goto bail;
        }
    }
    if (irt.add(cookie, obj0) != NULL) {
        ALOGE("Table overflow succeeded");
        goto bail;
    }
    if (irt.capacity() != (size_t)kTableMax) {
        ALOGE("Expected %d entries, found %d", kTableMax, irt.capacity());
        goto bail;
    }
    irt.dump("table with 20 entries, all filled");
    for (i = 0; i < kTableMax-1; i++) {
        if (!irt.remove(cookie, manyRefs[i])) {
            ALOGE("multi-remove failed at %d", i);
            goto bail;
        }
    }
    irt.dump("table with 20 entries, 19 of them holes");
    /* because of removal order, should have 20 entries, 19 of them holes */
    if (irt.capacity() != (size_t)kTableMax) {
        ALOGE("Expected %d entries (with holes), found %d",
                kTableMax, irt.capacity());
        goto bail;
    }
    if (!irt.remove(cookie, manyRefs[kTableMax-1])) {
        ALOGE("multi-remove final failed");
        goto bail;
    }
    if (irt.capacity() != 0) {
        ALOGE("multi-del not empty");
        goto bail;
    }

    /* Done */
    DBUG_MSG("+++ basic test complete\n");
    result = true;

bail:
    irt.destroy();
    return result;
}

static bool performanceTest()
{
    static const int kTableMax = 100;
    IndirectRefTable irt;
    IndirectRef manyRefs[kTableMax];
    ClassObject* clazz = dvmFindClass("Ljava/lang/Object;", NULL);
    Object* obj0 = dvmAllocObject(clazz, ALLOC_DONT_TRACK);
    const u4 cookie = IRT_FIRST_SEGMENT;
    const int kLoops = 100000;
    Stopwatch stopwatch;

    DBUG_MSG("+++ START performance\n");

    if (!irt.init(kTableMax, kTableMax, kIndirectKindGlobal)) {
        return false;
    }

    stopwatch.reset();
    for (int loop = 0; loop < kLoops; loop++) {
        for (int i = 0; i < kTableMax; i++) {
            manyRefs[i] = irt.add(cookie, obj0);
        }
        for (int i = 0; i < kTableMax; i++) {
            irt.remove(cookie, manyRefs[i]);
        }
    }
    DBUG_MSG("Add/remove %d objects FIFO order, %d iterations, %0.3fms / iteration",
            kTableMax, kLoops, stopwatch.elapsedSeconds() * 1000 / kLoops);

    stopwatch.reset();
    for (int loop = 0; loop < kLoops; loop++) {
        for (int i = 0; i < kTableMax; i++) {
            manyRefs[i] = irt.add(cookie, obj0);
        }
        for (int i = kTableMax; i-- > 0; ) {
            irt.remove(cookie, manyRefs[i]);
        }
    }
    DBUG_MSG("Add/remove %d objects LIFO order, %d iterations, %0.3fms / iteration",
            kTableMax, kLoops, stopwatch.elapsedSeconds() * 1000  / kLoops);

    for (int i = 0; i < kTableMax; i++) {
        manyRefs[i] = irt.add(cookie, obj0);
    }
    stopwatch.reset();
    for (int loop = 0; loop < kLoops; loop++) {
        for (int i = 0; i < kTableMax; i++) {
            irt.get(manyRefs[i]);
        }
    }
    DBUG_MSG("Get %d objects, %d iterations, %0.3fms / iteration",
            kTableMax, kLoops, stopwatch.elapsedSeconds() * 1000  / kLoops);
    for (int i = kTableMax; i-- > 0; ) {
        irt.remove(cookie, manyRefs[i]);
    }

    irt.destroy();
    return true;
}

/*
 * Some quick tests.
 */
bool dvmTestIndirectRefTable()
{
    if (!basicTest()) {
        ALOGE("IRT basic test failed");
        return false;
    }

    if (!performanceTest()) {
        ALOGE("IRT performance test failed");
        return false;
    }

    return true;
}

#endif /*NDEBUG*/
