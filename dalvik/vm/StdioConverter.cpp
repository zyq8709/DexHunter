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
 * Thread that reads from stdout/stderr and converts them to log messages.
 * (Sort of a hack.)
 */
#include "Dalvik.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define kFilenoStdout   1
#define kFilenoStderr   2

#define kMaxLine    512

/*
 * Hold some data.
 */
struct BufferedData {
    char    buf[kMaxLine+1];
    int     count;
};

// fwd
static void* stdioConverterThreadStart(void* arg);
static bool readAndLog(int fd, BufferedData* data, const char* tag);


/*
 * Crank up the stdout/stderr converter thread.
 *
 * Returns immediately.
 */
bool dvmStdioConverterStartup()
{
    gDvm.haltStdioConverter = false;

    dvmInitMutex(&gDvm.stdioConverterLock);
    pthread_cond_init(&gDvm.stdioConverterCond, NULL);

    if (pipe(gDvm.stdoutPipe) != 0) {
        ALOGW("pipe failed: %s", strerror(errno));
        return false;
    }
    if (pipe(gDvm.stderrPipe) != 0) {
        ALOGW("pipe failed: %s", strerror(errno));
        return false;
    }

    if (dup2(gDvm.stdoutPipe[1], kFilenoStdout) != kFilenoStdout) {
        ALOGW("dup2(1) failed: %s", strerror(errno));
        return false;
    }
    close(gDvm.stdoutPipe[1]);
    gDvm.stdoutPipe[1] = -1;
#ifdef HAVE_ANDROID_OS
    /* don't redirect stderr on sim -- logs get written there! */
    /* (don't need this on the sim anyway) */
    if (dup2(gDvm.stderrPipe[1], kFilenoStderr) != kFilenoStderr) {
        ALOGW("dup2(2) failed: %d %s", errno, strerror(errno));
        return false;
    }
    close(gDvm.stderrPipe[1]);
    gDvm.stderrPipe[1] = -1;
#endif


    /*
     * Create the thread.
     */
    dvmLockMutex(&gDvm.stdioConverterLock);

    if (!dvmCreateInternalThread(&gDvm.stdioConverterHandle,
                                 "Stdio Converter",
                                 stdioConverterThreadStart,
                                 NULL)) {
        return false;
    }

    while (!gDvm.stdioConverterReady) {
        dvmWaitCond(&gDvm.stdioConverterCond, &gDvm.stdioConverterLock);
    }
    dvmUnlockMutex(&gDvm.stdioConverterLock);

    return true;
}

/*
 * Shut down the stdio converter thread if it was started.
 *
 * Since we know the thread is just sitting around waiting for something
 * to arrive on stdout, print something.
 */
void dvmStdioConverterShutdown()
{
    gDvm.haltStdioConverter = true;
    if (gDvm.stdioConverterHandle == 0)    // not started, or still starting
        return;

    /* print something to wake it up */
    printf("Shutting down\n");
    fflush(stdout);

    ALOGD("Joining stdio converter...");
    pthread_join(gDvm.stdioConverterHandle, NULL);
}

/*
 * Select on stdout/stderr pipes, waiting for activity.
 *
 * DO NOT use printf from here.
 */
static void* stdioConverterThreadStart(void* arg)
{
    int cc;

    /* tell the main thread that we're ready */
    dvmLockMutex(&gDvm.stdioConverterLock);
    gDvm.stdioConverterReady = true;
    cc = pthread_cond_signal(&gDvm.stdioConverterCond);
    assert(cc == 0);
    dvmUnlockMutex(&gDvm.stdioConverterLock);

    /* we never do anything that affects the rest of the VM */
    dvmChangeStatus(NULL, THREAD_VMWAIT);

    /*
     * Allocate read buffers.
     */
    BufferedData* stdoutData = new BufferedData;
    BufferedData* stderrData = new BufferedData;
    stdoutData->count = stderrData->count = 0;

    /*
     * Read until shutdown time.
     */
    while (!gDvm.haltStdioConverter) {
        fd_set readfds;
        int maxFd, fdCount;

        FD_ZERO(&readfds);
        FD_SET(gDvm.stdoutPipe[0], &readfds);
        FD_SET(gDvm.stderrPipe[0], &readfds);
        maxFd = MAX(gDvm.stdoutPipe[0], gDvm.stderrPipe[0]);

        fdCount = select(maxFd+1, &readfds, NULL, NULL, NULL);

        if (fdCount < 0) {
            if (errno != EINTR) {
                ALOGE("select on stdout/stderr failed");
                break;
            }
            ALOGD("Got EINTR, ignoring");
        } else if (fdCount == 0) {
            ALOGD("WEIRD: select returned zero");
        } else {
            bool err = false;
            if (FD_ISSET(gDvm.stdoutPipe[0], &readfds)) {
                err |= !readAndLog(gDvm.stdoutPipe[0], stdoutData,
                    "stdout");
            }
            if (FD_ISSET(gDvm.stderrPipe[0], &readfds)) {
                err |= !readAndLog(gDvm.stderrPipe[0], stderrData,
                    "stderr");
            }

            /* probably EOF; give up */
            if (err) {
                ALOGW("stdio converter got read error; shutting it down");
                break;
            }
        }
    }

    close(gDvm.stdoutPipe[0]);
    close(gDvm.stderrPipe[0]);

    delete stdoutData;
    delete stderrData;

    /* change back for shutdown sequence */
    dvmChangeStatus(NULL, THREAD_RUNNING);
    return NULL;
}

/*
 * Data is pending on "fd".  Read as much as will fit in "data", then
 * write out any full lines and compact "data".
 */
static bool readAndLog(int fd, BufferedData* data, const char* tag)
{
    ssize_t actual;
    size_t want;

    assert(data->count < kMaxLine);

    want = kMaxLine - data->count;
    actual = read(fd, data->buf + data->count, want);
    if (actual <= 0) {
        ALOGW("read %s: (%d,%d) failed (%d): %s",
            tag, fd, want, (int)actual, strerror(errno));
        return false;
    } else {
        //ALOGI("read %s: %d at %d", tag, actual, data->count);
    }
    data->count += actual;

    /*
     * Got more data, look for an EOL.  We expect LF or CRLF, but will
     * try to handle a standalone CR.
     */
    char* cp = data->buf;
    const char* start = data->buf;
    int i = data->count;
    for (i = data->count; i > 0; i--, cp++) {
        if (*cp == '\n' || (*cp == '\r' && i != 0 && *(cp+1) != '\n')) {
            *cp = '\0';
            //ALOGW("GOT %d at %d '%s'", cp - start, start - data->buf, start);
            ALOG(LOG_INFO, tag, "%s", start);
            start = cp+1;
        }
    }

    /*
     * See if we overflowed.  If so, cut it off.
     */
    if (start == data->buf && data->count == kMaxLine) {
        data->buf[kMaxLine] = '\0';
        ALOG(LOG_INFO, tag, "%s!", start);
        start = cp + kMaxLine;
    }

    /*
     * Update "data" if we consumed some output.  If there's anything left
     * in the buffer, it's because we didn't see an EOL and need to keep
     * reading until we see one.
     */
    if (start != data->buf) {
        if (start >= data->buf + data->count) {
            /* consumed all available */
            data->count = 0;
        } else {
            /* some left over */
            int remaining = data->count - (start - data->buf);
            memmove(data->buf, start, remaining);
            data->count = remaining;
        }
    }

    return true;
}
