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
 * Main interpreter entry point and support functions.
 *
 * The entry point selects the "standard" or "debug" interpreter and
 * facilitates switching between them.  The standard interpreter may
 * use the "fast" or "portable" implementation.
 *
 * Some debugger support functions are included here.
 */
#include "Dalvik.h"
#include "interp/InterpDefs.h"
#if defined(WITH_JIT)
#include "interp/Jit.h"
#endif


/*
 * ===========================================================================
 *      Debugger support
 * ===========================================================================
 */

// fwd
static BreakpointSet* dvmBreakpointSetAlloc();
static void dvmBreakpointSetFree(BreakpointSet* pSet);

#if defined(WITH_JIT)
/* Target-specific save/restore */
extern "C" void dvmJitCalleeSave(double *saveArea);
extern "C" void dvmJitCalleeRestore(double *saveArea);
/* Interpreter entry points from compiled code */
extern "C" void dvmJitToInterpNormal();
extern "C" void dvmJitToInterpNoChain();
extern "C" void dvmJitToInterpPunt();
extern "C" void dvmJitToInterpSingleStep();
extern "C" void dvmJitToInterpTraceSelect();
#if defined(WITH_SELF_VERIFICATION)
extern "C" void dvmJitToInterpBackwardBranch();
#endif
#endif

/*
 * Initialize global breakpoint structures.
 */
bool dvmBreakpointStartup()
{
    gDvm.breakpointSet = dvmBreakpointSetAlloc();
    return (gDvm.breakpointSet != NULL);
}

/*
 * Free resources.
 */
void dvmBreakpointShutdown()
{
    dvmBreakpointSetFree(gDvm.breakpointSet);
}


/*
 * This represents a breakpoint inserted in the instruction stream.
 *
 * The debugger may ask us to create the same breakpoint multiple times.
 * We only remove the breakpoint when the last instance is cleared.
 */
struct Breakpoint {
    Method*     method;                 /* method we're associated with */
    u2*         addr;                   /* absolute memory address */
    u1          originalOpcode;         /* original 8-bit opcode value */
    int         setCount;               /* #of times this breakpoint was set */
};

/*
 * Set of breakpoints.
 */
struct BreakpointSet {
    /* grab lock before reading or writing anything else in here */
    pthread_mutex_t lock;

    /* vector of breakpoint structures */
    int         alloc;
    int         count;
    Breakpoint* breakpoints;
};

/*
 * Initialize a BreakpointSet.  Initially empty.
 */
static BreakpointSet* dvmBreakpointSetAlloc()
{
    BreakpointSet* pSet = (BreakpointSet*) calloc(1, sizeof(*pSet));

    dvmInitMutex(&pSet->lock);
    /* leave the rest zeroed -- will alloc on first use */

    return pSet;
}

/*
 * Free storage associated with a BreakpointSet.
 */
static void dvmBreakpointSetFree(BreakpointSet* pSet)
{
    if (pSet == NULL)
        return;

    free(pSet->breakpoints);
    free(pSet);
}

/*
 * Lock the breakpoint set.
 *
 * It's not currently necessary to switch to VMWAIT in the event of
 * contention, because nothing in here can block.  However, it's possible
 * that the bytecode-updater code could become fancier in the future, so
 * we do the trylock dance as a bit of future-proofing.
 */
static void dvmBreakpointSetLock(BreakpointSet* pSet)
{
    if (dvmTryLockMutex(&pSet->lock) != 0) {
        Thread* self = dvmThreadSelf();
        ThreadStatus oldStatus = dvmChangeStatus(self, THREAD_VMWAIT);
        dvmLockMutex(&pSet->lock);
        dvmChangeStatus(self, oldStatus);
    }
}

/*
 * Unlock the breakpoint set.
 */
static void dvmBreakpointSetUnlock(BreakpointSet* pSet)
{
    dvmUnlockMutex(&pSet->lock);
}

/*
 * Return the #of breakpoints.
 */
static int dvmBreakpointSetCount(const BreakpointSet* pSet)
{
    return pSet->count;
}

/*
 * See if we already have an entry for this address.
 *
 * The BreakpointSet's lock must be acquired before calling here.
 *
 * Returns the index of the breakpoint entry, or -1 if not found.
 */
static int dvmBreakpointSetFind(const BreakpointSet* pSet, const u2* addr)
{
    int i;

    for (i = 0; i < pSet->count; i++) {
        Breakpoint* pBreak = &pSet->breakpoints[i];
        if (pBreak->addr == addr)
            return i;
    }

    return -1;
}

/*
 * Retrieve the opcode that was originally at the specified location.
 *
 * The BreakpointSet's lock must be acquired before calling here.
 *
 * Returns "true" with the opcode in *pOrig on success.
 */
static bool dvmBreakpointSetOriginalOpcode(const BreakpointSet* pSet,
    const u2* addr, u1* pOrig)
{
    int idx = dvmBreakpointSetFind(pSet, addr);
    if (idx < 0)
        return false;

    *pOrig = pSet->breakpoints[idx].originalOpcode;
    return true;
}

/*
 * Check the opcode.  If it's a "magic" NOP, indicating the start of
 * switch or array data in the instruction stream, we don't want to set
 * a breakpoint.
 *
 * This can happen because the line number information dx generates
 * associates the switch data with the switch statement's line number,
 * and some debuggers put breakpoints at every address associated with
 * a given line.  The result is that the breakpoint stomps on the NOP
 * instruction that doubles as a data table magic number, and an explicit
 * check in the interpreter results in an exception being thrown.
 *
 * We don't want to simply refuse to add the breakpoint to the table,
 * because that confuses the housekeeping.  We don't want to reject the
 * debugger's event request, and we want to be sure that there's exactly
 * one un-set operation for every set op.
 */
static bool instructionIsMagicNop(const u2* addr)
{
    u2 curVal = *addr;
    return ((GET_OPCODE(curVal)) == OP_NOP && (curVal >> 8) != 0);
}

/*
 * Add a breakpoint at a specific address.  If the address is already
 * present in the table, this just increments the count.
 *
 * For a new entry, this will extract and preserve the current opcode from
 * the instruction stream, and replace it with a breakpoint opcode.
 *
 * The BreakpointSet's lock must be acquired before calling here.
 *
 * Returns "true" on success.
 */
static bool dvmBreakpointSetAdd(BreakpointSet* pSet, Method* method,
    unsigned int instrOffset)
{
    const int kBreakpointGrowth = 10;
    const u2* addr = method->insns + instrOffset;
    int idx = dvmBreakpointSetFind(pSet, addr);
    Breakpoint* pBreak;

    if (idx < 0) {
        if (pSet->count == pSet->alloc) {
            int newSize = pSet->alloc + kBreakpointGrowth;
            Breakpoint* newVec;

            ALOGV("+++ increasing breakpoint set size to %d", newSize);

            /* pSet->breakpoints will be NULL on first entry */
            newVec = (Breakpoint*)realloc(pSet->breakpoints, newSize * sizeof(Breakpoint));
            if (newVec == NULL)
                return false;

            pSet->breakpoints = newVec;
            pSet->alloc = newSize;
        }

        pBreak = &pSet->breakpoints[pSet->count++];
        pBreak->method = method;
        pBreak->addr = (u2*)addr;
        pBreak->originalOpcode = *(u1*)addr;
        pBreak->setCount = 1;

        /*
         * Change the opcode.  We must ensure that the BreakpointSet
         * updates happen before we change the opcode.
         *
         * If the method has not been verified, we do NOT insert the
         * breakpoint yet, since that will screw up the verifier.  The
         * debugger is allowed to insert breakpoints in unverified code,
         * but since we don't execute unverified code we don't need to
         * alter the bytecode yet.
         *
         * The class init code will "flush" all pending opcode writes
         * before verification completes.
         */
        assert(*(u1*)addr != OP_BREAKPOINT);
        if (dvmIsClassVerified(method->clazz)) {
            ALOGV("Class %s verified, adding breakpoint at %p",
                method->clazz->descriptor, addr);
            if (instructionIsMagicNop(addr)) {
                ALOGV("Refusing to set breakpoint on %04x at %s.%s + %#x",
                    *addr, method->clazz->descriptor, method->name,
                    instrOffset);
            } else {
                ANDROID_MEMBAR_FULL();
                dvmDexChangeDex1(method->clazz->pDvmDex, (u1*)addr,
                    OP_BREAKPOINT);
            }
        } else {
            ALOGV("Class %s NOT verified, deferring breakpoint at %p",
                method->clazz->descriptor, addr);
        }
    } else {
        /*
         * Breakpoint already exists, just increase the count.
         */
        pBreak = &pSet->breakpoints[idx];
        pBreak->setCount++;
    }

    return true;
}

/*
 * Remove one instance of the specified breakpoint.  When the count
 * reaches zero, the entry is removed from the table, and the original
 * opcode is restored.
 *
 * The BreakpointSet's lock must be acquired before calling here.
 */
static void dvmBreakpointSetRemove(BreakpointSet* pSet, Method* method,
    unsigned int instrOffset)
{
    const u2* addr = method->insns + instrOffset;
    int idx = dvmBreakpointSetFind(pSet, addr);

    if (idx < 0) {
        /* breakpoint not found in set -- unexpected */
        if (*(u1*)addr == OP_BREAKPOINT) {
            ALOGE("Unable to restore breakpoint opcode (%s.%s +%#x)",
                method->clazz->descriptor, method->name, instrOffset);
            dvmAbort();
        } else {
            ALOGW("Breakpoint was already restored? (%s.%s +%#x)",
                method->clazz->descriptor, method->name, instrOffset);
        }
    } else {
        Breakpoint* pBreak = &pSet->breakpoints[idx];
        if (pBreak->setCount == 1) {
            /*
             * Must restore opcode before removing set entry.
             *
             * If the breakpoint was never flushed, we could be ovewriting
             * a value with the same value.  Not a problem, though we
             * could end up causing a copy-on-write here when we didn't
             * need to.  (Not worth worrying about.)
             */
            dvmDexChangeDex1(method->clazz->pDvmDex, (u1*)addr,
                pBreak->originalOpcode);
            ANDROID_MEMBAR_FULL();

            if (idx != pSet->count-1) {
                /* shift down */
                memmove(&pSet->breakpoints[idx], &pSet->breakpoints[idx+1],
                    (pSet->count-1 - idx) * sizeof(pSet->breakpoints[0]));
            }
            pSet->count--;
            pSet->breakpoints[pSet->count].addr = (u2*) 0xdecadead; // debug
        } else {
            pBreak->setCount--;
            assert(pBreak->setCount > 0);
        }
    }
}

/*
 * Flush any breakpoints associated with methods in "clazz".  We want to
 * change the opcode, which might not have happened when the breakpoint
 * was initially set because the class was in the process of being
 * verified.
 *
 * The BreakpointSet's lock must be acquired before calling here.
 */
static void dvmBreakpointSetFlush(BreakpointSet* pSet, ClassObject* clazz)
{
    int i;
    for (i = 0; i < pSet->count; i++) {
        Breakpoint* pBreak = &pSet->breakpoints[i];
        if (pBreak->method->clazz == clazz) {
            /*
             * The breakpoint is associated with a method in this class.
             * It might already be there or it might not; either way,
             * flush it out.
             */
            ALOGV("Flushing breakpoint at %p for %s",
                pBreak->addr, clazz->descriptor);
            if (instructionIsMagicNop(pBreak->addr)) {
                ALOGV("Refusing to flush breakpoint on %04x at %s.%s + %#x",
                    *pBreak->addr, pBreak->method->clazz->descriptor,
                    pBreak->method->name, pBreak->addr - pBreak->method->insns);
            } else {
                dvmDexChangeDex1(clazz->pDvmDex, (u1*)pBreak->addr,
                    OP_BREAKPOINT);
            }
        }
    }
}


/*
 * Do any debugger-attach-time initialization.
 */
void dvmInitBreakpoints()
{
    /* quick sanity check */
    BreakpointSet* pSet = gDvm.breakpointSet;
    dvmBreakpointSetLock(pSet);
    if (dvmBreakpointSetCount(pSet) != 0) {
        ALOGW("WARNING: %d leftover breakpoints", dvmBreakpointSetCount(pSet));
        /* generally not good, but we can keep going */
    }
    dvmBreakpointSetUnlock(pSet);
}

/*
 * Add an address to the list, putting it in the first non-empty slot.
 *
 * Sometimes the debugger likes to add two entries for one breakpoint.
 * We add two entries here, so that we get the right behavior when it's
 * removed twice.
 *
 * This will only be run from the JDWP thread, and it will happen while
 * we are updating the event list, which is synchronized.  We're guaranteed
 * to be the only one adding entries, and the lock ensures that nobody
 * will be trying to remove them while we're in here.
 *
 * "addr" is the absolute address of the breakpoint bytecode.
 */
void dvmAddBreakAddr(Method* method, unsigned int instrOffset)
{
    BreakpointSet* pSet = gDvm.breakpointSet;
    dvmBreakpointSetLock(pSet);
    dvmBreakpointSetAdd(pSet, method, instrOffset);
    dvmBreakpointSetUnlock(pSet);
}

/*
 * Remove an address from the list by setting the entry to NULL.
 *
 * This can be called from the JDWP thread (because the debugger has
 * cancelled the breakpoint) or from an event thread (because it's a
 * single-shot breakpoint, e.g. "run to line").  We only get here as
 * the result of removing an entry from the event list, which is
 * synchronized, so it should not be possible for two threads to be
 * updating breakpoints at the same time.
 */
void dvmClearBreakAddr(Method* method, unsigned int instrOffset)
{
    BreakpointSet* pSet = gDvm.breakpointSet;
    dvmBreakpointSetLock(pSet);
    dvmBreakpointSetRemove(pSet, method, instrOffset);
    dvmBreakpointSetUnlock(pSet);
}

/*
 * Get the original opcode from under a breakpoint.
 *
 * On SMP hardware it's possible one core might try to execute a breakpoint
 * after another core has cleared it.  We need to handle the case where
 * there's no entry in the breakpoint set.  (The memory barriers in the
 * locks and in the breakpoint update code should ensure that, once we've
 * observed the absence of a breakpoint entry, we will also now observe
 * the restoration of the original opcode.  The fact that we're holding
 * the lock prevents other threads from confusing things further.)
 */
u1 dvmGetOriginalOpcode(const u2* addr)
{
    BreakpointSet* pSet = gDvm.breakpointSet;
    u1 orig = 0;

    dvmBreakpointSetLock(pSet);
    if (!dvmBreakpointSetOriginalOpcode(pSet, addr, &orig)) {
        orig = *(u1*)addr;
        if (orig == OP_BREAKPOINT) {
            ALOGE("GLITCH: can't find breakpoint, opcode is still set");
            dvmAbort();
        }
    }
    dvmBreakpointSetUnlock(pSet);

    return orig;
}

/*
 * Flush any breakpoints associated with methods in "clazz".
 *
 * We don't want to modify the bytecode of a method before the verifier
 * gets a chance to look at it, so we postpone opcode replacement until
 * after verification completes.
 */
void dvmFlushBreakpoints(ClassObject* clazz)
{
    BreakpointSet* pSet = gDvm.breakpointSet;

    if (pSet == NULL)
        return;

    assert(dvmIsClassVerified(clazz));
    dvmBreakpointSetLock(pSet);
    dvmBreakpointSetFlush(pSet, clazz);
    dvmBreakpointSetUnlock(pSet);
}

/*
 * Add a single step event.  Currently this is a global item.
 *
 * We set up some initial values based on the thread's current state.  This
 * won't work well if the thread is running, so it's up to the caller to
 * verify that it's suspended.
 *
 * This is only called from the JDWP thread.
 */
bool dvmAddSingleStep(Thread* thread, int size, int depth)
{
    StepControl* pCtrl = &gDvm.stepControl;

    if (pCtrl->active && thread != pCtrl->thread) {
        ALOGW("WARNING: single-step active for %p; adding %p",
            pCtrl->thread, thread);

        /*
         * Keep going, overwriting previous.  This can happen if you
         * suspend a thread in Object.wait, hit the single-step key, then
         * switch to another thread and do the same thing again.
         * The first thread's step is still pending.
         *
         * TODO: consider making single-step per-thread.  Adds to the
         * overhead, but could be useful in rare situations.
         */
    }

    pCtrl->size = static_cast<JdwpStepSize>(size);
    pCtrl->depth = static_cast<JdwpStepDepth>(depth);
    pCtrl->thread = thread;

    /*
     * We may be stepping into or over method calls, or running until we
     * return from the current method.  To make this work we need to track
     * the current line, current method, and current stack depth.  We need
     * to be checking these after most instructions, notably those that
     * call methods, return from methods, or are on a different line from the
     * previous instruction.
     *
     * We have to start with a snapshot of the current state.  If we're in
     * an interpreted method, everything we need is in the current frame.  If
     * we're in a native method, possibly with some extra JNI frames pushed
     * on by PushLocalFrame, we want to use the topmost native method.
     */
    const StackSaveArea* saveArea;
    u4* fp;
    u4* prevFp = NULL;

    for (fp = thread->interpSave.curFrame; fp != NULL;
         fp = saveArea->prevFrame) {
        const Method* method;

        saveArea = SAVEAREA_FROM_FP(fp);
        method = saveArea->method;

        if (!dvmIsBreakFrame((u4*)fp) && !dvmIsNativeMethod(method))
            break;
        prevFp = fp;
    }
    if (fp == NULL) {
        ALOGW("Unexpected: step req in native-only threadid=%d",
            thread->threadId);
        return false;
    }
    if (prevFp != NULL) {
        /*
         * First interpreted frame wasn't the one at the bottom.  Break
         * frames are only inserted when calling from native->interp, so we
         * don't need to worry about one being here.
         */
        ALOGV("##### init step while in native method");
        fp = prevFp;
        assert(!dvmIsBreakFrame((u4*)fp));
        assert(dvmIsNativeMethod(SAVEAREA_FROM_FP(fp)->method));
        saveArea = SAVEAREA_FROM_FP(fp);
    }

    /*
     * Pull the goodies out.  "xtra.currentPc" should be accurate since
     * we update it on every instruction while the debugger is connected.
     */
    pCtrl->method = saveArea->method;
    // Clear out any old address set
    if (pCtrl->pAddressSet != NULL) {
        // (discard const)
        free((void *)pCtrl->pAddressSet);
        pCtrl->pAddressSet = NULL;
    }
    if (dvmIsNativeMethod(pCtrl->method)) {
        pCtrl->line = -1;
    } else {
        pCtrl->line = dvmLineNumFromPC(saveArea->method,
                        saveArea->xtra.currentPc - saveArea->method->insns);
        pCtrl->pAddressSet
                = dvmAddressSetForLine(saveArea->method, pCtrl->line);
    }
    pCtrl->frameDepth =
        dvmComputeVagueFrameDepth(thread, thread->interpSave.curFrame);
    pCtrl->active = true;

    ALOGV("##### step init: thread=%p meth=%p '%s' line=%d frameDepth=%d depth=%s size=%s",
        pCtrl->thread, pCtrl->method, pCtrl->method->name,
        pCtrl->line, pCtrl->frameDepth,
        dvmJdwpStepDepthStr(pCtrl->depth),
        dvmJdwpStepSizeStr(pCtrl->size));

    return true;
}

/*
 * Disable a single step event.
 */
void dvmClearSingleStep(Thread* thread)
{
    UNUSED_PARAMETER(thread);

    gDvm.stepControl.active = false;
}

/*
 * The interpreter just threw.  Handle any special subMode requirements.
 * All interpSave state must be valid on entry.
 */
void dvmReportExceptionThrow(Thread* self, Object* exception)
{
    const Method* curMethod = self->interpSave.method;
#if defined(WITH_JIT)
    if (self->interpBreak.ctl.subMode & kSubModeJitTraceBuild) {
        dvmJitEndTraceSelect(self, self->interpSave.pc);
    }
    if (self->interpBreak.ctl.breakFlags & kInterpSingleStep) {
        /* Discard any single-step native returns to translation */
        self->jitResumeNPC = NULL;
    }
#endif
    if (self->interpBreak.ctl.subMode & kSubModeDebuggerActive) {
        void *catchFrame;
        int offset = self->interpSave.pc - curMethod->insns;
        int catchRelPc = dvmFindCatchBlock(self, offset, exception,
                                           true, &catchFrame);
        dvmDbgPostException(self->interpSave.curFrame, offset, catchFrame,
                            catchRelPc, exception);
    }
}

/*
 * The interpreter is preparing to do an invoke (both native & normal).
 * Handle any special subMode requirements.  All interpSave state
 * must be valid on entry.
 */
void dvmReportInvoke(Thread* self, const Method* methodToCall)
{
    TRACE_METHOD_ENTER(self, methodToCall);
}

/*
 * The interpreter is preparing to do a native invoke. Handle any
 * special subMode requirements.  NOTE: for a native invoke,
 * dvmReportInvoke() and dvmReportPreNativeInvoke() will both
 * be called prior to the invoke.  fp is the Dalvik FP of the calling
 * method.
 */
void dvmReportPreNativeInvoke(const Method* methodToCall, Thread* self, u4* fp)
{
#if defined(WITH_JIT)
    /*
     * Actively building a trace?  If so, end it now.   The trace
     * builder can't follow into or through a native method.
     */
    if (self->interpBreak.ctl.subMode & kSubModeJitTraceBuild) {
        dvmCheckJit(self->interpSave.pc, self);
    }
#endif
    if (self->interpBreak.ctl.subMode & kSubModeDebuggerActive) {
        Object* thisPtr = dvmGetThisPtr(self->interpSave.method, fp);
        assert(thisPtr == NULL || dvmIsHeapAddress(thisPtr));
        dvmDbgPostLocationEvent(methodToCall, -1, thisPtr, DBG_METHOD_ENTRY);
    }
}

/*
 * The interpreter has returned from a native invoke. Handle any
 * special subMode requirements.  fp is the Dalvik FP of the calling
 * method.
 */
void dvmReportPostNativeInvoke(const Method* methodToCall, Thread* self, u4* fp)
{
    if (self->interpBreak.ctl.subMode & kSubModeDebuggerActive) {
        Object* thisPtr = dvmGetThisPtr(self->interpSave.method, fp);
        assert(thisPtr == NULL || dvmIsHeapAddress(thisPtr));
        dvmDbgPostLocationEvent(methodToCall, -1, thisPtr, DBG_METHOD_EXIT);
    }
    if (self->interpBreak.ctl.subMode & kSubModeMethodTrace) {
        dvmFastNativeMethodTraceExit(methodToCall, self);
    }
}

/*
 * The interpreter has returned from a normal method.  Handle any special
 * subMode requirements.  All interpSave state must be valid on entry.
 */
void dvmReportReturn(Thread* self)
{
    TRACE_METHOD_EXIT(self, self->interpSave.method);
#if defined(WITH_JIT)
    if (dvmIsBreakFrame(self->interpSave.curFrame) &&
        (self->interpBreak.ctl.subMode & kSubModeJitTraceBuild)) {
        dvmCheckJit(self->interpSave.pc, self);
    }
#endif
}

/*
 * Update the debugger on interesting events, such as hitting a breakpoint
 * or a single-step point.  This is called from the top of the interpreter
 * loop, before the current instruction is processed.
 *
 * Set "methodEntry" if we've just entered the method.  This detects
 * method exit by checking to see if the next instruction is "return".
 *
 * This can't catch native method entry/exit, so we have to handle that
 * at the point of invocation.  We also need to catch it in dvmCallMethod
 * if we want to capture native->native calls made through JNI.
 *
 * Notes to self:
 * - Don't want to switch to VMWAIT while posting events to the debugger.
 *   Let the debugger code decide if we need to change state.
 * - We may want to check for debugger-induced thread suspensions on
 *   every instruction.  That would make a "suspend all" more responsive
 *   and reduce the chances of multiple simultaneous events occurring.
 *   However, it could change the behavior some.
 *
 * TODO: method entry/exit events are probably less common than location
 * breakpoints.  We may be able to speed things up a bit if we don't query
 * the event list unless we know there's at least one lurking within.
 */
static void updateDebugger(const Method* method, const u2* pc, const u4* fp,
                           Thread* self)
{
    int eventFlags = 0;

    /*
     * Update xtra.currentPc on every instruction.  We need to do this if
     * there's a chance that we could get suspended.  This can happen if
     * eventFlags != 0 here, or somebody manually requests a suspend
     * (which gets handled at PERIOD_CHECKS time).  One place where this
     * needs to be correct is in dvmAddSingleStep().
     */
    dvmExportPC(pc, fp);

    if (self->debugIsMethodEntry) {
        eventFlags |= DBG_METHOD_ENTRY;
        self->debugIsMethodEntry = false;
    }

    /*
     * See if we have a breakpoint here.
     *
     * Depending on the "mods" associated with event(s) on this address,
     * we may or may not actually send a message to the debugger.
     */
    if (GET_OPCODE(*pc) == OP_BREAKPOINT) {
        ALOGV("+++ breakpoint hit at %p", pc);
        eventFlags |= DBG_BREAKPOINT;
    }

    /*
     * If the debugger is single-stepping one of our threads, check to
     * see if we're that thread and we've reached a step point.
     */
    const StepControl* pCtrl = &gDvm.stepControl;
    if (pCtrl->active && pCtrl->thread == self) {
        int frameDepth;
        bool doStop = false;
        const char* msg = NULL;

        assert(!dvmIsNativeMethod(method));

        if (pCtrl->depth == SD_INTO) {
            /*
             * Step into method calls.  We break when the line number
             * or method pointer changes.  If we're in SS_MIN mode, we
             * always stop.
             */
            if (pCtrl->method != method) {
                doStop = true;
                msg = "new method";
            } else if (pCtrl->size == SS_MIN) {
                doStop = true;
                msg = "new instruction";
            } else if (!dvmAddressSetGet(
                    pCtrl->pAddressSet, pc - method->insns)) {
                doStop = true;
                msg = "new line";
            }
        } else if (pCtrl->depth == SD_OVER) {
            /*
             * Step over method calls.  We break when the line number is
             * different and the frame depth is <= the original frame
             * depth.  (We can't just compare on the method, because we
             * might get unrolled past it by an exception, and it's tricky
             * to identify recursion.)
             */
            frameDepth = dvmComputeVagueFrameDepth(self, fp);
            if (frameDepth < pCtrl->frameDepth) {
                /* popped up one or more frames, always trigger */
                doStop = true;
                msg = "method pop";
            } else if (frameDepth == pCtrl->frameDepth) {
                /* same depth, see if we moved */
                if (pCtrl->size == SS_MIN) {
                    doStop = true;
                    msg = "new instruction";
                } else if (!dvmAddressSetGet(pCtrl->pAddressSet,
                            pc - method->insns)) {
                    doStop = true;
                    msg = "new line";
                }
            }
        } else {
            assert(pCtrl->depth == SD_OUT);
            /*
             * Return from the current method.  We break when the frame
             * depth pops up.
             *
             * This differs from the "method exit" break in that it stops
             * with the PC at the next instruction in the returned-to
             * function, rather than the end of the returning function.
             */
            frameDepth = dvmComputeVagueFrameDepth(self, fp);
            if (frameDepth < pCtrl->frameDepth) {
                doStop = true;
                msg = "method pop";
            }
        }

        if (doStop) {
            ALOGV("#####S %s", msg);
            eventFlags |= DBG_SINGLE_STEP;
        }
    }

    /*
     * Check to see if this is a "return" instruction.  JDWP says we should
     * send the event *after* the code has been executed, but it also says
     * the location we provide is the last instruction.  Since the "return"
     * instruction has no interesting side effects, we should be safe.
     * (We can't just move this down to the returnFromMethod label because
     * we potentially need to combine it with other events.)
     *
     * We're also not supposed to generate a method exit event if the method
     * terminates "with a thrown exception".
     */
    u2 opcode = GET_OPCODE(*pc);
    if (opcode == OP_RETURN_VOID || opcode == OP_RETURN || opcode == OP_RETURN_VOID_BARRIER ||
        opcode == OP_RETURN_OBJECT || opcode == OP_RETURN_WIDE)
    {
        eventFlags |= DBG_METHOD_EXIT;
    }

    /*
     * If there's something interesting going on, see if it matches one
     * of the debugger filters.
     */
    if (eventFlags != 0) {
        Object* thisPtr = dvmGetThisPtr(method, fp);
        if (thisPtr != NULL && !dvmIsHeapAddress(thisPtr)) {
            /*
             * TODO: remove this check if we're confident that the "this"
             * pointer is where it should be -- slows us down, especially
             * during single-step.
             */
            char* desc = dexProtoCopyMethodDescriptor(&method->prototype);
            ALOGE("HEY: invalid 'this' ptr %p (%s.%s %s)", thisPtr,
                method->clazz->descriptor, method->name, desc);
            free(desc);
            dvmAbort();
        }
        dvmDbgPostLocationEvent(method, pc - method->insns, thisPtr,
            eventFlags);
    }
}

/*
 * Recover the "this" pointer from the current interpreted method.  "this"
 * is always in "in0" for non-static methods.
 *
 * The "ins" start at (#of registers - #of ins).  Note in0 != v0.
 *
 * This works because "dx" guarantees that it will work.  It's probably
 * fairly common to have a virtual method that doesn't use its "this"
 * pointer, in which case we're potentially wasting a register.  However,
 * the debugger doesn't treat "this" as just another argument.  For
 * example, events (such as breakpoints) can be enabled for specific
 * values of "this".  There is also a separate StackFrame.ThisObject call
 * in JDWP that is expected to work for any non-native non-static method.
 *
 * Because we need it when setting up debugger event filters, we want to
 * be able to do this quickly.
 */
Object* dvmGetThisPtr(const Method* method, const u4* fp)
{
    if (dvmIsStaticMethod(method))
        return NULL;
    return (Object*)fp[method->registersSize - method->insSize];
}


#if defined(WITH_TRACKREF_CHECKS)
/*
 * Verify that all internally-tracked references have been released.  If
 * they haven't, print them and abort the VM.
 *
 * "debugTrackedRefStart" indicates how many refs were on the list when
 * we were first invoked.
 */
void dvmInterpCheckTrackedRefs(Thread* self, const Method* method,
    int debugTrackedRefStart)
{
    if (dvmReferenceTableEntries(&self->internalLocalRefTable)
        != (size_t) debugTrackedRefStart)
    {
        char* desc;
        Object** top;
        int count;

        count = dvmReferenceTableEntries(&self->internalLocalRefTable);

        ALOGE("TRACK: unreleased internal reference (prev=%d total=%d)",
            debugTrackedRefStart, count);
        desc = dexProtoCopyMethodDescriptor(&method->prototype);
        ALOGE("       current method is %s.%s %s", method->clazz->descriptor,
            method->name, desc);
        free(desc);
        top = self->internalLocalRefTable.table + debugTrackedRefStart;
        while (top < self->internalLocalRefTable.nextEntry) {
            ALOGE("  %p (%s)",
                 *top,
                 ((*top)->clazz != NULL) ? (*top)->clazz->descriptor : "");
            top++;
        }
        dvmDumpThread(self, false);

        dvmAbort();
    }
    //ALOGI("TRACK OK");
}
#endif


#ifdef LOG_INSTR
/*
 * Dump the v-registers.  Sent to the ILOG log tag.
 */
void dvmDumpRegs(const Method* method, const u4* framePtr, bool inOnly)
{
    int i, localCount;

    localCount = method->registersSize - method->insSize;

    ALOG(LOG_VERBOSE, LOG_TAG"i", "Registers (fp=%p):", framePtr);
    for (i = method->registersSize-1; i >= 0; i--) {
        if (i >= localCount) {
            ALOG(LOG_VERBOSE, LOG_TAG"i", "  v%-2d in%-2d : 0x%08x",
                i, i-localCount, framePtr[i]);
        } else {
            if (inOnly) {
                ALOG(LOG_VERBOSE, LOG_TAG"i", "  [...]");
                break;
            }
            const char* name = "";
#if 0   // "locals" structure has changed -- need to rewrite this
            int j;
            DexFile* pDexFile = method->clazz->pDexFile;
            const DexCode* pDexCode = dvmGetMethodCode(method);
            int localsSize = dexGetLocalsSize(pDexFile, pDexCode);
            const DexLocal* locals = dvmDexGetLocals(pDexFile, pDexCode);
            for (j = 0; j < localsSize, j++) {
                if (locals[j].registerNum == (u4) i) {
                    name = dvmDexStringStr(locals[j].pName);
                    break;
                }
            }
#endif
            ALOG(LOG_VERBOSE, LOG_TAG"i", "  v%-2d      : 0x%08x %s",
                i, framePtr[i], name);
        }
    }
}
#endif


/*
 * ===========================================================================
 *      Entry point and general support functions
 * ===========================================================================
 */

/*
 * Find the matching case.  Returns the offset to the handler instructions.
 *
 * Returns 3 if we don't find a match (it's the size of the packed-switch
 * instruction).
 */
s4 dvmInterpHandlePackedSwitch(const u2* switchData, s4 testVal)
{
    const int kInstrLen = 3;

    /*
     * Packed switch data format:
     *  ushort ident = 0x0100   magic value
     *  ushort size             number of entries in the table
     *  int first_key           first (and lowest) switch case value
     *  int targets[size]       branch targets, relative to switch opcode
     *
     * Total size is (4+size*2) 16-bit code units.
     */
    if (*switchData++ != kPackedSwitchSignature) {
        /* should have been caught by verifier */
        dvmThrowInternalError("bad packed switch magic");
        return kInstrLen;
    }

    u2 size = *switchData++;
    assert(size > 0);

    s4 firstKey = *switchData++;
    firstKey |= (*switchData++) << 16;

    int index = testVal - firstKey;
    if (index < 0 || index >= size) {
        LOGVV("Value %d not found in switch (%d-%d)",
            testVal, firstKey, firstKey+size-1);
        return kInstrLen;
    }

    /* The entries are guaranteed to be aligned on a 32-bit boundary;
     * we can treat them as a native int array.
     */
    const s4* entries = (const s4*) switchData;
    assert(((u4)entries & 0x3) == 0);

    assert(index >= 0 && index < size);
    LOGVV("Value %d found in slot %d (goto 0x%02x)",
        testVal, index,
        s4FromSwitchData(&entries[index]));
    return s4FromSwitchData(&entries[index]);
}

/*
 * Find the matching case.  Returns the offset to the handler instructions.
 *
 * Returns 3 if we don't find a match (it's the size of the sparse-switch
 * instruction).
 */
s4 dvmInterpHandleSparseSwitch(const u2* switchData, s4 testVal)
{
    const int kInstrLen = 3;
    u2 size;
    const s4* keys;
    const s4* entries;

    /*
     * Sparse switch data format:
     *  ushort ident = 0x0200   magic value
     *  ushort size             number of entries in the table; > 0
     *  int keys[size]          keys, sorted low-to-high; 32-bit aligned
     *  int targets[size]       branch targets, relative to switch opcode
     *
     * Total size is (2+size*4) 16-bit code units.
     */

    if (*switchData++ != kSparseSwitchSignature) {
        /* should have been caught by verifier */
        dvmThrowInternalError("bad sparse switch magic");
        return kInstrLen;
    }

    size = *switchData++;
    assert(size > 0);

    /* The keys are guaranteed to be aligned on a 32-bit boundary;
     * we can treat them as a native int array.
     */
    keys = (const s4*) switchData;
    assert(((u4)keys & 0x3) == 0);

    /* The entries are guaranteed to be aligned on a 32-bit boundary;
     * we can treat them as a native int array.
     */
    entries = keys + size;
    assert(((u4)entries & 0x3) == 0);

    /*
     * Binary-search through the array of keys, which are guaranteed to
     * be sorted low-to-high.
     */
    int lo = 0;
    int hi = size - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;

        s4 foundVal = s4FromSwitchData(&keys[mid]);
        if (testVal < foundVal) {
            hi = mid - 1;
        } else if (testVal > foundVal) {
            lo = mid + 1;
        } else {
            LOGVV("Value %d found in entry %d (goto 0x%02x)",
                testVal, mid, s4FromSwitchData(&entries[mid]));
            return s4FromSwitchData(&entries[mid]);
        }
    }

    LOGVV("Value %d not found in switch", testVal);
    return kInstrLen;
}

/*
 * Copy data for a fill-array-data instruction.  On a little-endian machine
 * we can just do a memcpy(), on a big-endian system we have work to do.
 *
 * The trick here is that dexopt has byte-swapped each code unit, which is
 * exactly what we want for short/char data.  For byte data we need to undo
 * the swap, and for 4- or 8-byte values we need to swap pieces within
 * each word.
 */
static void copySwappedArrayData(void* dest, const u2* src, u4 size, u2 width)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    memcpy(dest, src, size*width);
#else
    int i;

    switch (width) {
    case 1:
        /* un-swap pairs of bytes as we go */
        for (i = (size-1) & ~1; i >= 0; i -= 2) {
            ((u1*)dest)[i] = ((u1*)src)[i+1];
            ((u1*)dest)[i+1] = ((u1*)src)[i];
        }
        /*
         * "src" is padded to end on a two-byte boundary, but we don't want to
         * assume "dest" is, so we handle odd length specially.
         */
        if ((size & 1) != 0) {
            ((u1*)dest)[size-1] = ((u1*)src)[size];
        }
        break;
    case 2:
        /* already swapped correctly */
        memcpy(dest, src, size*width);
        break;
    case 4:
        /* swap word halves */
        for (i = 0; i < (int) size; i++) {
            ((u4*)dest)[i] = (src[(i << 1) + 1] << 16) | src[i << 1];
        }
        break;
    case 8:
        /* swap word halves and words */
        for (i = 0; i < (int) (size << 1); i += 2) {
            ((int*)dest)[i] = (src[(i << 1) + 3] << 16) | src[(i << 1) + 2];
            ((int*)dest)[i+1] = (src[(i << 1) + 1] << 16) | src[i << 1];
        }
        break;
    default:
        ALOGE("Unexpected width %d in copySwappedArrayData", width);
        dvmAbort();
        break;
    }
#endif
}

/*
 * Fill the array with predefined constant values.
 *
 * Returns true if job is completed, otherwise false to indicate that
 * an exception has been thrown.
 */
bool dvmInterpHandleFillArrayData(ArrayObject* arrayObj, const u2* arrayData)
{
    u2 width;
    u4 size;

    if (arrayObj == NULL) {
        dvmThrowNullPointerException(NULL);
        return false;
    }
    assert (!IS_CLASS_FLAG_SET(((Object *)arrayObj)->clazz,
                               CLASS_ISOBJECTARRAY));

    /*
     * Array data table format:
     *  ushort ident = 0x0300   magic value
     *  ushort width            width of each element in the table
     *  uint   size             number of elements in the table
     *  ubyte  data[size*width] table of data values (may contain a single-byte
     *                          padding at the end)
     *
     * Total size is 4+(width * size + 1)/2 16-bit code units.
     */
    if (arrayData[0] != kArrayDataSignature) {
        dvmThrowInternalError("bad array data magic");
        return false;
    }

    width = arrayData[1];
    size = arrayData[2] | (((u4)arrayData[3]) << 16);

    if (size > arrayObj->length) {
        dvmThrowArrayIndexOutOfBoundsException(arrayObj->length, size);
        return false;
    }
    copySwappedArrayData(arrayObj->contents, &arrayData[4], size, width);
    return true;
}

/*
 * Find the concrete method that corresponds to "methodIdx".  The code in
 * "method" is executing invoke-method with "thisClass" as its first argument.
 *
 * Returns NULL with an exception raised on failure.
 */
Method* dvmInterpFindInterfaceMethod(ClassObject* thisClass, u4 methodIdx,
    const Method* method, DvmDex* methodClassDex)
{
    Method* absMethod;
    Method* methodToCall;
    int i, vtableIndex;

    /*
     * Resolve the method.  This gives us the abstract method from the
     * interface class declaration.
     */
    absMethod = dvmDexGetResolvedMethod(methodClassDex, methodIdx);
    if (absMethod == NULL) {
        absMethod = dvmResolveInterfaceMethod(method->clazz, methodIdx);
        if (absMethod == NULL) {
            ALOGV("+ unknown method");
            return NULL;
        }
    }

    /* make sure absMethod->methodIndex means what we think it means */
    assert(dvmIsAbstractMethod(absMethod));

    /*
     * Run through the "this" object's iftable.  Find the entry for
     * absMethod's class, then use absMethod->methodIndex to find
     * the method's entry.  The value there is the offset into our
     * vtable of the actual method to execute.
     *
     * The verifier does not guarantee that objects stored into
     * interface references actually implement the interface, so this
     * check cannot be eliminated.
     */
    for (i = 0; i < thisClass->iftableCount; i++) {
        if (thisClass->iftable[i].clazz == absMethod->clazz)
            break;
    }
    if (i == thisClass->iftableCount) {
        /* impossible in verified DEX, need to check for it in unverified */
        dvmThrowIncompatibleClassChangeError("interface not implemented");
        return NULL;
    }

    assert(absMethod->methodIndex <
        thisClass->iftable[i].clazz->virtualMethodCount);

    vtableIndex =
        thisClass->iftable[i].methodIndexArray[absMethod->methodIndex];
    assert(vtableIndex >= 0 && vtableIndex < thisClass->vtableCount);
    methodToCall = thisClass->vtable[vtableIndex];

#if 0
    /* this can happen when there's a stale class file */
    if (dvmIsAbstractMethod(methodToCall)) {
        dvmThrowAbstractMethodError("interface method not implemented");
        return NULL;
    }
#else
    assert(!dvmIsAbstractMethod(methodToCall) ||
        methodToCall->nativeFunc != NULL);
#endif

    LOGVV("+++ interface=%s.%s concrete=%s.%s",
        absMethod->clazz->descriptor, absMethod->name,
        methodToCall->clazz->descriptor, methodToCall->name);
    assert(methodToCall != NULL);

    return methodToCall;
}



/*
 * Helpers for dvmThrowVerificationError().
 *
 * Each returns a newly-allocated string.
 */
#define kThrowShow_accessFromClass     1
static std::string classNameFromIndex(const Method* method, int ref,
    VerifyErrorRefType refType, int flags)
{
    const DvmDex* pDvmDex = method->clazz->pDvmDex;
    if (refType == VERIFY_ERROR_REF_FIELD) {
        /* get class ID from field ID */
        const DexFieldId* pFieldId = dexGetFieldId(pDvmDex->pDexFile, ref);
        ref = pFieldId->classIdx;
    } else if (refType == VERIFY_ERROR_REF_METHOD) {
        /* get class ID from method ID */
        const DexMethodId* pMethodId = dexGetMethodId(pDvmDex->pDexFile, ref);
        ref = pMethodId->classIdx;
    }

    const char* className = dexStringByTypeIdx(pDvmDex->pDexFile, ref);
    std::string dotClassName(dvmHumanReadableDescriptor(className));
    if (flags == 0) {
        return dotClassName;
    }

    std::string result;
    if ((flags & kThrowShow_accessFromClass) != 0) {
        result += "tried to access class " + dotClassName;
        result += " from class " + dvmHumanReadableDescriptor(method->clazz->descriptor);
    } else {
        assert(false);      // should've been caught above
    }

    return result;
}
static std::string fieldNameFromIndex(const Method* method, int ref,
    VerifyErrorRefType refType, int flags)
{
    if (refType != VERIFY_ERROR_REF_FIELD) {
        ALOGW("Expected ref type %d, got %d", VERIFY_ERROR_REF_FIELD, refType);
        return NULL;    /* no message */
    }

    const DvmDex* pDvmDex = method->clazz->pDvmDex;
    const DexFieldId* pFieldId = dexGetFieldId(pDvmDex->pDexFile, ref);
    const char* className = dexStringByTypeIdx(pDvmDex->pDexFile, pFieldId->classIdx);
    const char* fieldName = dexStringById(pDvmDex->pDexFile, pFieldId->nameIdx);

    std::string dotName(dvmHumanReadableDescriptor(className));

    if ((flags & kThrowShow_accessFromClass) != 0) {
        std::string result;
        result += "tried to access field ";
        result += dotName + "." + fieldName;
        result += " from class ";
        result += dvmHumanReadableDescriptor(method->clazz->descriptor);
        return result;
    }
    return dotName + "." + fieldName;
}
static std::string methodNameFromIndex(const Method* method, int ref,
    VerifyErrorRefType refType, int flags)
{
    if (refType != VERIFY_ERROR_REF_METHOD) {
        ALOGW("Expected ref type %d, got %d", VERIFY_ERROR_REF_METHOD,refType);
        return NULL;    /* no message */
    }

    const DvmDex* pDvmDex = method->clazz->pDvmDex;
    const DexMethodId* pMethodId = dexGetMethodId(pDvmDex->pDexFile, ref);
    const char* className = dexStringByTypeIdx(pDvmDex->pDexFile, pMethodId->classIdx);
    const char* methodName = dexStringById(pDvmDex->pDexFile, pMethodId->nameIdx);

    std::string dotName(dvmHumanReadableDescriptor(className));

    if ((flags & kThrowShow_accessFromClass) != 0) {
        char* desc = dexProtoCopyMethodDescriptor(&method->prototype);
        std::string result;
        result += "tried to access method ";
        result += dotName + "." + methodName + ":" + desc;
        result += " from class " + dvmHumanReadableDescriptor(method->clazz->descriptor);
        free(desc);
        return result;
    }
    return dotName + "." + methodName;
}

/*
 * Throw an exception for a problem identified by the verifier.
 *
 * This is used by the invoke-verification-error instruction.  It always
 * throws an exception.
 *
 * "kind" indicates the kind of failure encountered by the verifier.  It
 * has two parts, an error code and an indication of the reference type.
 */
void dvmThrowVerificationError(const Method* method, int kind, int ref)
{
    int errorPart = kind & ~(0xff << kVerifyErrorRefTypeShift);
    int errorRefPart = kind >> kVerifyErrorRefTypeShift;
    VerifyError errorKind = static_cast<VerifyError>(errorPart);
    VerifyErrorRefType refType = static_cast<VerifyErrorRefType>(errorRefPart);
    ClassObject* exceptionClass = gDvm.exVerifyError;
    std::string msg;

    switch ((VerifyError) errorKind) {
    case VERIFY_ERROR_NO_CLASS:
        exceptionClass = gDvm.exNoClassDefFoundError;
        msg = classNameFromIndex(method, ref, refType, 0);
        break;
    case VERIFY_ERROR_NO_FIELD:
        exceptionClass = gDvm.exNoSuchFieldError;
        msg = fieldNameFromIndex(method, ref, refType, 0);
        break;
    case VERIFY_ERROR_NO_METHOD:
        exceptionClass = gDvm.exNoSuchMethodError;
        msg = methodNameFromIndex(method, ref, refType, 0);
        break;
    case VERIFY_ERROR_ACCESS_CLASS:
        exceptionClass = gDvm.exIllegalAccessError;
        msg = classNameFromIndex(method, ref, refType,
            kThrowShow_accessFromClass);
        break;
    case VERIFY_ERROR_ACCESS_FIELD:
        exceptionClass = gDvm.exIllegalAccessError;
        msg = fieldNameFromIndex(method, ref, refType,
            kThrowShow_accessFromClass);
        break;
    case VERIFY_ERROR_ACCESS_METHOD:
        exceptionClass = gDvm.exIllegalAccessError;
        msg = methodNameFromIndex(method, ref, refType,
            kThrowShow_accessFromClass);
        break;
    case VERIFY_ERROR_CLASS_CHANGE:
        exceptionClass = gDvm.exIncompatibleClassChangeError;
        msg = classNameFromIndex(method, ref, refType, 0);
        break;
    case VERIFY_ERROR_INSTANTIATION:
        exceptionClass = gDvm.exInstantiationError;
        msg = classNameFromIndex(method, ref, refType, 0);
        break;

    case VERIFY_ERROR_GENERIC:
        /* generic VerifyError; use default exception, no message */
        break;
    case VERIFY_ERROR_NONE:
        /* should never happen; use default exception */
        assert(false);
        msg = "weird - no error specified";
        break;

    /* no default clause -- want warning if enum updated */
    }

    dvmThrowException(exceptionClass, msg.c_str());
}

/*
 * Update interpBreak for a single thread.
 */
void updateInterpBreak(Thread* thread, ExecutionSubModes subMode, bool enable)
{
    InterpBreak oldValue, newValue;
    do {
        oldValue = newValue = thread->interpBreak;
        newValue.ctl.breakFlags = kInterpNoBreak;  // Assume full reset
        if (enable)
            newValue.ctl.subMode |= subMode;
        else
            newValue.ctl.subMode &= ~subMode;
        if (newValue.ctl.subMode & SINGLESTEP_BREAK_MASK)
            newValue.ctl.breakFlags |= kInterpSingleStep;
        if (newValue.ctl.subMode & SAFEPOINT_BREAK_MASK)
            newValue.ctl.breakFlags |= kInterpSafePoint;
#ifndef DVM_NO_ASM_INTERP
        newValue.ctl.curHandlerTable = (newValue.ctl.breakFlags) ?
            thread->altHandlerTable : thread->mainHandlerTable;
#endif
    } while (dvmQuasiAtomicCas64(oldValue.all, newValue.all,
             &thread->interpBreak.all) != 0);
}

/*
 * Update interpBreak for all threads.
 */
void updateAllInterpBreak(ExecutionSubModes subMode, bool enable)
{
    Thread* self = dvmThreadSelf();
    Thread* thread;

    dvmLockThreadList(self);
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        updateInterpBreak(thread, subMode, enable);
    }
    dvmUnlockThreadList();
}

/*
 * Update the normal and debugger suspend counts for a thread.
 * threadSuspendCount must be acquired before calling this to
 * ensure a clean update of suspendCount, dbgSuspendCount and
 * sumThreadSuspendCount.
 *
 * CLEANUP TODO: Currently only the JIT is using sumThreadSuspendCount.
 * Move under WITH_JIT ifdefs.
*/
void dvmAddToSuspendCounts(Thread* thread, int delta, int dbgDelta)
{
    thread->suspendCount += delta;
    thread->dbgSuspendCount += dbgDelta;
    updateInterpBreak(thread, kSubModeSuspendPending,
                      (thread->suspendCount != 0));
    // Update the global suspend count total
    gDvm.sumThreadSuspendCount += delta;
}


void dvmDisableSubMode(Thread* thread, ExecutionSubModes subMode)
{
    updateInterpBreak(thread, subMode, false);
}

void dvmEnableSubMode(Thread* thread, ExecutionSubModes subMode)
{
    updateInterpBreak(thread, subMode, true);
}

void dvmEnableAllSubMode(ExecutionSubModes subMode)
{
    updateAllInterpBreak(subMode, true);
}

void dvmDisableAllSubMode(ExecutionSubModes subMode)
{
    updateAllInterpBreak(subMode, false);
}

/*
 * Do a sanity check on interpreter state saved to Thread.
 * A failure here doesn't necessarily mean that something is wrong,
 * so this code should only be used during development to suggest
 * a possible problem.
 */
void dvmCheckInterpStateConsistency()
{
    Thread* self = dvmThreadSelf();
    Thread* thread;
    uint8_t breakFlags;
    uint8_t subMode;
#ifndef DVM_NO_ASM_INTERP
    void* handlerTable;
#endif

    dvmLockThreadList(self);
    breakFlags = self->interpBreak.ctl.breakFlags;
    subMode = self->interpBreak.ctl.subMode;
#ifndef DVM_NO_ASM_INTERP
    handlerTable = self->interpBreak.ctl.curHandlerTable;
#endif
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (subMode != thread->interpBreak.ctl.subMode) {
            ALOGD("Warning: subMode mismatch - %#x:%#x, tid[%d]",
                subMode,thread->interpBreak.ctl.subMode,thread->threadId);
         }
        if (breakFlags != thread->interpBreak.ctl.breakFlags) {
            ALOGD("Warning: breakFlags mismatch - %#x:%#x, tid[%d]",
                breakFlags,thread->interpBreak.ctl.breakFlags,thread->threadId);
         }
#ifndef DVM_NO_ASM_INTERP
        if (handlerTable != thread->interpBreak.ctl.curHandlerTable) {
            ALOGD("Warning: curHandlerTable mismatch - %#x:%#x, tid[%d]",
                (int)handlerTable,(int)thread->interpBreak.ctl.curHandlerTable,
                thread->threadId);
         }
#endif
#if defined(WITH_JIT)
         if (thread->pJitProfTable != gDvmJit.pProfTable) {
             ALOGD("Warning: pJitProfTable mismatch - %#x:%#x, tid[%d]",
                  (int)thread->pJitProfTable,(int)gDvmJit.pProfTable,
                  thread->threadId);
         }
         if (thread->jitThreshold != gDvmJit.threshold) {
             ALOGD("Warning: jitThreshold mismatch - %#x:%#x, tid[%d]",
                  (int)thread->jitThreshold,(int)gDvmJit.threshold,
                  thread->threadId);
         }
#endif
    }
    dvmUnlockThreadList();
}

/*
 * Arm a safepoint callback for a thread.  If funct is null,
 * clear any pending callback.
 * TODO: only gc is currently using this feature, and will have
 * at most a single outstanding callback request.  Until we need
 * something more capable and flexible, enforce this limit.
 */
void dvmArmSafePointCallback(Thread* thread, SafePointCallback funct,
                             void* arg)
{
    dvmLockMutex(&thread->callbackMutex);
    if ((funct == NULL) || (thread->callback == NULL)) {
        thread->callback = funct;
        thread->callbackArg = arg;
        if (funct != NULL) {
            dvmEnableSubMode(thread, kSubModeCallbackPending);
        } else {
            dvmDisableSubMode(thread, kSubModeCallbackPending);
        }
    } else {
        // Already armed.  Different?
        if ((funct != thread->callback) ||
            (arg != thread->callbackArg)) {
            // Yes - report failure and die
            ALOGE("ArmSafePointCallback failed, thread %d", thread->threadId);
            dvmUnlockMutex(&thread->callbackMutex);
            dvmAbort();
        }
    }
    dvmUnlockMutex(&thread->callbackMutex);
}

/*
 * One-time initialization at thread creation.  Here we initialize
 * useful constants.
 */
void dvmInitInterpreterState(Thread* self)
{
#if defined(WITH_JIT)
    /*
     * Reserve a static entity here to quickly setup runtime contents as
     * gcc will issue block copy instructions.
     */
    static struct JitToInterpEntries jitToInterpEntries = {
        dvmJitToInterpNormal,
        dvmJitToInterpNoChain,
        dvmJitToInterpPunt,
        dvmJitToInterpSingleStep,
        dvmJitToInterpTraceSelect,
#if defined(WITH_SELF_VERIFICATION)
        dvmJitToInterpBackwardBranch,
#else
        NULL,
#endif
    };
#endif

    // Begin initialization
    self->cardTable = gDvm.biasedCardTableBase;
#if defined(WITH_JIT)
    // One-time initializations
    self->jitToInterpEntries = jitToInterpEntries;
    self->icRechainCount = PREDICTED_CHAIN_COUNTER_RECHAIN;
    self->pProfileCountdown = &gDvmJit.profileCountdown;
    // Jit state that can change
    dvmJitUpdateThreadStateSingle(self);
#endif
    dvmInitializeInterpBreak(self);
}

/*
 * For a newly-created thread, we need to start off with interpBreak
 * set to any existing global modes.  The caller must hold the
 * thread list lock.
 */
void dvmInitializeInterpBreak(Thread* thread)
{
    if (gDvm.instructionCountEnableCount > 0) {
        dvmEnableSubMode(thread, kSubModeInstCounting);
    }
    TracingMode mode = dvmGetMethodTracingMode();
    if (mode != TRACING_INACTIVE) {
        if (mode == SAMPLE_PROFILING_ACTIVE) {
            dvmEnableSubMode(thread, kSubModeSampleTrace);
        } else {
            dvmEnableSubMode(thread, kSubModeMethodTrace);
        }
    }
    if (gDvm.emulatorTraceEnableCount > 0) {
        dvmEnableSubMode(thread, kSubModeEmulatorTrace);
    }
    if (gDvm.debuggerActive) {
        dvmEnableSubMode(thread, kSubModeDebuggerActive);
    }
#if defined(WITH_JIT)
    dvmJitUpdateThreadStateSingle(thread);
#endif
#if 0
    // Debugging stress mode - force checkBefore
    dvmEnableSubMode(thread, kSubModeCheckAlways);
#endif
}

/*
 * Inter-instruction handler invoked in between instruction interpretations
 * to handle exceptional events such as debugging housekeeping, instruction
 * count profiling, JIT trace building, etc.  Dalvik PC has been exported
 * prior to call, but Thread copy of dPC & fp are not current.
 */
void dvmCheckBefore(const u2 *pc, u4 *fp, Thread* self)
{
    const Method* method = self->interpSave.method;
    assert(pc >= method->insns && pc <
           method->insns + dvmGetMethodInsnsSize(method));

#if 0
    /*
     * When we hit a specific method, enable verbose instruction logging.
     * Sometimes it's helpful to use the debugger attach as a trigger too.
     */
    if (*pIsMethodEntry) {
        static const char* cd = "Landroid/test/Arithmetic;";
        static const char* mn = "shiftTest2";
        static const char* sg = "()V";

        if (/*self->interpBreak.ctl.subMode & kSubModeDebuggerActive &&*/
            strcmp(method->clazz->descriptor, cd) == 0 &&
            strcmp(method->name, mn) == 0 &&
            strcmp(method->shorty, sg) == 0)
        {
            ALOGW("Reached %s.%s, enabling verbose mode",
                method->clazz->descriptor, method->name);
            android_setMinPriority(LOG_TAG"i", ANDROID_LOG_VERBOSE);
            dumpRegs(method, fp, true);
        }

        if (!gDvm.debuggerActive)
            *pIsMethodEntry = false;
    }
#endif

    /* Safe point handling */
    if (self->suspendCount ||
        (self->interpBreak.ctl.subMode & kSubModeCallbackPending)) {
        // Are we are a safe point?
        int flags;
        flags = dexGetFlagsFromOpcode(dexOpcodeFromCodeUnit(*pc));
        if (flags & (VERIFY_GC_INST_MASK & ~kInstrCanThrow)) {
            // Yes, at a safe point.  Pending callback?
            if (self->interpBreak.ctl.subMode & kSubModeCallbackPending) {
                SafePointCallback callback;
                void* arg;
                // Get consistent funct/arg pair
                dvmLockMutex(&self->callbackMutex);
                callback = self->callback;
                arg = self->callbackArg;
                dvmUnlockMutex(&self->callbackMutex);
                // Update Thread structure
                self->interpSave.pc = pc;
                self->interpSave.curFrame = fp;
                if (callback != NULL) {
                    // Do the callback
                    if (!callback(self,arg)) {
                        // disarm
                        dvmArmSafePointCallback(self, NULL, NULL);
                    }
                }
            }
            // Need to suspend?
            if (self->suspendCount) {
                dvmExportPC(pc, fp);
                dvmCheckSuspendPending(self);
            }
        }
    }

    if (self->interpBreak.ctl.subMode & kSubModeDebuggerActive) {
        updateDebugger(method, pc, fp, self);
    }
    if (gDvm.instructionCountEnableCount != 0) {
        /*
         * Count up the #of executed instructions.  This isn't synchronized
         * for thread-safety; if we need that we should make this
         * thread-local and merge counts into the global area when threads
         * exit (perhaps suspending all other threads GC-style and pulling
         * the data out of them).
         */
        gDvm.executedInstrCounts[GET_OPCODE(*pc)]++;
    }


#if defined(WITH_TRACKREF_CHECKS)
    dvmInterpCheckTrackedRefs(self, method,
                              self->interpSave.debugTrackedRefStart);
#endif

#if defined(WITH_JIT)
    // Does the JIT need anything done now?
    if (self->interpBreak.ctl.subMode &
            (kSubModeJitTraceBuild | kSubModeJitSV)) {
        // Are we building a trace?
        if (self->interpBreak.ctl.subMode & kSubModeJitTraceBuild) {
            dvmCheckJit(pc, self);
        }

#if defined(WITH_SELF_VERIFICATION)
        // Are we replaying a trace?
        if (self->interpBreak.ctl.subMode & kSubModeJitSV) {
            dvmCheckSelfVerification(pc, self);
        }
#endif
    }
#endif

    /*
     * CountedStep processing.  NOTE: must be the last here to allow
     * preceeding special case handler to manipulate single-step count.
     */
    if (self->interpBreak.ctl.subMode & kSubModeCountedStep) {
        if (self->singleStepCount == 0) {
            // We've exhausted our single step count
            dvmDisableSubMode(self, kSubModeCountedStep);
#if defined(WITH_JIT)
#if 0
            /*
             * For debugging.  If jitResumeDPC is non-zero, then
             * we expect to return to a trace in progress.   There
             * are valid reasons why we wouldn't (such as an exception
             * throw), but here we can keep track.
             */
            if (self->jitResumeDPC != NULL) {
                if (self->jitResumeDPC == pc) {
                    if (self->jitResumeNPC != NULL) {
                        ALOGD("SS return to trace - pc:%#x to 0x:%x",
                             (int)pc, (int)self->jitResumeNPC);
                    } else {
                        ALOGD("SS return to interp - pc:%#x",(int)pc);
                    }
                } else {
                    ALOGD("SS failed to return.  Expected %#x, now at %#x",
                         (int)self->jitResumeDPC, (int)pc);
                }
            }
#endif
#if 0
            // TODO - fix JIT single-stepping resume mode (b/5551114)
            // self->jitResumeNPC needs to be cleared in callPrep

            // If we've got a native return and no other reasons to
            // remain in singlestep/break mode, do a long jump
            if (self->jitResumeNPC != NULL &&
                self->interpBreak.ctl.breakFlags == 0) {
                assert(self->jitResumeDPC == pc);
                self->jitResumeDPC = NULL;
                dvmJitResumeTranslation(self, pc, fp);
                // Doesn't return
                dvmAbort();
            }
            // In case resume is blocked by non-zero breakFlags, clear
            // jitResumeNPC here.
            self->jitResumeNPC = NULL;
            self->jitResumeDPC = NULL;
            self->inJitCodeCache = NULL;
#endif
#endif
        } else {
            self->singleStepCount--;
#if defined(WITH_JIT)
            if ((self->singleStepCount > 0) && (self->jitResumeNPC != NULL)) {
                /*
                 * Direct return to an existing translation following a
                 * single step is valid only if we step once.  If we're
                 * here, an additional step was added so we need to invalidate
                 * the return to translation.
                 */
                self->jitResumeNPC = NULL;
                self->inJitCodeCache = NULL;
            }
#endif
        }
    }
}

/*
 * Main interpreter loop entry point.
 *
 * This begins executing code at the start of "method".  On exit, "pResult"
 * holds the return value of the method (or, if "method" returns NULL, it
 * holds an undefined value).
 *
 * The interpreted stack frame, which holds the method arguments, has
 * already been set up.
 */
void dvmInterpret(Thread* self, const Method* method, JValue* pResult)
{
    InterpSaveState interpSaveState;
    ExecutionSubModes savedSubModes;

#if defined(WITH_JIT)
    /* Target-specific save/restore */
    double calleeSave[JIT_CALLEE_SAVE_DOUBLE_COUNT];
    /*
     * If the previous VM left the code cache through single-stepping the
     * inJitCodeCache flag will be set when the VM is re-entered (for example,
     * in self-verification mode we single-step NEW_INSTANCE which may re-enter
     * the VM through findClassFromLoaderNoInit). Because of that, we cannot
     * assert that self->inJitCodeCache is NULL here.
     */
#endif

    /*
     * Save interpreter state from previous activation, linking
     * new to last.
     */
    interpSaveState = self->interpSave;
    self->interpSave.prev = &interpSaveState;
    /*
     * Strip out and save any flags that should not be inherited by
     * nested interpreter activation.
     */
    savedSubModes = (ExecutionSubModes)(
              self->interpBreak.ctl.subMode & LOCAL_SUBMODE);
    if (savedSubModes != kSubModeNormal) {
        dvmDisableSubMode(self, savedSubModes);
    }
#if defined(WITH_JIT)
    dvmJitCalleeSave(calleeSave);
#endif


#if defined(WITH_TRACKREF_CHECKS)
    self->interpSave.debugTrackedRefStart =
        dvmReferenceTableEntries(&self->internalLocalRefTable);
#endif
    self->debugIsMethodEntry = true;
#if defined(WITH_JIT)
    /* Initialize the state to kJitNot */
    self->jitState = kJitNot;
#endif

    /*
     * Initialize working state.
     *
     * No need to initialize "retval".
     */
    self->interpSave.method = method;
    self->interpSave.curFrame = (u4*) self->interpSave.curFrame;
    self->interpSave.pc = method->insns;

    assert(!dvmIsNativeMethod(method));

    /*
     * Make sure the class is ready to go.  Shouldn't be possible to get
     * here otherwise.
     */
    if (method->clazz->status < CLASS_INITIALIZING ||
        method->clazz->status == CLASS_ERROR)
    {
        ALOGE("ERROR: tried to execute code in unprepared class '%s' (%d)",
            method->clazz->descriptor, method->clazz->status);
        dvmDumpThread(self, false);
        dvmAbort();
    }

    typedef void (*Interpreter)(Thread*);
    Interpreter stdInterp;
    if (gDvm.executionMode == kExecutionModeInterpFast)
        stdInterp = dvmMterpStd;
#if defined(WITH_JIT)
    else if (gDvm.executionMode == kExecutionModeJit ||
             gDvm.executionMode == kExecutionModeNcgO0 ||
             gDvm.executionMode == kExecutionModeNcgO1)
        stdInterp = dvmMterpStd;
#endif
    else
        stdInterp = dvmInterpretPortable;

    // Call the interpreter
    (*stdInterp)(self);

    *pResult = self->interpSave.retval;

    /* Restore interpreter state from previous activation */
    self->interpSave = interpSaveState;
#if defined(WITH_JIT)
    dvmJitCalleeRestore(calleeSave);
#endif
    if (savedSubModes != kSubModeNormal) {
        dvmEnableSubMode(self, savedSubModes);
    }
}
