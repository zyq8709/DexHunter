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
 * Command-line DEX optimization and verification entry point.
 *
 * There are three ways to launch this:
 * (1) From the VM.  This takes a dozen args, one of which is a file
 *     descriptor that acts as both input and output.  This allows us to
 *     remain ignorant of where the DEX data originally came from.
 * (2) From installd or another native application.  Pass in a file
 *     descriptor for a zip file, a file descriptor for the output, and
 *     a filename for debug messages.  Many assumptions are made about
 *     what's going on (verification + optimization are enabled, boot
 *     class path is in BOOTCLASSPATH, etc).
 * (3) On the host during a build for preoptimization. This behaves
 *     almost the same as (2), except it takes file names instead of
 *     file descriptors.
 *
 * There are some fragile aspects around bootclasspath entries, owing
 * largely to the VM's history of working on whenever it thought it needed
 * instead of strictly doing what it was told.  If optimizing bootclasspath
 * entries, always do them in the order in which they appear in the path.
 */
#include "Dalvik.h"
#include "libdex/OptInvocation.h"

#include "cutils/log.h"
#include "cutils/process_name.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char* kClassesDex = "classes.dex";


/*
 * Extract "classes.dex" from zipFd into "cacheFd", leaving a little space
 * up front for the DEX optimization header.
 */
static int extractAndProcessZip(int zipFd, int cacheFd,
    const char* debugFileName, bool isBootstrap, const char* bootClassPath,
    const char* dexoptFlagStr)
{
    ZipArchive zippy;
    ZipEntry zipEntry;
    size_t uncompLen;
    long modWhen, crc32;
    off_t dexOffset;
    int err;
    int result = -1;
    int dexoptFlags = 0;        /* bit flags, from enum DexoptFlags */
    DexClassVerifyMode verifyMode = VERIFY_MODE_ALL;
    DexOptimizerMode dexOptMode = OPTIMIZE_MODE_VERIFIED;

    memset(&zippy, 0, sizeof(zippy));

    /* make sure we're still at the start of an empty file */
    if (lseek(cacheFd, 0, SEEK_END) != 0) {
        ALOGE("DexOptZ: new cache file '%s' is not empty", debugFileName);
        goto bail;
    }

    /*
     * Write a skeletal DEX optimization header.  We want the classes.dex
     * to come just after it.
     */
    err = dexOptCreateEmptyHeader(cacheFd);
    if (err != 0)
        goto bail;

    /* record the file position so we can get back here later */
    dexOffset = lseek(cacheFd, 0, SEEK_CUR);
    if (dexOffset < 0)
        goto bail;

    /*
     * Open the zip archive, find the DEX entry.
     */
    if (dexZipPrepArchive(zipFd, debugFileName, &zippy) != 0) {
        ALOGW("DexOptZ: unable to open zip archive '%s'", debugFileName);
        goto bail;
    }

    zipEntry = dexZipFindEntry(&zippy, kClassesDex);
    if (zipEntry == NULL) {
        ALOGW("DexOptZ: zip archive '%s' does not include %s",
            debugFileName, kClassesDex);
        goto bail;
    }

    /*
     * Extract some info about the zip entry.
     */
    if (dexZipGetEntryInfo(&zippy, zipEntry, NULL, &uncompLen, NULL, NULL,
            &modWhen, &crc32) != 0)
    {
        ALOGW("DexOptZ: zip archive GetEntryInfo failed on %s", debugFileName);
        goto bail;
    }

    uncompLen = uncompLen;
    modWhen = modWhen;
    crc32 = crc32;

    /*
     * Extract the DEX data into the cache file at the current offset.
     */
    if (dexZipExtractEntryToFile(&zippy, zipEntry, cacheFd) != 0) {
        ALOGW("DexOptZ: extraction of %s from %s failed",
            kClassesDex, debugFileName);
        goto bail;
    }

    /* Parse the options. */
    if (dexoptFlagStr[0] != '\0') {
        const char* opc;
        const char* val;

        opc = strstr(dexoptFlagStr, "v=");      /* verification */
        if (opc != NULL) {
            switch (*(opc+2)) {
            case 'n':   verifyMode = VERIFY_MODE_NONE;          break;
            case 'r':   verifyMode = VERIFY_MODE_REMOTE;        break;
            case 'a':   verifyMode = VERIFY_MODE_ALL;           break;
            default:                                            break;
            }
        }

        opc = strstr(dexoptFlagStr, "o=");      /* optimization */
        if (opc != NULL) {
            switch (*(opc+2)) {
            case 'n':   dexOptMode = OPTIMIZE_MODE_NONE;        break;
            case 'v':   dexOptMode = OPTIMIZE_MODE_VERIFIED;    break;
            case 'a':   dexOptMode = OPTIMIZE_MODE_ALL;         break;
            case 'f':   dexOptMode = OPTIMIZE_MODE_FULL;        break;
            default:                                            break;
            }
        }

        opc = strstr(dexoptFlagStr, "m=y");     /* register map */
        if (opc != NULL) {
            dexoptFlags |= DEXOPT_GEN_REGISTER_MAPS;
        }

        opc = strstr(dexoptFlagStr, "u=");      /* uniprocessor target */
        if (opc != NULL) {
            switch (*(opc+2)) {
            case 'y':   dexoptFlags |= DEXOPT_UNIPROCESSOR;     break;
            case 'n':   dexoptFlags |= DEXOPT_SMP;              break;
            default:                                            break;
            }
        }
    }

    /*
     * Prep the VM and perform the optimization.
     */

    if (dvmPrepForDexOpt(bootClassPath, dexOptMode, verifyMode,
            dexoptFlags) != 0)
    {
        ALOGE("DexOptZ: VM init failed");
        goto bail;
    }

    //vmStarted = 1;

    /* do the optimization */
    if (!dvmContinueOptimization(cacheFd, dexOffset, uncompLen, debugFileName,
            modWhen, crc32, isBootstrap))
    {
        ALOGE("Optimization failed");
        goto bail;
    }

    /* we don't shut the VM down -- process is about to exit */

    result = 0;

bail:
    dexZipCloseArchive(&zippy);
    return result;
}

/*
 * Common functionality for normal device-side processing as well as
 * preoptimization.
 */
static int processZipFile(int zipFd, int cacheFd, const char* zipName,
        const char *dexoptFlags)
{
    char* bcpCopy = NULL;

    /*
     * Check to see if this is a bootstrap class entry. If so, truncate
     * the path.
     */
    const char* bcp = getenv("BOOTCLASSPATH");
    if (bcp == NULL) {
        ALOGE("DexOptZ: BOOTCLASSPATH not set");
        return -1;
    }

    bool isBootstrap = false;
    const char* match = strstr(bcp, zipName);
    if (match != NULL) {
        /*
         * TODO: we have a partial string match, but that doesn't mean
         * we've matched an entire path component. We should make sure
         * that we're matching on the full zipName, and if not we
         * should re-do the strstr starting at (match+1).
         *
         * The scenario would be a bootclasspath with something like
         * "/system/framework/core.jar" while we're trying to optimize
         * "/framework/core.jar". Not very likely since all paths are
         * absolute and end with ".jar", but not impossible.
         */
        int matchOffset = match - bcp;
        if (matchOffset > 0 && bcp[matchOffset-1] == ':')
            matchOffset--;
        ALOGV("DexOptZ: found '%s' in bootclasspath, cutting off at %d",
            zipName, matchOffset);
        bcpCopy = strdup(bcp);
        bcpCopy[matchOffset] = '\0';

        bcp = bcpCopy;
        ALOGD("DexOptZ: truncated BOOTCLASSPATH to '%s'", bcp);
        isBootstrap = true;
    }

    int result = extractAndProcessZip(zipFd, cacheFd, zipName, isBootstrap,
            bcp, dexoptFlags);

    free(bcpCopy);
    return result;
}

/* advance to the next arg and extract it */
#define GET_ARG(_var, _func, _msg)                                          \
    {                                                                       \
        char* endp;                                                         \
        (_var) = _func(*++argv, &endp, 0);                                  \
        if (*endp != '\0') {                                                \
            ALOGE("%s '%s'", _msg, *argv);                                   \
            goto bail;                                                      \
        }                                                                   \
        --argc;                                                             \
    }

/*
 * Parse arguments.  We want:
 *   0. (name of dexopt command -- ignored)
 *   1. "--zip"
 *   2. zip fd (input, read-only)
 *   3. cache fd (output, read-write, locked with flock)
 *   4. filename of zipfile being optimized (used for debug messages and
 *      for comparing against BOOTCLASSPATH; does not need to be
 *      accessible or even exist)
 *   5. dexopt flags
 *
 * The BOOTCLASSPATH environment variable is assumed to hold the correct
 * boot class path.  If the filename provided appears in the boot class
 * path, the path will be truncated just before that entry (so that, if
 * you were to dexopt "core.jar", your bootclasspath would be empty).
 *
 * This does not try to normalize the boot class path name, so the
 * filename test won't catch you if you get creative.
 */
static int fromZip(int argc, char* const argv[])
{
    int result = -1;
    int zipFd, cacheFd;
    const char* zipName;
    char* bcpCopy = NULL;
    const char* dexoptFlags;

    if (argc != 6) {
        ALOGE("Wrong number of args for --zip (found %d)", argc);
        goto bail;
    }

    /* skip "--zip" */
    argc--;
    argv++;

    GET_ARG(zipFd, strtol, "bad zip fd");
    GET_ARG(cacheFd, strtol, "bad cache fd");
    zipName = *++argv;
    --argc;
    dexoptFlags = *++argv;
    --argc;

    result = processZipFile(zipFd, cacheFd, zipName, dexoptFlags);

bail:
    return result;
}

/*
 * Parse arguments for a preoptimization run. This is when dalvikvm is run
 * on a host to optimize dex files for eventual running on a (different)
 * device. We want:
 *   0. (name of dexopt command -- ignored)
 *   1. "--preopt"
 *   2. zipfile name
 *   3. output file name
 *   4. dexopt flags
 *
 * The BOOTCLASSPATH environment variable is assumed to hold the correct
 * boot class path.  If the filename provided appears in the boot class
 * path, the path will be truncated just before that entry (so that, if
 * you were to dexopt "core.jar", your bootclasspath would be empty).
 *
 * This does not try to normalize the boot class path name, so the
 * filename test won't catch you if you get creative.
 */
static int preopt(int argc, char* const argv[])
{
    int zipFd = -1;
    int outFd = -1;
    int result = -1;

    if (argc != 5) {
        /*
         * Use stderr here, since this variant is meant to be called on
         * the host side.
         */
        fprintf(stderr, "Wrong number of args for --preopt (found %d)\n",
                argc);
        return -1;
    }

    const char* zipName = argv[2];
    const char* outName = argv[3];
    const char* dexoptFlags = argv[4];

    if (strstr(dexoptFlags, "u=y") == NULL &&
        strstr(dexoptFlags, "u=n") == NULL)
    {
        fprintf(stderr, "Either 'u=y' or 'u=n' must be specified\n");
        return -1;
    }

    zipFd = open(zipName, O_RDONLY);
    if (zipFd < 0) {
        perror(argv[0]);
        return -1;
    }

    outFd = open(outName, O_RDWR | O_EXCL | O_CREAT, 0666);
    if (outFd < 0) {
        perror(argv[0]);
        goto bail;
    }

    result = processZipFile(zipFd, outFd, zipName, dexoptFlags);

bail:
    if (zipFd >= 0) {
        close(zipFd);
    }

    if (outFd >= 0) {
        close(outFd);
    }

    return result;
}

/*
 * Parse arguments for an "old-style" invocation directly from the VM.
 *
 * Here's what we want:
 *   0. (name of dexopt command -- ignored)
 *   1. "--dex"
 *   2. DALVIK_VM_BUILD value, as a sanity check
 *   3. file descriptor, locked with flock, for DEX file being optimized
 *   4. DEX offset within file
 *   5. DEX length
 *   6. filename of file being optimized (for debug messages only)
 *   7. modification date of source (goes into dependency section)
 *   8. CRC of source (goes into dependency section)
 *   9. flags (optimization level, isBootstrap)
 *  10. bootclasspath entry #1
 *  11. bootclasspath entry #2
 *   ...
 *
 * dvmOptimizeDexFile() in dalvik/vm/analysis/DexOptimize.c builds the
 * argument list and calls this executable.
 *
 * The bootclasspath entries become the dependencies for this DEX file.
 *
 * The open file descriptor MUST NOT be for one of the bootclasspath files.
 * The parent has the descriptor locked, and we'll try to lock it again as
 * part of processing the bootclasspath.  (We can catch this and return
 * an error by comparing filenames or by opening the bootclasspath files
 * and stat()ing them for inode numbers).
 */
static int fromDex(int argc, char* const argv[])
{
    int result = -1;
    bool vmStarted = false;
    char* bootClassPath = NULL;
    int fd, flags, vmBuildVersion;
    long offset, length;
    const char* debugFileName;
    u4 crc, modWhen;
    char* endp;
    bool onlyOptVerifiedDex = false;
    DexClassVerifyMode verifyMode;
    DexOptimizerMode dexOptMode;

    if (argc < 10) {
        /* don't have all mandatory args */
        ALOGE("Not enough arguments for --dex (found %d)", argc);
        goto bail;
    }

    /* skip "--dex" */
    argc--;
    argv++;

    /*
     * Extract the args.
     */
    GET_ARG(vmBuildVersion, strtol, "bad vm build");
    if (vmBuildVersion != DALVIK_VM_BUILD) {
        ALOGE("DexOpt: build rev does not match VM: %d vs %d",
            vmBuildVersion, DALVIK_VM_BUILD);
        goto bail;
    }
    GET_ARG(fd, strtol, "bad fd");
    GET_ARG(offset, strtol, "bad offset");
    GET_ARG(length, strtol, "bad length");
    debugFileName = *++argv;
    --argc;
    GET_ARG(modWhen, strtoul, "bad modWhen");
    GET_ARG(crc, strtoul, "bad crc");
    GET_ARG(flags, strtol, "bad flags");

    ALOGV("Args: fd=%d off=%ld len=%ld name='%s' mod=%#x crc=%#x flg=%d (argc=%d)",
        fd, offset, length, debugFileName, modWhen, crc, flags, argc);
    assert(argc > 0);

    if (--argc == 0) {
        bootClassPath = strdup("");
    } else {
        int i, bcpLen;
        char* const* argp;
        char* cp;

        bcpLen = 0;
        for (i = 0, argp = argv; i < argc; i++) {
            ++argp;
            ALOGV("DEP: '%s'", *argp);
            bcpLen += strlen(*argp) + 1;
        }

        cp = bootClassPath = (char*) malloc(bcpLen +1);
        for (i = 0, argp = argv; i < argc; i++) {
            int strLen;

            ++argp;
            strLen = strlen(*argp);
            if (i != 0)
                *cp++ = ':';
            memcpy(cp, *argp, strLen);
            cp += strLen;
        }
        *cp = '\0';

        assert((int) strlen(bootClassPath) == bcpLen-1);
    }
    ALOGV("  bootclasspath is '%s'", bootClassPath);

    /* start the VM partway */

    /* ugh -- upgrade these to a bit field if they get any more complex */
    if ((flags & DEXOPT_VERIFY_ENABLED) != 0) {
        if ((flags & DEXOPT_VERIFY_ALL) != 0)
            verifyMode = VERIFY_MODE_ALL;
        else
            verifyMode = VERIFY_MODE_REMOTE;
    } else {
        verifyMode = VERIFY_MODE_NONE;
    }
    if ((flags & DEXOPT_OPT_ENABLED) != 0) {
        if ((flags & DEXOPT_OPT_ALL) != 0)
            dexOptMode = OPTIMIZE_MODE_ALL;
        else
            dexOptMode = OPTIMIZE_MODE_VERIFIED;
    } else {
        dexOptMode = OPTIMIZE_MODE_NONE;
    }

    if (dvmPrepForDexOpt(bootClassPath, dexOptMode, verifyMode, flags) != 0) {
        ALOGE("VM init failed");
        goto bail;
    }

    vmStarted = true;

    /* do the optimization */
    if (!dvmContinueOptimization(fd, offset, length, debugFileName,
            modWhen, crc, (flags & DEXOPT_IS_BOOTSTRAP) != 0))
    {
        ALOGE("Optimization failed");
        goto bail;
    }

    result = 0;

bail:
    /*
     * In theory we should gracefully shut the VM down at this point.  In
     * practice that only matters if we're checking for memory leaks with
     * valgrind -- simply exiting is much faster.
     *
     * As it turns out, the DEX optimizer plays a little fast and loose
     * with class loading.  We load all of the classes from a partially-
     * formed DEX file, which is unmapped when we're done.  If we want to
     * do clean shutdown here, perhaps for testing with valgrind, we need
     * to skip the munmap call there.
     */
#if 0
    if (vmStarted) {
        ALOGI("DexOpt shutting down, result=%d", result);
        dvmShutdown();
    }
#endif

    free(bootClassPath);
    ALOGV("DexOpt command complete (result=%d)", result);
    return result;
}

/*
 * Main entry point.  Decide where to go.
 */
int main(int argc, char* const argv[])
{
    set_process_name("dexopt");

    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc > 1) {
        if (strcmp(argv[1], "--zip") == 0)
            return fromZip(argc, argv);
        else if (strcmp(argv[1], "--dex") == 0)
            return fromDex(argc, argv);
        else if (strcmp(argv[1], "--preopt") == 0)
            return preopt(argc, argv);
    }

    fprintf(stderr,
        "Usage:\n\n"
        "Short version: Don't use this.\n\n"
        "Slightly longer version: This system-internal tool is used to\n"
        "produce optimized dex files. See the source code for details.\n");

    return 1;
}
