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
 * Miscellaneous utility functions.
 */
#include "Dalvik.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <cutils/ashmem.h>
#include <sys/mman.h>

/*
 * Print a hex dump in this format:
 *
01234567: 00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff  0123456789abcdef\n
 *
 * If "mode" is kHexDumpLocal, we start at offset zero, and show a full
 * 16 bytes on the first line.  If it's kHexDumpMem, we make this look
 * like a memory dump, using the actual address, outputting a partial line
 * if "vaddr" isn't aligned on a 16-byte boundary.
 *
 * "priority" and "tag" determine the values passed to the log calls.
 *
 * Does not use printf() or other string-formatting calls.
 */
void dvmPrintHexDumpEx(int priority, const char* tag, const void* vaddr,
    size_t length, HexDumpMode mode)
{
    static const char gHexDigit[] = "0123456789abcdef";
    const unsigned char* addr = (const unsigned char*)vaddr;
    char out[77];           /* exact fit */
    unsigned int offset;    /* offset to show while printing */
    char* hex;
    char* asc;
    int gap;
    //int trickle = 0;

    if (mode == kHexDumpLocal)
        offset = 0;
    else
        offset = (int) addr;

    memset(out, ' ', sizeof(out)-1);
    out[8] = ':';
    out[sizeof(out)-2] = '\n';
    out[sizeof(out)-1] = '\0';

    gap = (int) offset & 0x0f;
    while (length) {
        unsigned int lineOffset = offset & ~0x0f;
        int i, count;

        hex = out;
        asc = out + 59;

        for (i = 0; i < 8; i++) {
            *hex++ = gHexDigit[lineOffset >> 28];
            lineOffset <<= 4;
        }
        hex++;
        hex++;

        count = ((int)length > 16-gap) ? 16-gap : (int)length; /* cap length */
        assert(count != 0);
        assert(count+gap <= 16);

        if (gap) {
            /* only on first line */
            hex += gap * 3;
            asc += gap;
        }

        for (i = gap ; i < count+gap; i++) {
            *hex++ = gHexDigit[*addr >> 4];
            *hex++ = gHexDigit[*addr & 0x0f];
            hex++;
            if (*addr >= 0x20 && *addr < 0x7f /*isprint(*addr)*/)
                *asc++ = *addr;
            else
                *asc++ = '.';
            addr++;
        }
        for ( ; i < 16; i++) {
            /* erase extra stuff; only happens on last line */
            *hex++ = ' ';
            *hex++ = ' ';
            hex++;
            *asc++ = ' ';
        }

        LOG_PRI(priority, tag, "%s", out);
#if 0 //def HAVE_ANDROID_OS
        /*
         * We can overrun logcat easily by writing at full speed.  On the
         * other hand, we can make Eclipse time out if we're showing
         * packet dumps while debugging JDWP.
         */
        {
            if (trickle++ == 8) {
                trickle = 0;
                usleep(20000);
            }
        }
#endif

        gap = 0;
        length -= count;
        offset += count;
    }
}


/*
 * Fill out a DebugOutputTarget, suitable for printing to the log.
 */
void dvmCreateLogOutputTarget(DebugOutputTarget* target, int priority,
    const char* tag)
{
    assert(target != NULL);
    assert(tag != NULL);

    target->which = kDebugTargetLog;
    target->data.log.priority = priority;
    target->data.log.tag = tag;
}

/*
 * Fill out a DebugOutputTarget suitable for printing to a file pointer.
 */
void dvmCreateFileOutputTarget(DebugOutputTarget* target, FILE* fp)
{
    assert(target != NULL);
    assert(fp != NULL);

    target->which = kDebugTargetFile;
    target->data.file.fp = fp;
}

/*
 * Free "target" and any associated data.
 */
void dvmFreeOutputTarget(DebugOutputTarget* target)
{
    free(target);
}

/*
 * Print a debug message, to either a file or the log.
 */
void dvmPrintDebugMessage(const DebugOutputTarget* target, const char* format,
    ...)
{
    va_list args;

    va_start(args, format);

    switch (target->which) {
    case kDebugTargetLog:
        LOG_PRI_VA(target->data.log.priority, target->data.log.tag,
            format, args);
        break;
    case kDebugTargetFile:
        vfprintf(target->data.file.fp, format, args);
        break;
    default:
        ALOGE("unexpected 'which' %d", target->which);
        break;
    }

    va_end(args);
}


/*
 * Return a newly-allocated string in which all occurrences of '.' have
 * been changed to '/'.  If we find a '/' in the original string, NULL
 * is returned to avoid ambiguity.
 */
char* dvmDotToSlash(const char* str)
{
    char* newStr = strdup(str);
    char* cp = newStr;

    if (newStr == NULL)
        return NULL;

    while (*cp != '\0') {
        if (*cp == '/') {
            assert(false);
            return NULL;
        }
        if (*cp == '.')
            *cp = '/';
        cp++;
    }

    return newStr;
}

std::string dvmHumanReadableDescriptor(const char* descriptor) {
    // Count the number of '['s to get the dimensionality.
    const char* c = descriptor;
    size_t dim = 0;
    while (*c == '[') {
        dim++;
        c++;
    }

    // Reference or primitive?
    if (*c == 'L') {
        // "[[La/b/C;" -> "a.b.C[][]".
        c++; // Skip the 'L'.
    } else {
        // "[[B" -> "byte[][]".
        // To make life easier, we make primitives look like unqualified
        // reference types.
        switch (*c) {
        case 'B': c = "byte;"; break;
        case 'C': c = "char;"; break;
        case 'D': c = "double;"; break;
        case 'F': c = "float;"; break;
        case 'I': c = "int;"; break;
        case 'J': c = "long;"; break;
        case 'S': c = "short;"; break;
        case 'Z': c = "boolean;"; break;
        default: return descriptor;
        }
    }

    // At this point, 'c' is a string of the form "fully/qualified/Type;"
    // or "primitive;". Rewrite the type with '.' instead of '/':
    std::string result;
    const char* p = c;
    while (*p != ';') {
        char ch = *p++;
        if (ch == '/') {
          ch = '.';
        }
        result.push_back(ch);
    }
    // ...and replace the semicolon with 'dim' "[]" pairs:
    while (dim--) {
        result += "[]";
    }
    return result;
}

std::string dvmHumanReadableType(const Object* obj)
{
    if (obj == NULL) {
        return "null";
    }
    if (obj->clazz == NULL) {
        /* should only be possible right after a plain dvmMalloc() */
        return "(raw)";
    }
    std::string result(dvmHumanReadableDescriptor(obj->clazz->descriptor));
    if (dvmIsClassObject(obj)) {
        const ClassObject* clazz = reinterpret_cast<const ClassObject*>(obj);
        result += "<" + dvmHumanReadableDescriptor(clazz->descriptor) + ">";
    }
    return result;
}

std::string dvmHumanReadableField(const Field* field)
{
    if (field == NULL) {
        return "(null)";
    }
    std::string result(dvmHumanReadableDescriptor(field->clazz->descriptor));
    result += '.';
    result += field->name;
    return result;
}

std::string dvmHumanReadableMethod(const Method* method, bool withSignature)
{
    if (method == NULL) {
        return "(null)";
    }
    std::string result(dvmHumanReadableDescriptor(method->clazz->descriptor));
    result += '.';
    result += method->name;
    if (withSignature) {
        // TODO: the types in this aren't human readable!
        char* signature = dexProtoCopyMethodDescriptor(&method->prototype);
        result += signature;
        free(signature);
    }
    return result;
}

/*
 * Return a newly-allocated string for the "dot version" of the class
 * name for the given type descriptor. That is, The initial "L" and
 * final ";" (if any) have been removed and all occurrences of '/'
 * have been changed to '.'.
 *
 * "Dot version" names are used in the class loading machinery.
 * See also dvmHumanReadableDescriptor.
 */
char* dvmDescriptorToDot(const char* str)
{
    size_t at = strlen(str);
    char* newStr;

    if ((at >= 2) && (str[0] == 'L') && (str[at - 1] == ';')) {
        at -= 2; /* Two fewer chars to copy. */
        str++; /* Skip the 'L'. */
    }

    newStr = (char*)malloc(at + 1); /* Add one for the '\0'. */
    if (newStr == NULL)
        return NULL;

    newStr[at] = '\0';

    while (at > 0) {
        at--;
        newStr[at] = (str[at] == '/') ? '.' : str[at];
    }

    return newStr;
}

/*
 * Return a newly-allocated string for the type descriptor
 * corresponding to the "dot version" of the given class name. That
 * is, non-array names are surrounded by "L" and ";", and all
 * occurrences of '.' have been changed to '/'.
 *
 * "Dot version" names are used in the class loading machinery.
 */
char* dvmDotToDescriptor(const char* str)
{
    size_t length = strlen(str);
    int wrapElSemi = 0;
    char* newStr;
    char* at;

    if (str[0] != '[') {
        length += 2; /* for "L" and ";" */
        wrapElSemi = 1;
    }

    newStr = at = (char*)malloc(length + 1); /* + 1 for the '\0' */

    if (newStr == NULL) {
        return NULL;
    }

    if (wrapElSemi) {
        *(at++) = 'L';
    }

    while (*str) {
        char c = *(str++);
        if (c == '.') {
            c = '/';
        }
        *(at++) = c;
    }

    if (wrapElSemi) {
        *(at++) = ';';
    }

    *at = '\0';
    return newStr;
}

/*
 * Return a newly-allocated string for the internal-form class name for
 * the given type descriptor. That is, the initial "L" and final ";" (if
 * any) have been removed.
 */
char* dvmDescriptorToName(const char* str)
{
    if (str[0] == 'L') {
        size_t length = strlen(str) - 1;
        char* newStr = (char*)malloc(length);

        if (newStr == NULL) {
            return NULL;
        }

        strlcpy(newStr, str + 1, length);
        return newStr;
    }

    return strdup(str);
}

/*
 * Return a newly-allocated string for the type descriptor for the given
 * internal-form class name. That is, a non-array class name will get
 * surrounded by "L" and ";", while array names are left as-is.
 */
char* dvmNameToDescriptor(const char* str)
{
    if (str[0] != '[') {
        size_t length = strlen(str);
        char* descriptor = (char*)malloc(length + 3);

        if (descriptor == NULL) {
            return NULL;
        }

        descriptor[0] = 'L';
        strcpy(descriptor + 1, str);
        descriptor[length + 1] = ';';
        descriptor[length + 2] = '\0';

        return descriptor;
    }

    return strdup(str);
}

/*
 * Get a notion of the current time, in nanoseconds.  This is meant for
 * computing durations (e.g. "operation X took 52nsec"), so the result
 * should not be used to get the current date/time.
 */
u8 dvmGetRelativeTimeNsec()
{
#ifdef HAVE_POSIX_CLOCKS
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (u8)now.tv_sec*1000000000LL + now.tv_nsec;
#else
    struct timeval now;
    gettimeofday(&now, NULL);
    return (u8)now.tv_sec*1000000000LL + now.tv_usec * 1000LL;
#endif
}

/*
 * Get the per-thread CPU time, in nanoseconds.
 *
 * Only useful for time deltas.
 */
u8 dvmGetThreadCpuTimeNsec()
{
#ifdef HAVE_POSIX_CLOCKS
    struct timespec now;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    return (u8)now.tv_sec*1000000000LL + now.tv_nsec;
#else
    return (u8) -1;
#endif
}

/*
 * Get the per-thread CPU time, in nanoseconds, for the specified thread.
 */
u8 dvmGetOtherThreadCpuTimeNsec(pthread_t thread)
{
#if 0 /*def HAVE_POSIX_CLOCKS*/
    int clockId;

    if (pthread_getcpuclockid(thread, &clockId) != 0)
        return (u8) -1;

    struct timespec now;
    clock_gettime(clockId, &now);
    return (u8)now.tv_sec*1000000000LL + now.tv_nsec;
#else
    return (u8) -1;
#endif
}


/*
 * Call this repeatedly, with successively higher values for "iteration",
 * to sleep for a period of time not to exceed "maxTotalSleep".
 *
 * For example, when called with iteration==0 we will sleep for a very
 * brief time.  On the next call we will sleep for a longer time.  When
 * the sum total of all sleeps reaches "maxTotalSleep", this returns false.
 *
 * The initial start time value for "relStartTime" MUST come from the
 * dvmGetRelativeTimeUsec call.  On the device this must come from the
 * monotonic clock source, not the wall clock.
 *
 * This should be used wherever you might be tempted to call sched_yield()
 * in a loop.  The problem with sched_yield is that, for a high-priority
 * thread, the kernel might not actually transfer control elsewhere.
 *
 * Returns "false" if we were unable to sleep because our time was up.
 */
bool dvmIterativeSleep(int iteration, int maxTotalSleep, u8 relStartTime)
{
    /*
     * Minimum sleep is one millisecond, it is important to keep this value
     * low to ensure short GC pauses since dvmSuspendAllThreads() uses this
     * function.
     */
    const int minSleep = 1000;
    u8 curTime;
    int curDelay;

    /*
     * Get current time, and see if we've already exceeded the limit.
     */
    curTime = dvmGetRelativeTimeUsec();
    if (curTime >= relStartTime + maxTotalSleep) {
        LOGVV("exsl: sleep exceeded (start=%llu max=%d now=%llu)",
            relStartTime, maxTotalSleep, curTime);
        return false;
    }

    /*
     * Compute current delay.  We're bounded by "maxTotalSleep", so no
     * real risk of overflow assuming "usleep" isn't returning early.
     * (Besides, 2^30 usec is about 18 minutes by itself.)
     *
     * For iteration==0 we just call sched_yield(), so the first sleep
     * at iteration==1 is actually (minSleep * 2).
     */
    curDelay = minSleep;
    while (iteration-- > 0)
        curDelay *= 2;
    assert(curDelay > 0);

    if (curTime + curDelay >= relStartTime + maxTotalSleep) {
        LOGVV("exsl: reduced delay from %d to %d",
            curDelay, (int) ((relStartTime + maxTotalSleep) - curTime));
        curDelay = (int) ((relStartTime + maxTotalSleep) - curTime);
    }

    if (iteration == 0) {
        LOGVV("exsl: yield");
        sched_yield();
    } else {
        LOGVV("exsl: sleep for %d", curDelay);
        usleep(curDelay);
    }
    return true;
}


/*
 * Set the "close on exec" flag so we don't expose our file descriptors
 * to processes launched by us.
 */
bool dvmSetCloseOnExec(int fd)
{
    int flags;

    /*
     * There's presently only one flag defined, so getting the previous
     * value of the fd flags is probably unnecessary.
     */
    flags = fcntl(fd, F_GETFD);
    if (flags < 0) {
        ALOGW("Unable to get fd flags for fd %d", fd);
        return false;
    }
    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
        ALOGW("Unable to set close-on-exec for fd %d", fd);
        return false;
    }
    return true;
}

#if (!HAVE_STRLCPY)
/* Implementation of strlcpy() for platforms that don't already have it. */
size_t strlcpy(char *dst, const char *src, size_t size) {
    size_t srcLength = strlen(src);
    size_t copyLength = srcLength;

    if (srcLength > (size - 1)) {
        copyLength = size - 1;
    }

    if (size != 0) {
        strncpy(dst, src, copyLength);
        dst[copyLength] = '\0';
    }

    return srcLength;
}
#endif

/*
 *  Allocates a memory region using ashmem and mmap, initialized to
 *  zero.  Actual allocation rounded up to page multiple.  Returns
 *  NULL on failure.
 */
void *dvmAllocRegion(size_t byteCount, int prot, const char *name) {
    void *base;
    int fd, ret;

    byteCount = ALIGN_UP_TO_PAGE_SIZE(byteCount);
    fd = ashmem_create_region(name, byteCount);
    if (fd == -1) {
        return NULL;
    }
    base = mmap(NULL, byteCount, prot, MAP_PRIVATE, fd, 0);
    ret = close(fd);
    if (base == MAP_FAILED) {
        return NULL;
    }
    if (ret == -1) {
        munmap(base, byteCount);
        return NULL;
    }
    return base;
}

/*
 * Get some per-thread stats.
 *
 * This is currently generated by opening the appropriate "stat" file
 * in /proc and reading the pile of stuff that comes out.
 */
bool dvmGetThreadStats(ProcStatData* pData, pid_t tid)
{
    /*
    int pid;
    char comm[128];
    char state;
    int ppid, pgrp, session, tty_nr, tpgid;
    unsigned long flags, minflt, cminflt, majflt, cmajflt, utime, stime;
    long cutime, cstime, priority, nice, zero, itrealvalue;
    unsigned long starttime, vsize;
    long rss;
    unsigned long rlim, startcode, endcode, startstack, kstkesp, kstkeip;
    unsigned long signal, blocked, sigignore, sigcatch, wchan, nswap, cnswap;
    int exit_signal, processor;
    unsigned long rt_priority, policy;

    scanf("%d %s %c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu %ld %ld %ld "
          "%ld %ld %ld %lu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
          "%lu %lu %lu %d %d %lu %lu",
        &pid, comm, &state, &ppid, &pgrp, &session, &tty_nr, &tpgid,
        &flags, &minflt, &cminflt, &majflt, &cmajflt, &utime, &stime,
        &cutime, &cstime, &priority, &nice, &zero, &itrealvalue,
        &starttime, &vsize, &rss, &rlim, &startcode, &endcode,
        &startstack, &kstkesp, &kstkeip, &signal, &blocked, &sigignore,
        &sigcatch, &wchan, &nswap, &cnswap, &exit_signal, &processor,
        &rt_priority, &policy);

        (new: delayacct_blkio_ticks %llu (since Linux 2.6.18))
    */

    char nameBuf[64];
    int i, fd;

    /*
     * Open and read the appropriate file.  This is expected to work on
     * Linux but will fail on other platforms (e.g. Mac sim).
     */
    sprintf(nameBuf, "/proc/self/task/%d/stat", (int) tid);
    fd = open(nameBuf, O_RDONLY);
    if (fd < 0) {
        ALOGV("Unable to open '%s': %s", nameBuf, strerror(errno));
        return false;
    }

    char lineBuf[512];      /* > 2x typical */
    int cc = read(fd, lineBuf, sizeof(lineBuf)-1);
    if (cc <= 0) {
        const char* msg = (cc == 0) ? "unexpected EOF" : strerror(errno);
        ALOGI("Unable to read '%s': %s", nameBuf, msg);
        close(fd);
        return false;
    }
    close(fd);
    lineBuf[cc] = '\0';

    /*
     * Skip whitespace-separated tokens.  For the most part we can assume
     * that tokens do not contain spaces, and are separated by exactly one
     * space character.  The only exception is the second field ("comm")
     * which may contain spaces but is surrounded by parenthesis.
     */
    char* cp = strchr(lineBuf, ')');
    if (cp == NULL)
        goto parse_fail;
    cp += 2;
    pData->state = *cp++;

    for (i = 3; i < 13; i++) {
        cp = strchr(cp+1, ' ');
        if (cp == NULL)
            goto parse_fail;
    }

    /*
     * Grab utime/stime.
     */
    char* endp;
    pData->utime = strtoul(cp+1, &endp, 10);
    if (endp == cp+1)
        ALOGI("Warning: strtoul failed on utime ('%.30s...')", cp);

    cp = strchr(cp+1, ' ');
    if (cp == NULL)
        goto parse_fail;

    pData->stime = strtoul(cp+1, &endp, 10);
    if (endp == cp+1)
        ALOGI("Warning: strtoul failed on stime ('%.30s...')", cp);

    /*
     * Skip more stuff we don't care about.
     */
    for (i = 14; i < 38; i++) {
        cp = strchr(cp+1, ' ');
        if (cp == NULL)
            goto parse_fail;
    }

    /*
     * Grab processor number.
     */
    pData->processor = strtol(cp+1, &endp, 10);
    if (endp == cp+1)
        ALOGI("Warning: strtoul failed on processor ('%.30s...')", cp);

    return true;

parse_fail:
    ALOGI("stat parse failed (%s)", lineBuf);
    return false;
}

/* documented in header file */
const char* dvmPathToAbsolutePortion(const char* path) {
    if (path == NULL) {
        return NULL;
    }

    if (path[0] == '/') {
        /* It's a regular absolute path. Return it. */
        return path;
    }

    const char* sentinel = strstr(path, "/./");

    if (sentinel != NULL) {
        /* It's got the sentinel. Return a pointer to the second slash. */
        return sentinel + 2;
    }

    return NULL;
}

// From RE2.
void StringAppendV(std::string* dst, const char* format, va_list ap) {
    // First try with a small fixed size buffer
    char space[1024];

    // It's possible for methods that use a va_list to invalidate
    // the data in it upon use.  The fix is to make a copy
    // of the structure before using it and use that copy instead.
    va_list backup_ap;
    va_copy(backup_ap, ap);
    int result = vsnprintf(space, sizeof(space), format, backup_ap);
    va_end(backup_ap);

    if ((result >= 0) && ((size_t) result < sizeof(space))) {
        // It fit
        dst->append(space, result);
        return;
    }

    // Repeatedly increase buffer size until it fits
    int length = sizeof(space);
    while (true) {
        if (result < 0) {
            // Older behavior: just try doubling the buffer size
            length *= 2;
        } else {
            // We need exactly "result+1" characters
            length = result+1;
        }
        char* buf = new char[length];

        // Restore the va_list before we use it again
        va_copy(backup_ap, ap);
        result = vsnprintf(buf, length, format, backup_ap);
        va_end(backup_ap);

        if ((result >= 0) && (result < length)) {
            // It fit
            dst->append(buf, result);
            delete[] buf;
            return;
        }
        delete[] buf;
    }
}

std::string StringPrintf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::string result;
    StringAppendV(&result, fmt, ap);
    va_end(ap);
    return result;
}

void StringAppendF(std::string* dst, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    StringAppendV(dst, format, ap);
    va_end(ap);
}
