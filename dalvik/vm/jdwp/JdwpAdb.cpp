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

#include "jdwp/JdwpPriv.h"
#include "jdwp/JdwpHandler.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>
#include <cutils/sockets.h>

/*
 * The JDWP <-> ADB transport protocol is explained in detail
 * in system/core/adb/jdwp_service.c. Here's a summary.
 *
 * 1/ when the JDWP thread starts, it tries to connect to a Unix
 *    domain stream socket (@jdwp-control) that is opened by the
 *    ADB daemon.
 *
 * 2/ it then sends the current process PID as a string of 4 hexadecimal
 *    chars (no terminating zero)
 *
 * 3/ then, it uses recvmsg to receive file descriptors from the
 *    daemon. each incoming file descriptor is a pass-through to
 *    a given JDWP debugger, that can be used to read the usual
 *    JDWP-handshake, etc...
 */

#define kInputBufferSize    8192

#define kMagicHandshake     "JDWP-Handshake"
#define kMagicHandshakeLen  (sizeof(kMagicHandshake)-1)

#define kJdwpControlName    "\0jdwp-control"
#define kJdwpControlNameLen (sizeof(kJdwpControlName)-1)

struct JdwpNetState : public JdwpNetStateBase {
    int                 controlSock;
    bool                awaitingHandshake;
    bool                shuttingDown;
    int                 wakeFds[2];

    int                 inputCount;
    unsigned char       inputBuffer[kInputBufferSize];

    socklen_t           controlAddrLen;
    union {
        struct sockaddr_un  controlAddrUn;
        struct sockaddr     controlAddrPlain;
    } controlAddr;

    JdwpNetState()
    {
        controlSock = -1;
        awaitingHandshake = false;
        shuttingDown = false;
        wakeFds[0] = -1;
        wakeFds[1] = -1;

        inputCount = 0;

        controlAddr.controlAddrUn.sun_family = AF_UNIX;
        controlAddrLen = sizeof(controlAddr.controlAddrUn.sun_family) +
                kJdwpControlNameLen;
        memcpy(controlAddr.controlAddrUn.sun_path, kJdwpControlName,
                kJdwpControlNameLen);
    }
};

static void
adbStateFree( JdwpNetState*  netState )
{
    if (netState == NULL)
        return;

    if (netState->clientSock >= 0) {
        shutdown(netState->clientSock, SHUT_RDWR);
        close(netState->clientSock);
    }
    if (netState->controlSock >= 0) {
        shutdown(netState->controlSock, SHUT_RDWR);
        close(netState->controlSock);
    }
    if (netState->wakeFds[0] >= 0) {
        close(netState->wakeFds[0]);
        netState->wakeFds[0] = -1;
    }
    if (netState->wakeFds[1] >= 0) {
        close(netState->wakeFds[1]);
        netState->wakeFds[1] = -1;
    }

    delete netState;
}

/*
 * Do initial prep work, e.g. binding to ports and opening files.  This
 * runs in the main thread, before the JDWP thread starts, so it shouldn't
 * do anything that might block forever.
 */
static bool startup(struct JdwpState* state, const JdwpStartupParams* pParams)
{
    JdwpNetState*  netState;

    ALOGV("ADB transport startup");

    state->netState = netState = new JdwpNetState;
    if (netState == NULL)
        return false;

    return true;
}

/*
 * Receive a file descriptor from ADB.  The fd can be used to communicate
 * directly with a debugger or DDMS.
 *
 * Returns the file descriptor on success.  On failure, returns -1 and
 * closes netState->controlSock.
 */
static int  receiveClientFd(JdwpNetState*  netState)
{
    struct msghdr    msg;
    struct cmsghdr*  cmsg;
    struct iovec     iov;
    char             dummy = '!';
    union {
        struct cmsghdr cm;
        char buffer[CMSG_SPACE(sizeof(int))];
    } cm_un;
    int              ret;

    iov.iov_base       = &dummy;
    iov.iov_len        = 1;
    msg.msg_name       = NULL;
    msg.msg_namelen    = 0;
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;
    msg.msg_flags      = 0;
    msg.msg_control    = cm_un.buffer;
    msg.msg_controllen = sizeof(cm_un.buffer);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len   = msg.msg_controllen;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    ((int*)(void*)CMSG_DATA(cmsg))[0] = -1;

    do {
        ret = recvmsg(netState->controlSock, &msg, 0);
    } while (ret < 0 && errno == EINTR);

    if (ret <= 0) {
        if (ret < 0) {
            ALOGW("receiving file descriptor from ADB failed (socket %d): %s",
                 netState->controlSock, strerror(errno));
        }
        close(netState->controlSock);
        netState->controlSock = -1;
        return -1;
    }

    return ((int*)(void*)CMSG_DATA(cmsg))[0];
}

/*
 * Block forever, waiting for a debugger to connect to us.  Called from the
 * JDWP thread.
 *
 * This needs to un-block and return "false" if the VM is shutting down.  It
 * should return "true" when it successfully accepts a connection.
 */
static bool acceptConnection(struct JdwpState* state)
{
    JdwpNetState*  netState = state->netState;
    int retryCount = 0;

    /* first, ensure that we get a connection to the ADB daemon */

retry:
    if (netState->shuttingDown)
        return false;

    if (netState->controlSock < 0) {
        int        sleep_ms     = 500;
        const int  sleep_max_ms = 2*1000;
        char       buff[5];

        netState->controlSock = socket(PF_UNIX, SOCK_STREAM, 0);
        if (netState->controlSock < 0) {
            ALOGE("Could not create ADB control socket:%s",
                 strerror(errno));
            return false;
        }

        if (pipe(netState->wakeFds) < 0) {
            ALOGE("pipe failed");
            return false;
        }

        snprintf(buff, sizeof(buff), "%04x", getpid());
        buff[4] = 0;

        for (;;) {
            /*
             * If adbd isn't running, because USB debugging was disabled or
             * perhaps the system is restarting it for "adb root", the
             * connect() will fail.  We loop here forever waiting for it
             * to come back.
             *
             * Waking up and polling every couple of seconds is generally a
             * bad thing to do, but we only do this if the application is
             * debuggable *and* adbd isn't running.  Still, for the sake
             * of battery life, we should consider timing out and giving
             * up after a few minutes in case somebody ships an app with
             * the debuggable flag set.
             */
            int  ret = connect(netState->controlSock,
                               &netState->controlAddr.controlAddrPlain,
                               netState->controlAddrLen);
            if (!ret) {
                if (!socket_peer_is_trusted(netState->controlSock)) {
                    if (shutdown(netState->controlSock, SHUT_RDWR)) {
                        ALOGE("trouble shutting down socket: %s", strerror(errno));
                    }
                    return false;
                }

                /* now try to send our pid to the ADB daemon */
                do {
                    ret = send( netState->controlSock, buff, 4, 0 );
                } while (ret < 0 && errno == EINTR);

                if (ret >= 0) {
                    ALOGV("PID sent as '%.*s' to ADB", 4, buff);
                    break;
                }

                ALOGE("Weird, can't send JDWP process pid to ADB: %s",
                     strerror(errno));
                return false;
            }
            ALOGV("Can't connect to ADB control socket:%s",
                 strerror(errno));

            usleep( sleep_ms*1000 );

            sleep_ms += (sleep_ms >> 1);
            if (sleep_ms > sleep_max_ms)
                sleep_ms = sleep_max_ms;

            if (netState->shuttingDown)
                return false;
        }
    }

    ALOGV("trying to receive file descriptor from ADB");
    /* now we can receive a client file descriptor */
    netState->clientSock = receiveClientFd(netState);
    if (netState->shuttingDown)
        return false;       // suppress logs and additional activity

    if (netState->clientSock < 0) {
        if (++retryCount > 5) {
            ALOGE("adb connection max retries exceeded");
            return false;
        }
        goto retry;
    } else {
        ALOGV("received file descriptor %d from ADB", netState->clientSock);
        netState->awaitingHandshake = 1;
        netState->inputCount = 0;
        return true;
    }
}

/*
 * Connect out to a debugger (for server=n).  Not required.
 */
static bool establishConnection(struct JdwpState* state)
{
    return false;
}

/*
 * Close a connection from a debugger (which may have already dropped us).
 * Only called from the JDWP thread.
 */
static void closeConnection(struct JdwpState* state)
{
    JdwpNetState* netState;

    assert(state != NULL && state->netState != NULL);

    netState = state->netState;
    if (netState->clientSock < 0)
        return;

    ALOGV("+++ closed JDWP <-> ADB connection");

    close(netState->clientSock);
    netState->clientSock = -1;
}

/*
 * Close all network stuff, including the socket we use to listen for
 * new connections.
 *
 * May be called from a non-JDWP thread, e.g. when the VM is shutting down.
 */
static void adbStateShutdown(struct JdwpNetState* netState)
{
    int  controlSock;
    int  clientSock;

    if (netState == NULL)
        return;

    netState->shuttingDown = true;

    clientSock = netState->clientSock;
    if (clientSock >= 0) {
        shutdown(clientSock, SHUT_RDWR);
        netState->clientSock = -1;
    }

    controlSock = netState->controlSock;
    if (controlSock >= 0) {
        shutdown(controlSock, SHUT_RDWR);
        netState->controlSock = -1;
    }

    if (netState->wakeFds[1] >= 0) {
        ALOGV("+++ writing to wakePipe");
        TEMP_FAILURE_RETRY(write(netState->wakeFds[1], "", 1));
    }
}

static void netShutdown(JdwpState* state)
{
    adbStateShutdown(state->netState);
}

/*
 * Free up anything we put in state->netState.  This is called after
 * "netShutdown", after the JDWP thread has stopped.
 */
static void netFree(struct JdwpState* state)
{
    JdwpNetState*  netState = state->netState;

    adbStateFree(netState);
}

/*
 * Is a debugger connected to us?
 */
static bool isConnected(struct JdwpState* state)
{
    return (state->netState != NULL   &&
            state->netState->clientSock >= 0);
}

/*
 * Are we still waiting for the JDWP handshake?
 */
static bool awaitingHandshake(struct JdwpState* state)
{
    return state->netState->awaitingHandshake;
}

/*
 * Figure out if we have a full packet in the buffer.
 */
static bool haveFullPacket(JdwpNetState* netState)
{
    long length;

    if (netState->awaitingHandshake)
        return (netState->inputCount >= (int) kMagicHandshakeLen);

    if (netState->inputCount < 4)
        return false;

    length = get4BE(netState->inputBuffer);
    return (netState->inputCount >= length);
}

/*
 * Consume bytes from the buffer.
 *
 * This would be more efficient with a circular buffer.  However, we're
 * usually only going to find one packet, which is trivial to handle.
 */
static void consumeBytes(JdwpNetState* netState, int count)
{
    assert(count > 0);
    assert(count <= netState->inputCount);

    if (count == netState->inputCount) {
        netState->inputCount = 0;
        return;
    }

    memmove(netState->inputBuffer, netState->inputBuffer + count,
        netState->inputCount - count);
    netState->inputCount -= count;
}

/*
 * Handle a packet.  Returns "false" if we encounter a connection-fatal error.
 */
static bool handlePacket(JdwpState* state)
{
    JdwpNetState* netState = state->netState;
    const unsigned char* buf = netState->inputBuffer;
    JdwpReqHeader hdr;
    u4 length, id;
    u1 flags, cmdSet, cmd;
    u2 error;
    bool reply;
    int dataLen;

    cmd = cmdSet = 0;       // shut up gcc

    length = read4BE(&buf);
    id = read4BE(&buf);
    flags = read1(&buf);
    if ((flags & kJDWPFlagReply) != 0) {
        reply = true;
        error = read2BE(&buf);
    } else {
        reply = false;
        cmdSet = read1(&buf);
        cmd = read1(&buf);
    }

    assert((int) length <= netState->inputCount);
    dataLen = length - (buf - netState->inputBuffer);

    if (!reply) {
        ExpandBuf* pReply = expandBufAlloc();

        hdr.length = length;
        hdr.id = id;
        hdr.cmdSet = cmdSet;
        hdr.cmd = cmd;
        dvmJdwpProcessRequest(state, &hdr, buf, dataLen, pReply);
        if (expandBufGetLength(pReply) > 0) {
            ssize_t cc = netState->writePacket(pReply);

            if (cc != (ssize_t) expandBufGetLength(pReply)) {
                ALOGE("Failed sending reply to debugger: %s", strerror(errno));
                expandBufFree(pReply);
                return false;
            }
        } else {
            ALOGW("No reply created for set=%d cmd=%d", cmdSet, cmd);
        }
        expandBufFree(pReply);
    } else {
        ALOGV("reply?!");
        assert(false);
    }

    ALOGV("----------");

    consumeBytes(netState, length);
    return true;
}

/*
 * Process incoming data.  If no data is available, this will block until
 * some arrives.
 *
 * If we get a full packet, handle it.
 *
 * To take some of the mystery out of life, we want to reject incoming
 * connections if we already have a debugger attached.  If we don't, the
 * debugger will just mysteriously hang until it times out.  We could just
 * close the listen socket, but there's a good chance we won't be able to
 * bind to the same port again, which would confuse utilities.
 *
 * Returns "false" on error (indicating that the connection has been severed),
 * "true" if things are still okay.
 */
static bool processIncoming(JdwpState* state)
{
    JdwpNetState* netState = state->netState;
    int readCount;

    assert(netState->clientSock >= 0);

    if (!haveFullPacket(netState)) {
        /* read some more, looping until we have data */
        errno = 0;
        while (1) {
            int selCount;
            fd_set readfds;
            int maxfd = -1;
            int fd;

            FD_ZERO(&readfds);

            /* configure fds; note these may get zapped by another thread */
            fd = netState->controlSock;
            if (fd >= 0) {
                FD_SET(fd, &readfds);
                if (maxfd < fd)
                    maxfd = fd;
            }
            fd = netState->clientSock;
            if (fd >= 0) {
                FD_SET(fd, &readfds);
                if (maxfd < fd)
                    maxfd = fd;
            }
            fd = netState->wakeFds[0];
            if (fd >= 0) {
                FD_SET(fd, &readfds);
                if (maxfd < fd)
                    maxfd = fd;
            } else {
                ALOGI("NOTE: entering select w/o wakepipe");
            }

            if (maxfd < 0) {
                ALOGV("+++ all fds are closed");
                return false;
            }

            /*
             * Select blocks until it sees activity on the file descriptors.
             * Closing the local file descriptor does not count as activity,
             * so we can't rely on that to wake us up (it works for read()
             * and accept(), but not select()).
             *
             * We can do one of three things: (1) send a signal and catch
             * EINTR, (2) open an additional fd ("wakePipe") and write to
             * it when it's time to exit, or (3) time out periodically and
             * re-issue the select.  We're currently using #2, as it's more
             * reliable than #1 and generally better than #3.  Wastes two fds.
             */
            selCount = select(maxfd+1, &readfds, NULL, NULL, NULL);
            if (selCount < 0) {
                if (errno == EINTR)
                    continue;
                ALOGE("select failed: %s", strerror(errno));
                goto fail;
            }

            if (netState->wakeFds[0] >= 0 &&
                FD_ISSET(netState->wakeFds[0], &readfds))
            {
                ALOGD("Got wake-up signal, bailing out of select");
                goto fail;
            }
            if (netState->controlSock >= 0 &&
                FD_ISSET(netState->controlSock, &readfds))
            {
                int  sock = receiveClientFd(netState);
                if (sock >= 0) {
                    ALOGI("Ignoring second debugger -- accepting and dropping");
                    close(sock);
                } else {
                    assert(netState->controlSock < 0);
                    /*
                     * Remote side most likely went away, so our next read
                     * on netState->clientSock will fail and throw us out
                     * of the loop.
                     */
                }
            }
            if (netState->clientSock >= 0 &&
                FD_ISSET(netState->clientSock, &readfds))
            {
                readCount = read(netState->clientSock,
                                netState->inputBuffer + netState->inputCount,
                    sizeof(netState->inputBuffer) - netState->inputCount);
                if (readCount < 0) {
                    /* read failed */
                    if (errno != EINTR)
                        goto fail;
                    ALOGD("+++ EINTR hit");
                    return true;
                } else if (readCount == 0) {
                    /* EOF hit -- far end went away */
                    ALOGV("+++ peer disconnected");
                    goto fail;
                } else
                    break;
            }
        }

        netState->inputCount += readCount;
        if (!haveFullPacket(netState))
            return true;        /* still not there yet */
    }

    /*
     * Special-case the initial handshake.  For some bizarre reason we're
     * expected to emulate bad tty settings by echoing the request back
     * exactly as it was sent.  Note the handshake is always initiated by
     * the debugger, no matter who connects to whom.
     *
     * Other than this one case, the protocol [claims to be] stateless.
     */
    if (netState->awaitingHandshake) {
        int cc;

        if (memcmp(netState->inputBuffer,
                kMagicHandshake, kMagicHandshakeLen) != 0)
        {
            ALOGE("ERROR: bad handshake '%.14s'", netState->inputBuffer);
            goto fail;
        }

        errno = 0;
        cc = TEMP_FAILURE_RETRY(write(netState->clientSock, netState->inputBuffer,
                                      kMagicHandshakeLen));
        if (cc != kMagicHandshakeLen) {
            ALOGE("Failed writing handshake bytes: %s (%d of %d)",
                strerror(errno), cc, (int) kMagicHandshakeLen);
            goto fail;
        }

        consumeBytes(netState, kMagicHandshakeLen);
        netState->awaitingHandshake = false;
        ALOGV("+++ handshake complete");
        return true;
    }

    /*
     * Handle this packet.
     */
    return handlePacket(state);

fail:
    closeConnection(state);
    return false;
}

/*
 * Send a request.
 *
 * The entire packet must be sent with a single write() call to avoid
 * threading issues.
 *
 * Returns "true" if it was sent successfully.
 */
static bool sendRequest(JdwpState* state, ExpandBuf* pReq)
{
    JdwpNetState* netState = state->netState;

    if (netState->clientSock < 0) {
        /* can happen with some DDMS events */
        ALOGV("NOT sending request -- no debugger is attached");
        return false;
    }

    errno = 0;

    ssize_t cc = netState->writePacket(pReq);

    if (cc != (ssize_t) expandBufGetLength(pReq)) {
        ALOGE("Failed sending req to debugger: %s (%d of %d)",
            strerror(errno), (int) cc, (int) expandBufGetLength(pReq));
        return false;
    }

    return true;
}

/*
 * Send a request that was split into multiple buffers.
 *
 * The entire packet must be sent with a single writev() call to avoid
 * threading issues.
 *
 * Returns "true" if it was sent successfully.
 */
static bool sendBufferedRequest(JdwpState* state, const struct iovec* iov,
    int iovcnt)
{
    JdwpNetState* netState = state->netState;

    if (netState->clientSock < 0) {
        /* can happen with some DDMS events */
        ALOGV("NOT sending request -- no debugger is attached");
        return false;
    }

    size_t expected = 0;
    int i;
    for (i = 0; i < iovcnt; i++)
        expected += iov[i].iov_len;

    ssize_t actual = netState->writeBufferedPacket(iov, iovcnt);

    if ((size_t)actual != expected) {
        ALOGE("Failed sending b-req to debugger: %s (%d of %zu)",
            strerror(errno), (int) actual, expected);
        return false;
    }

    return true;
}


/*
 * Our functions.
 */
static const JdwpTransport socketTransport = {
    startup,
    acceptConnection,
    establishConnection,
    closeConnection,
    netShutdown,
    netFree,
    isConnected,
    awaitingHandshake,
    processIncoming,
    sendRequest,
    sendBufferedRequest
};

/*
 * Return our set.
 */
const JdwpTransport* dvmJdwpAndroidAdbTransport()
{
    return &socketTransport;
}
