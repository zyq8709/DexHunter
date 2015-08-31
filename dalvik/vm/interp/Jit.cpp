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
#ifdef WITH_JIT

/*
 * Target independent portion of Android's Jit
 */

#include "Dalvik.h"
#include "Jit.h"

#include "libdex/DexOpcodes.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include "compiler/Compiler.h"
#include "compiler/CompilerUtility.h"
#include "compiler/CompilerIR.h"
#include <errno.h>

#if defined(WITH_SELF_VERIFICATION)
/* Allocate space for per-thread ShadowSpace data structures */
void* dvmSelfVerificationShadowSpaceAlloc(Thread* self)
{
    self->shadowSpace = (ShadowSpace*) calloc(1, sizeof(ShadowSpace));
    if (self->shadowSpace == NULL)
        return NULL;

    self->shadowSpace->registerSpaceSize = REG_SPACE;
    self->shadowSpace->registerSpace =
        (int*) calloc(self->shadowSpace->registerSpaceSize, sizeof(int));

    return self->shadowSpace->registerSpace;
}

/* Free per-thread ShadowSpace data structures */
void dvmSelfVerificationShadowSpaceFree(Thread* self)
{
    free(self->shadowSpace->registerSpace);
    free(self->shadowSpace);
}

/*
 * Save out PC, FP, thread state, and registers to shadow space.
 * Return a pointer to the shadow space for JIT to use.
 *
 * The set of saved state from the Thread structure is:
 *     pc  (Dalvik PC)
 *     fp  (Dalvik FP)
 *     retval
 *     method
 *     methodClassDex
 *     interpStackEnd
 */
void* dvmSelfVerificationSaveState(const u2* pc, u4* fp,
                                   Thread* self, int targetTrace)
{
    ShadowSpace *shadowSpace = self->shadowSpace;
    unsigned preBytes = self->interpSave.method->outsSize*4 +
        sizeof(StackSaveArea);
    unsigned postBytes = self->interpSave.method->registersSize*4;

    //ALOGD("### selfVerificationSaveState(%d) pc: %#x fp: %#x",
    //    self->threadId, (int)pc, (int)fp);

    if (shadowSpace->selfVerificationState != kSVSIdle) {
        ALOGD("~~~ Save: INCORRECT PREVIOUS STATE(%d): %d",
            self->threadId, shadowSpace->selfVerificationState);
        ALOGD("********** SHADOW STATE DUMP **********");
        ALOGD("PC: %#x FP: %#x", (int)pc, (int)fp);
    }
    shadowSpace->selfVerificationState = kSVSStart;

    // Dynamically grow shadow register space if necessary
    if (preBytes + postBytes > shadowSpace->registerSpaceSize * sizeof(u4)) {
        free(shadowSpace->registerSpace);
        shadowSpace->registerSpaceSize = (preBytes + postBytes) / sizeof(u4);
        shadowSpace->registerSpace =
            (int*) calloc(shadowSpace->registerSpaceSize, sizeof(u4));
    }

    // Remember original state
    shadowSpace->startPC = pc;
    shadowSpace->fp = fp;
    shadowSpace->retval = self->interpSave.retval;
    shadowSpace->interpStackEnd = self->interpStackEnd;

    /*
     * Store the original method here in case the trace ends with a
     * return/invoke, the last method.
     */
    shadowSpace->method = self->interpSave.method;
    shadowSpace->methodClassDex = self->interpSave.methodClassDex;

    shadowSpace->shadowFP = shadowSpace->registerSpace +
                            shadowSpace->registerSpaceSize - postBytes/4;

    self->interpSave.curFrame = (u4*)shadowSpace->shadowFP;
    self->interpStackEnd = (u1*)shadowSpace->registerSpace;

    // Create a copy of the stack
    memcpy(((char*)shadowSpace->shadowFP)-preBytes, ((char*)fp)-preBytes,
        preBytes+postBytes);

    // Setup the shadowed heap space
    shadowSpace->heapSpaceTail = shadowSpace->heapSpace;

    // Reset trace length
    shadowSpace->traceLength = 0;

    return shadowSpace;
}

/*
 * Save ending PC, FP and compiled code exit point to shadow space.
 * Return a pointer to the shadow space for JIT to restore state.
 */
void* dvmSelfVerificationRestoreState(const u2* pc, u4* fp,
                                      SelfVerificationState exitState,
                                      Thread* self)
{
    ShadowSpace *shadowSpace = self->shadowSpace;
    shadowSpace->endPC = pc;
    shadowSpace->endShadowFP = fp;
    shadowSpace->jitExitState = exitState;

    //ALOGD("### selfVerificationRestoreState(%d) pc: %#x fp: %#x endPC: %#x",
    //    self->threadId, (int)shadowSpace->startPC, (int)shadowSpace->fp,
    //    (int)pc);

    if (shadowSpace->selfVerificationState != kSVSStart) {
        ALOGD("~~~ Restore: INCORRECT PREVIOUS STATE(%d): %d",
            self->threadId, shadowSpace->selfVerificationState);
        ALOGD("********** SHADOW STATE DUMP **********");
        ALOGD("Dalvik PC: %#x endPC: %#x", (int)shadowSpace->startPC,
            (int)shadowSpace->endPC);
        ALOGD("Interp FP: %#x", (int)shadowSpace->fp);
        ALOGD("Shadow FP: %#x endFP: %#x", (int)shadowSpace->shadowFP,
            (int)shadowSpace->endShadowFP);
    }

    // Special case when punting after a single instruction
    if (exitState == kSVSPunt && pc == shadowSpace->startPC) {
        shadowSpace->selfVerificationState = kSVSIdle;
    } else {
        shadowSpace->selfVerificationState = exitState;
    }

    /* Restore state before returning */
    self->interpSave.pc = shadowSpace->startPC;
    self->interpSave.curFrame = shadowSpace->fp;
    self->interpSave.method = shadowSpace->method;
    self->interpSave.methodClassDex = shadowSpace->methodClassDex;
    self->interpSave.retval = shadowSpace->retval;
    self->interpStackEnd = shadowSpace->interpStackEnd;

    return shadowSpace;
}

/* Print contents of virtual registers */
static void selfVerificationPrintRegisters(int* addr, int* addrRef,
                                           int numWords)
{
    int i;
    for (i = 0; i < numWords; i++) {
        ALOGD("(v%d) 0x%8x%s", i, addr[i], addr[i] != addrRef[i] ? " X" : "");
    }
}

/* Print values maintained in shadowSpace */
static void selfVerificationDumpState(const u2* pc, Thread* self)
{
    ShadowSpace* shadowSpace = self->shadowSpace;
    StackSaveArea* stackSave = SAVEAREA_FROM_FP(self->interpSave.curFrame);
    int frameBytes = (int) shadowSpace->registerSpace +
                     shadowSpace->registerSpaceSize*4 -
                     (int) shadowSpace->shadowFP;
    int localRegs = 0;
    int frameBytes2 = 0;
    if ((uintptr_t)self->interpSave.curFrame < (uintptr_t)shadowSpace->fp) {
        localRegs = (stackSave->method->registersSize -
                     stackSave->method->insSize)*4;
        frameBytes2 = (int) shadowSpace->fp -
                      (int)self->interpSave.curFrame - localRegs;
    }
    ALOGD("********** SHADOW STATE DUMP **********");
    ALOGD("CurrentPC: %#x, Offset: 0x%04x", (int)pc,
        (int)(pc - stackSave->method->insns));
    ALOGD("Class: %s", shadowSpace->method->clazz->descriptor);
    ALOGD("Method: %s", shadowSpace->method->name);
    ALOGD("Dalvik PC: %#x endPC: %#x", (int)shadowSpace->startPC,
        (int)shadowSpace->endPC);
    ALOGD("Interp FP: %#x endFP: %#x", (int)shadowSpace->fp,
        (int)self->interpSave.curFrame);
    ALOGD("Shadow FP: %#x endFP: %#x", (int)shadowSpace->shadowFP,
        (int)shadowSpace->endShadowFP);
    ALOGD("Frame1 Bytes: %d Frame2 Local: %d Bytes: %d", frameBytes,
        localRegs, frameBytes2);
    ALOGD("Trace length: %d State: %d", shadowSpace->traceLength,
        shadowSpace->selfVerificationState);
}

/* Print decoded instructions in the current trace */
static void selfVerificationDumpTrace(const u2* pc, Thread* self)
{
    ShadowSpace* shadowSpace = self->shadowSpace;
    StackSaveArea* stackSave = SAVEAREA_FROM_FP(self->interpSave.curFrame);
    int i, addr, offset;
    DecodedInstruction *decInsn;

    ALOGD("********** SHADOW TRACE DUMP **********");
    for (i = 0; i < shadowSpace->traceLength; i++) {
        addr = shadowSpace->trace[i].addr;
        offset =  (int)((u2*)addr - stackSave->method->insns);
        decInsn = &(shadowSpace->trace[i].decInsn);
        /* Not properly decoding instruction, some registers may be garbage */
        ALOGD("%#x: (0x%04x) %s",
            addr, offset, dexGetOpcodeName(decInsn->opcode));
    }
}

/* Code is forced into this spin loop when a divergence is detected */
static void selfVerificationSpinLoop(ShadowSpace *shadowSpace)
{
    const u2 *startPC = shadowSpace->startPC;
    JitTraceDescription* desc = dvmCopyTraceDescriptor(startPC, NULL);
    if (desc) {
        dvmCompilerWorkEnqueue(startPC, kWorkOrderTraceDebug, desc);
        /*
         * This function effectively terminates the VM right here, so not
         * freeing the desc pointer when the enqueuing fails is acceptable.
         */
    }
    gDvmJit.selfVerificationSpin = true;
    while(gDvmJit.selfVerificationSpin) sleep(10);
}

/*
 * If here, we're re-interpreting an instruction that was included
 * in a trace that was just executed.  This routine is called for
 * each instruction in the original trace, and compares state
 * when it reaches the end point.
 *
 * TUNING: the interpretation mechanism now supports a counted
 * single-step mechanism.  If we were to associate an instruction
 * count with each trace exit, we could just single-step the right
 * number of cycles and then compare.  This would improve detection
 * of control divergences, as well as (slightly) simplify this code.
 */
void dvmCheckSelfVerification(const u2* pc, Thread* self)
{
    ShadowSpace *shadowSpace = self->shadowSpace;
    SelfVerificationState state = shadowSpace->selfVerificationState;

    DecodedInstruction decInsn;
    dexDecodeInstruction(pc, &decInsn);

    //ALOGD("### DbgIntp(%d): PC: %#x endPC: %#x state: %d len: %d %s",
    //    self->threadId, (int)pc, (int)shadowSpace->endPC, state,
    //    shadowSpace->traceLength, dexGetOpcodeName(decInsn.opcode));

    if (state == kSVSIdle || state == kSVSStart) {
        ALOGD("~~~ DbgIntrp: INCORRECT PREVIOUS STATE(%d): %d",
            self->threadId, state);
        selfVerificationDumpState(pc, self);
        selfVerificationDumpTrace(pc, self);
    }

    /*
     * Generalize the self verification state to kSVSDebugInterp unless the
     * entry reason is kSVSBackwardBranch or kSVSSingleStep.
     */
    if (state != kSVSBackwardBranch && state != kSVSSingleStep) {
        shadowSpace->selfVerificationState = kSVSDebugInterp;
    }

    /*
     * Check if the current pc matches the endPC. Only check for non-zero
     * trace length when backward branches are involved.
     */
    if (pc == shadowSpace->endPC &&
        (state == kSVSDebugInterp || state == kSVSSingleStep ||
         (state == kSVSBackwardBranch && shadowSpace->traceLength != 0))) {

        shadowSpace->selfVerificationState = kSVSIdle;

        /* Check register space */
        int frameBytes = (int) shadowSpace->registerSpace +
                         shadowSpace->registerSpaceSize*4 -
                         (int) shadowSpace->shadowFP;
        if (memcmp(shadowSpace->fp, shadowSpace->shadowFP, frameBytes)) {
            if (state == kSVSBackwardBranch) {
                /* State mismatch on backward branch - try one more iteration */
                shadowSpace->selfVerificationState = kSVSDebugInterp;
                goto log_and_continue;
            }
            ALOGD("~~~ DbgIntp(%d): REGISTERS DIVERGENCE!", self->threadId);
            selfVerificationDumpState(pc, self);
            selfVerificationDumpTrace(pc, self);
            ALOGD("*** Interp Registers: addr: %#x bytes: %d",
                (int)shadowSpace->fp, frameBytes);
            selfVerificationPrintRegisters((int*)shadowSpace->fp,
                                           (int*)shadowSpace->shadowFP,
                                           frameBytes/4);
            ALOGD("*** Shadow Registers: addr: %#x bytes: %d",
                (int)shadowSpace->shadowFP, frameBytes);
            selfVerificationPrintRegisters((int*)shadowSpace->shadowFP,
                                           (int*)shadowSpace->fp,
                                           frameBytes/4);
            selfVerificationSpinLoop(shadowSpace);
        }
        /* Check new frame if it exists (invokes only) */
        if ((uintptr_t)self->interpSave.curFrame < (uintptr_t)shadowSpace->fp) {
            StackSaveArea* stackSave =
                SAVEAREA_FROM_FP(self->interpSave.curFrame);
            int localRegs = (stackSave->method->registersSize -
                             stackSave->method->insSize)*4;
            int frameBytes2 = (int) shadowSpace->fp -
                              (int) self->interpSave.curFrame - localRegs;
            if (memcmp(((char*)self->interpSave.curFrame)+localRegs,
                ((char*)shadowSpace->endShadowFP)+localRegs, frameBytes2)) {
                if (state == kSVSBackwardBranch) {
                    /*
                     * State mismatch on backward branch - try one more
                     * iteration.
                     */
                    shadowSpace->selfVerificationState = kSVSDebugInterp;
                    goto log_and_continue;
                }
                ALOGD("~~~ DbgIntp(%d): REGISTERS (FRAME2) DIVERGENCE!",
                    self->threadId);
                selfVerificationDumpState(pc, self);
                selfVerificationDumpTrace(pc, self);
                ALOGD("*** Interp Registers: addr: %#x l: %d bytes: %d",
                    (int)self->interpSave.curFrame, localRegs, frameBytes2);
                selfVerificationPrintRegisters((int*)self->interpSave.curFrame,
                                               (int*)shadowSpace->endShadowFP,
                                               (frameBytes2+localRegs)/4);
                ALOGD("*** Shadow Registers: addr: %#x l: %d bytes: %d",
                    (int)shadowSpace->endShadowFP, localRegs, frameBytes2);
                selfVerificationPrintRegisters((int*)shadowSpace->endShadowFP,
                                               (int*)self->interpSave.curFrame,
                                               (frameBytes2+localRegs)/4);
                selfVerificationSpinLoop(shadowSpace);
            }
        }

        /* Check memory space */
        bool memDiff = false;
        ShadowHeap* heapSpacePtr;
        for (heapSpacePtr = shadowSpace->heapSpace;
             heapSpacePtr != shadowSpace->heapSpaceTail; heapSpacePtr++) {
            int memData = *((unsigned int*) heapSpacePtr->addr);
            if (heapSpacePtr->data != memData) {
                if (state == kSVSBackwardBranch) {
                    /*
                     * State mismatch on backward branch - try one more
                     * iteration.
                     */
                    shadowSpace->selfVerificationState = kSVSDebugInterp;
                    goto log_and_continue;
                }
                ALOGD("~~~ DbgIntp(%d): MEMORY DIVERGENCE!", self->threadId);
                ALOGD("Addr: %#x Intrp Data: %#x Jit Data: %#x",
                    heapSpacePtr->addr, memData, heapSpacePtr->data);
                selfVerificationDumpState(pc, self);
                selfVerificationDumpTrace(pc, self);
                memDiff = true;
            }
        }
        if (memDiff) selfVerificationSpinLoop(shadowSpace);


        /*
         * Success.  If this shadowed trace included a single-stepped
         * instruction, we need to stay in the interpreter for one
         * more interpretation before resuming.
         */
        if (state == kSVSSingleStep) {
            assert(self->jitResumeNPC != NULL);
            assert(self->singleStepCount == 0);
            self->singleStepCount = 1;
            dvmEnableSubMode(self, kSubModeCountedStep);
        }

        /*
         * Switch off shadow replay mode.  The next shadowed trace
         * execution will turn it back on.
         */
        dvmDisableSubMode(self, kSubModeJitSV);

        self->jitState = kJitDone;
        return;
    }
log_and_continue:
    /* If end not been reached, make sure max length not exceeded */
    if (shadowSpace->traceLength >= JIT_MAX_TRACE_LEN) {
        ALOGD("~~~ DbgIntp(%d): CONTROL DIVERGENCE!", self->threadId);
        ALOGD("startPC: %#x endPC: %#x currPC: %#x",
            (int)shadowSpace->startPC, (int)shadowSpace->endPC, (int)pc);
        selfVerificationDumpState(pc, self);
        selfVerificationDumpTrace(pc, self);
        selfVerificationSpinLoop(shadowSpace);
        return;
    }
    /* Log the instruction address and decoded instruction for debug */
    shadowSpace->trace[shadowSpace->traceLength].addr = (int)pc;
    shadowSpace->trace[shadowSpace->traceLength].decInsn = decInsn;
    shadowSpace->traceLength++;
}
#endif

/*
 * If one of our fixed tables or the translation buffer fills up,
 * call this routine to avoid wasting cycles on future translation requests.
 */
void dvmJitStopTranslationRequests()
{
    /*
     * Note 1: This won't necessarily stop all translation requests, and
     * operates on a delayed mechanism.  Running threads look to the copy
     * of this value in their private thread structures and won't see
     * this change until it is refreshed (which happens on interpreter
     * entry).
     * Note 2: This is a one-shot memory leak on this table. Because this is a
     * permanent off switch for Jit profiling, it is a one-time leak of 1K
     * bytes, and no further attempt will be made to re-allocate it.  Can't
     * free it because some thread may be holding a reference.
     */
    gDvmJit.pProfTable = NULL;
    dvmJitUpdateThreadStateAll();
}

#if defined(WITH_JIT_TUNING)
/* Convenience function to increment counter from assembly code */
void dvmBumpNoChain(int from)
{
    gDvmJit.noChainExit[from]++;
}

/* Convenience function to increment counter from assembly code */
void dvmBumpNormal()
{
    gDvmJit.normalExit++;
}

/* Convenience function to increment counter from assembly code */
void dvmBumpPunt(int from)
{
    gDvmJit.puntExit++;
}
#endif

/* Dumps debugging & tuning stats to the log */
void dvmJitStats()
{
    int i;
    int hit;
    int not_hit;
    int chains;
    int stubs;
    if (gDvmJit.pJitEntryTable) {
        for (i=0, stubs=chains=hit=not_hit=0;
             i < (int) gDvmJit.jitTableSize;
             i++) {
            if (gDvmJit.pJitEntryTable[i].dPC != 0) {
                hit++;
                if (gDvmJit.pJitEntryTable[i].codeAddress ==
                      dvmCompilerGetInterpretTemplate())
                    stubs++;
            } else
                not_hit++;
            if (gDvmJit.pJitEntryTable[i].u.info.chain != gDvmJit.jitTableSize)
                chains++;
        }
        ALOGD("JIT: table size is %d, entries used is %d",
             gDvmJit.jitTableSize,  gDvmJit.jitTableEntriesUsed);
        ALOGD("JIT: %d traces, %d slots, %d chains, %d thresh, %s",
             hit, not_hit + hit, chains, gDvmJit.threshold,
             gDvmJit.blockingMode ? "Blocking" : "Non-blocking");

#if defined(WITH_JIT_TUNING)
        ALOGD("JIT: Code cache patches: %d", gDvmJit.codeCachePatches);

        ALOGD("JIT: Lookups: %d hits, %d misses; %d normal, %d punt",
             gDvmJit.addrLookupsFound, gDvmJit.addrLookupsNotFound,
             gDvmJit.normalExit, gDvmJit.puntExit);

        ALOGD("JIT: ICHits: %d", gDvmICHitCount);

        ALOGD("JIT: noChainExit: %d IC miss, %d interp callsite, "
             "%d switch overflow",
             gDvmJit.noChainExit[kInlineCacheMiss],
             gDvmJit.noChainExit[kCallsiteInterpreted],
             gDvmJit.noChainExit[kSwitchOverflow]);

        ALOGD("JIT: ICPatch: %d init, %d rejected, %d lock-free, %d queued, "
             "%d dropped",
             gDvmJit.icPatchInit, gDvmJit.icPatchRejected,
             gDvmJit.icPatchLockFree, gDvmJit.icPatchQueued,
             gDvmJit.icPatchDropped);

        ALOGD("JIT: Invoke: %d mono, %d poly, %d native, %d return",
             gDvmJit.invokeMonomorphic, gDvmJit.invokePolymorphic,
             gDvmJit.invokeNative, gDvmJit.returnOp);
        ALOGD("JIT: Inline: %d mgetter, %d msetter, %d pgetter, %d psetter",
             gDvmJit.invokeMonoGetterInlined, gDvmJit.invokeMonoSetterInlined,
             gDvmJit.invokePolyGetterInlined, gDvmJit.invokePolySetterInlined);
        ALOGD("JIT: Total compilation time: %llu ms", gDvmJit.jitTime / 1000);
        ALOGD("JIT: Avg unit compilation time: %llu us",
             gDvmJit.numCompilations == 0 ? 0 :
             gDvmJit.jitTime / gDvmJit.numCompilations);
        ALOGD("JIT: Potential GC blocked by compiler: max %llu us / "
             "avg %llu us (%d)",
             gDvmJit.maxCompilerThreadBlockGCTime,
             gDvmJit.numCompilerThreadBlockGC == 0 ?
                 0 : gDvmJit.compilerThreadBlockGCTime /
                     gDvmJit.numCompilerThreadBlockGC,
             gDvmJit.numCompilerThreadBlockGC);
#endif

        ALOGD("JIT: %d Translation chains, %d interp stubs",
             gDvmJit.translationChains, stubs);
        if (gDvmJit.profileMode == kTraceProfilingContinuous) {
            dvmCompilerSortAndPrintTraceProfiles();
        }
    }
}


/* End current trace now & don't include current instruction */
void dvmJitEndTraceSelect(Thread* self, const u2* dPC)
{
    if (self->jitState == kJitTSelect) {
        self->jitState = kJitTSelectEnd;
    }
    if (self->jitState == kJitTSelectEnd) {
        // Clean up and finish now.
        dvmCheckJit(dPC, self);
    }
}

/*
 * Find an entry in the JitTable, creating if necessary.
 * Returns null if table is full.
 */
static JitEntry *lookupAndAdd(const u2* dPC, bool callerLocked,
                              bool isMethodEntry)
{
    u4 chainEndMarker = gDvmJit.jitTableSize;
    u4 idx = dvmJitHash(dPC);

    /*
     * Walk the bucket chain to find an exact match for our PC and trace/method
     * type
     */
    while ((gDvmJit.pJitEntryTable[idx].u.info.chain != chainEndMarker) &&
           ((gDvmJit.pJitEntryTable[idx].dPC != dPC) ||
            (gDvmJit.pJitEntryTable[idx].u.info.isMethodEntry !=
             isMethodEntry))) {
        idx = gDvmJit.pJitEntryTable[idx].u.info.chain;
    }

    if (gDvmJit.pJitEntryTable[idx].dPC != dPC ||
        gDvmJit.pJitEntryTable[idx].u.info.isMethodEntry != isMethodEntry) {
        /*
         * No match.  Aquire jitTableLock and find the last
         * slot in the chain. Possibly continue the chain walk in case
         * some other thread allocated the slot we were looking
         * at previuosly (perhaps even the dPC we're trying to enter).
         */
        if (!callerLocked)
            dvmLockMutex(&gDvmJit.tableLock);
        /*
         * At this point, if .dPC is NULL, then the slot we're
         * looking at is the target slot from the primary hash
         * (the simple, and common case).  Otherwise we're going
         * to have to find a free slot and chain it.
         */
        ANDROID_MEMBAR_FULL(); /* Make sure we reload [].dPC after lock */
        if (gDvmJit.pJitEntryTable[idx].dPC != NULL) {
            u4 prev;
            while (gDvmJit.pJitEntryTable[idx].u.info.chain != chainEndMarker) {
                if (gDvmJit.pJitEntryTable[idx].dPC == dPC &&
                    gDvmJit.pJitEntryTable[idx].u.info.isMethodEntry ==
                        isMethodEntry) {
                    /* Another thread got there first for this dPC */
                    if (!callerLocked)
                        dvmUnlockMutex(&gDvmJit.tableLock);
                    return &gDvmJit.pJitEntryTable[idx];
                }
                idx = gDvmJit.pJitEntryTable[idx].u.info.chain;
            }
            /* Here, idx should be pointing to the last cell of an
             * active chain whose last member contains a valid dPC */
            assert(gDvmJit.pJitEntryTable[idx].dPC != NULL);
            /* Linear walk to find a free cell and add it to the end */
            prev = idx;
            while (true) {
                idx++;
                if (idx == chainEndMarker)
                    idx = 0;  /* Wraparound */
                if ((gDvmJit.pJitEntryTable[idx].dPC == NULL) ||
                    (idx == prev))
                    break;
            }
            if (idx != prev) {
                JitEntryInfoUnion oldValue;
                JitEntryInfoUnion newValue;
                /*
                 * Although we hold the lock so that noone else will
                 * be trying to update a chain field, the other fields
                 * packed into the word may be in use by other threads.
                 */
                do {
                    oldValue = gDvmJit.pJitEntryTable[prev].u;
                    newValue = oldValue;
                    newValue.info.chain = idx;
                } while (android_atomic_release_cas(oldValue.infoWord,
                        newValue.infoWord,
                        &gDvmJit.pJitEntryTable[prev].u.infoWord) != 0);
            }
        }
        if (gDvmJit.pJitEntryTable[idx].dPC == NULL) {
            gDvmJit.pJitEntryTable[idx].u.info.isMethodEntry = isMethodEntry;
            /*
             * Initialize codeAddress and allocate the slot.  Must
             * happen in this order (since dPC is set, the entry is live.
             */
            android_atomic_release_store((int32_t)dPC,
                 (volatile int32_t *)(void *)&gDvmJit.pJitEntryTable[idx].dPC);
            /* for simulator mode, we need to initialized codeAddress to null */
            gDvmJit.pJitEntryTable[idx].codeAddress = NULL;
            gDvmJit.pJitEntryTable[idx].dPC = dPC;
            gDvmJit.jitTableEntriesUsed++;
        } else {
            /* Table is full */
            idx = chainEndMarker;
        }
        if (!callerLocked)
            dvmUnlockMutex(&gDvmJit.tableLock);
    }
    return (idx == chainEndMarker) ? NULL : &gDvmJit.pJitEntryTable[idx];
}

/* Dump a trace description */
void dvmJitDumpTraceDesc(JitTraceDescription *trace)
{
    int i;
    bool done = false;
    const u2* dpc;
    const u2* dpcBase;
    int curFrag = 0;
    ALOGD("===========================================");
    ALOGD("Trace dump %#x, Method %s off %#x",(int)trace,
         trace->method->name,trace->trace[curFrag].info.frag.startOffset);
    dpcBase = trace->method->insns;
    while (!done) {
        DecodedInstruction decInsn;
        if (trace->trace[curFrag].isCode) {
            ALOGD("Frag[%d]- Insts: %d, start: %#x, hint: %#x, end: %d",
                 curFrag, trace->trace[curFrag].info.frag.numInsts,
                 trace->trace[curFrag].info.frag.startOffset,
                 trace->trace[curFrag].info.frag.hint,
                 trace->trace[curFrag].info.frag.runEnd);
            dpc = dpcBase + trace->trace[curFrag].info.frag.startOffset;
            for (i=0; i<trace->trace[curFrag].info.frag.numInsts; i++) {
                dexDecodeInstruction(dpc, &decInsn);
                ALOGD("    0x%04x - %s %#x",(dpc-dpcBase),
                     dexGetOpcodeName(decInsn.opcode),(int)dpc);
                dpc += dexGetWidthFromOpcode(decInsn.opcode);
            }
            if (trace->trace[curFrag].info.frag.runEnd) {
                done = true;
            }
        } else {
            ALOGD("Frag[%d]- META info: 0x%08x", curFrag,
                 (int)trace->trace[curFrag].info.meta);
        }
        curFrag++;
    }
    ALOGD("-------------------------------------------");
}

/*
 * Append the class ptr of "this" and the current method ptr to the current
 * trace. That is, the trace runs will contain the following components:
 *  + trace run that ends with an invoke (existing entry)
 *  + thisClass (new)
 *  + calleeMethod (new)
 */
static void insertClassMethodInfo(Thread* self,
                                  const ClassObject* thisClass,
                                  const Method* calleeMethod,
                                  const DecodedInstruction* insn)
{
    int currTraceRun = ++self->currTraceRun;
    self->trace[currTraceRun].info.meta = thisClass ?
                                    (void *) thisClass->descriptor : NULL;
    self->trace[currTraceRun].isCode = false;

    currTraceRun = ++self->currTraceRun;
    self->trace[currTraceRun].info.meta = thisClass ?
                                    (void *) thisClass->classLoader : NULL;
    self->trace[currTraceRun].isCode = false;

    currTraceRun = ++self->currTraceRun;
    self->trace[currTraceRun].info.meta = (void *) calleeMethod;
    self->trace[currTraceRun].isCode = false;
}

/*
 * Check if the next instruction following the invoke is a move-result and if
 * so add it to the trace. That is, this will add the trace run that includes
 * the move-result to the trace list.
 *
 *  + trace run that ends with an invoke (existing entry)
 *  + thisClass (existing entry)
 *  + calleeMethod (existing entry)
 *  + move result (new)
 *
 * lastPC, len, offset are all from the preceding invoke instruction
 */
static void insertMoveResult(const u2 *lastPC, int len, int offset,
                             Thread *self)
{
    DecodedInstruction nextDecInsn;
    const u2 *moveResultPC = lastPC + len;

    dexDecodeInstruction(moveResultPC, &nextDecInsn);
    if ((nextDecInsn.opcode != OP_MOVE_RESULT) &&
        (nextDecInsn.opcode != OP_MOVE_RESULT_WIDE) &&
        (nextDecInsn.opcode != OP_MOVE_RESULT_OBJECT))
        return;

    /* We need to start a new trace run */
    int currTraceRun = ++self->currTraceRun;
    self->currRunHead = moveResultPC;
    self->trace[currTraceRun].info.frag.startOffset = offset + len;
    self->trace[currTraceRun].info.frag.numInsts = 1;
    self->trace[currTraceRun].info.frag.runEnd = false;
    self->trace[currTraceRun].info.frag.hint = kJitHintNone;
    self->trace[currTraceRun].isCode = true;
    self->totalTraceLen++;

    self->currRunLen = dexGetWidthFromInstruction(moveResultPC);
}

/*
 * Adds to the current trace request one instruction at a time, just
 * before that instruction is interpreted.  This is the primary trace
 * selection function.  NOTE: return instruction are handled a little
 * differently.  In general, instructions are "proposed" to be added
 * to the current trace prior to interpretation.  If the interpreter
 * then successfully completes the instruction, is will be considered
 * part of the request.  This allows us to examine machine state prior
 * to interpretation, and also abort the trace request if the instruction
 * throws or does something unexpected.  However, return instructions
 * will cause an immediate end to the translation request - which will
 * be passed to the compiler before the return completes.  This is done
 * in response to special handling of returns by the interpreter (and
 * because returns cannot throw in a way that causes problems for the
 * translated code.
 */
void dvmCheckJit(const u2* pc, Thread* self)
{
    const ClassObject *thisClass = self->callsiteClass;
    const Method* curMethod = self->methodToCall;
    int flags, len;
    int allDone = false;
    /* Stay in break/single-stop mode for the next instruction */
    bool stayOneMoreInst = false;

    /* Prepare to handle last PC and stage the current PC & method*/
    const u2 *lastPC = self->lastPC;

    self->lastPC = pc;

    switch (self->jitState) {
        int offset;
        DecodedInstruction decInsn;
        case kJitTSelect:
            /* First instruction - just remember the PC and exit */
            if (lastPC == NULL) break;
            /* Grow the trace around the last PC if jitState is kJitTSelect */
            dexDecodeInstruction(lastPC, &decInsn);
#if TRACE_OPCODE_FILTER
            /* Only add JIT support opcode to trace. End the trace if
             * this opcode is not supported.
             */
            if (!dvmIsOpcodeSupportedByJit(decInsn.opcode)) {
                self->jitState = kJitTSelectEnd;
                break;
            }
#endif
            /*
             * Treat {PACKED,SPARSE}_SWITCH as trace-ending instructions due
             * to the amount of space it takes to generate the chaining
             * cells.
             */
            if (self->totalTraceLen != 0 &&
                (decInsn.opcode == OP_PACKED_SWITCH ||
                 decInsn.opcode == OP_SPARSE_SWITCH)) {
                self->jitState = kJitTSelectEnd;
                break;
            }

#if defined(SHOW_TRACE)
            ALOGD("TraceGen: adding %s. lpc:%#x, pc:%#x",
                 dexGetOpcodeName(decInsn.opcode), (int)lastPC, (int)pc);
#endif
            flags = dexGetFlagsFromOpcode(decInsn.opcode);
            len = dexGetWidthFromInstruction(lastPC);
            offset = lastPC - self->traceMethod->insns;
            assert((unsigned) offset <
                   dvmGetMethodInsnsSize(self->traceMethod));
            if (lastPC != self->currRunHead + self->currRunLen) {
                int currTraceRun;
                /* We need to start a new trace run */
                currTraceRun = ++self->currTraceRun;
                self->currRunLen = 0;
                self->currRunHead = (u2*)lastPC;
                self->trace[currTraceRun].info.frag.startOffset = offset;
                self->trace[currTraceRun].info.frag.numInsts = 0;
                self->trace[currTraceRun].info.frag.runEnd = false;
                self->trace[currTraceRun].info.frag.hint = kJitHintNone;
                self->trace[currTraceRun].isCode = true;
            }
            self->trace[self->currTraceRun].info.frag.numInsts++;
            self->totalTraceLen++;
            self->currRunLen += len;

            /*
             * If the last instruction is an invoke, we will try to sneak in
             * the move-result* (if existent) into a separate trace run.
             */
            {
              int needReservedRun = (flags & kInstrInvoke) ? 1 : 0;

              /* Will probably never hit this with the current trace builder */
              if (self->currTraceRun ==
                   (MAX_JIT_RUN_LEN - 1 - needReservedRun)) {
                self->jitState = kJitTSelectEnd;
              }
            }

            if (!dexIsGoto(flags) &&
                  ((flags & (kInstrCanBranch |
                             kInstrCanSwitch |
                             kInstrCanReturn |
                             kInstrInvoke)) != 0)) {
                    self->jitState = kJitTSelectEnd;
#if defined(SHOW_TRACE)
                ALOGD("TraceGen: ending on %s, basic block end",
                     dexGetOpcodeName(decInsn.opcode));
#endif

                /*
                 * If the current invoke is a {virtual,interface}, get the
                 * current class/method pair into the trace as well.
                 * If the next instruction is a variant of move-result, insert
                 * it to the trace too.
                 */
                if (flags & kInstrInvoke) {
                    insertClassMethodInfo(self, thisClass, curMethod,
                                          &decInsn);
                    insertMoveResult(lastPC, len, offset, self);
                }
            }
            /* Break on throw or self-loop */
            if ((decInsn.opcode == OP_THROW) || (lastPC == pc)){
                self->jitState = kJitTSelectEnd;
            }
            if (self->totalTraceLen >= JIT_MAX_TRACE_LEN) {
                self->jitState = kJitTSelectEnd;
            }
            if ((flags & kInstrCanReturn) != kInstrCanReturn) {
                break;
            }
            else {
                /*
                 * Last instruction is a return - stay in the dbg interpreter
                 * for one more instruction if it is a non-void return, since
                 * we don't want to start a trace with move-result as the first
                 * instruction (which is already included in the trace
                 * containing the invoke.
                 */
                if (decInsn.opcode != OP_RETURN_VOID) {
                    stayOneMoreInst = true;
                }
            }
            /* NOTE: intentional fallthrough for returns */
        case kJitTSelectEnd:
            {
                /* Empty trace - set to bail to interpreter */
                if (self->totalTraceLen == 0) {
                    dvmJitSetCodeAddr(self->currTraceHead,
                                      dvmCompilerGetInterpretTemplate(),
                                      dvmCompilerGetInterpretTemplateSet(),
                                      false /* Not method entry */, 0);
                    self->jitState = kJitDone;
                    allDone = true;
                    break;
                }

                int lastTraceDesc = self->currTraceRun;

                /* Extend a new empty desc if the last slot is meta info */
                if (!self->trace[lastTraceDesc].isCode) {
                    lastTraceDesc = ++self->currTraceRun;
                    self->trace[lastTraceDesc].info.frag.startOffset = 0;
                    self->trace[lastTraceDesc].info.frag.numInsts = 0;
                    self->trace[lastTraceDesc].info.frag.hint = kJitHintNone;
                    self->trace[lastTraceDesc].isCode = true;
                }

                /* Mark the end of the trace runs */
                self->trace[lastTraceDesc].info.frag.runEnd = true;

                JitTraceDescription* desc =
                   (JitTraceDescription*)malloc(sizeof(JitTraceDescription) +
                     sizeof(JitTraceRun) * (self->currTraceRun+1));

                if (desc == NULL) {
                    ALOGE("Out of memory in trace selection");
                    dvmJitStopTranslationRequests();
                    self->jitState = kJitDone;
                    allDone = true;
                    break;
                }

                desc->method = self->traceMethod;
                memcpy((char*)&(desc->trace[0]),
                    (char*)&(self->trace[0]),
                    sizeof(JitTraceRun) * (self->currTraceRun+1));
#if defined(SHOW_TRACE)
                ALOGD("TraceGen:  trace done, adding to queue");
                dvmJitDumpTraceDesc(desc);
#endif
                if (dvmCompilerWorkEnqueue(
                       self->currTraceHead,kWorkOrderTrace,desc)) {
                    /* Work order successfully enqueued */
                    if (gDvmJit.blockingMode) {
                        dvmCompilerDrainQueue();
                    }
                } else {
                    /*
                     * Make sure the descriptor for the abandoned work order is
                     * freed.
                     */
                    free(desc);
                }
                self->jitState = kJitDone;
                allDone = true;
            }
            break;
        case kJitDone:
            allDone = true;
            break;
        case kJitNot:
            allDone = true;
            break;
        default:
            ALOGE("Unexpected JIT state: %d", self->jitState);
            dvmAbort();
            break;
    }

    /*
     * If we're done with trace selection, switch off the control flags.
     */
     if (allDone) {
         dvmDisableSubMode(self, kSubModeJitTraceBuild);
         if (stayOneMoreInst) {
             // Clear jitResumeNPC explicitly since we know we don't need it
             // here.
             self->jitResumeNPC = NULL;
             // Keep going in single-step mode for at least one more inst
             if (self->singleStepCount == 0)
                 self->singleStepCount = 1;
             dvmEnableSubMode(self, kSubModeCountedStep);
         }
     }
     return;
}

JitEntry *dvmJitFindEntry(const u2* pc, bool isMethodEntry)
{
    int idx = dvmJitHash(pc);

    /* Expect a high hit rate on 1st shot */
    if ((gDvmJit.pJitEntryTable[idx].dPC == pc) &&
        (gDvmJit.pJitEntryTable[idx].u.info.isMethodEntry == isMethodEntry))
        return &gDvmJit.pJitEntryTable[idx];
    else {
        int chainEndMarker = gDvmJit.jitTableSize;
        while (gDvmJit.pJitEntryTable[idx].u.info.chain != chainEndMarker) {
            idx = gDvmJit.pJitEntryTable[idx].u.info.chain;
            if ((gDvmJit.pJitEntryTable[idx].dPC == pc) &&
                (gDvmJit.pJitEntryTable[idx].u.info.isMethodEntry ==
                isMethodEntry))
                return &gDvmJit.pJitEntryTable[idx];
        }
    }
    return NULL;
}

/*
 * Walk through the JIT profile table and find the corresponding JIT code, in
 * the specified format (ie trace vs method). This routine needs to be fast.
 */
void* getCodeAddrCommon(const u2* dPC, bool methodEntry)
{
    int idx = dvmJitHash(dPC);
    const u2* pc = gDvmJit.pJitEntryTable[idx].dPC;
    if (pc != NULL) {
        bool hideTranslation = dvmJitHideTranslation();
        if (pc == dPC &&
            gDvmJit.pJitEntryTable[idx].u.info.isMethodEntry == methodEntry) {
            int offset = (gDvmJit.profileMode >= kTraceProfilingContinuous) ?
                 0 : gDvmJit.pJitEntryTable[idx].u.info.profileOffset;
            intptr_t codeAddress =
                (intptr_t)gDvmJit.pJitEntryTable[idx].codeAddress;
#if defined(WITH_JIT_TUNING)
            gDvmJit.addrLookupsFound++;
#endif
            return hideTranslation || !codeAddress ?  NULL :
                  (void *)(codeAddress + offset);
        } else {
            int chainEndMarker = gDvmJit.jitTableSize;
            while (gDvmJit.pJitEntryTable[idx].u.info.chain != chainEndMarker) {
                idx = gDvmJit.pJitEntryTable[idx].u.info.chain;
                if (gDvmJit.pJitEntryTable[idx].dPC == dPC &&
                    gDvmJit.pJitEntryTable[idx].u.info.isMethodEntry ==
                        methodEntry) {
                    int offset = (gDvmJit.profileMode >=
                        kTraceProfilingContinuous) ? 0 :
                        gDvmJit.pJitEntryTable[idx].u.info.profileOffset;
                    intptr_t codeAddress =
                        (intptr_t)gDvmJit.pJitEntryTable[idx].codeAddress;
#if defined(WITH_JIT_TUNING)
                    gDvmJit.addrLookupsFound++;
#endif
                    return hideTranslation || !codeAddress ? NULL :
                        (void *)(codeAddress + offset);
                }
            }
        }
    }
#if defined(WITH_JIT_TUNING)
    gDvmJit.addrLookupsNotFound++;
#endif
    return NULL;
}

/*
 * If a translated code address, in trace format, exists for the davik byte code
 * pointer return it.
 */
void* dvmJitGetTraceAddr(const u2* dPC)
{
    return getCodeAddrCommon(dPC, false /* method entry */);
}

/*
 * If a translated code address, in whole-method format, exists for the davik
 * byte code pointer return it.
 */
void* dvmJitGetMethodAddr(const u2* dPC)
{
    return getCodeAddrCommon(dPC, true /* method entry */);
}

/*
 * Similar to dvmJitGetTraceAddr, but returns null if the calling
 * thread is in a single-step mode.
 */
void* dvmJitGetTraceAddrThread(const u2* dPC, Thread* self)
{
    return (self->interpBreak.ctl.breakFlags != 0) ? NULL :
            getCodeAddrCommon(dPC, false /* method entry */);
}

/*
 * Similar to dvmJitGetMethodAddr, but returns null if the calling
 * thread is in a single-step mode.
 */
void* dvmJitGetMethodAddrThread(const u2* dPC, Thread* self)
{
    return (self->interpBreak.ctl.breakFlags != 0) ? NULL :
            getCodeAddrCommon(dPC, true /* method entry */);
}

/*
 * Register the translated code pointer into the JitTable.
 * NOTE: Once a codeAddress field transitions from initial state to
 * JIT'd code, it must not be altered without first halting all
 * threads.  We defer the setting of the profile prefix size until
 * after the new code address is set to ensure that the prefix offset
 * is never applied to the initial interpret-only translation.  All
 * translations with non-zero profile prefixes will still be correct
 * if entered as if the profile offset is 0, but the interpret-only
 * template cannot handle a non-zero prefix.
 * NOTE: JitTable must not be in danger of reset while this
 * code is executing. see Issue 4271784 for details.
 */
void dvmJitSetCodeAddr(const u2* dPC, void *nPC, JitInstructionSetType set,
                       bool isMethodEntry, int profilePrefixSize)
{
    JitEntryInfoUnion oldValue;
    JitEntryInfoUnion newValue;
    /*
     * Get the JitTable slot for this dPC (or create one if JitTable
     * has been reset between the time the trace was requested and
     * now.
     */
    JitEntry *jitEntry = isMethodEntry ?
        lookupAndAdd(dPC, false /* caller holds tableLock */, isMethodEntry) :
                     dvmJitFindEntry(dPC, isMethodEntry);
    assert(jitEntry);
    /* Note: order of update is important */
    do {
        oldValue = jitEntry->u;
        newValue = oldValue;
        newValue.info.isMethodEntry = isMethodEntry;
        newValue.info.instructionSet = set;
        newValue.info.profileOffset = profilePrefixSize;
    } while (android_atomic_release_cas(
             oldValue.infoWord, newValue.infoWord,
             &jitEntry->u.infoWord) != 0);
    jitEntry->codeAddress = nPC;
}

/*
 * Determine if valid trace-bulding request is active.  If so, set
 * the proper flags in interpBreak and return.  Trace selection will
 * then begin normally via dvmCheckBefore.
 */
void dvmJitCheckTraceRequest(Thread* self)
{
    int i;
    /*
     * A note on trace "hotness" filtering:
     *
     * Our first level trigger is intentionally loose - we need it to
     * fire easily not just to identify potential traces to compile, but
     * also to allow re-entry into the code cache.
     *
     * The 2nd level filter (done here) exists to be selective about
     * what we actually compile.  It works by requiring the same
     * trace head "key" (defined as filterKey below) to appear twice in
     * a relatively short period of time.   The difficulty is defining the
     * shape of the filterKey.  Unfortunately, there is no "one size fits
     * all" approach.
     *
     * For spiky execution profiles dominated by a smallish
     * number of very hot loops, we would want the second-level filter
     * to be very selective.  A good selective filter is requiring an
     * exact match of the Dalvik PC.  In other words, defining filterKey as:
     *     intptr_t filterKey = (intptr_t)self->interpSave.pc
     *
     * However, for flat execution profiles we do best when aggressively
     * translating.  A heuristically decent proxy for this is to use
     * the value of the method pointer containing the trace as the filterKey.
     * Intuitively, this is saying that once any trace in a method appears hot,
     * immediately translate any other trace from that same method that
     * survives the first-level filter.  Here, filterKey would be defined as:
     *     intptr_t filterKey = (intptr_t)self->interpSave.method
     *
     * The problem is that we can't easily detect whether we're dealing
     * with a spiky or flat profile.  If we go with the "pc" match approach,
     * flat profiles perform poorly.  If we go with the loose "method" match,
     * we end up generating a lot of useless translations.  Probably the
     * best approach in the future will be to retain profile information
     * across runs of each application in order to determine it's profile,
     * and then choose once we have enough history.
     *
     * However, for now we've decided to chose a compromise filter scheme that
     * includes elements of both.  The high order bits of the filter key
     * are drawn from the enclosing method, and are combined with a slice
     * of the low-order bits of the Dalvik pc of the trace head.  The
     * looseness of the filter can be adjusted by changing with width of
     * the Dalvik pc slice (JIT_TRACE_THRESH_FILTER_PC_BITS).  The wider
     * the slice, the tighter the filter.
     *
     * Note: the fixed shifts in the function below reflect assumed word
     * alignment for method pointers, and half-word alignment of the Dalvik pc.
     * for method pointers and half-word alignment for dalvik pc.
     */
    u4 methodKey = (u4)self->interpSave.method <<
                   (JIT_TRACE_THRESH_FILTER_PC_BITS - 2);
    u4 pcKey = ((u4)self->interpSave.pc >> 1) &
               ((1 << JIT_TRACE_THRESH_FILTER_PC_BITS) - 1);
    intptr_t filterKey = (intptr_t)(methodKey | pcKey);

    // Shouldn't be here if already building a trace.
    assert((self->interpBreak.ctl.subMode & kSubModeJitTraceBuild)==0);

    /* Check if the JIT request can be handled now */
    if ((gDvmJit.pJitEntryTable != NULL) &&
        ((self->interpBreak.ctl.breakFlags & kInterpSingleStep) == 0)){
        /* Bypass the filter for hot trace requests or during stress mode */
        if (self->jitState == kJitTSelectRequest &&
            gDvmJit.threshold > 6) {
            /* Two-level filtering scheme */
            for (i=0; i< JIT_TRACE_THRESH_FILTER_SIZE; i++) {
                if (filterKey == self->threshFilter[i]) {
                    self->threshFilter[i] = 0; // Reset filter entry
                    break;
                }
            }
            if (i == JIT_TRACE_THRESH_FILTER_SIZE) {
                /*
                 * Use random replacement policy - otherwise we could miss a
                 * large loop that contains more traces than the size of our
                 * filter array.
                 */
                i = rand() % JIT_TRACE_THRESH_FILTER_SIZE;
                self->threshFilter[i] = filterKey;
                self->jitState = kJitDone;
            }
        }

        /* If the compiler is backlogged, cancel any JIT actions */
        if (gDvmJit.compilerQueueLength >= gDvmJit.compilerHighWater) {
            self->jitState = kJitDone;
        }

        /*
         * Check for additional reasons that might force the trace select
         * request to be dropped
         */
        if (self->jitState == kJitTSelectRequest ||
            self->jitState == kJitTSelectRequestHot) {
            if (dvmJitFindEntry(self->interpSave.pc, false)) {
                /* In progress - nothing do do */
               self->jitState = kJitDone;
            } else {
                JitEntry *slot = lookupAndAdd(self->interpSave.pc,
                                              false /* lock */,
                                              false /* method entry */);
                if (slot == NULL) {
                    /*
                     * Table is full.  This should have been
                     * detected by the compiler thread and the table
                     * resized before we run into it here.  Assume bad things
                     * are afoot and disable profiling.
                     */
                    self->jitState = kJitDone;
                    ALOGD("JIT: JitTable full, disabling profiling");
                    dvmJitStopTranslationRequests();
                }
            }
        }

        switch (self->jitState) {
            case kJitTSelectRequest:
            case kJitTSelectRequestHot:
                self->jitState = kJitTSelect;
                self->traceMethod = self->interpSave.method;
                self->currTraceHead = self->interpSave.pc;
                self->currTraceRun = 0;
                self->totalTraceLen = 0;
                self->currRunHead = self->interpSave.pc;
                self->currRunLen = 0;
                self->trace[0].info.frag.startOffset =
                     self->interpSave.pc - self->interpSave.method->insns;
                self->trace[0].info.frag.numInsts = 0;
                self->trace[0].info.frag.runEnd = false;
                self->trace[0].info.frag.hint = kJitHintNone;
                self->trace[0].isCode = true;
                self->lastPC = 0;
                /* Turn on trace selection mode */
                dvmEnableSubMode(self, kSubModeJitTraceBuild);
#if defined(SHOW_TRACE)
                ALOGD("Starting trace for %s at %#x",
                     self->interpSave.method->name, (int)self->interpSave.pc);
#endif
                break;
            case kJitDone:
                break;
            default:
                ALOGE("Unexpected JIT state: %d", self->jitState);
                dvmAbort();
        }
    } else {
        /* Cannot build trace this time */
        self->jitState = kJitDone;
    }
}

/*
 * Resizes the JitTable.  Must be a power of 2, and returns true on failure.
 * Stops all threads, and thus is a heavyweight operation. May only be called
 * by the compiler thread.
 */
bool dvmJitResizeJitTable( unsigned int size )
{
    JitEntry *pNewTable;
    JitEntry *pOldTable;
    JitEntry tempEntry;
    unsigned int oldSize;
    unsigned int i;

    assert(gDvmJit.pJitEntryTable != NULL);
    assert(size && !(size & (size - 1)));   /* Is power of 2? */

    ALOGI("Jit: resizing JitTable from %d to %d", gDvmJit.jitTableSize, size);

    if (size <= gDvmJit.jitTableSize) {
        return true;
    }

    /* Make sure requested size is compatible with chain field width */
    tempEntry.u.info.chain = size;
    if (tempEntry.u.info.chain != size) {
        ALOGD("Jit: JitTable request of %d too big", size);
        return true;
    }

    pNewTable = (JitEntry*)calloc(size, sizeof(*pNewTable));
    if (pNewTable == NULL) {
        return true;
    }
    for (i=0; i< size; i++) {
        pNewTable[i].u.info.chain = size;  /* Initialize chain termination */
    }

    /* Stop all other interpreting/jit'ng threads */
    dvmSuspendAllThreads(SUSPEND_FOR_TBL_RESIZE);

    pOldTable = gDvmJit.pJitEntryTable;
    oldSize = gDvmJit.jitTableSize;

    dvmLockMutex(&gDvmJit.tableLock);
    gDvmJit.pJitEntryTable = pNewTable;
    gDvmJit.jitTableSize = size;
    gDvmJit.jitTableMask = size - 1;
    gDvmJit.jitTableEntriesUsed = 0;

    for (i=0; i < oldSize; i++) {
        if (pOldTable[i].dPC) {
            JitEntry *p;
            u2 chain;
            p = lookupAndAdd(pOldTable[i].dPC, true /* holds tableLock*/,
                             pOldTable[i].u.info.isMethodEntry);
            p->codeAddress = pOldTable[i].codeAddress;
            /* We need to preserve the new chain field, but copy the rest */
            chain = p->u.info.chain;
            p->u = pOldTable[i].u;
            p->u.info.chain = chain;
        }
    }

    dvmUnlockMutex(&gDvmJit.tableLock);

    free(pOldTable);

    /* Restart the world */
    dvmResumeAllThreads(SUSPEND_FOR_TBL_RESIZE);

    return false;
}

/*
 * Reset the JitTable to the initial clean state.
 */
void dvmJitResetTable()
{
    JitEntry *jitEntry = gDvmJit.pJitEntryTable;
    unsigned int size = gDvmJit.jitTableSize;
    unsigned int i;

    dvmLockMutex(&gDvmJit.tableLock);

    /* Note: If need to preserve any existing counts. Do so here. */
    if (gDvmJit.pJitTraceProfCounters) {
        for (i=0; i < JIT_PROF_BLOCK_BUCKETS; i++) {
            if (gDvmJit.pJitTraceProfCounters->buckets[i])
                memset((void *) gDvmJit.pJitTraceProfCounters->buckets[i],
                       0, sizeof(JitTraceCounter_t) * JIT_PROF_BLOCK_ENTRIES);
        }
        gDvmJit.pJitTraceProfCounters->next = 0;
    }

    memset((void *) jitEntry, 0, sizeof(JitEntry) * size);
    for (i=0; i< size; i++) {
        jitEntry[i].u.info.chain = size;  /* Initialize chain termination */
    }
    gDvmJit.jitTableEntriesUsed = 0;
    dvmUnlockMutex(&gDvmJit.tableLock);
}

/*
 * Return the address of the next trace profile counter.  This address
 * will be embedded in the generated code for the trace, and thus cannot
 * change while the trace exists.
 */
JitTraceCounter_t *dvmJitNextTraceCounter()
{
    int idx = gDvmJit.pJitTraceProfCounters->next / JIT_PROF_BLOCK_ENTRIES;
    int elem = gDvmJit.pJitTraceProfCounters->next % JIT_PROF_BLOCK_ENTRIES;
    JitTraceCounter_t *res;
    /* Lazily allocate blocks of counters */
    if (!gDvmJit.pJitTraceProfCounters->buckets[idx]) {
        JitTraceCounter_t *p =
              (JitTraceCounter_t*) calloc(JIT_PROF_BLOCK_ENTRIES, sizeof(*p));
        if (!p) {
            ALOGE("Failed to allocate block of trace profile counters");
            dvmAbort();
        }
        gDvmJit.pJitTraceProfCounters->buckets[idx] = p;
    }
    res = &gDvmJit.pJitTraceProfCounters->buckets[idx][elem];
    gDvmJit.pJitTraceProfCounters->next++;
    return res;
}

/*
 * Float/double conversion requires clamping to min and max of integer form.  If
 * target doesn't support this normally, use these.
 */
s8 dvmJitd2l(double d)
{
    static const double kMaxLong = (double)(s8)0x7fffffffffffffffULL;
    static const double kMinLong = (double)(s8)0x8000000000000000ULL;
    if (d >= kMaxLong)
        return (s8)0x7fffffffffffffffULL;
    else if (d <= kMinLong)
        return (s8)0x8000000000000000ULL;
    else if (d != d) // NaN case
        return 0;
    else
        return (s8)d;
}

s8 dvmJitf2l(float f)
{
    static const float kMaxLong = (float)(s8)0x7fffffffffffffffULL;
    static const float kMinLong = (float)(s8)0x8000000000000000ULL;
    if (f >= kMaxLong)
        return (s8)0x7fffffffffffffffULL;
    else if (f <= kMinLong)
        return (s8)0x8000000000000000ULL;
    else if (f != f) // NaN case
        return 0;
    else
        return (s8)f;
}

/* Should only be called by the compiler thread */
void dvmJitChangeProfileMode(TraceProfilingModes newState)
{
    if (gDvmJit.profileMode != newState) {
        gDvmJit.profileMode = newState;
        dvmJitUnchainAll();
    }
}

void dvmJitTraceProfilingOn()
{
    if (gDvmJit.profileMode == kTraceProfilingPeriodicOff)
        dvmCompilerForceWorkEnqueue(NULL, kWorkOrderProfileMode,
                                    (void*) kTraceProfilingPeriodicOn);
    else if (gDvmJit.profileMode == kTraceProfilingDisabled)
        dvmCompilerForceWorkEnqueue(NULL, kWorkOrderProfileMode,
                                    (void*) kTraceProfilingContinuous);
}

void dvmJitTraceProfilingOff()
{
    if (gDvmJit.profileMode == kTraceProfilingPeriodicOn)
        dvmCompilerForceWorkEnqueue(NULL, kWorkOrderProfileMode,
                                    (void*) kTraceProfilingPeriodicOff);
    else if (gDvmJit.profileMode == kTraceProfilingContinuous)
        dvmCompilerForceWorkEnqueue(NULL, kWorkOrderProfileMode,
                                    (void*) kTraceProfilingDisabled);
}

/*
 * Update JIT-specific info in Thread structure for a single thread
 */
void dvmJitUpdateThreadStateSingle(Thread* thread)
{
    thread->pJitProfTable = gDvmJit.pProfTable;
    thread->jitThreshold = gDvmJit.threshold;
}

/*
 * Walk through the thread list and refresh all local copies of
 * JIT global state (which was placed there for fast access).
 */
void dvmJitUpdateThreadStateAll()
{
    Thread* self = dvmThreadSelf();
    Thread* thread;

    dvmLockThreadList(self);
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        dvmJitUpdateThreadStateSingle(thread);
    }
    dvmUnlockThreadList();

}
#endif /* WITH_JIT */
