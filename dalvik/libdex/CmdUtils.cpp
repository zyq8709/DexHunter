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
 * Some utility functions for use with command-line utilities.
 */
#include "DexFile.h"
#include "ZipArchive.h"
#include "CmdUtils.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
 * Extract "classes.dex" from archive file.
 *
 * If "quiet" is set, don't report common errors.
 */
UnzipToFileResult dexUnzipToFile(const char* zipFileName,
    const char* outFileName, bool quiet)
{
    UnzipToFileResult result = kUTFRSuccess;
    static const char* kFileToExtract = "classes.dex";
    ZipArchive archive;
    ZipEntry entry;
    bool unlinkOnFailure = false;
    int fd = -1;

    if (dexZipOpenArchive(zipFileName, &archive) != 0) {
        if (!quiet) {
            fprintf(stderr, "Unable to open '%s' as zip archive\n",
                zipFileName);
        }
        result = kUTFRNotZip;
        goto bail;
    }

    fd = open(outFileName, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        fprintf(stderr, "Unable to create output file '%s': %s\n",
            outFileName, strerror(errno));
        result = kUTFROutputFileProblem;
        goto bail;
    }

    unlinkOnFailure = true;

    entry = dexZipFindEntry(&archive, kFileToExtract);
    if (entry == NULL) {
        if (!quiet) {
            fprintf(stderr, "Unable to find '%s' in '%s'\n",
                kFileToExtract, zipFileName);
        }
        result = kUTFRNoClassesDex;
        goto bail;
    }

    if (dexZipExtractEntryToFile(&archive, entry, fd) != 0) {
        fprintf(stderr, "Extract of '%s' from '%s' failed\n",
            kFileToExtract, zipFileName);
        result = kUTFRBadZip;
        goto bail;
    }

bail:
    if (fd >= 0)
        close(fd);
    if (unlinkOnFailure && result != kUTFRSuccess)
        unlink(outFileName);
    dexZipCloseArchive(&archive);
    return result;
}

/*
 * Map the specified DEX file read-only (possibly after expanding it into a
 * temp file from a Jar).  Pass in a MemMapping struct to hold the info.
 * If the file is an unoptimized DEX file, then byte-swapping and structural
 * verification are performed on it before the memory is made read-only.
 *
 * The temp file is deleted after the map succeeds.
 *
 * This is intended for use by tools (e.g. dexdump) that need to get a
 * read-only copy of a DEX file that could be in a number of different states.
 *
 * If "tempFileName" is NULL, a default value is used.  The temp file is
 * deleted after the map succeeds.
 *
 * If "quiet" is set, don't report common errors.
 *
 * Returns 0 (kUTFRSuccess) on success.
 */
UnzipToFileResult dexOpenAndMap(const char* fileName, const char* tempFileName,
    MemMapping* pMap, bool quiet)
{
    UnzipToFileResult result = kUTFRGenericFailure;
    int len = strlen(fileName);
    char tempNameBuf[32];
    bool removeTemp = false;
    int fd = -1;

    if (len < 5) {
        if (!quiet) {
            fprintf(stderr,
                "ERROR: filename must end in .dex, .zip, .jar, or .apk\n");
        }
        result = kUTFRBadArgs;
        goto bail;
    }

    if (strcasecmp(fileName + len -3, "dex") != 0) {
        if (tempFileName == NULL) {
            /*
             * Try .zip/.jar/.apk, all of which are Zip archives with
             * "classes.dex" inside.  We need to extract the compressed
             * data to a temp file, the location of which varies.
             *
             * On the device we must use /sdcard because most other
             * directories aren't writable (either because of permissions
             * or because the volume is mounted read-only).  On desktop
             * it's nice to use the designated temp directory.
             */
            if (access("/tmp", W_OK) == 0) {
                sprintf(tempNameBuf, "/tmp/dex-temp-%d", getpid());
            } else if (access("/sdcard", W_OK) == 0) {
                sprintf(tempNameBuf, "/sdcard/dex-temp-%d", getpid());
            } else {
                fprintf(stderr,
                    "NOTE: /tmp and /sdcard unavailable for temp files\n");
                sprintf(tempNameBuf, "dex-temp-%d", getpid());
            }

            tempFileName = tempNameBuf;
        }

        result = dexUnzipToFile(fileName, tempFileName, quiet);

        if (result == kUTFRSuccess) {
            //printf("+++ Good unzip to '%s'\n", tempFileName);
            fileName = tempFileName;
            removeTemp = true;
        } else if (result == kUTFRNotZip) {
            if (!quiet) {
                fprintf(stderr, "Not Zip, retrying as DEX\n");
            }
        } else {
            if (!quiet && result == kUTFRNoClassesDex) {
                fprintf(stderr, "Zip has no classes.dex\n");
            }
            goto bail;
        }
    }

    result = kUTFRGenericFailure;

    /*
     * Pop open the (presumed) DEX file.
     */
    fd = open(fileName, O_RDONLY | O_BINARY);
    if (fd < 0) {
        if (!quiet) {
            fprintf(stderr, "ERROR: unable to open '%s': %s\n",
                fileName, strerror(errno));
        }
        goto bail;
    }

    if (sysMapFileInShmemWritableReadOnly(fd, pMap) != 0) {
        fprintf(stderr, "ERROR: Unable to map '%s'\n", fileName);
        goto bail;
    }

    /*
     * This call will fail if the file exists on a filesystem that
     * doesn't support mprotect(). If that's the case, then the file
     * will have already been mapped private-writable by the previous
     * call, so we don't need to do anything special if this call
     * returns non-zero.
     */
    sysChangeMapAccess(pMap->addr, pMap->length, true, pMap);

    if (dexSwapAndVerifyIfNecessary((u1*) pMap->addr, pMap->length)) {
        fprintf(stderr, "ERROR: Failed structural verification of '%s'\n",
            fileName);
        goto bail;
    }

    /*
     * Similar to above, this call will fail if the file wasn't ever
     * read-only to begin with. This is innocuous, though it is
     * undesirable from a memory hygiene perspective.
     */
    sysChangeMapAccess(pMap->addr, pMap->length, false, pMap);

    /*
     * Success!  Close the file and return with the start/length in pMap.
     */
    result = kUTFRSuccess;

bail:
    if (fd >= 0)
        close(fd);
    if (removeTemp) {
        /* this will fail if the OS doesn't allow removal of a mapped file */
        if (unlink(tempFileName) != 0) {
            fprintf(stderr, "WARNING: unable to remove temp '%s'\n",
                tempFileName);
        }
    }
    return result;
}
