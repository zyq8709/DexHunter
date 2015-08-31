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
 * JDWP internal interfaces.
 */
#ifndef DALVIK_JDWP_JDWPPRIV_H_
#define DALVIK_JDWP_JDWPPRIV_H_

#define LOG_TAG "jdwp"

#include "jdwp/Jdwp.h"
#include "jdwp/JdwpEvent.h"
#include "Debugger.h"

#include <pthread.h>
#include <sys/uio.h>

/*
 * JDWP constants.
 */
#define kJDWPHeaderLen  11
#define kJDWPFlagReply  0x80

/* DDM support */
#define kJDWPDdmCmdSet  199     /* 0xc7, or 'G'+128 */
#define kJDWPDdmCmd     1


/*
 * Transport-specific network status.
 */
struct JdwpNetState;
struct JdwpState;

/*
 * Transport functions.
 */
struct JdwpTransport {
    bool (*startup)(struct JdwpState* state, const JdwpStartupParams* pParams);
    bool (*accept)(struct JdwpState* state);
    bool (*establish)(struct JdwpState* state);
    void (*close)(struct JdwpState* state);
    void (*shutdown)(struct JdwpState* state);
    void (*free)(struct JdwpState* state);
    bool (*isConnected)(struct JdwpState* state);
    bool (*awaitingHandshake)(struct JdwpState* state);
    bool (*processIncoming)(struct JdwpState* state);
    bool (*sendRequest)(struct JdwpState* state, ExpandBuf* pReq);
    bool (*sendBufferedRequest)(struct JdwpState* state,
        const struct iovec* iov, int iovcnt);
};

const JdwpTransport* dvmJdwpSocketTransport();
const JdwpTransport* dvmJdwpAndroidAdbTransport();


/*
 * State for JDWP functions.
 */
struct JdwpState {
    JdwpStartupParams   params;

    /* wait for creation of the JDWP thread */
    pthread_mutex_t threadStartLock;
    pthread_cond_t  threadStartCond;

    int             debugThreadStarted;
    pthread_t       debugThreadHandle;
    ObjectId        debugThreadId;
    bool            run;

    const JdwpTransport*    transport;
    JdwpNetState*   netState;

    /* for wait-for-debugger */
    pthread_mutex_t attachLock;
    pthread_cond_t  attachCond;

    /* time of last debugger activity, in milliseconds */
    s8              lastActivityWhen;

    /* global counters and a mutex to protect them */
    u4              requestSerial;
    u4              eventSerial;
    pthread_mutex_t serialLock;

    /*
     * Events requested by the debugger (breakpoints, class prep, etc).
     */
    int             numEvents;      /* #of elements in eventList */
    JdwpEvent*      eventList;      /* linked list of events */
    pthread_mutex_t eventLock;      /* guards numEvents/eventList */

    /*
     * Synchronize suspension of event thread (to avoid receiving "resume"
     * events before the thread has finished suspending itself).
     */
    pthread_mutex_t eventThreadLock;
    pthread_cond_t  eventThreadCond;
    ObjectId        eventThreadId;

    /*
     * DDM support.
     */
    bool            ddmActive;
};

/*
 * Base class for JdwpNetState
 */
class JdwpNetStateBase {
public:
    int             clientSock;     /* active connection to debugger */

    JdwpNetStateBase();
    ssize_t writePacket(ExpandBuf* pReply);
    ssize_t writeBufferedPacket(const struct iovec* iov, int iovcnt);

private:
    pthread_mutex_t socketLock;     /* socket synchronization */
};


/* reset all session-specific data */
void dvmJdwpResetState(JdwpState* state);

/* atomic ops to get next serial number */
u4 dvmJdwpNextRequestSerial(JdwpState* state);
u4 dvmJdwpNextEventSerial(JdwpState* state);

/* get current time, in msec */
s8 dvmJdwpGetNowMsec(void);


/*
 * Transport functions.
 */
INLINE bool dvmJdwpNetStartup(JdwpState* state,
    const JdwpStartupParams* pParams)
{
    return (*state->transport->startup)(state, pParams);
}
INLINE bool dvmJdwpAcceptConnection(JdwpState* state) {
    return (*state->transport->accept)(state);
}
INLINE bool dvmJdwpEstablishConnection(JdwpState* state) {
    return (*state->transport->establish)(state);
}
INLINE void dvmJdwpCloseConnection(JdwpState* state) {
    (*state->transport->close)(state);
}
INLINE void dvmJdwpNetShutdown(JdwpState* state) {
    (*state->transport->shutdown)(state);
}
INLINE void dvmJdwpNetFree(JdwpState* state) {
    (*state->transport->free)(state);
}
INLINE bool dvmJdwpIsTransportDefined(JdwpState* state) {
    return state != NULL && state->transport != NULL;
}
INLINE bool dvmJdwpIsConnected(JdwpState* state) {
    return state != NULL && (*state->transport->isConnected)(state);
}
INLINE bool dvmJdwpAwaitingHandshake(JdwpState* state) {
    return (*state->transport->awaitingHandshake)(state);
}
INLINE bool dvmJdwpProcessIncoming(JdwpState* state) {
    return (*state->transport->processIncoming)(state);
}
INLINE bool dvmJdwpSendRequest(JdwpState* state, ExpandBuf* pReq) {
    return (*state->transport->sendRequest)(state, pReq);
}
INLINE bool dvmJdwpSendBufferedRequest(JdwpState* state,
    const struct iovec* iov, int iovcnt)
{
    return (*state->transport->sendBufferedRequest)(state, iov, iovcnt);
}

#endif  // DALVIK_JDWP_JDWPPRIV_H_
