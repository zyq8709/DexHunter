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
 * Handle Dalvik Debug Monitor requests and events.
 *
 * Remember that all DDM traffic is big-endian since it travels over the
 * JDWP connection.
 */
#include "Dalvik.h"

#include <fcntl.h>
#include <errno.h>

/*
 * "buf" contains a full JDWP packet, possibly with multiple chunks.  We
 * need to process each, accumulate the replies, and ship the whole thing
 * back.
 *
 * Returns "true" if we have a reply.  The reply buffer is newly allocated,
 * and includes the chunk type/length, followed by the data.
 *
 * TODO: we currently assume that the request and reply include a single
 * chunk.  If this becomes inconvenient we will need to adapt.
 */
bool dvmDdmHandlePacket(const u1* buf, int dataLen, u1** pReplyBuf,
    int* pReplyLen)
{
    Thread* self = dvmThreadSelf();
    const int kChunkHdrLen = 8;
    ArrayObject* dataArray = NULL;
    Object* chunk = NULL;
    bool result = false;

    assert(dataLen >= 0);

    if (!dvmIsClassInitialized(gDvm.classOrgApacheHarmonyDalvikDdmcChunk)) {
        if (!dvmInitClass(gDvm.classOrgApacheHarmonyDalvikDdmcChunk)) {
            dvmLogExceptionStackTrace();
            dvmClearException(self);
            goto bail;
        }
    }

    /*
     * The chunk handlers are written in the Java programming language, so
     * we need to convert the buffer to a byte array.
     */
    dataArray = dvmAllocPrimitiveArray('B', dataLen, ALLOC_DEFAULT);
    if (dataArray == NULL) {
        ALOGW("array alloc failed (%d)", dataLen);
        dvmClearException(self);
        goto bail;
    }
    memcpy(dataArray->contents, buf, dataLen);

    /*
     * Run through and find all chunks.  [Currently just find the first.]
     */
    unsigned int offset, length, type;
    type = get4BE((u1*)dataArray->contents + 0);
    length = get4BE((u1*)dataArray->contents + 4);
    offset = kChunkHdrLen;
    if (offset+length > (unsigned int) dataLen) {
        ALOGW("WARNING: bad chunk found (len=%u pktLen=%d)", length, dataLen);
        goto bail;
    }

    /*
     * Call the handler.
     */
    JValue callRes;
    dvmCallMethod(self, gDvm.methDalvikDdmcServer_dispatch, NULL, &callRes,
        type, dataArray, offset, length);
    if (dvmCheckException(self)) {
        ALOGI("Exception thrown by dispatcher for 0x%08x", type);
        dvmLogExceptionStackTrace();
        dvmClearException(self);
        goto bail;
    }

    ArrayObject* replyData;
    chunk = (Object*) callRes.l;
    if (chunk == NULL)
        goto bail;

    /* not strictly necessary -- we don't alloc from managed heap here */
    dvmAddTrackedAlloc(chunk, self);

    /*
     * Pull the pieces out of the chunk.  We copy the results into a
     * newly-allocated buffer that the caller can free.  We don't want to
     * continue using the Chunk object because nothing has a reference to it.
     *
     * We could avoid this by returning type/data/offset/length and having
     * the caller be aware of the object lifetime issues, but that
     * integrates the JDWP code more tightly into the VM, and doesn't work
     * if we have responses for multiple chunks.
     *
     * So we're pretty much stuck with copying data around multiple times.
     */
    type = dvmGetFieldInt(chunk, gDvm.offDalvikDdmcChunk_type);
    replyData =
        (ArrayObject*) dvmGetFieldObject(chunk, gDvm.offDalvikDdmcChunk_data);
    offset = dvmGetFieldInt(chunk, gDvm.offDalvikDdmcChunk_offset);
    length = dvmGetFieldInt(chunk, gDvm.offDalvikDdmcChunk_length);

    ALOGV("DDM reply: type=0x%08x data=%p offset=%d length=%d",
        type, replyData, offset, length);

    if (length == 0 || replyData == NULL)
        goto bail;
    if (offset + length > replyData->length) {
        ALOGW("WARNING: chunk off=%d len=%d exceeds reply array len %d",
            offset, length, replyData->length);
        goto bail;
    }

    u1* reply;
    reply = (u1*) malloc(length + kChunkHdrLen);
    if (reply == NULL) {
        ALOGW("malloc %d failed", length+kChunkHdrLen);
        goto bail;
    }
    set4BE(reply + 0, type);
    set4BE(reply + 4, length);
    memcpy(reply+kChunkHdrLen, (const u1*)replyData->contents + offset, length);

    *pReplyBuf = reply;
    *pReplyLen = length + kChunkHdrLen;
    result = true;

    ALOGV("dvmHandleDdm returning type=%.4s buf=%p len=%d",
        (char*) reply, reply, length);

bail:
    dvmReleaseTrackedAlloc((Object*) dataArray, self);
    dvmReleaseTrackedAlloc(chunk, self);
    return result;
}

/* defined in org.apache.harmony.dalvik.ddmc.DdmServer */
#define CONNECTED       1
#define DISCONNECTED    2

/*
 * Broadcast an event to all handlers.
 */
static void broadcast(int event)
{
    Thread* self = dvmThreadSelf();

    if (self->status != THREAD_RUNNING) {
        ALOGE("ERROR: DDM broadcast with thread status=%d", self->status);
        /* try anyway? */
    }

    if (!dvmIsClassInitialized(gDvm.classOrgApacheHarmonyDalvikDdmcDdmServer)) {
        if (!dvmInitClass(gDvm.classOrgApacheHarmonyDalvikDdmcDdmServer)) {
            dvmLogExceptionStackTrace();
            dvmClearException(self);
            return;
        }
    }

    JValue unused;
    dvmCallMethod(self, gDvm.methDalvikDdmcServer_broadcast, NULL, &unused,
        event);
    if (dvmCheckException(self)) {
        ALOGI("Exception thrown by broadcast(%d)", event);
        dvmLogExceptionStackTrace();
        dvmClearException(self);
        return;
    }
}

/*
 * First DDM packet has arrived over JDWP.  Notify the press.
 *
 * We can do some initialization here too.
 */
void dvmDdmConnected()
{
    // TODO: any init

    ALOGV("Broadcasting DDM connect");
    broadcast(CONNECTED);
}

/*
 * JDWP connection has dropped.
 *
 * Do some cleanup.
 */
void dvmDdmDisconnected()
{
    ALOGV("Broadcasting DDM disconnect");
    broadcast(DISCONNECTED);

    gDvm.ddmThreadNotification = false;
}


/*
 * Turn thread notification on or off.
 */
void dvmDdmSetThreadNotification(bool enable)
{
    /*
     * We lock the thread list to avoid sending duplicate events or missing
     * a thread change.  We should be okay holding this lock while sending
     * the messages out.  (We have to hold it while accessing a live thread.)
     */
    dvmLockThreadList(NULL);
    gDvm.ddmThreadNotification = enable;

    if (enable) {
        Thread* thread;
        for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
            //ALOGW("notify %d", thread->threadId);
            dvmDdmSendThreadNotification(thread, true);
        }
    }

    dvmUnlockThreadList();
}

/*
 * Send a notification when a thread starts or stops.
 *
 * Because we broadcast the full set of threads when the notifications are
 * first enabled, it's possible for "thread" to be actively executing.
 */
void dvmDdmSendThreadNotification(Thread* thread, bool started)
{
    if (!gDvm.ddmThreadNotification) {
        return;
    }

    StringObject* nameObj = NULL;
    Object* threadObj = thread->threadObj;

    if (threadObj != NULL) {
        nameObj = (StringObject*)
            dvmGetFieldObject(threadObj, gDvm.offJavaLangThread_name);
    }

    int type, len;
    u1 buf[256];

    if (started) {
        const u2* chars;
        u2* outChars;
        size_t stringLen;

        type = CHUNK_TYPE("THCR");

        if (nameObj != NULL) {
            stringLen = nameObj->length();
            chars = nameObj->chars();
        } else {
            stringLen = 0;
            chars = NULL;
        }

        /* leave room for the two integer fields */
        if (stringLen > (sizeof(buf) - sizeof(u4)*2) / 2) {
            stringLen = (sizeof(buf) - sizeof(u4)*2) / 2;
        }
        len = stringLen*2 + sizeof(u4)*2;

        set4BE(&buf[0x00], thread->threadId);
        set4BE(&buf[0x04], stringLen);

        /* copy the UTF-16 string, transforming to big-endian */
        outChars = (u2*)(void*)&buf[0x08];
        while (stringLen--) {
            set2BE((u1*) (outChars++), *chars++);
        }
    } else {
        type = CHUNK_TYPE("THDE");

        len = 4;

        set4BE(&buf[0x00], thread->threadId);
    }

    dvmDbgDdmSendChunk(type, len, buf);
}

/*
 * Send a notification when a thread's name changes.
 */
void dvmDdmSendThreadNameChange(int threadId, StringObject* newName)
{
    if (!gDvm.ddmThreadNotification) {
        return;
    }

    size_t stringLen = newName->length();
    const u2* chars = newName->chars();

    /*
     * Output format:
     *  (4b) thread ID
     *  (4b) stringLen
     *  (xb) string chars
     */
    int bufLen = 4 + 4 + (stringLen * 2);
    u1 buf[bufLen];

    set4BE(&buf[0x00], threadId);
    set4BE(&buf[0x04], stringLen);
    u2* outChars = (u2*)(void*)&buf[0x08];
    while (stringLen--) {
        set2BE((u1*) (outChars++), *chars++);
    }

    dvmDbgDdmSendChunk(CHUNK_TYPE("THNM"), bufLen, buf);
}

/*
 * Generate the contents of a THST chunk.  The data encompasses all known
 * threads.
 *
 * Response has:
 *  (1b) header len
 *  (1b) bytes per entry
 *  (2b) thread count
 * Then, for each thread:
 *  (4b) threadId
 *  (1b) thread status
 *  (4b) tid
 *  (4b) utime
 *  (4b) stime
 *  (1b) is daemon?
 *
 * The length fields exist in anticipation of adding additional fields
 * without wanting to break ddms or bump the full protocol version.  I don't
 * think it warrants full versioning.  They might be extraneous and could
 * be removed from a future version.
 *
 * Returns a new byte[] with the data inside, or NULL on failure.  The
 * caller must call dvmReleaseTrackedAlloc() on the array.
 */
ArrayObject* dvmDdmGenerateThreadStats()
{
    const int kHeaderLen = 4;
    const int kBytesPerEntry = 18;

    dvmLockThreadList(NULL);

    Thread* thread;
    int threadCount = 0;
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next)
        threadCount++;

    /*
     * Create a temporary buffer.  We can't perform heap allocation with
     * the thread list lock held (could cause a GC).  The output is small
     * enough to sit on the stack.
     */
    int bufLen = kHeaderLen + threadCount * kBytesPerEntry;
    u1 tmpBuf[bufLen];
    u1* buf = tmpBuf;

    set1(buf+0, kHeaderLen);
    set1(buf+1, kBytesPerEntry);
    set2BE(buf+2, (u2) threadCount);
    buf += kHeaderLen;

    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        bool isDaemon = false;

        ProcStatData procStatData;
        if (!dvmGetThreadStats(&procStatData, thread->systemTid)) {
            /* failed; show zero */
            memset(&procStatData, 0, sizeof(procStatData));
        }

        Object* threadObj = thread->threadObj;
        if (threadObj != NULL) {
            isDaemon = dvmGetFieldBoolean(threadObj,
                            gDvm.offJavaLangThread_daemon);
        }

        set4BE(buf+0, thread->threadId);
        set1(buf+4, thread->status);
        set4BE(buf+5, thread->systemTid);
        set4BE(buf+9, procStatData.utime);
        set4BE(buf+13, procStatData.stime);
        set1(buf+17, isDaemon);

        buf += kBytesPerEntry;
    }
    dvmUnlockThreadList();


    /*
     * Create a byte array to hold the data.
     */
    ArrayObject* arrayObj = dvmAllocPrimitiveArray('B', bufLen, ALLOC_DEFAULT);
    if (arrayObj != NULL)
        memcpy(arrayObj->contents, tmpBuf, bufLen);
    return arrayObj;
}


/*
 * Find the specified thread and return its stack trace as an array of
 * StackTraceElement objects.
 */
ArrayObject* dvmDdmGetStackTraceById(u4 threadId)
{
    Thread* self = dvmThreadSelf();
    Thread* thread;
    int* traceBuf;

    dvmLockThreadList(self);

    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (thread->threadId == threadId)
            break;
    }
    if (thread == NULL) {
        ALOGI("dvmDdmGetStackTraceById: threadid=%d not found", threadId);
        dvmUnlockThreadList();
        return NULL;
    }

    /*
     * Suspend the thread, pull out the stack trace, then resume the thread
     * and release the thread list lock.  If we're being asked to examine
     * our own stack trace, skip the suspend/resume.
     */
    size_t stackDepth;
    if (thread != self)
        dvmSuspendThread(thread);
    traceBuf = dvmFillInStackTraceRaw(thread, &stackDepth);
    if (thread != self)
        dvmResumeThread(thread);
    dvmUnlockThreadList();

    /*
     * Convert the raw buffer into an array of StackTraceElement.
     */
    ArrayObject* trace = dvmGetStackTraceRaw(traceBuf, stackDepth);
    free(traceBuf);
    return trace;
}

/*
 * Gather up the allocation data and copy it into a byte[].
 *
 * Returns NULL on failure with an exception raised.
 */
ArrayObject* dvmDdmGetRecentAllocations()
{
    u1* data;
    size_t len;

    if (!dvmGenerateTrackedAllocationReport(&data, &len)) {
        /* assume OOM */
        dvmThrowOutOfMemoryError("recent alloc native");
        return NULL;
    }

    ArrayObject* arrayObj = dvmAllocPrimitiveArray('B', len, ALLOC_DEFAULT);
    if (arrayObj != NULL)
        memcpy(arrayObj->contents, data, len);
    return arrayObj;
}
