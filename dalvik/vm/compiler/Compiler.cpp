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

#include <sys/mman.h>
#include <errno.h>
#include <cutils/ashmem.h>

#include "Dalvik.h"
#include "interp/Jit.h"
#include "CompilerInternals.h"
#ifdef ARCH_IA32
#include "codegen/x86/Translator.h"
#include "codegen/x86/Lower.h"
#endif

extern "C" void dvmCompilerTemplateStart(void);
extern "C" void dvmCompilerTemplateEnd(void);

static inline bool workQueueLength(void)
{
    return gDvmJit.compilerQueueLength;
}

static CompilerWorkOrder workDequeue(void)
{
    assert(gDvmJit.compilerWorkQueue[gDvmJit.compilerWorkDequeueIndex].kind
           != kWorkOrderInvalid);
    CompilerWorkOrder work =
        gDvmJit.compilerWorkQueue[gDvmJit.compilerWorkDequeueIndex];
    gDvmJit.compilerWorkQueue[gDvmJit.compilerWorkDequeueIndex++].kind =
        kWorkOrderInvalid;
    if (gDvmJit.compilerWorkDequeueIndex == COMPILER_WORK_QUEUE_SIZE) {
        gDvmJit.compilerWorkDequeueIndex = 0;
    }
    gDvmJit.compilerQueueLength--;
    if (gDvmJit.compilerQueueLength == 0) {
        dvmSignalCond(&gDvmJit.compilerQueueEmpty);
    }

    /* Remember the high water mark of the queue length */
    if (gDvmJit.compilerQueueLength > gDvmJit.compilerMaxQueued)
        gDvmJit.compilerMaxQueued = gDvmJit.compilerQueueLength;

    return work;
}

/*
 * Enqueue a work order - retrying until successful.  If attempt to enqueue
 * is repeatedly unsuccessful, assume the JIT is in a bad state and force a
 * code cache reset.
 */
#define ENQUEUE_MAX_RETRIES 20
void dvmCompilerForceWorkEnqueue(const u2 *pc, WorkOrderKind kind, void* info)
{
    bool success;
    int retries = 0;
    do {
        success = dvmCompilerWorkEnqueue(pc, kind, info);
        if (!success) {
            retries++;
            if (retries > ENQUEUE_MAX_RETRIES) {
                ALOGE("JIT: compiler queue wedged - forcing reset");
                gDvmJit.codeCacheFull = true;  // Force reset
                success = true;  // Because we'll drop the order now anyway
            } else {
                dvmLockMutex(&gDvmJit.compilerLock);
                pthread_cond_wait(&gDvmJit.compilerQueueActivity,
                                  &gDvmJit.compilerLock);
                dvmUnlockMutex(&gDvmJit.compilerLock);

            }
        }
    } while (!success);
}

/*
 * Attempt to enqueue a work order, returning true if successful.
 *
 * NOTE: Make sure that the caller frees the info pointer if the return value
 * is false.
 */
bool dvmCompilerWorkEnqueue(const u2 *pc, WorkOrderKind kind, void* info)
{
    int cc;
    int i;
    int numWork;
    bool result = true;

    dvmLockMutex(&gDvmJit.compilerLock);

    /*
     * Return if queue or code cache is full.
     */
    if (gDvmJit.compilerQueueLength == COMPILER_WORK_QUEUE_SIZE ||
        gDvmJit.codeCacheFull == true) {
        dvmUnlockMutex(&gDvmJit.compilerLock);
        return false;
    }

    for (numWork = gDvmJit.compilerQueueLength,
           i = gDvmJit.compilerWorkDequeueIndex;
         numWork > 0;
         numWork--) {
        /* Already enqueued */
        if (gDvmJit.compilerWorkQueue[i++].pc == pc) {
            dvmUnlockMutex(&gDvmJit.compilerLock);
            return true;
        }
        /* Wrap around */
        if (i == COMPILER_WORK_QUEUE_SIZE)
            i = 0;
    }

    CompilerWorkOrder *newOrder =
        &gDvmJit.compilerWorkQueue[gDvmJit.compilerWorkEnqueueIndex];
    newOrder->pc = pc;
    newOrder->kind = kind;
    newOrder->info = info;
    newOrder->result.methodCompilationAborted = NULL;
    newOrder->result.codeAddress = NULL;
    newOrder->result.discardResult =
        (kind == kWorkOrderTraceDebug) ? true : false;
    newOrder->result.cacheVersion = gDvmJit.cacheVersion;
    newOrder->result.requestingThread = dvmThreadSelf();

    gDvmJit.compilerWorkEnqueueIndex++;
    if (gDvmJit.compilerWorkEnqueueIndex == COMPILER_WORK_QUEUE_SIZE)
        gDvmJit.compilerWorkEnqueueIndex = 0;
    gDvmJit.compilerQueueLength++;
    cc = pthread_cond_signal(&gDvmJit.compilerQueueActivity);
    assert(cc == 0);

    dvmUnlockMutex(&gDvmJit.compilerLock);
    return result;
}

/* Block until the queue length is 0, or there is a pending suspend request */
void dvmCompilerDrainQueue(void)
{
    Thread *self = dvmThreadSelf();

    dvmLockMutex(&gDvmJit.compilerLock);
    while (workQueueLength() != 0 && !gDvmJit.haltCompilerThread &&
           self->suspendCount == 0) {
        /*
         * Use timed wait here - more than one mutator threads may be blocked
         * but the compiler thread will only signal once when the queue is
         * emptied. Furthermore, the compiler thread may have been shutdown
         * so the blocked thread may never get the wakeup signal.
         */
        dvmRelativeCondWait(&gDvmJit.compilerQueueEmpty, &gDvmJit.compilerLock,                             1000, 0);
    }
    dvmUnlockMutex(&gDvmJit.compilerLock);
}

bool dvmCompilerSetupCodeCache(void)
{
    int fd;

    /* Allocate the code cache */
    fd = ashmem_create_region("dalvik-jit-code-cache", gDvmJit.codeCacheSize);
    if (fd < 0) {
        ALOGE("Could not create %u-byte ashmem region for the JIT code cache",
             gDvmJit.codeCacheSize);
        return false;
    }
    gDvmJit.codeCache = mmap(NULL, gDvmJit.codeCacheSize,
                             PROT_READ | PROT_WRITE | PROT_EXEC,
                             MAP_PRIVATE , fd, 0);
    close(fd);
    if (gDvmJit.codeCache == MAP_FAILED) {
        ALOGE("Failed to mmap the JIT code cache of size %d: %s", gDvmJit.codeCacheSize, strerror(errno));
        return false;
    }

    gDvmJit.pageSizeMask = getpagesize() - 1;

    /* This can be found through "dalvik-jit-code-cache" in /proc/<pid>/maps */
    // ALOGD("Code cache starts at %p", gDvmJit.codeCache);

#ifndef ARCH_IA32
    /* Copy the template code into the beginning of the code cache */
    int templateSize = (intptr_t) dvmCompilerTemplateEnd -
                       (intptr_t) dvmCompilerTemplateStart;
    memcpy((void *) gDvmJit.codeCache,
           (void *) dvmCompilerTemplateStart,
           templateSize);

    /*
     * Work around a CPU bug by keeping the 32-bit ARM handler code in its own
     * page.
     */
    if (dvmCompilerInstructionSet() == DALVIK_JIT_THUMB2) {
        templateSize = (templateSize + 4095) & ~4095;
    }

    gDvmJit.templateSize = templateSize;
    gDvmJit.codeCacheByteUsed = templateSize;

    /* Only flush the part in the code cache that is being used now */
    dvmCompilerCacheFlush((intptr_t) gDvmJit.codeCache,
                          (intptr_t) gDvmJit.codeCache + templateSize, 0);
#else
    gDvmJit.codeCacheByteUsed = 0;
    stream = (char*)gDvmJit.codeCache + gDvmJit.codeCacheByteUsed;
    ALOGV("codeCache = %p stream = %p before initJIT", gDvmJit.codeCache, stream);
    streamStart = stream;
    initJIT(NULL, NULL);
    gDvmJit.templateSize = (stream - streamStart);
    gDvmJit.codeCacheByteUsed = (stream - streamStart);
    ALOGV("stream = %p after initJIT", stream);
#endif

    int result = mprotect(gDvmJit.codeCache, gDvmJit.codeCacheSize,
                          PROTECT_CODE_CACHE_ATTRS);

    if (result == -1) {
        ALOGE("Failed to remove the write permission for the code cache");
        dvmAbort();
    }

    return true;
}

static void crawlDalvikStack(Thread *thread, bool print)
{
    void *fp = thread->interpSave.curFrame;
    StackSaveArea* saveArea = NULL;
    int stackLevel = 0;

    if (print) {
        ALOGD("Crawling tid %d (%s / %p %s)", thread->systemTid,
             dvmGetThreadStatusStr(thread->status),
             thread->inJitCodeCache,
             thread->inJitCodeCache ? "jit" : "interp");
    }
    /* Crawl the Dalvik stack frames to clear the returnAddr field */
    while (fp != NULL) {
        saveArea = SAVEAREA_FROM_FP(fp);

        if (print) {
            if (dvmIsBreakFrame((u4*)fp)) {
                ALOGD("  #%d: break frame (%p)",
                     stackLevel, saveArea->returnAddr);
            }
            else {
                ALOGD("  #%d: %s.%s%s (%p)",
                     stackLevel,
                     saveArea->method->clazz->descriptor,
                     saveArea->method->name,
                     dvmIsNativeMethod(saveArea->method) ?
                         " (native)" : "",
                     saveArea->returnAddr);
            }
        }
        stackLevel++;
        saveArea->returnAddr = NULL;
        assert(fp != saveArea->prevFrame);
        fp = saveArea->prevFrame;
    }
    /* Make sure the stack is fully unwound to the bottom */
    assert(saveArea == NULL ||
           (u1 *) (saveArea+1) == thread->interpStackStart);
}

static void resetCodeCache(void)
{
    Thread* thread;
    u8 startTime = dvmGetRelativeTimeUsec();
    int inJit = 0;
    int byteUsed = gDvmJit.codeCacheByteUsed;

    /* If any thread is found stuck in the JIT state, don't reset the cache  */
    dvmLockThreadList(NULL);
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        /*
         * Crawl the stack to wipe out the returnAddr field so that
         * 1) the soon-to-be-deleted code in the JIT cache won't be used
         * 2) or the thread stuck in the JIT land will soon return
         *    to the interpreter land
         */
        crawlDalvikStack(thread, false);
        if (thread->inJitCodeCache) {
            inJit++;
        }
        /* Cancel any ongoing trace selection */
        dvmDisableSubMode(thread, kSubModeJitTraceBuild);
    }
    dvmUnlockThreadList();

    if (inJit) {
        ALOGD("JIT code cache reset delayed (%d bytes %d/%d)",
             gDvmJit.codeCacheByteUsed, gDvmJit.numCodeCacheReset,
             ++gDvmJit.numCodeCacheResetDelayed);
        return;
    }

    /* Lock the mutex to clean up the work queue */
    dvmLockMutex(&gDvmJit.compilerLock);

    /* Update the translation cache version */
    gDvmJit.cacheVersion++;

    /* Drain the work queue to free the work orders */
    while (workQueueLength()) {
        CompilerWorkOrder work = workDequeue();
        free(work.info);
    }

    /* Reset the JitEntry table contents to the initial unpopulated state */
    dvmJitResetTable();

    UNPROTECT_CODE_CACHE(gDvmJit.codeCache, gDvmJit.codeCacheByteUsed);
    /*
     * Wipe out the code cache content to force immediate crashes if
     * stale JIT'ed code is invoked.
     */
    dvmCompilerCacheClear((char *) gDvmJit.codeCache + gDvmJit.templateSize,
                          gDvmJit.codeCacheByteUsed - gDvmJit.templateSize);

    dvmCompilerCacheFlush((intptr_t) gDvmJit.codeCache,
                          (intptr_t) gDvmJit.codeCache +
                          gDvmJit.codeCacheByteUsed, 0);

    PROTECT_CODE_CACHE(gDvmJit.codeCache, gDvmJit.codeCacheByteUsed);

    /* Reset the current mark of used bytes to the end of template code */
    gDvmJit.codeCacheByteUsed = gDvmJit.templateSize;
    gDvmJit.numCompilations = 0;

    /* Reset the work queue */
    memset(gDvmJit.compilerWorkQueue, 0,
           sizeof(CompilerWorkOrder) * COMPILER_WORK_QUEUE_SIZE);
    gDvmJit.compilerWorkEnqueueIndex = gDvmJit.compilerWorkDequeueIndex = 0;
    gDvmJit.compilerQueueLength = 0;

    /* Reset the IC patch work queue */
    dvmLockMutex(&gDvmJit.compilerICPatchLock);
    gDvmJit.compilerICPatchIndex = 0;
    dvmUnlockMutex(&gDvmJit.compilerICPatchLock);

    /*
     * Reset the inflight compilation address (can only be done in safe points
     * or by the compiler thread when its thread state is RUNNING).
     */
    gDvmJit.inflightBaseAddr = NULL;

    /* All clear now */
    gDvmJit.codeCacheFull = false;

    dvmUnlockMutex(&gDvmJit.compilerLock);

    ALOGD("JIT code cache reset in %lld ms (%d bytes %d/%d)",
         (dvmGetRelativeTimeUsec() - startTime) / 1000,
         byteUsed, ++gDvmJit.numCodeCacheReset,
         gDvmJit.numCodeCacheResetDelayed);
}

/*
 * Perform actions that are only safe when all threads are suspended. Currently
 * we do:
 * 1) Check if the code cache is full. If so reset it and restart populating it
 *    from scratch.
 * 2) Patch predicted chaining cells by consuming recorded work orders.
 */
void dvmCompilerPerformSafePointChecks(void)
{
    if (gDvmJit.codeCacheFull) {
        resetCodeCache();
    }
    dvmCompilerPatchInlineCache();
}

static bool compilerThreadStartup(void)
{
    JitEntry *pJitTable = NULL;
    unsigned char *pJitProfTable = NULL;
    JitTraceProfCounters *pJitTraceProfCounters = NULL;
    unsigned int i;

    if (!dvmCompilerArchInit())
        goto fail;

    /*
     * Setup the code cache if we have not inherited a valid code cache
     * from the zygote.
     */
    if (gDvmJit.codeCache == NULL) {
        if (!dvmCompilerSetupCodeCache())
            goto fail;
    }

    /* Allocate the initial arena block */
    if (dvmCompilerHeapInit() == false) {
        goto fail;
    }

    /* Cache the thread pointer */
    gDvmJit.compilerThread = dvmThreadSelf();

    dvmLockMutex(&gDvmJit.compilerLock);

    /* Track method-level compilation statistics */
    gDvmJit.methodStatsTable =  dvmHashTableCreate(32, NULL);

#if defined(WITH_JIT_TUNING)
    gDvm.verboseShutdown = true;
#endif

    dvmUnlockMutex(&gDvmJit.compilerLock);

    /* Set up the JitTable */

    /* Power of 2? */
    assert(gDvmJit.jitTableSize &&
           !(gDvmJit.jitTableSize & (gDvmJit.jitTableSize - 1)));

    dvmInitMutex(&gDvmJit.tableLock);
    dvmLockMutex(&gDvmJit.tableLock);
    pJitTable = (JitEntry*)
                calloc(gDvmJit.jitTableSize, sizeof(*pJitTable));
    if (!pJitTable) {
        ALOGE("jit table allocation failed");
        dvmUnlockMutex(&gDvmJit.tableLock);
        goto fail;
    }
    /*
     * NOTE: the profile table must only be allocated once, globally.
     * Profiling is turned on and off by nulling out gDvm.pJitProfTable
     * and then restoring its original value.  However, this action
     * is not synchronized for speed so threads may continue to hold
     * and update the profile table after profiling has been turned
     * off by null'ng the global pointer.  Be aware.
     */
    pJitProfTable = (unsigned char *)malloc(JIT_PROF_SIZE);
    if (!pJitProfTable) {
        ALOGE("jit prof table allocation failed");
        free(pJitTable);
        dvmUnlockMutex(&gDvmJit.tableLock);
        goto fail;
    }
    memset(pJitProfTable, gDvmJit.threshold, JIT_PROF_SIZE);
    for (i=0; i < gDvmJit.jitTableSize; i++) {
       pJitTable[i].u.info.chain = gDvmJit.jitTableSize;
    }
    /* Is chain field wide enough for termination pattern? */
    assert(pJitTable[0].u.info.chain == gDvmJit.jitTableSize);

    /* Allocate the trace profiling structure */
    pJitTraceProfCounters = (JitTraceProfCounters*)
                             calloc(1, sizeof(*pJitTraceProfCounters));
    if (!pJitTraceProfCounters) {
        ALOGE("jit trace prof counters allocation failed");
        free(pJitTable);
        free(pJitProfTable);
        dvmUnlockMutex(&gDvmJit.tableLock);
        goto fail;
    }

    gDvmJit.pJitEntryTable = pJitTable;
    gDvmJit.jitTableMask = gDvmJit.jitTableSize - 1;
    gDvmJit.jitTableEntriesUsed = 0;
    gDvmJit.compilerHighWater =
        COMPILER_WORK_QUEUE_SIZE - (COMPILER_WORK_QUEUE_SIZE/4);
    /*
     * If the VM is launched with wait-on-the-debugger, we will need to hide
     * the profile table here
     */
    gDvmJit.pProfTable = dvmDebuggerOrProfilerActive() ? NULL : pJitProfTable;
    gDvmJit.pProfTableCopy = pJitProfTable;
    gDvmJit.pJitTraceProfCounters = pJitTraceProfCounters;
    dvmJitUpdateThreadStateAll();
    dvmUnlockMutex(&gDvmJit.tableLock);

    /* Signal running threads to refresh their cached pJitTable pointers */
    dvmSuspendAllThreads(SUSPEND_FOR_REFRESH);
    dvmResumeAllThreads(SUSPEND_FOR_REFRESH);

    /* Enable signature breakpoints by customizing the following code */
#if defined(SIGNATURE_BREAKPOINT)
    /*
     * Suppose one sees the following native crash in the bugreport:
     * I/DEBUG   ( 1638): Build fingerprint: 'unknown'
     * I/DEBUG   ( 1638): pid: 2468, tid: 2507  >>> com.google.android.gallery3d
     * I/DEBUG   ( 1638): signal 11 (SIGSEGV), fault addr 00001400
     * I/DEBUG   ( 1638):  r0 44ea7190  r1 44e4f7b8  r2 44ebc710  r3 00000000
     * I/DEBUG   ( 1638):  r4 00000a00  r5 41862dec  r6 4710dc10  r7 00000280
     * I/DEBUG   ( 1638):  r8 ad010f40  r9 46a37a12  10 001116b0  fp 42a78208
     * I/DEBUG   ( 1638):  ip 00000090  sp 4710dbc8  lr ad060e67  pc 46b90682
     * cpsr 00000030
     * I/DEBUG   ( 1638):  #00  pc 46b90682 /dev/ashmem/dalvik-jit-code-cache
     * I/DEBUG   ( 1638):  #01  pc 00060e62  /system/lib/libdvm.so
     *
     * I/DEBUG   ( 1638): code around pc:
     * I/DEBUG   ( 1638): 46b90660 6888d01c 34091dcc d2174287 4a186b68
     * I/DEBUG   ( 1638): 46b90670 d0052800 68006809 28004790 6b68d00e
     * I/DEBUG   ( 1638): 46b90680 512000bc 37016eaf 6ea866af 6f696028
     * I/DEBUG   ( 1638): 46b90690 682a6069 429a686b e003da08 6df1480b
     * I/DEBUG   ( 1638): 46b906a0 1c2d4788 47806d70 46a378fa 47806d70
     *
     * Clearly it is a JIT bug. To find out which translation contains the
     * offending code, the content of the memory dump around the faulting PC
     * can be pasted into the gDvmJit.signatureBreakpoint[] array and next time
     * when a similar compilation is being created, the JIT compiler replay the
     * trace in the verbose mode and one can investigate the instruction
     * sequence in details.
     *
     * The length of the signature may need additional experiments to determine.
     * The rule of thumb is don't include PC-relative instructions in the
     * signature since it may be affected by the alignment of the compiled code.
     * However, a signature that's too short might increase the chance of false
     * positive matches. Using gdbjithelper to disassembly the memory content
     * first might be a good companion approach.
     *
     * For example, if the next 4 words starting from 46b90680 is pasted into
     * the data structure:
     */

    gDvmJit.signatureBreakpointSize = 4;
    gDvmJit.signatureBreakpoint =
        malloc(sizeof(u4) * gDvmJit.signatureBreakpointSize);
    gDvmJit.signatureBreakpoint[0] = 0x512000bc;
    gDvmJit.signatureBreakpoint[1] = 0x37016eaf;
    gDvmJit.signatureBreakpoint[2] = 0x6ea866af;
    gDvmJit.signatureBreakpoint[3] = 0x6f696028;

    /*
     * The following log will be printed when a match is found in subsequent
     * testings:
     *
     * D/dalvikvm( 2468): Signature match starting from offset 0x34 (4 words)
     * D/dalvikvm( 2468): --------
     * D/dalvikvm( 2468): Compiler: Building trace for computeVisibleItems,
     * offset 0x1f7
     * D/dalvikvm( 2468): 0x46a37a12: 0x0090 add-int v42, v5, v26
     * D/dalvikvm( 2468): 0x46a37a16: 0x004d aput-object v13, v14, v42
     * D/dalvikvm( 2468): 0x46a37a1a: 0x0028 goto, (#0), (#0)
     * D/dalvikvm( 2468): 0x46a3794e: 0x00d8 add-int/lit8 v26, v26, (#1)
     * D/dalvikvm( 2468): 0x46a37952: 0x0028 goto, (#0), (#0)
     * D/dalvikvm( 2468): 0x46a378ee: 0x0002 move/from16 v0, v26, (#0)
     * D/dalvikvm( 2468): 0x46a378f2: 0x0002 move/from16 v1, v29, (#0)
     * D/dalvikvm( 2468): 0x46a378f6: 0x0035 if-ge v0, v1, (#10)
     * D/dalvikvm( 2468): TRACEINFO (554): 0x46a37624
     * Lcom/cooliris/media/GridLayer;computeVisibleItems 0x1f7 14 of 934, 8
     * blocks
     *     :
     *     :
     * D/dalvikvm( 2468): 0x20 (0020): ldr     r0, [r5, #52]
     * D/dalvikvm( 2468): 0x22 (0022): ldr     r2, [pc, #96]
     * D/dalvikvm( 2468): 0x24 (0024): cmp     r0, #0
     * D/dalvikvm( 2468): 0x26 (0026): beq     0x00000034
     * D/dalvikvm( 2468): 0x28 (0028): ldr     r1, [r1, #0]
     * D/dalvikvm( 2468): 0x2a (002a): ldr     r0, [r0, #0]
     * D/dalvikvm( 2468): 0x2c (002c): blx     r2
     * D/dalvikvm( 2468): 0x2e (002e): cmp     r0, #0
     * D/dalvikvm( 2468): 0x30 (0030): beq     0x00000050
     * D/dalvikvm( 2468): 0x32 (0032): ldr     r0, [r5, #52]
     * D/dalvikvm( 2468): 0x34 (0034): lsls    r4, r7, #2
     * D/dalvikvm( 2468): 0x36 (0036): str     r0, [r4, r4]
     * D/dalvikvm( 2468): -------- dalvik offset: 0x01fb @ goto, (#0), (#0)
     * D/dalvikvm( 2468): L0x0195:
     * D/dalvikvm( 2468): -------- dalvik offset: 0x0195 @ add-int/lit8 v26,
     * v26, (#1)
     * D/dalvikvm( 2468): 0x38 (0038): ldr     r7, [r5, #104]
     * D/dalvikvm( 2468): 0x3a (003a): adds    r7, r7, #1
     * D/dalvikvm( 2468): 0x3c (003c): str     r7, [r5, #104]
     * D/dalvikvm( 2468): -------- dalvik offset: 0x0197 @ goto, (#0), (#0)
     * D/dalvikvm( 2468): L0x0165:
     * D/dalvikvm( 2468): -------- dalvik offset: 0x0165 @ move/from16 v0, v26,
     * (#0)
     * D/dalvikvm( 2468): 0x3e (003e): ldr     r0, [r5, #104]
     * D/dalvikvm( 2468): 0x40 (0040): str     r0, [r5, #0]
     *
     * The "str r0, [r4, r4]" is indeed the culprit of the native crash.
     */
#endif

    return true;

fail:
    return false;

}

static void *compilerThreadStart(void *arg)
{
    dvmChangeStatus(NULL, THREAD_VMWAIT);

    /*
     * If we're not running stand-alone, wait a little before
     * recieving translation requests on the assumption that process start
     * up code isn't worth compiling.  We'll resume when the framework
     * signals us that the first screen draw has happened, or the timer
     * below expires (to catch daemons).
     *
     * There is a theoretical race between the callback to
     * VMRuntime.startJitCompiation and when the compiler thread reaches this
     * point. In case the callback happens earlier, in order not to permanently
     * hold the system_server (which is not using the timed wait) in
     * interpreter-only mode we bypass the delay here.
     */
    if (gDvmJit.runningInAndroidFramework &&
        !gDvmJit.alreadyEnabledViaFramework) {
        /*
         * If the current VM instance is the system server (detected by having
         * 0 in gDvm.systemServerPid), we will use the indefinite wait on the
         * conditional variable to determine whether to start the JIT or not.
         * If the system server detects that the whole system is booted in
         * safe mode, the conditional variable will never be signaled and the
         * system server will remain in the interpreter-only mode. All
         * subsequent apps will be started with the --enable-safemode flag
         * explicitly appended.
         */
        if (gDvm.systemServerPid == 0) {
            dvmLockMutex(&gDvmJit.compilerLock);
            pthread_cond_wait(&gDvmJit.compilerQueueActivity,
                              &gDvmJit.compilerLock);
            dvmUnlockMutex(&gDvmJit.compilerLock);
            ALOGD("JIT started for system_server");
        } else {
            dvmLockMutex(&gDvmJit.compilerLock);
            /*
             * TUNING: experiment with the delay & perhaps make it
             * target-specific
             */
            dvmRelativeCondWait(&gDvmJit.compilerQueueActivity,
                                 &gDvmJit.compilerLock, 3000, 0);
            dvmUnlockMutex(&gDvmJit.compilerLock);
        }
        if (gDvmJit.haltCompilerThread) {
             return NULL;
        }
    }

    compilerThreadStartup();

    dvmLockMutex(&gDvmJit.compilerLock);
    /*
     * Since the compiler thread will not touch any objects on the heap once
     * being created, we just fake its state as VMWAIT so that it can be a
     * bit late when there is suspend request pending.
     */
    while (!gDvmJit.haltCompilerThread) {
        if (workQueueLength() == 0) {
            int cc;
            cc = pthread_cond_signal(&gDvmJit.compilerQueueEmpty);
            assert(cc == 0);
            pthread_cond_wait(&gDvmJit.compilerQueueActivity,
                              &gDvmJit.compilerLock);
            continue;
        } else {
            do {
                CompilerWorkOrder work = workDequeue();
                dvmUnlockMutex(&gDvmJit.compilerLock);
#if defined(WITH_JIT_TUNING)
                /*
                 * This is live across setjmp().  Mark it volatile to suppress
                 * a gcc warning.  We should not need this since it is assigned
                 * only once but gcc is not smart enough.
                 */
                volatile u8 startTime = dvmGetRelativeTimeUsec();
#endif
                /*
                 * Check whether there is a suspend request on me.  This
                 * is necessary to allow a clean shutdown.
                 *
                 * However, in the blocking stress testing mode, let the
                 * compiler thread continue doing compilations to unblock
                 * other requesting threads. This may occasionally cause
                 * shutdown from proceeding cleanly in the standalone invocation
                 * of the vm but this should be acceptable.
                 */
                if (!gDvmJit.blockingMode)
                    dvmCheckSuspendPending(dvmThreadSelf());
                /* Is JitTable filling up? */
                if (gDvmJit.jitTableEntriesUsed >
                    (gDvmJit.jitTableSize - gDvmJit.jitTableSize/4)) {
                    bool resizeFail =
                        dvmJitResizeJitTable(gDvmJit.jitTableSize * 2);
                    /*
                     * If the jit table is full, consider it's time to reset
                     * the code cache too.
                     */
                    gDvmJit.codeCacheFull |= resizeFail;
                }
                if (gDvmJit.haltCompilerThread) {
                    ALOGD("Compiler shutdown in progress - discarding request");
                } else if (!gDvmJit.codeCacheFull) {
                    jmp_buf jmpBuf;
                    work.bailPtr = &jmpBuf;
                    bool aborted = setjmp(jmpBuf);
                    if (!aborted) {
                        bool codeCompiled = dvmCompilerDoWork(&work);
                        /*
                         * Make sure we are still operating with the
                         * same translation cache version.  See
                         * Issue 4271784 for details.
                         */
                        dvmLockMutex(&gDvmJit.compilerLock);
                        if ((work.result.cacheVersion ==
                             gDvmJit.cacheVersion) &&
                             codeCompiled &&
                             !work.result.discardResult &&
                             work.result.codeAddress) {
                            dvmJitSetCodeAddr(work.pc, work.result.codeAddress,
                                              work.result.instructionSet,
                                              false, /* not method entry */
                                              work.result.profileCodeSize);
                        }
                        dvmUnlockMutex(&gDvmJit.compilerLock);
                    }
                    dvmCompilerArenaReset();
                }
                free(work.info);
#if defined(WITH_JIT_TUNING)
                gDvmJit.jitTime += dvmGetRelativeTimeUsec() - startTime;
#endif
                dvmLockMutex(&gDvmJit.compilerLock);
            } while (workQueueLength() != 0);
        }
    }
    pthread_cond_signal(&gDvmJit.compilerQueueEmpty);
    dvmUnlockMutex(&gDvmJit.compilerLock);

    /*
     * As part of detaching the thread we need to call into Java code to update
     * the ThreadGroup, and we should not be in VMWAIT state while executing
     * interpreted code.
     */
    dvmChangeStatus(NULL, THREAD_RUNNING);

    if (gDvm.verboseShutdown)
        ALOGD("Compiler thread shutting down");
    return NULL;
}

bool dvmCompilerStartup(void)
{

    dvmInitMutex(&gDvmJit.compilerLock);
    dvmInitMutex(&gDvmJit.compilerICPatchLock);
    dvmInitMutex(&gDvmJit.codeCacheProtectionLock);
    dvmLockMutex(&gDvmJit.compilerLock);
    pthread_cond_init(&gDvmJit.compilerQueueActivity, NULL);
    pthread_cond_init(&gDvmJit.compilerQueueEmpty, NULL);

    /* Reset the work queue */
    gDvmJit.compilerWorkEnqueueIndex = gDvmJit.compilerWorkDequeueIndex = 0;
    gDvmJit.compilerQueueLength = 0;
    dvmUnlockMutex(&gDvmJit.compilerLock);

    /*
     * Defer rest of initialization until we're sure JIT'ng makes sense. Launch
     * the compiler thread, which will do the real initialization if and
     * when it is signalled to do so.
     */
    return dvmCreateInternalThread(&gDvmJit.compilerHandle, "Compiler",
                                   compilerThreadStart, NULL);
}

void dvmCompilerShutdown(void)
{
    void *threadReturn;

    /* Disable new translation requests */
    gDvmJit.pProfTable = NULL;
    gDvmJit.pProfTableCopy = NULL;
    dvmJitUpdateThreadStateAll();

    if (gDvm.verboseShutdown ||
            gDvmJit.profileMode == kTraceProfilingContinuous) {
        dvmCompilerDumpStats();
        while (gDvmJit.compilerQueueLength)
          sleep(5);
    }

    if (gDvmJit.compilerHandle) {

        gDvmJit.haltCompilerThread = true;

        dvmLockMutex(&gDvmJit.compilerLock);
        pthread_cond_signal(&gDvmJit.compilerQueueActivity);
        dvmUnlockMutex(&gDvmJit.compilerLock);

        if (pthread_join(gDvmJit.compilerHandle, &threadReturn) != 0)
            ALOGW("Compiler thread join failed");
        else if (gDvm.verboseShutdown)
            ALOGD("Compiler thread has shut down");
    }

    /* Break loops within the translation cache */
    dvmJitUnchainAll();

    /*
     * NOTE: our current implementatation doesn't allow for the compiler
     * thread to be restarted after it exits here.  We aren't freeing
     * the JitTable or the ProfTable because threads which still may be
     * running or in the process of shutting down may hold references to
     * them.
     */
}

void dvmCompilerUpdateGlobalState()
{
    bool jitActive;
    bool jitActivate;
    bool needUnchain = false;

    /*
     * The tableLock might not be initialized yet by the compiler thread if
     * debugger is attached from the very beginning of the VM launch. If
     * pProfTableCopy is NULL, the lock is not initialized yet and we don't
     * need to refresh anything either.
     */
    if (gDvmJit.pProfTableCopy == NULL) {
        return;
    }

    /*
     * On the first enabling of method tracing, switch the compiler
     * into a mode that includes trace support for invokes and returns.
     * If there are any existing translations, flush them.  NOTE:  we
     * can't blindly flush the translation cache because this code
     * may be executed before the compiler thread has finished
     * initialization.
     */
    if ((gDvm.activeProfilers != 0) &&
        !gDvmJit.methodTraceSupport) {
        bool resetRequired;
        /*
         * compilerLock will prevent new compilations from being
         * installed while we are working.
         */
        dvmLockMutex(&gDvmJit.compilerLock);
        gDvmJit.cacheVersion++; // invalidate compilations in flight
        gDvmJit.methodTraceSupport = true;
        resetRequired = (gDvmJit.numCompilations != 0);
        dvmUnlockMutex(&gDvmJit.compilerLock);
        if (resetRequired) {
            dvmSuspendAllThreads(SUSPEND_FOR_CC_RESET);
            resetCodeCache();
            dvmResumeAllThreads(SUSPEND_FOR_CC_RESET);
        }
    }

    dvmLockMutex(&gDvmJit.tableLock);
    jitActive = gDvmJit.pProfTable != NULL;
    jitActivate = !dvmDebuggerOrProfilerActive();

    if (jitActivate && !jitActive) {
        gDvmJit.pProfTable = gDvmJit.pProfTableCopy;
    } else if (!jitActivate && jitActive) {
        gDvmJit.pProfTable = NULL;
        needUnchain = true;
    }
    dvmUnlockMutex(&gDvmJit.tableLock);
    if (needUnchain)
        dvmJitUnchainAll();
    // Make sure all threads have current values
    dvmJitUpdateThreadStateAll();
}
