/*
 * Copyright (C) 2010 The Android Open Source Project
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
 * This provides a handful of correctness and speed tests on our atomic
 * operations.
 *
 * This doesn't really belong here, but we currently lack a better place
 * for it, so this will do for now.
 */
#include "Dalvik.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <cutils/atomic.h>
#ifdef __arm__
# include <machine/cpu-features.h>
#endif

#define USE_ATOMIC      1
#define THREAD_COUNT    10
#define ITERATION_COUNT 500000

#ifdef HAVE_ANDROID_OS
/*#define TEST_BIONIC 1*/
#endif


#ifdef TEST_BIONIC
extern int __atomic_cmpxchg(int old, int _new, volatile int *ptr);
extern int __atomic_swap(int _new, volatile int *ptr);
extern int __atomic_dec(volatile int *ptr);
extern int __atomic_inc(volatile int *ptr);
#endif

static pthread_mutex_t waitLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t waitCond = PTHREAD_COND_INITIALIZER;

static volatile int threadsStarted = 0;

/* results */
static int incTest = 0;
static int decTest = 0;
static int addTest = 0;
static int andTest = 0;
static int orTest = 0;
static int casTest = 0;
static int failingCasTest = 0;
static int64_t wideCasTest = 0x6600000077000000LL;

/*
 * Get a relative time value.
 */
static int64_t getRelativeTimeNsec()
{
#define HAVE_POSIX_CLOCKS
#ifdef HAVE_POSIX_CLOCKS
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t) now.tv_sec*1000000000LL + now.tv_nsec;
#else
    struct timeval now;
    gettimeofday(&now, NULL);
    return (int64_t) now.tv_sec*1000000000LL + now.tv_usec * 1000LL;
#endif
}


/*
 * Non-atomic implementations, for comparison.
 *
 * If these get inlined the compiler may figure out what we're up to and
 * completely elide the operations.
 */
static void incr() __attribute__((noinline));
static void decr() __attribute__((noinline));
static void add(int addVal) __attribute__((noinline));
static int compareAndSwap(int oldVal, int newVal, int* addr) __attribute__((noinline));
static int compareAndSwapWide(int64_t oldVal, int64_t newVal, int64_t* addr) __attribute__((noinline));

static void incr()
{
    incTest++;
}
static void decr()
{
    decTest--;
}
static void add(int32_t addVal)
{
    addTest += addVal;
}
static int compareAndSwap(int32_t oldVal, int32_t newVal, int32_t* addr)
{
    if (*addr == oldVal) {
        *addr = newVal;
        return 0;
    }
    return 1;
}
static int compareAndSwapWide(int64_t oldVal, int64_t newVal, int64_t* addr)
{
    if (*addr == oldVal) {
        *addr = newVal;
        return 0;
    }
    return 1;
}

/*
 * Exercise several of the atomic ops.
 */
static void doAtomicTest(int num)
{
    int addVal = (num & 0x01) + 1;

    int i;
    for (i = 0; i < ITERATION_COUNT; i++) {
        if (USE_ATOMIC) {
            android_atomic_inc(&incTest);
            android_atomic_dec(&decTest);
            android_atomic_add(addVal, &addTest);

            int val;
            do {
                val = casTest;
            } while (android_atomic_release_cas(val, val+3, &casTest) != 0);
            do {
                val = casTest;
            } while (android_atomic_acquire_cas(val, val-1, &casTest) != 0);

            int64_t wval;
            do {
                wval = dvmQuasiAtomicRead64(&wideCasTest);
            } while (dvmQuasiAtomicCas64(wval,
                        wval + 0x0000002000000001LL, &wideCasTest) != 0);
            do {
                wval = dvmQuasiAtomicRead64(&wideCasTest);
            } while (dvmQuasiAtomicCas64(wval,
                        wval - 0x0000002000000001LL, &wideCasTest) != 0);
        } else {
            incr();
            decr();
            add(addVal);

            int val;
            do {
                val = casTest;
            } while (compareAndSwap(val, val+3, &casTest) != 0);
            do {
                val = casTest;
            } while (compareAndSwap(val, val-1, &casTest) != 0);

            int64_t wval;
            do {
                wval = wideCasTest;
            } while (compareAndSwapWide(wval,
                        wval + 0x0000002000000001LL, &wideCasTest) != 0);
            do {
                wval = wideCasTest;
            } while (compareAndSwapWide(wval,
                        wval - 0x0000002000000001LL, &wideCasTest) != 0);
        }
    }
}

/*
 * Entry point for multi-thread test.
 */
static void* atomicTest(void* arg)
{
    pthread_mutex_lock(&waitLock);
    threadsStarted++;
    pthread_cond_wait(&waitCond, &waitLock);
    pthread_mutex_unlock(&waitLock);

    doAtomicTest((int) arg);

    return NULL;
}

/* lifted from a VM test */
static int64_t testAtomicSpeedSub(int repeatCount)
{
    static int value = 7;
    int* valuePtr = &value;
    int64_t start, end;
    int i;

    start = getRelativeTimeNsec();

    for (i = repeatCount / 10; i != 0; i--) {
        if (USE_ATOMIC) {
            // succeed 10x
            android_atomic_release_cas(7, 7, valuePtr);
            android_atomic_release_cas(7, 7, valuePtr);
            android_atomic_release_cas(7, 7, valuePtr);
            android_atomic_release_cas(7, 7, valuePtr);
            android_atomic_release_cas(7, 7, valuePtr);
            android_atomic_release_cas(7, 7, valuePtr);
            android_atomic_release_cas(7, 7, valuePtr);
            android_atomic_release_cas(7, 7, valuePtr);
            android_atomic_release_cas(7, 7, valuePtr);
            android_atomic_release_cas(7, 7, valuePtr);
        } else {
            // succeed 10x
            compareAndSwap(7, 7, valuePtr);
            compareAndSwap(7, 7, valuePtr);
            compareAndSwap(7, 7, valuePtr);
            compareAndSwap(7, 7, valuePtr);
            compareAndSwap(7, 7, valuePtr);
            compareAndSwap(7, 7, valuePtr);
            compareAndSwap(7, 7, valuePtr);
            compareAndSwap(7, 7, valuePtr);
            compareAndSwap(7, 7, valuePtr);
            compareAndSwap(7, 7, valuePtr);
        }
    }

    end = getRelativeTimeNsec();

    dvmFprintf(stdout, ".");
    fflush(stdout);
    return end - start;
}

static void testAtomicSpeed()
{
    static const int kIterations = 10;
    static const int kRepeatCount = 5 * 1000 * 1000;
    static const int kDelay = 50 * 1000;
    int64_t results[kIterations];
    int i;

    for (i = 0; i < kIterations; i++) {
        results[i] = testAtomicSpeedSub(kRepeatCount);
        usleep(kDelay);
    }

    dvmFprintf(stdout, "\n");
    dvmFprintf(stdout, "%s speed test results (%d per iteration):\n",
        USE_ATOMIC ? "Atomic" : "Non-atomic", kRepeatCount);
    for (i = 0; i < kIterations; i++) {
        dvmFprintf(stdout,
            " %2d: %.3fns\n", i, (double) results[i] / kRepeatCount);
    }
}

/*
 * Start tests, show results.
 */
bool dvmTestAtomicSpeed()
{
    pthread_t threads[THREAD_COUNT];
    void *(*startRoutine)(void*) = atomicTest;
    int64_t startWhen, endWhen;

#if defined(__ARM_ARCH__)
    dvmFprintf(stdout, "__ARM_ARCH__ is %d\n", __ARM_ARCH__);
#endif
#if defined(ANDROID_SMP)
    dvmFprintf(stdout, "ANDROID_SMP is %d\n", ANDROID_SMP);
#endif
    dvmFprintf(stdout, "Creating threads\n");

    int i;
    for (i = 0; i < THREAD_COUNT; i++) {
        void* arg = (void*) i;
        if (pthread_create(&threads[i], NULL, startRoutine, arg) != 0) {
            dvmFprintf(stderr, "thread create failed\n");
        }
    }

    /* wait for all the threads to reach the starting line */
    while (1) {
        pthread_mutex_lock(&waitLock);
        if (threadsStarted == THREAD_COUNT) {
            dvmFprintf(stdout, "Starting test\n");
            startWhen = getRelativeTimeNsec();
            pthread_cond_broadcast(&waitCond);
            pthread_mutex_unlock(&waitLock);
            break;
        }
        pthread_mutex_unlock(&waitLock);
        usleep(100000);
    }

    for (i = 0; i < THREAD_COUNT; i++) {
        void* retval;
        if (pthread_join(threads[i], &retval) != 0) {
            dvmFprintf(stderr, "thread join (%d) failed\n", i);
        }
    }

    endWhen = getRelativeTimeNsec();
    dvmFprintf(stdout, "All threads stopped, time is %.6fms\n",
        (endWhen - startWhen) / 1000000.0);

    /*
     * Show results; expecting:
     *
     * incTest = 5000000
     * decTest = -5000000
     * addTest = 7500000
     * casTest = 10000000
     * wideCasTest = 0x6600000077000000
     */
    dvmFprintf(stdout, "incTest = %d\n", incTest);
    dvmFprintf(stdout, "decTest = %d\n", decTest);
    dvmFprintf(stdout, "addTest = %d\n", addTest);
    dvmFprintf(stdout, "casTest = %d\n", casTest);
    dvmFprintf(stdout, "wideCasTest = 0x%llx\n", wideCasTest);

    /* do again, serially (SMP check) */
    startWhen = getRelativeTimeNsec();
    for (i = 0; i < THREAD_COUNT; i++) {
        doAtomicTest(i);
    }
    endWhen = getRelativeTimeNsec();
    dvmFprintf(stdout, "Same iterations done serially: time is %.6fms\n",
        (endWhen - startWhen) / 1000000.0);

    /*
     * Hard to do a meaningful thrash test on these, so just do a simple
     * function test.
     */
    andTest = 0xffd7fa96;
    orTest = 0x122221ff;
    android_atomic_and(0xfffdaf96, &andTest);
    android_atomic_or(0xdeaaeb00, &orTest);
    if (android_atomic_release_cas(failingCasTest+1, failingCasTest-1,
            &failingCasTest) == 0)
        dvmFprintf(stdout, "failing test did not fail!\n");

    dvmFprintf(stdout, "andTest = %#x\n", andTest);
    dvmFprintf(stdout, "orTest = %#x\n", orTest);
    dvmFprintf(stdout, "failingCasTest = %d\n", failingCasTest);

#ifdef TEST_BIONIC
    /*
     * Quick function test on the bionic ops.
     */
    int prev;
    int tester = 7;
    prev = __atomic_inc(&tester);
    __atomic_inc(&tester);
    __atomic_inc(&tester);
    dvmFprintf(stdout, "bionic 3 inc: %d -> %d\n", prev, tester);
    prev = __atomic_dec(&tester);
    __atomic_dec(&tester);
    __atomic_dec(&tester);
    dvmFprintf(stdout, "bionic 3 dec: %d -> %d\n", prev, tester);
    prev = __atomic_swap(27, &tester);
    dvmFprintf(stdout, "bionic swap: %d -> %d\n", prev, tester);
    int swapok = __atomic_cmpxchg(27, 72, &tester);
    dvmFprintf(stdout, "bionic cmpxchg: %d (%d)\n", tester, swapok);
#endif

    testAtomicSpeed();

    return 0;
}
