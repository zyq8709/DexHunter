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
 * JDWP initialization.
 */
#include "jdwp/JdwpPriv.h"
#include "Dalvik.h"
#include "Atomic.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>


static void* jdwpThreadStart(void* arg);

/*
 * JdwpNetStateBase class implementation
 */
JdwpNetStateBase::JdwpNetStateBase()
{
    clientSock = -1;
    dvmDbgInitMutex(&socketLock);
}

/*
 * Write a packet. Grabs a mutex to assure atomicity.
 */
ssize_t JdwpNetStateBase::writePacket(ExpandBuf* pReply)
{
    dvmDbgLockMutex(&socketLock);
    ssize_t cc = TEMP_FAILURE_RETRY(write(clientSock, expandBufGetBuffer(pReply),
                                          expandBufGetLength(pReply)));
    dvmDbgUnlockMutex(&socketLock);

    return cc;
}

/*
 * Write a buffered packet. Grabs a mutex to assure atomicity.
 */
ssize_t JdwpNetStateBase::writeBufferedPacket(const struct iovec* iov,
    int iovcnt)
{
    dvmDbgLockMutex(&socketLock);
    ssize_t actual = TEMP_FAILURE_RETRY(writev(clientSock, iov, iovcnt));
    dvmDbgUnlockMutex(&socketLock);

    return actual;
}

/*
 * Initialize JDWP.
 *
 * Does not return until JDWP thread is running, but may return before
 * the thread is accepting network connections.
 */
JdwpState* dvmJdwpStartup(const JdwpStartupParams* pParams)
{
    JdwpState* state = NULL;

    /* comment this out when debugging JDWP itself */
    android_setMinPriority(LOG_TAG, ANDROID_LOG_DEBUG);

    state = (JdwpState*) calloc(1, sizeof(JdwpState));

    state->params = *pParams;

    state->requestSerial = 0x10000000;
    state->eventSerial = 0x20000000;
    dvmDbgInitMutex(&state->threadStartLock);
    dvmDbgInitMutex(&state->attachLock);
    dvmDbgInitMutex(&state->serialLock);
    dvmDbgInitMutex(&state->eventLock);
    state->eventThreadId = 0;
    dvmDbgInitMutex(&state->eventThreadLock);
    dvmDbgInitCond(&state->threadStartCond);
    dvmDbgInitCond(&state->attachCond);
    dvmDbgInitCond(&state->eventThreadCond);

    switch (pParams->transport) {
    case kJdwpTransportSocket:
        // ALOGD("prepping for JDWP over TCP");
        state->transport = dvmJdwpSocketTransport();
        break;
    case kJdwpTransportAndroidAdb:
        // ALOGD("prepping for JDWP over ADB");
        state->transport = dvmJdwpAndroidAdbTransport();
        /* TODO */
        break;
    default:
        ALOGE("Unknown transport %d", pParams->transport);
        assert(false);
        goto fail;
    }

    if (!dvmJdwpNetStartup(state, pParams))
        goto fail;

    /*
     * Grab a mutex or two before starting the thread.  This ensures they
     * won't signal the cond var before we're waiting.
     */
    dvmDbgLockMutex(&state->threadStartLock);
    if (pParams->suspend)
        dvmDbgLockMutex(&state->attachLock);

    /*
     * We have bound to a port, or are trying to connect outbound to a
     * debugger.  Create the JDWP thread and let it continue the mission.
     */
    if (!dvmCreateInternalThread(&state->debugThreadHandle, "JDWP",
            jdwpThreadStart, state))
    {
        /* state is getting tossed, but unlock these anyway for cleanliness */
        dvmDbgUnlockMutex(&state->threadStartLock);
        if (pParams->suspend)
            dvmDbgUnlockMutex(&state->attachLock);
        goto fail;
    }

    /*
     * Wait until the thread finishes basic initialization.
     * TODO: cond vars should be waited upon in a loop
     */
    dvmDbgCondWait(&state->threadStartCond, &state->threadStartLock);
    dvmDbgUnlockMutex(&state->threadStartLock);


    /*
     * For suspend=y, wait for the debugger to connect to us or for us to
     * connect to the debugger.
     *
     * The JDWP thread will signal us when it connects successfully or
     * times out (for timeout=xxx), so we have to check to see what happened
     * when we wake up.
     */
    if (pParams->suspend) {
        dvmChangeStatus(NULL, THREAD_VMWAIT);
        dvmDbgCondWait(&state->attachCond, &state->attachLock);
        dvmDbgUnlockMutex(&state->attachLock);
        dvmChangeStatus(NULL, THREAD_RUNNING);

        if (!dvmJdwpIsActive(state)) {
            ALOGE("JDWP connection failed");
            goto fail;
        }

        ALOGI("JDWP connected");

        /*
         * Ordinarily we would pause briefly to allow the debugger to set
         * breakpoints and so on, but for "suspend=y" the VM init code will
         * pause the VM when it sends the VM_START message.
         */
    }

    return state;

fail:
    dvmJdwpShutdown(state);     // frees state
    return NULL;
}

/*
 * Reset all session-related state.  There should not be an active connection
 * to the client at this point.  The rest of the VM still thinks there is
 * a debugger attached.
 *
 * This includes freeing up the debugger event list.
 */
void dvmJdwpResetState(JdwpState* state)
{
    /* could reset the serial numbers, but no need to */

    dvmJdwpUnregisterAll(state);
    assert(state->eventList == NULL);

    /*
     * Should not have one of these in progress.  If the debugger went away
     * mid-request, though, we could see this.
     */
    if (state->eventThreadId != 0) {
        ALOGW("WARNING: resetting state while event in progress");
        assert(false);
    }
}

/*
 * Tell the JDWP thread to shut down.  Frees "state".
 */
void dvmJdwpShutdown(JdwpState* state)
{
    void* threadReturn;

    if (state == NULL)
        return;

    if (dvmJdwpIsTransportDefined(state)) {
        if (dvmJdwpIsConnected(state))
            dvmJdwpPostVMDeath(state);

        /*
         * Close down the network to inspire the thread to halt.
         */
        if (gDvm.verboseShutdown)
            ALOGD("JDWP shutting down net...");
        dvmJdwpNetShutdown(state);

        if (state->debugThreadStarted) {
            state->run = false;
            if (pthread_join(state->debugThreadHandle, &threadReturn) != 0) {
                ALOGW("JDWP thread join failed");
            }
        }

        if (gDvm.verboseShutdown)
            ALOGD("JDWP freeing netstate...");
        dvmJdwpNetFree(state);
        state->netState = NULL;
    }
    assert(state->netState == NULL);

    dvmJdwpResetState(state);
    free(state);
}

/*
 * Are we talking to a debugger?
 */
bool dvmJdwpIsActive(JdwpState* state)
{
    return dvmJdwpIsConnected(state);
}

/*
 * Entry point for JDWP thread.  The thread was created through the VM
 * mechanisms, so there is a java/lang/Thread associated with us.
 */
static void* jdwpThreadStart(void* arg)
{
    JdwpState* state = (JdwpState*) arg;

    ALOGV("JDWP: thread running");

    /*
     * Finish initializing "state", then notify the creating thread that
     * we're running.
     */
    state->debugThreadHandle = dvmThreadSelf()->handle;
    state->run = true;
    android_atomic_release_store(true, &state->debugThreadStarted);

    dvmDbgLockMutex(&state->threadStartLock);
    dvmDbgCondBroadcast(&state->threadStartCond);
    dvmDbgUnlockMutex(&state->threadStartLock);

    /* set the thread state to VMWAIT so GCs don't wait for us */
    dvmDbgThreadWaiting();

    /*
     * Loop forever if we're in server mode, processing connections.  In
     * non-server mode, we bail out of the thread when the debugger drops
     * us.
     *
     * We broadcast a notification when a debugger attaches, after we
     * successfully process the handshake.
     */
    while (state->run) {
        bool first;

        if (state->params.server) {
            /*
             * Block forever, waiting for a connection.  To support the
             * "timeout=xxx" option we'll need to tweak this.
             */
            if (!dvmJdwpAcceptConnection(state))
                break;
        } else {
            /*
             * If we're not acting as a server, we need to connect out to the
             * debugger.  To support the "timeout=xxx" option we need to
             * have a timeout if the handshake reply isn't received in a
             * reasonable amount of time.
             */
            if (!dvmJdwpEstablishConnection(state)) {
                /* wake anybody who was waiting for us to succeed */
                dvmDbgLockMutex(&state->attachLock);
                dvmDbgCondBroadcast(&state->attachCond);
                dvmDbgUnlockMutex(&state->attachLock);
                break;
            }
        }

        /* prep debug code to handle the new connection */
        dvmDbgConnected();

        /* process requests until the debugger drops */
        first = true;
        while (true) {
            // sanity check -- shouldn't happen?
            if (dvmThreadSelf()->status != THREAD_VMWAIT) {
                ALOGE("JDWP thread no longer in VMWAIT (now %d); resetting",
                    dvmThreadSelf()->status);
                dvmDbgThreadWaiting();
            }

            if (!dvmJdwpProcessIncoming(state))     /* blocking read */
                break;

            if (first && !dvmJdwpAwaitingHandshake(state)) {
                /* handshake worked, tell the interpreter that we're active */
                first = false;

                /* set thread ID; requires object registry to be active */
                state->debugThreadId = dvmDbgGetThreadSelfId();

                /* wake anybody who's waiting for us */
                dvmDbgLockMutex(&state->attachLock);
                dvmDbgCondBroadcast(&state->attachCond);
                dvmDbgUnlockMutex(&state->attachLock);
            }
        }

        dvmJdwpCloseConnection(state);

        if (state->ddmActive) {
            state->ddmActive = false;

            /* broadcast the disconnect; must be in RUNNING state */
            dvmDbgThreadRunning();
            dvmDbgDdmDisconnected();
            dvmDbgThreadWaiting();
        }

        /* release session state, e.g. remove breakpoint instructions */
        dvmJdwpResetState(state);

        /* tell the interpreter that the debugger is no longer around */
        dvmDbgDisconnected();

        /* if we had threads suspended, resume them now */
        dvmUndoDebuggerSuspensions();

        /* if we connected out, this was a one-shot deal */
        if (!state->params.server)
            state->run = false;
    }

    /* back to running, for thread shutdown */
    dvmDbgThreadRunning();

    ALOGV("JDWP: thread exiting");
    return NULL;
}


/*
 * Return the thread handle, or (pthread_t)0 if the debugger isn't running.
 */
pthread_t dvmJdwpGetDebugThread(JdwpState* state)
{
    if (state == NULL)
        return 0;

    return state->debugThreadHandle;
}


/*
 * Support routines for waitForDebugger().
 *
 * We can't have a trivial "waitForDebugger" function that returns the
 * instant the debugger connects, because we run the risk of executing code
 * before the debugger has had a chance to configure breakpoints or issue
 * suspend calls.  It would be nice to just sit in the suspended state, but
 * most debuggers don't expect any threads to be suspended when they attach.
 *
 * There's no JDWP event we can post to tell the debugger, "we've stopped,
 * and we like it that way".  We could send a fake breakpoint, which should
 * cause the debugger to immediately send a resume, but the debugger might
 * send the resume immediately or might throw an exception of its own upon
 * receiving a breakpoint event that it didn't ask for.
 *
 * What we really want is a "wait until the debugger is done configuring
 * stuff" event.  We can approximate this with a "wait until the debugger
 * has been idle for a brief period".
 */

/*
 * Get a notion of the current time, in milliseconds.
 */
s8 dvmJdwpGetNowMsec()
{
#ifdef HAVE_POSIX_CLOCKS
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000LL + now.tv_nsec / 1000000LL;
#else
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000LL + now.tv_usec / 1000LL;
#endif
}

/*
 * Return the time, in milliseconds, since the last debugger activity.
 *
 * Returns -1 if no debugger is attached, or 0 if we're in the middle of
 * processing a debugger request.
 */
s8 dvmJdwpLastDebuggerActivity(JdwpState* state)
{
    if (!gDvm.debuggerActive) {
        ALOGD("dvmJdwpLastDebuggerActivity: no active debugger");
        return -1;
    }

    s8 last = dvmQuasiAtomicRead64(&state->lastActivityWhen);

    /* initializing or in the middle of something? */
    if (last == 0) {
        ALOGV("+++ last=busy");
        return 0;
    }

    /* now get the current time */
    s8 now = dvmJdwpGetNowMsec();
    assert(now >= last);

    ALOGV("+++ debugger interval=%lld", now - last);
    return now - last;
}
