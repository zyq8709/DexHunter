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
 * Open an unoptimized DEX file.
 */

#include "Dalvik.h"
#include "libdex/OptInvocation.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * Copy the given number of bytes from one fd to another, first
 * seeking the source fd to the start of the file.
 */
static int copyFileToFile(int destFd, int srcFd, size_t size)
{
    if (lseek(srcFd, 0, SEEK_SET) != 0) {
        ALOGE("lseek failure: %s", strerror(errno));
        return -1;
    }

    return sysCopyFileToFile(destFd, srcFd, size);
}

/*
 * Get the modification time and size in bytes for the given fd.
 */
static int getModTimeAndSize(int fd, u4* modTime, size_t* size)
{
    struct stat buf;
    int result = fstat(fd, &buf);

    if (result < 0) {
        ALOGE("Unable to determine mod time: %s", strerror(errno));
        return -1;
    }

    *modTime = (u4) buf.st_mtime;
    *size = (size_t) buf.st_size;
    assert((size_t) buf.st_size == buf.st_size);

    return 0;
}

/*
 * Verify the dex file magic number, and get the adler32 checksum out
 * of the given fd, which is presumed to be a reference to a dex file
 * with the cursor at the start of the file. The fd's cursor is
 * modified by this operation.
 */
static int verifyMagicAndGetAdler32(int fd, u4 *adler32)
{
    /*
     * The start of a dex file is eight bytes of magic followed by
     * four bytes of checksum.
     */
    u1 headerStart[12];
    ssize_t amt = read(fd, headerStart, sizeof(headerStart));

    if (amt < 0) {
        ALOGE("Unable to read header: %s", strerror(errno));
        return -1;
    }

    if (amt != sizeof(headerStart)) {
        ALOGE("Unable to read full header (only got %d bytes)", (int) amt);
        return -1;
    }

    if (!dexHasValidMagic((DexHeader*) (void*) headerStart)) {
        return -1;
    }

    /*
     * We can't just cast the data to a u4 and read it, since the
     * platform might be big-endian (also, because that would make the
     * compiler complain about type-punned pointers). We assume here
     * that the dex file is in the standard little-endian format; if
     * that assumption turns out to be invalid, code that runs later
     * will notice and complain.
     */
    *adler32 = (u4) headerStart[8]
        | (((u4) headerStart[9]) << 8)
        | (((u4) headerStart[10]) << 16)
        | (((u4) headerStart[11]) << 24);

    return 0;
}

/* See documentation comment in header. */
int dvmRawDexFileOpen(const char* fileName, const char* odexOutputName,
    RawDexFile** ppRawDexFile, bool isBootstrap)
{
    /*
     * TODO: This duplicates a lot of code from dvmJarFileOpen() in
     * JarFile.c. This should be refactored.
     */

    DvmDex* pDvmDex = NULL;
    char* cachedName = NULL;
    int result = -1;
    int dexFd = -1;
    int optFd = -1;
    u4 modTime = 0;
    u4 adler32 = 0;
    size_t fileSize = 0;
    bool newFile = false;
    bool locked = false;

    dexFd = open(fileName, O_RDONLY);
    if (dexFd < 0) goto bail;

    /* If we fork/exec into dexopt, don't let it inherit the open fd. */
    dvmSetCloseOnExec(dexFd);

    if (verifyMagicAndGetAdler32(dexFd, &adler32) < 0) {
        ALOGE("Error with header for %s", fileName);
        goto bail;
    }

    if (getModTimeAndSize(dexFd, &modTime, &fileSize) < 0) {
        ALOGE("Error with stat for %s", fileName);
        goto bail;
    }

    /*
     * See if the cached file matches. If so, optFd will become a reference
     * to the cached file and will have been seeked to just past the "opt"
     * header.
     */

    if (odexOutputName == NULL) {
        cachedName = dexOptGenerateCacheFileName(fileName, NULL);
        if (cachedName == NULL)
            goto bail;
    } else {
        cachedName = strdup(odexOutputName);
    }

    ALOGV("dvmRawDexFileOpen: Checking cache for %s (%s)",
            fileName, cachedName);

    optFd = dvmOpenCachedDexFile(fileName, cachedName, modTime,
        adler32, isBootstrap, &newFile, /*createIfMissing=*/true);

    if (optFd < 0) {
        ALOGI("Unable to open or create cache for %s (%s)",
                fileName, cachedName);
        goto bail;
    }
    locked = true;

    /*
     * If optFd points to a new file (because there was no cached
     * version, or the cached version was stale), generate the
     * optimized DEX. The file descriptor returned is still locked,
     * and is positioned just past the optimization header.
     */
    if (newFile) {
        u8 startWhen, copyWhen, endWhen;
        bool result;
        off_t dexOffset;

        dexOffset = lseek(optFd, 0, SEEK_CUR);
        result = (dexOffset > 0);

        if (result) {
            startWhen = dvmGetRelativeTimeUsec();
            result = copyFileToFile(optFd, dexFd, fileSize) == 0;
            copyWhen = dvmGetRelativeTimeUsec();
        }

        if (result) {
            result = dvmOptimizeDexFile(optFd, dexOffset, fileSize,
                fileName, modTime, adler32, isBootstrap);
        }

        if (!result) {
            ALOGE("Unable to extract+optimize DEX from '%s'", fileName);
            goto bail;
        }

        endWhen = dvmGetRelativeTimeUsec();
        ALOGD("DEX prep '%s': copy in %dms, rewrite %dms",
            fileName,
            (int) (copyWhen - startWhen) / 1000,
            (int) (endWhen - copyWhen) / 1000);
    }

    /*
     * Map the cached version.  This immediately rewinds the fd, so it
     * doesn't have to be seeked anywhere in particular.
     */
    if (dvmDexFileOpenFromFd(optFd, &pDvmDex) != 0) {
        ALOGI("Unable to map cached %s", fileName);
        goto bail;
    }

    if (locked) {
        /* unlock the fd */
        if (!dvmUnlockCachedDexFile(optFd)) {
            /* uh oh -- this process needs to exit or we'll wedge the system */
            ALOGE("Unable to unlock DEX file");
            goto bail;
        }
        locked = false;
    }

    ALOGV("Successfully opened '%s'", fileName);

    *ppRawDexFile = (RawDexFile*) calloc(1, sizeof(RawDexFile));
    (*ppRawDexFile)->cacheFileName = cachedName;
    (*ppRawDexFile)->pDvmDex = pDvmDex;
    cachedName = NULL;      // don't free it below
    result = 0;

bail:
    free(cachedName);
    if (dexFd >= 0) {
        close(dexFd);
    }
    if (optFd >= 0) {
        if (locked)
            (void) dvmUnlockCachedDexFile(optFd);
        close(optFd);
    }
    return result;
}

/* See documentation comment in header. */
int dvmRawDexFileOpenArray(u1* pBytes, u4 length, RawDexFile** ppRawDexFile)
{
    DvmDex* pDvmDex = NULL;

    if (!dvmPrepareDexInMemory(pBytes, length, &pDvmDex)) {
        ALOGD("Unable to open raw DEX from array");
        return -1;
    }
    assert(pDvmDex != NULL);

    *ppRawDexFile = (RawDexFile*) calloc(1, sizeof(RawDexFile));
    (*ppRawDexFile)->pDvmDex = pDvmDex;

    return 0;
}

/*
 * Close a RawDexFile and free the struct.
 */
void dvmRawDexFileFree(RawDexFile* pRawDexFile)
{
    if (pRawDexFile == NULL)
        return;

    dvmDexFileFree(pRawDexFile->pDvmDex);
    free(pRawDexFile->cacheFileName);
    free(pRawDexFile);
}
