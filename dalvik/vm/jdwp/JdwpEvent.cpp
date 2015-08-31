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
 * Send events to the debugger.
 */
#include "jdwp/JdwpPriv.h"
#include "jdwp/JdwpConstants.h"
#include "jdwp/JdwpHandler.h"
#include "jdwp/JdwpEvent.h"
#include "jdwp/ExpandBuf.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>     /* for offsetof() */
#include <unistd.h>

/*
General notes:

The event add/remove stuff usually happens from the debugger thread,
in response to requests from the debugger, but can also happen as the
result of an event in an arbitrary thread (e.g. an event with a "count"
mod expires).  It's important to keep the event list locked when processing
events.

Event posting can happen from any thread.  The JDWP thread will not usually
post anything but VM start/death, but if a JDWP request causes a class
to be loaded, the ClassPrepare event will come from the JDWP thread.


We can have serialization issues when we post an event to the debugger.
For example, a thread could send an "I hit a breakpoint and am suspending
myself" message to the debugger.  Before it manages to suspend itself, the
debugger's response ("not interested, resume thread") arrives and is
processed.  We try to resume a thread that hasn't yet suspended.

This means that, after posting an event to the debugger, we need to wait
for the event thread to suspend itself (and, potentially, all other threads)
before processing any additional requests from the debugger.  While doing
so we need to be aware that multiple threads may be hitting breakpoints
or other events simultaneously, so we either need to wait for all of them
or serialize the events with each other.

The current mechanism works like this:
  Event thread:
   - If I'm going to suspend, grab the "I am posting an event" token.  Wait
     for it if it's not currently available.
   - Post the event to the debugger.
   - If appropriate, suspend others and then myself.  As part of suspending
     myself, release the "I am posting" token.
  JDWP thread:
   - When an event arrives, see if somebody is posting an event.  If so,
     sleep until we can acquire the "I am posting an event" token.  Release
     it immediately and continue processing -- the event we have already
     received should not interfere with other events that haven't yet
     been posted.

Some care must be taken to avoid deadlock:

 - thread A and thread B exit near-simultaneously, and post thread-death
   events with a "suspend all" clause
 - thread A gets the event token, thread B sits and waits for it
 - thread A wants to suspend all other threads, but thread B is waiting
   for the token and can't be suspended

So we need to mark thread B in such a way that thread A doesn't wait for it.

If we just bracket the "grab event token" call with a change to VMWAIT
before sleeping, the switch back to RUNNING state when we get the token
will cause thread B to suspend (remember, thread A's global suspend is
still in force, even after it releases the token).  Suspending while
holding the event token is very bad, because it prevents the JDWP thread
from processing incoming messages.

We need to change to VMWAIT state at the *start* of posting an event,
and stay there until we either finish posting the event or decide to
put ourselves to sleep.  That way we don't interfere with anyone else and
don't allow anyone else to interfere with us.
*/


#define kJdwpEventCommandSet    64
#define kJdwpCompositeCommand   100

/*
 * Stuff to compare against when deciding if a mod matches.  Only the
 * values for mods valid for the event being evaluated will be filled in.
 * The rest will be zeroed.
 */
struct ModBasket {
    const JdwpLocation* pLoc;           /* LocationOnly */
    const char*         className;      /* ClassMatch/ClassExclude */
    ObjectId            threadId;       /* ThreadOnly */
    RefTypeId           classId;        /* ClassOnly */
    RefTypeId           excepClassId;   /* ExceptionOnly */
    bool                caught;         /* ExceptionOnly */
    FieldId             field;          /* FieldOnly */
    ObjectId            thisPtr;        /* InstanceOnly */
    /* nothing for StepOnly -- handled differently */
};

/*
 * Get the next "request" serial number.  We use this when sending
 * packets to the debugger.
 */
u4 dvmJdwpNextRequestSerial(JdwpState* state)
{
    dvmDbgLockMutex(&state->serialLock);
    u4 result = state->requestSerial++;
    dvmDbgUnlockMutex(&state->serialLock);

    return result;
}

/*
 * Get the next "event" serial number.  We use this in the response to
 * message type EventRequest.Set.
 */
u4 dvmJdwpNextEventSerial(JdwpState* state)
{
    dvmDbgLockMutex(&state->serialLock);
    u4 result = state->eventSerial++;
    dvmDbgUnlockMutex(&state->serialLock);

    return result;
}

/*
 * Lock the "event" mutex, which guards the list of registered events.
 */
static void lockEventMutex(JdwpState* state)
{
    //dvmDbgThreadWaiting();
    dvmDbgLockMutex(&state->eventLock);
    //dvmDbgThreadRunning();
}

/*
 * Unlock the "event" mutex.
 */
static void unlockEventMutex(JdwpState* state)
{
    dvmDbgUnlockMutex(&state->eventLock);
}

/*
 * Dump an event to the log file.
 */
static void dumpEvent(const JdwpEvent* pEvent)
{
    ALOGI("Event id=0x%4x %p (prev=%p next=%p):",
        pEvent->requestId, pEvent, pEvent->prev, pEvent->next);
    ALOGI("  kind=%s susp=%s modCount=%d",
        dvmJdwpEventKindStr(pEvent->eventKind),
        dvmJdwpSuspendPolicyStr(pEvent->suspendPolicy),
        pEvent->modCount);

    for (int i = 0; i < pEvent->modCount; i++) {
        const JdwpEventMod* pMod = &pEvent->mods[i];
        JdwpModKind kind = static_cast<JdwpModKind>(pMod->modKind);
        ALOGI("  %s", dvmJdwpModKindStr(kind));
        /* TODO - show details */
    }
}

/*
 * Add an event to the list.  Ordering is not important.
 *
 * If something prevents the event from being registered, e.g. it's a
 * single-step request on a thread that doesn't exist, the event will
 * not be added to the list, and an appropriate error will be returned.
 */
JdwpError dvmJdwpRegisterEvent(JdwpState* state, JdwpEvent* pEvent)
{
    lockEventMutex(state);

    assert(state != NULL);
    assert(pEvent != NULL);
    assert(pEvent->prev == NULL);
    assert(pEvent->next == NULL);

    /*
     * If one or more "break"-type mods are used, register them with
     * the interpreter.
     */
    for (int i = 0; i < pEvent->modCount; i++) {
        const JdwpEventMod* pMod = &pEvent->mods[i];
        if (pMod->modKind == MK_LOCATION_ONLY) {
            /* should only be for Breakpoint, Step, and Exception */
            dvmDbgWatchLocation(&pMod->locationOnly.loc);
        } else if (pMod->modKind == MK_STEP) {
            /* should only be for EK_SINGLE_STEP; should only be one */
            JdwpStepSize size = static_cast<JdwpStepSize>(pMod->step.size);
            JdwpStepDepth depth = static_cast<JdwpStepDepth>(pMod->step.depth);
            dvmDbgConfigureStep(pMod->step.threadId, size, depth);
        } else if (pMod->modKind == MK_FIELD_ONLY) {
            /* should be for EK_FIELD_ACCESS or EK_FIELD_MODIFICATION */
            dumpEvent(pEvent);  /* TODO - need for field watches */
        }
    }

    /*
     * Add to list.
     */
    if (state->eventList != NULL) {
        pEvent->next = state->eventList;
        state->eventList->prev = pEvent;
    }
    state->eventList = pEvent;
    state->numEvents++;

    unlockEventMutex(state);

    return ERR_NONE;
}

/*
 * Remove an event from the list.  This will also remove the event from
 * any optimization tables, e.g. breakpoints.
 *
 * Does not free the JdwpEvent.
 *
 * Grab the eventLock before calling here.
 */
static void unregisterEvent(JdwpState* state, JdwpEvent* pEvent)
{
    if (pEvent->prev == NULL) {
        /* head of the list */
        assert(state->eventList == pEvent);

        state->eventList = pEvent->next;
    } else {
        pEvent->prev->next = pEvent->next;
    }

    if (pEvent->next != NULL) {
        pEvent->next->prev = pEvent->prev;
        pEvent->next = NULL;
    }
    pEvent->prev = NULL;

    /*
     * Unhook us from the interpreter, if necessary.
     */
    for (int i = 0; i < pEvent->modCount; i++) {
        JdwpEventMod* pMod = &pEvent->mods[i];
        if (pMod->modKind == MK_LOCATION_ONLY) {
            /* should only be for Breakpoint, Step, and Exception */
            dvmDbgUnwatchLocation(&pMod->locationOnly.loc);
        }
        if (pMod->modKind == MK_STEP) {
            /* should only be for EK_SINGLE_STEP; should only be one */
            dvmDbgUnconfigureStep(pMod->step.threadId);
        }
    }

    state->numEvents--;
    assert(state->numEvents != 0 || state->eventList == NULL);
}

/*
 * Remove the event with the given ID from the list.
 *
 * Failure to find the event isn't really an error, but it is a little
 * weird.  (It looks like Eclipse will try to be extra careful and will
 * explicitly remove one-off single-step events.)
 */
void dvmJdwpUnregisterEventById(JdwpState* state, u4 requestId)
{
    lockEventMutex(state);

    JdwpEvent* pEvent = state->eventList;
    while (pEvent != NULL) {
        if (pEvent->requestId == requestId) {
            unregisterEvent(state, pEvent);
            dvmJdwpEventFree(pEvent);
            goto done;      /* there can be only one with a given ID */
        }

        pEvent = pEvent->next;
    }

    //ALOGD("Odd: no match when removing event reqId=0x%04x", requestId);

done:
    unlockEventMutex(state);
}

/*
 * Remove all entries from the event list.
 */
void dvmJdwpUnregisterAll(JdwpState* state)
{
    lockEventMutex(state);

    JdwpEvent* pEvent = state->eventList;
    while (pEvent != NULL) {
        JdwpEvent* pNextEvent = pEvent->next;

        unregisterEvent(state, pEvent);
        dvmJdwpEventFree(pEvent);
        pEvent = pNextEvent;
    }

    state->eventList = NULL;

    unlockEventMutex(state);
}



/*
 * Allocate a JdwpEvent struct with enough space to hold the specified
 * number of mod records.
 */
JdwpEvent* dvmJdwpEventAlloc(int numMods)
{
    JdwpEvent* newEvent;
    int allocSize = offsetof(JdwpEvent, mods) +
                    numMods * sizeof(newEvent->mods[0]);

    newEvent = (JdwpEvent*)calloc(1, allocSize);
    return newEvent;
}

/*
 * Free a JdwpEvent.
 *
 * Do not call this until the event has been removed from the list.
 */
void dvmJdwpEventFree(JdwpEvent* pEvent)
{
    if (pEvent == NULL)
        return;

    /* make sure it was removed from the list */
    assert(pEvent->prev == NULL);
    assert(pEvent->next == NULL);
    /* want to assert state->eventList != pEvent */

    /*
     * Free any hairy bits in the mods.
     */
    for (int i = 0; i < pEvent->modCount; i++) {
        if (pEvent->mods[i].modKind == MK_CLASS_MATCH) {
            free(pEvent->mods[i].classMatch.classPattern);
            pEvent->mods[i].classMatch.classPattern = NULL;
        }
        if (pEvent->mods[i].modKind == MK_CLASS_EXCLUDE) {
            free(pEvent->mods[i].classExclude.classPattern);
            pEvent->mods[i].classExclude.classPattern = NULL;
        }
    }

    free(pEvent);
}

/*
 * Allocate storage for matching events.  To keep things simple we
 * use an array with enough storage for the entire list.
 *
 * The state->eventLock should be held before calling.
 */
static JdwpEvent** allocMatchList(JdwpState* state)
{
    return (JdwpEvent**) malloc(sizeof(JdwpEvent*) * state->numEvents);
}

/*
 * Run through the list and remove any entries with an expired "count" mod
 * from the event list, then free the match list.
 */
static void cleanupMatchList(JdwpState* state, JdwpEvent** matchList,
    int matchCount)
{
    JdwpEvent** ppEvent = matchList;

    while (matchCount--) {
        JdwpEvent* pEvent = *ppEvent;

        for (int i = 0; i < pEvent->modCount; i++) {
            if (pEvent->mods[i].modKind == MK_COUNT &&
                pEvent->mods[i].count.count == 0)
            {
                ALOGV("##### Removing expired event");
                unregisterEvent(state, pEvent);
                dvmJdwpEventFree(pEvent);
                break;
            }
        }

        ppEvent++;
    }

    free(matchList);
}

/*
 * Match a string against a "restricted regular expression", which is just
 * a string that may start or end with '*' (e.g. "*.Foo" or "java.*").
 *
 * ("Restricted name globbing" might have been a better term.)
 */
static bool patternMatch(const char* pattern, const char* target)
{
    int patLen = strlen(pattern);

    if (pattern[0] == '*') {
        int targetLen = strlen(target);
        patLen--;
        // TODO: remove printf when we find a test case to verify this
        ALOGE(">>> comparing '%s' to '%s'",
            pattern+1, target + (targetLen-patLen));

        if (targetLen < patLen)
            return false;
        return strcmp(pattern+1, target + (targetLen-patLen)) == 0;
    } else if (pattern[patLen-1] == '*') {
        return strncmp(pattern, target, patLen-1) == 0;
    } else {
        return strcmp(pattern, target) == 0;
    }
}

/*
 * See if two locations are equal.
 *
 * It's tempting to do a bitwise compare ("struct ==" or memcmp), but if
 * the storage wasn't zeroed out there could be undefined values in the
 * padding.  Besides, the odds of "idx" being equal while the others aren't
 * is very small, so this is usually just a simple integer comparison.
 */
static inline bool locationMatch(const JdwpLocation* pLoc1,
    const JdwpLocation* pLoc2)
{
    return pLoc1->idx == pLoc2->idx &&
           pLoc1->methodId == pLoc2->methodId &&
           pLoc1->classId == pLoc2->classId &&
           pLoc1->typeTag == pLoc2->typeTag;
}

/*
 * See if the event's mods match up with the contents of "basket".
 *
 * If we find a Count mod before rejecting an event, we decrement it.  We
 * need to do this even if later mods cause us to ignore the event.
 */
static bool modsMatch(JdwpState* state, JdwpEvent* pEvent, ModBasket* basket)
{
    JdwpEventMod* pMod = pEvent->mods;

    for (int i = pEvent->modCount; i > 0; i--, pMod++) {
        switch (pMod->modKind) {
        case MK_COUNT:
            assert(pMod->count.count > 0);
            pMod->count.count--;
            break;
        case MK_CONDITIONAL:
            assert(false);  // should not be getting these
            break;
        case MK_THREAD_ONLY:
            if (pMod->threadOnly.threadId != basket->threadId)
                return false;
            break;
        case MK_CLASS_ONLY:
            if (!dvmDbgMatchType(basket->classId, pMod->classOnly.refTypeId))
                return false;
            break;
        case MK_CLASS_MATCH:
            if (!patternMatch(pMod->classMatch.classPattern,
                    basket->className))
                return false;
            break;
        case MK_CLASS_EXCLUDE:
            if (patternMatch(pMod->classMatch.classPattern,
                    basket->className))
                return false;
            break;
        case MK_LOCATION_ONLY:
            if (!locationMatch(&pMod->locationOnly.loc, basket->pLoc))
                return false;
            break;
        case MK_EXCEPTION_ONLY:
            if (pMod->exceptionOnly.refTypeId != 0 &&
                !dvmDbgMatchType(basket->excepClassId,
                                 pMod->exceptionOnly.refTypeId))
                return false;
            if ((basket->caught && !pMod->exceptionOnly.caught) ||
                (!basket->caught && !pMod->exceptionOnly.uncaught))
                return false;
            break;
        case MK_FIELD_ONLY:
            if (!dvmDbgMatchType(basket->classId, pMod->fieldOnly.refTypeId) ||
                    pMod->fieldOnly.fieldId != basket->field)
                return false;
            break;
        case MK_STEP:
            if (pMod->step.threadId != basket->threadId)
                return false;
            break;
        case MK_INSTANCE_ONLY:
            if (pMod->instanceOnly.objectId != basket->thisPtr)
                return false;
            break;
        default:
            ALOGE("unhandled mod kind %d", pMod->modKind);
            assert(false);
            break;
        }
    }

    return true;
}

/*
 * Find all events of type "eventKind" with mods that match up with the
 * rest of the arguments.
 *
 * Found events are appended to "matchList", and "*pMatchCount" is advanced,
 * so this may be called multiple times for grouped events.
 *
 * DO NOT call this multiple times for the same eventKind, as Count mods are
 * decremented during the scan.
 */
static void findMatchingEvents(JdwpState* state, JdwpEventKind eventKind,
    ModBasket* basket, JdwpEvent** matchList, int* pMatchCount)
{
    /* start after the existing entries */
    matchList += *pMatchCount;

    JdwpEvent* pEvent = state->eventList;
    while (pEvent != NULL) {
        if (pEvent->eventKind == eventKind && modsMatch(state, pEvent, basket))
        {
            *matchList++ = pEvent;
            (*pMatchCount)++;
        }

        pEvent = pEvent->next;
    }
}

/*
 * Scan through the list of matches and determine the most severe
 * suspension policy.
 */
static JdwpSuspendPolicy scanSuspendPolicy(JdwpEvent** matchList,
    int matchCount)
{
    JdwpSuspendPolicy policy = SP_NONE;

    while (matchCount--) {
        if ((*matchList)->suspendPolicy > policy)
            policy = (*matchList)->suspendPolicy;
        matchList++;
    }

    return policy;
}

/*
 * Three possibilities:
 *  SP_NONE - do nothing
 *  SP_EVENT_THREAD - suspend ourselves
 *  SP_ALL - suspend everybody except JDWP support thread
 */
static void suspendByPolicy(JdwpState* state, JdwpSuspendPolicy suspendPolicy)
{
    if (suspendPolicy == SP_NONE)
        return;

    if (suspendPolicy == SP_ALL) {
        dvmDbgSuspendVM(true);
    } else {
        assert(suspendPolicy == SP_EVENT_THREAD);
    }

    /* this is rare but possible -- see CLASS_PREPARE handling */
    if (dvmDbgGetThreadSelfId() == state->debugThreadId) {
        ALOGI("NOTE: suspendByPolicy not suspending JDWP thread");
        return;
    }

    DebugInvokeReq* pReq = dvmDbgGetInvokeReq();
    while (true) {
        pReq->ready = true;
        dvmDbgSuspendSelf();
        pReq->ready = false;

        /*
         * The JDWP thread has told us (and possibly all other threads) to
         * resume.  See if it has left anything in our DebugInvokeReq mailbox.
         */
        if (!pReq->invokeNeeded) {
            /*LOGD("suspendByPolicy: no invoke needed");*/
            break;
        }

        /* grab this before posting/suspending again */
        dvmJdwpSetWaitForEventThread(state, dvmDbgGetThreadSelfId());

        /* leave pReq->invokeNeeded raised so we can check reentrancy */
        ALOGV("invoking method...");
        dvmDbgExecuteMethod(pReq);

        pReq->err = ERR_NONE;

        /* clear this before signaling */
        pReq->invokeNeeded = false;

        ALOGV("invoke complete, signaling and self-suspending");
        dvmDbgLockMutex(&pReq->lock);
        dvmDbgCondSignal(&pReq->cv);
        dvmDbgUnlockMutex(&pReq->lock);
    }
}

/*
 * Determine if there is a method invocation in progress in the current
 * thread.
 *
 * We look at the "invokeNeeded" flag in the per-thread DebugInvokeReq
 * state.  If set, we're in the process of invoking a method.
 */
static bool invokeInProgress(JdwpState* state)
{
    DebugInvokeReq* pReq = dvmDbgGetInvokeReq();
    return pReq->invokeNeeded;
}

/*
 * We need the JDWP thread to hold off on doing stuff while we post an
 * event and then suspend ourselves.
 *
 * Call this with a threadId of zero if you just want to wait for the
 * current thread operation to complete.
 *
 * This could go to sleep waiting for another thread, so it's important
 * that the thread be marked as VMWAIT before calling here.
 */
void dvmJdwpSetWaitForEventThread(JdwpState* state, ObjectId threadId)
{
    bool waited = false;

    /* this is held for very brief periods; contention is unlikely */
    dvmDbgLockMutex(&state->eventThreadLock);

    /*
     * If another thread is already doing stuff, wait for it.  This can
     * go to sleep indefinitely.
     */
    while (state->eventThreadId != 0) {
        ALOGV("event in progress (0x%llx), 0x%llx sleeping",
            state->eventThreadId, threadId);
        waited = true;
        dvmDbgCondWait(&state->eventThreadCond, &state->eventThreadLock);
    }

    if (waited || threadId != 0)
        ALOGV("event token grabbed (0x%llx)", threadId);
    if (threadId != 0)
        state->eventThreadId = threadId;

    dvmDbgUnlockMutex(&state->eventThreadLock);
}

/*
 * Clear the threadId and signal anybody waiting.
 */
void dvmJdwpClearWaitForEventThread(JdwpState* state)
{
    /*
     * Grab the mutex.  Don't try to go in/out of VMWAIT mode, as this
     * function is called by dvmSuspendSelf(), and the transition back
     * to RUNNING would confuse it.
     */
    dvmDbgLockMutex(&state->eventThreadLock);

    assert(state->eventThreadId != 0);
    ALOGV("cleared event token (0x%llx)", state->eventThreadId);

    state->eventThreadId = 0;

    dvmDbgCondSignal(&state->eventThreadCond);

    dvmDbgUnlockMutex(&state->eventThreadLock);
}


/*
 * Prep an event.  Allocates storage for the message and leaves space for
 * the header.
 */
static ExpandBuf* eventPrep()
{
    ExpandBuf* pReq = expandBufAlloc();
    expandBufAddSpace(pReq, kJDWPHeaderLen);

    return pReq;
}

/*
 * Write the header into the buffer and send the packet off to the debugger.
 *
 * Takes ownership of "pReq" (currently discards it).
 */
static void eventFinish(JdwpState* state, ExpandBuf* pReq)
{
    u1* buf = expandBufGetBuffer(pReq);

    set4BE(buf, expandBufGetLength(pReq));
    set4BE(buf+4, dvmJdwpNextRequestSerial(state));
    set1(buf+8, 0);     /* flags */
    set1(buf+9, kJdwpEventCommandSet);
    set1(buf+10, kJdwpCompositeCommand);

    dvmJdwpSendRequest(state, pReq);

    expandBufFree(pReq);
}


/*
 * Tell the debugger that we have finished initializing.  This is always
 * sent, even if the debugger hasn't requested it.
 *
 * This should be sent "before the main thread is started and before
 * any application code has been executed".  The thread ID in the message
 * must be for the main thread.
 */
bool dvmJdwpPostVMStart(JdwpState* state, bool suspend)
{
    JdwpSuspendPolicy suspendPolicy;
    ObjectId threadId = dvmDbgGetThreadSelfId();

    if (suspend)
        suspendPolicy = SP_ALL;
    else
        suspendPolicy = SP_NONE;

    /* probably don't need this here */
    lockEventMutex(state);

    ExpandBuf* pReq = NULL;
    if (true) {
        ALOGV("EVENT: %s", dvmJdwpEventKindStr(EK_VM_START));
        ALOGV("  suspendPolicy=%s", dvmJdwpSuspendPolicyStr(suspendPolicy));

        pReq = eventPrep();
        expandBufAdd1(pReq, suspendPolicy);
        expandBufAdd4BE(pReq, 1);

        expandBufAdd1(pReq, EK_VM_START);
        expandBufAdd4BE(pReq, 0);       /* requestId */
        expandBufAdd8BE(pReq, threadId);
    }

    unlockEventMutex(state);

    /* send request and possibly suspend ourselves */
    if (pReq != NULL) {
        int oldStatus = dvmDbgThreadWaiting();
        if (suspendPolicy != SP_NONE)
            dvmJdwpSetWaitForEventThread(state, threadId);

        eventFinish(state, pReq);

        suspendByPolicy(state, suspendPolicy);
        dvmDbgThreadContinuing(oldStatus);
    }

    return true;
}

/*
 * A location of interest has been reached.  This handles:
 *   Breakpoint
 *   SingleStep
 *   MethodEntry
 *   MethodExit
 * These four types must be grouped together in a single response.  The
 * "eventFlags" indicates the type of event(s) that have happened.
 *
 * Valid mods:
 *   Count, ThreadOnly, ClassOnly, ClassMatch, ClassExclude, InstanceOnly
 *   LocationOnly (for breakpoint/step only)
 *   Step (for step only)
 *
 * Interesting test cases:
 *  - Put a breakpoint on a native method.  Eclipse creates METHOD_ENTRY
 *    and METHOD_EXIT events with a ClassOnly mod on the method's class.
 *  - Use "run to line".  Eclipse creates a BREAKPOINT with Count=1.
 *  - Single-step to a line with a breakpoint.  Should get a single
 *    event message with both events in it.
 */
bool dvmJdwpPostLocationEvent(JdwpState* state, const JdwpLocation* pLoc,
    ObjectId thisPtr, int eventFlags)
{
    JdwpSuspendPolicy suspendPolicy = SP_NONE;
    ModBasket basket;
    char* nameAlloc = NULL;

    memset(&basket, 0, sizeof(basket));
    basket.pLoc = pLoc;
    basket.classId = pLoc->classId;
    basket.thisPtr = thisPtr;
    basket.threadId = dvmDbgGetThreadSelfId();
    basket.className = nameAlloc =
        dvmDescriptorToName(dvmDbgGetClassDescriptor(pLoc->classId));

    /*
     * On rare occasions we may need to execute interpreted code in the VM
     * while handling a request from the debugger.  Don't fire breakpoints
     * while doing so.  (I don't think we currently do this at all, so
     * this is mostly paranoia.)
     */
    if (basket.threadId == state->debugThreadId) {
        ALOGV("Ignoring location event in JDWP thread");
        free(nameAlloc);
        return false;
    }

    /*
     * The debugger variable display tab may invoke the interpreter to format
     * complex objects.  We want to ignore breakpoints and method entry/exit
     * traps while working on behalf of the debugger.
     *
     * If we don't ignore them, the VM will get hung up, because we'll
     * suspend on a breakpoint while the debugger is still waiting for its
     * method invocation to complete.
     */
    if (invokeInProgress(state)) {
        ALOGV("Not checking breakpoints during invoke (%s)", basket.className);
        free(nameAlloc);
        return false;
    }

    /* don't allow the list to be updated while we scan it */
    lockEventMutex(state);

    JdwpEvent** matchList = allocMatchList(state);
    int matchCount = 0;

    if ((eventFlags & DBG_BREAKPOINT) != 0)
        findMatchingEvents(state, EK_BREAKPOINT, &basket, matchList,
            &matchCount);
    if ((eventFlags & DBG_SINGLE_STEP) != 0)
        findMatchingEvents(state, EK_SINGLE_STEP, &basket, matchList,
            &matchCount);
    if ((eventFlags & DBG_METHOD_ENTRY) != 0)
        findMatchingEvents(state, EK_METHOD_ENTRY, &basket, matchList,
            &matchCount);
    if ((eventFlags & DBG_METHOD_EXIT) != 0)
        findMatchingEvents(state, EK_METHOD_EXIT, &basket, matchList,
            &matchCount);

    ExpandBuf* pReq = NULL;
    if (matchCount != 0) {
        ALOGV("EVENT: %s(%d total) %s.%s thread=%llx code=%llx)",
            dvmJdwpEventKindStr(matchList[0]->eventKind), matchCount,
            basket.className,
            dvmDbgGetMethodName(pLoc->classId, pLoc->methodId),
            basket.threadId, pLoc->idx);

        suspendPolicy = scanSuspendPolicy(matchList, matchCount);
        ALOGV("  suspendPolicy=%s",
            dvmJdwpSuspendPolicyStr(suspendPolicy));

        pReq = eventPrep();
        expandBufAdd1(pReq, suspendPolicy);
        expandBufAdd4BE(pReq, matchCount);

        for (int i = 0; i < matchCount; i++) {
            expandBufAdd1(pReq, matchList[i]->eventKind);
            expandBufAdd4BE(pReq, matchList[i]->requestId);
            expandBufAdd8BE(pReq, basket.threadId);
            dvmJdwpAddLocation(pReq, pLoc);
        }
    }

    cleanupMatchList(state, matchList, matchCount);
    unlockEventMutex(state);

    /* send request and possibly suspend ourselves */
    if (pReq != NULL) {
        int oldStatus = dvmDbgThreadWaiting();
        if (suspendPolicy != SP_NONE)
            dvmJdwpSetWaitForEventThread(state, basket.threadId);

        eventFinish(state, pReq);

        suspendByPolicy(state, suspendPolicy);
        dvmDbgThreadContinuing(oldStatus);
    }

    free(nameAlloc);
    return matchCount != 0;
}

/*
 * A thread is starting or stopping.
 *
 * Valid mods:
 *  Count, ThreadOnly
 */
bool dvmJdwpPostThreadChange(JdwpState* state, ObjectId threadId, bool start)
{
    JdwpSuspendPolicy suspendPolicy = SP_NONE;

    assert(threadId == dvmDbgGetThreadSelfId());

    /*
     * I don't think this can happen.
     */
    if (invokeInProgress(state)) {
        ALOGW("Not posting thread change during invoke");
        return false;
    }

    ModBasket basket;
    memset(&basket, 0, sizeof(basket));
    basket.threadId = threadId;

    /* don't allow the list to be updated while we scan it */
    lockEventMutex(state);

    JdwpEvent** matchList = allocMatchList(state);
    int matchCount = 0;

    if (start)
        findMatchingEvents(state, EK_THREAD_START, &basket, matchList,
            &matchCount);
    else
        findMatchingEvents(state, EK_THREAD_DEATH, &basket, matchList,
            &matchCount);

    ExpandBuf* pReq = NULL;
    if (matchCount != 0) {
        ALOGV("EVENT: %s(%d total) thread=%llx)",
            dvmJdwpEventKindStr(matchList[0]->eventKind), matchCount,
            basket.threadId);

        suspendPolicy = scanSuspendPolicy(matchList, matchCount);
        ALOGV("  suspendPolicy=%s",
            dvmJdwpSuspendPolicyStr(suspendPolicy));

        pReq = eventPrep();
        expandBufAdd1(pReq, suspendPolicy);
        expandBufAdd4BE(pReq, matchCount);

        for (int i = 0; i < matchCount; i++) {
            expandBufAdd1(pReq, matchList[i]->eventKind);
            expandBufAdd4BE(pReq, matchList[i]->requestId);
            expandBufAdd8BE(pReq, basket.threadId);
        }

    }

    cleanupMatchList(state, matchList, matchCount);
    unlockEventMutex(state);

    /* send request and possibly suspend ourselves */
    if (pReq != NULL) {
        int oldStatus = dvmDbgThreadWaiting();
        if (suspendPolicy != SP_NONE)
            dvmJdwpSetWaitForEventThread(state, basket.threadId);

        eventFinish(state, pReq);

        suspendByPolicy(state, suspendPolicy);
        dvmDbgThreadContinuing(oldStatus);
    }

    return matchCount != 0;
}

/*
 * Send a polite "VM is dying" message to the debugger.
 *
 * Skips the usual "event token" stuff.
 */
bool dvmJdwpPostVMDeath(JdwpState* state)
{
    ALOGV("EVENT: %s", dvmJdwpEventKindStr(EK_VM_DEATH));

    ExpandBuf* pReq = eventPrep();
    expandBufAdd1(pReq, SP_NONE);
    expandBufAdd4BE(pReq, 1);

    expandBufAdd1(pReq, EK_VM_DEATH);
    expandBufAdd4BE(pReq, 0);
    eventFinish(state, pReq);
    return true;
}


/*
 * An exception has been thrown.  It may or may not have been caught.
 *
 * Valid mods:
 *  Count, ThreadOnly, ClassOnly, ClassMatch, ClassExclude, LocationOnly,
 *    ExceptionOnly, InstanceOnly
 *
 * The "exceptionId" has not been added to the GC-visible object registry,
 * because there's a pretty good chance that we're not going to send it
 * up the debugger.
 */
bool dvmJdwpPostException(JdwpState* state, const JdwpLocation* pThrowLoc,
    ObjectId exceptionId, RefTypeId exceptionClassId,
    const JdwpLocation* pCatchLoc, ObjectId thisPtr)
{
    JdwpSuspendPolicy suspendPolicy = SP_NONE;
    ModBasket basket;
    char* nameAlloc = NULL;

    memset(&basket, 0, sizeof(basket));
    basket.pLoc = pThrowLoc;
    basket.classId = pThrowLoc->classId;
    basket.threadId = dvmDbgGetThreadSelfId();
    basket.className = nameAlloc =
        dvmDescriptorToName(dvmDbgGetClassDescriptor(basket.classId));
    basket.excepClassId = exceptionClassId;
    basket.caught = (pCatchLoc->classId != 0);
    basket.thisPtr = thisPtr;

    /* don't try to post an exception caused by the debugger */
    if (invokeInProgress(state)) {
        ALOGV("Not posting exception hit during invoke (%s)",basket.className);
        free(nameAlloc);
        return false;
    }

    /* don't allow the list to be updated while we scan it */
    lockEventMutex(state);

    JdwpEvent** matchList = allocMatchList(state);
    int matchCount = 0;

    findMatchingEvents(state, EK_EXCEPTION, &basket, matchList, &matchCount);

    ExpandBuf* pReq = NULL;
    if (matchCount != 0) {
        ALOGV("EVENT: %s(%d total) thread=%llx exceptId=%llx caught=%d)",
            dvmJdwpEventKindStr(matchList[0]->eventKind), matchCount,
            basket.threadId, exceptionId, basket.caught);
        ALOGV("  throw: %d %llx %x %lld (%s.%s)", pThrowLoc->typeTag,
            pThrowLoc->classId, pThrowLoc->methodId, pThrowLoc->idx,
            dvmDbgGetClassDescriptor(pThrowLoc->classId),
            dvmDbgGetMethodName(pThrowLoc->classId, pThrowLoc->methodId));
        if (pCatchLoc->classId == 0) {
            ALOGV("  catch: (not caught)");
        } else {
            ALOGV("  catch: %d %llx %x %lld (%s.%s)", pCatchLoc->typeTag,
                pCatchLoc->classId, pCatchLoc->methodId, pCatchLoc->idx,
                dvmDbgGetClassDescriptor(pCatchLoc->classId),
                dvmDbgGetMethodName(pCatchLoc->classId, pCatchLoc->methodId));
        }

        suspendPolicy = scanSuspendPolicy(matchList, matchCount);
        ALOGV("  suspendPolicy=%s",
            dvmJdwpSuspendPolicyStr(suspendPolicy));

        pReq = eventPrep();
        expandBufAdd1(pReq, suspendPolicy);
        expandBufAdd4BE(pReq, matchCount);

        for (int i = 0; i < matchCount; i++) {
            expandBufAdd1(pReq, matchList[i]->eventKind);
            expandBufAdd4BE(pReq, matchList[i]->requestId);
            expandBufAdd8BE(pReq, basket.threadId);

            dvmJdwpAddLocation(pReq, pThrowLoc);
            expandBufAdd1(pReq, JT_OBJECT);
            expandBufAdd8BE(pReq, exceptionId);
            dvmJdwpAddLocation(pReq, pCatchLoc);
        }

        /* don't let the GC discard it */
        dvmDbgRegisterObjectId(exceptionId);
    }

    cleanupMatchList(state, matchList, matchCount);
    unlockEventMutex(state);

    /* send request and possibly suspend ourselves */
    if (pReq != NULL) {
        int oldStatus = dvmDbgThreadWaiting();
        if (suspendPolicy != SP_NONE)
            dvmJdwpSetWaitForEventThread(state, basket.threadId);

        eventFinish(state, pReq);

        suspendByPolicy(state, suspendPolicy);
        dvmDbgThreadContinuing(oldStatus);
    }

    free(nameAlloc);
    return matchCount != 0;
}

/*
 * Announce that a class has been loaded.
 *
 * Valid mods:
 *  Count, ThreadOnly, ClassOnly, ClassMatch, ClassExclude
 */
bool dvmJdwpPostClassPrepare(JdwpState* state, int tag, RefTypeId refTypeId,
    const char* signature, int status)
{
    JdwpSuspendPolicy suspendPolicy = SP_NONE;
    ModBasket basket;
    char* nameAlloc = NULL;

    memset(&basket, 0, sizeof(basket));
    basket.classId = refTypeId;
    basket.threadId = dvmDbgGetThreadSelfId();
    basket.className = nameAlloc =
        dvmDescriptorToName(dvmDbgGetClassDescriptor(basket.classId));

    /* suppress class prep caused by debugger */
    if (invokeInProgress(state)) {
        ALOGV("Not posting class prep caused by invoke (%s)",basket.className);
        free(nameAlloc);
        return false;
    }

    /* don't allow the list to be updated while we scan it */
    lockEventMutex(state);

    JdwpEvent** matchList = allocMatchList(state);
    int matchCount = 0;

    findMatchingEvents(state, EK_CLASS_PREPARE, &basket, matchList,
        &matchCount);

    ExpandBuf* pReq = NULL;
    if (matchCount != 0) {
        ALOGV("EVENT: %s(%d total) thread=%llx)",
            dvmJdwpEventKindStr(matchList[0]->eventKind), matchCount,
            basket.threadId);

        suspendPolicy = scanSuspendPolicy(matchList, matchCount);
        ALOGV("  suspendPolicy=%s",
            dvmJdwpSuspendPolicyStr(suspendPolicy));

        if (basket.threadId == state->debugThreadId) {
            /*
             * JDWP says that, for a class prep in the debugger thread, we
             * should set threadId to null and if any threads were supposed
             * to be suspended then we suspend all other threads.
             */
            ALOGV("  NOTE: class prepare in debugger thread!");
            basket.threadId = 0;
            if (suspendPolicy == SP_EVENT_THREAD)
                suspendPolicy = SP_ALL;
        }

        pReq = eventPrep();
        expandBufAdd1(pReq, suspendPolicy);
        expandBufAdd4BE(pReq, matchCount);

        for (int i = 0; i < matchCount; i++) {
            expandBufAdd1(pReq, matchList[i]->eventKind);
            expandBufAdd4BE(pReq, matchList[i]->requestId);
            expandBufAdd8BE(pReq, basket.threadId);

            expandBufAdd1(pReq, tag);
            expandBufAdd8BE(pReq, refTypeId);
            expandBufAddUtf8String(pReq, (const u1*) signature);
            expandBufAdd4BE(pReq, status);
        }
    }

    cleanupMatchList(state, matchList, matchCount);

    unlockEventMutex(state);

    /* send request and possibly suspend ourselves */
    if (pReq != NULL) {
        int oldStatus = dvmDbgThreadWaiting();
        if (suspendPolicy != SP_NONE)
            dvmJdwpSetWaitForEventThread(state, basket.threadId);

        eventFinish(state, pReq);

        suspendByPolicy(state, suspendPolicy);
        dvmDbgThreadContinuing(oldStatus);
    }

    free(nameAlloc);
    return matchCount != 0;
}

/*
 * Unload a class.
 *
 * Valid mods:
 *  Count, ClassMatch, ClassExclude
 */
bool dvmJdwpPostClassUnload(JdwpState* state, RefTypeId refTypeId)
{
    assert(false);      // TODO
    return false;
}

/*
 * Get or set a field.
 *
 * Valid mods:
 *  Count, ThreadOnly, ClassOnly, ClassMatch, ClassExclude, FieldOnly,
 *    InstanceOnly
 */
bool dvmJdwpPostFieldAccess(JdwpState* state, int STUFF, ObjectId thisPtr,
    bool modified, JValue newValue)
{
    assert(false);      // TODO
    return false;
}

/*
 * Send up a chunk of DDM data.
 *
 * While this takes the form of a JDWP "event", it doesn't interact with
 * other debugger traffic, and can't suspend the VM, so we skip all of
 * the fun event token gymnastics.
 */
void dvmJdwpDdmSendChunkV(JdwpState* state, int type, const struct iovec* iov,
    int iovcnt)
{
    u1 header[kJDWPHeaderLen + 8];
    size_t dataLen = 0;

    assert(iov != NULL);
    assert(iovcnt > 0 && iovcnt < 10);

    /*
     * "Wrap" the contents of the iovec with a JDWP/DDMS header.  We do
     * this by creating a new copy of the vector with space for the header.
     */
    struct iovec wrapiov[iovcnt+1];
    for (int i = 0; i < iovcnt; i++) {
        wrapiov[i+1].iov_base = iov[i].iov_base;
        wrapiov[i+1].iov_len = iov[i].iov_len;
        dataLen += iov[i].iov_len;
    }

    /* form the header (JDWP plus DDMS) */
    set4BE(header, sizeof(header) + dataLen);
    set4BE(header+4, dvmJdwpNextRequestSerial(state));
    set1(header+8, 0);     /* flags */
    set1(header+9, kJDWPDdmCmdSet);
    set1(header+10, kJDWPDdmCmd);
    set4BE(header+11, type);
    set4BE(header+15, dataLen);

    wrapiov[0].iov_base = header;
    wrapiov[0].iov_len = sizeof(header);

    /*
     * Make sure we're in VMWAIT in case the write blocks.
     */
    int oldStatus = dvmDbgThreadWaiting();
    dvmJdwpSendBufferedRequest(state, wrapiov, iovcnt+1);
    dvmDbgThreadContinuing(oldStatus);
}
