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
 * Prepare a DEX file for use by the VM.  Depending upon the VM options
 * we will attempt to verify and/or optimize the code, possibly appending
 * register maps.
 *
 * TODO: the format of the optimized header is currently "whatever we
 * happen to write", since the VM that writes it is by definition the same
 * as the VM that reads it.  Still, it should be better documented and
 * more rigorously structured.
 */
#include "Dalvik.h"
#include "libdex/OptInvocation.h"
#include "analysis/RegisterMap.h"
#include "analysis/Optimize.h"

#include <string>

#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <zlib.h>

/* fwd */
static bool rewriteDex(u1* addr, int len, bool doVerify, bool doOpt,
    DexClassLookup** ppClassLookup, DvmDex** ppDvmDex);
static bool loadAllClasses(DvmDex* pDvmDex);
static void verifyAndOptimizeClasses(DexFile* pDexFile, bool doVerify,
    bool doOpt);
static void verifyAndOptimizeClass(DexFile* pDexFile, ClassObject* clazz,
    const DexClassDef* pClassDef, bool doVerify, bool doOpt);
static void updateChecksum(u1* addr, int len, DexHeader* pHeader);
static int writeDependencies(int fd, u4 modWhen, u4 crc);
static bool writeOptData(int fd, const DexClassLookup* pClassLookup,\
    const RegisterMapBuilder* pRegMapBuilder);
static bool computeFileChecksum(int fd, off_t start, size_t length, u4* pSum);

/*
 * Get just the directory portion of the given path. Equivalent to dirname(3).
 */
static std::string saneDirName(const std::string& path) {
    size_t n = path.rfind('/');
    if (n == std::string::npos) {
        return ".";
    }
    return path.substr(0, n);
}

/*
 * Helper for dvmOpenCacheDexFile() in a known-error case: Check to
 * see if the directory part of the given path (all but the last
 * component) exists and is writable. Complain to the log if not.
 */
static bool directoryIsValid(const std::string& fileName)
{
    std::string dirName(saneDirName(fileName));

    struct stat sb;
    if (stat(dirName.c_str(), &sb) < 0) {
        ALOGE("Could not stat dex cache directory '%s': %s", dirName.c_str(), strerror(errno));
        return false;
    }

    if (!S_ISDIR(sb.st_mode)) {
        ALOGE("Dex cache directory isn't a directory: %s", dirName.c_str());
        return false;
    }

    if (access(dirName.c_str(), W_OK) < 0) {
        ALOGE("Dex cache directory isn't writable: %s", dirName.c_str());
        return false;
    }

    if (access(dirName.c_str(), R_OK) < 0) {
        ALOGE("Dex cache directory isn't readable: %s", dirName.c_str());
        return false;
    }

    return true;
}

/*
 * Return the fd of an open file in the DEX file cache area.  If the cache
 * file doesn't exist or is out of date, this will remove the old entry,
 * create a new one (writing only the file header), and return with the
 * "new file" flag set.
 *
 * It's possible to execute from an unoptimized DEX file directly,
 * assuming the byte ordering and structure alignment is correct, but
 * disadvantageous because some significant optimizations are not possible.
 * It's not generally possible to do the same from an uncompressed Jar
 * file entry, because we have to guarantee 32-bit alignment in the
 * memory-mapped file.
 *
 * For a Jar/APK file (a zip archive with "classes.dex" inside), "modWhen"
 * and "crc32" come from the Zip directory entry.  For a stand-alone DEX
 * file, it's the modification date of the file and the Adler32 from the
 * DEX header (which immediately follows the magic).  If these don't
 * match what's stored in the opt header, we reject the file immediately.
 *
 * On success, the file descriptor will be positioned just past the "opt"
 * file header, and will be locked with flock.  "*pCachedName" will point
 * to newly-allocated storage.
 */
int dvmOpenCachedDexFile(const char* fileName, const char* cacheFileName,
    u4 modWhen, u4 crc, bool isBootstrap, bool* pNewFile, bool createIfMissing)
{
    int fd, cc;
    struct stat fdStat, fileStat;
    bool readOnly = false;

    *pNewFile = false;

retry:
    /*
     * Try to open the cache file.  If we've been asked to,
     * create it if it doesn't exist.
     */
    fd = createIfMissing ? open(cacheFileName, O_CREAT|O_RDWR, 0644) : -1;
    if (fd < 0) {
        fd = open(cacheFileName, O_RDONLY, 0);
        if (fd < 0) {
            if (createIfMissing) {
                // TODO: write an equivalent of strerror_r that returns a std::string.
                const std::string errnoString(strerror(errno));
                if (directoryIsValid(cacheFileName)) {
                    ALOGE("Can't open dex cache file '%s': %s", cacheFileName, errnoString.c_str());
                }
            }
            return fd;
        }
        readOnly = true;
    } else {
        fchmod(fd, 0644);
    }

    /*
     * Grab an exclusive lock on the cache file.  If somebody else is
     * working on it, we'll block here until they complete.  Because
     * we're waiting on an external resource, we go into VMWAIT mode.
     */
    ALOGV("DexOpt: locking cache file %s (fd=%d, boot=%d)",
        cacheFileName, fd, isBootstrap);
    ThreadStatus oldStatus = dvmChangeStatus(NULL, THREAD_VMWAIT);
    cc = flock(fd, LOCK_EX | LOCK_NB);
    if (cc != 0) {
        ALOGD("DexOpt: sleeping on flock(%s)", cacheFileName);
        cc = flock(fd, LOCK_EX);
    }
    dvmChangeStatus(NULL, oldStatus);
    if (cc != 0) {
        ALOGE("Can't lock dex cache '%s': %d", cacheFileName, cc);
        close(fd);
        return -1;
    }
    ALOGV("DexOpt:  locked cache file");

    /*
     * Check to see if the fd we opened and locked matches the file in
     * the filesystem.  If they don't, then somebody else unlinked ours
     * and created a new file, and we need to use that one instead.  (If
     * we caught them between the unlink and the create, we'll get an
     * ENOENT from the file stat.)
     */
    cc = fstat(fd, &fdStat);
    if (cc != 0) {
        ALOGE("Can't stat open file '%s'", cacheFileName);
        LOGVV("DexOpt: unlocking cache file %s", cacheFileName);
        goto close_fail;
    }
    cc = stat(cacheFileName, &fileStat);
    if (cc != 0 ||
        fdStat.st_dev != fileStat.st_dev || fdStat.st_ino != fileStat.st_ino)
    {
        ALOGD("DexOpt: our open cache file is stale; sleeping and retrying");
        LOGVV("DexOpt: unlocking cache file %s", cacheFileName);
        flock(fd, LOCK_UN);
        close(fd);
        usleep(250 * 1000);     /* if something is hosed, don't peg machine */
        goto retry;
    }

    /*
     * We have the correct file open and locked.  If the file size is zero,
     * then it was just created by us, and we want to fill in some fields
     * in the "opt" header and set "*pNewFile".  Otherwise, we want to
     * verify that the fields in the header match our expectations, and
     * reset the file if they don't.
     */
    if (fdStat.st_size == 0) {
        if (readOnly) {
            ALOGW("DexOpt: file has zero length and isn't writable");
            goto close_fail;
        }
        cc = dexOptCreateEmptyHeader(fd);
        if (cc != 0)
            goto close_fail;
        *pNewFile = true;
        ALOGV("DexOpt: successfully initialized new cache file");
    } else {
        bool expectVerify, expectOpt;

        if (gDvm.classVerifyMode == VERIFY_MODE_NONE) {
            expectVerify = false;
        } else if (gDvm.classVerifyMode == VERIFY_MODE_REMOTE) {
            expectVerify = !isBootstrap;
        } else /*if (gDvm.classVerifyMode == VERIFY_MODE_ALL)*/ {
            expectVerify = true;
        }

        if (gDvm.dexOptMode == OPTIMIZE_MODE_NONE) {
            expectOpt = false;
        } else if (gDvm.dexOptMode == OPTIMIZE_MODE_VERIFIED ||
                   gDvm.dexOptMode == OPTIMIZE_MODE_FULL) {
            expectOpt = expectVerify;
        } else /*if (gDvm.dexOptMode == OPTIMIZE_MODE_ALL)*/ {
            expectOpt = true;
        }

        ALOGV("checking deps, expecting vfy=%d opt=%d",
            expectVerify, expectOpt);

        if (!dvmCheckOptHeaderAndDependencies(fd, true, modWhen, crc,
                expectVerify, expectOpt))
        {
            if (readOnly) {
                /*
                 * We could unlink and rewrite the file if we own it or
                 * the "sticky" bit isn't set on the directory.  However,
                 * we're not able to truncate it, which spoils things.  So,
                 * give up now.
                 */
                if (createIfMissing) {
                    ALOGW("Cached DEX '%s' (%s) is stale and not writable",
                        fileName, cacheFileName);
                }
                goto close_fail;
            }

            /*
             * If we truncate the existing file before unlinking it, any
             * process that has it mapped will fail when it tries to touch
             * the pages.
             *
             * This is very important.  The zygote process will have the
             * boot DEX files (core, framework, etc.) mapped early.  If
             * (say) core.dex gets updated, and somebody launches an app
             * that uses App.dex, then App.dex gets reoptimized because it's
             * dependent upon the boot classes.  However, dexopt will be
             * using the *new* core.dex to do the optimizations, while the
             * app will actually be running against the *old* core.dex
             * because it starts from zygote.
             *
             * Even without zygote, it's still possible for a class loader
             * to pull in an APK that was optimized against an older set
             * of DEX files.  We must ensure that everything fails when a
             * boot DEX gets updated, and for general "why aren't my
             * changes doing anything" purposes its best if we just make
             * everything crash when a DEX they're using gets updated.
             */
            ALOGD("ODEX file is stale or bad; removing and retrying (%s)",
                cacheFileName);
            if (ftruncate(fd, 0) != 0) {
                ALOGW("Warning: unable to truncate cache file '%s': %s",
                    cacheFileName, strerror(errno));
                /* keep going */
            }
            if (unlink(cacheFileName) != 0) {
                ALOGW("Warning: unable to remove cache file '%s': %d %s",
                    cacheFileName, errno, strerror(errno));
                /* keep going; permission failure should probably be fatal */
            }
            LOGVV("DexOpt: unlocking cache file %s", cacheFileName);
            flock(fd, LOCK_UN);
            close(fd);
            goto retry;
        } else {
            ALOGV("DexOpt: good deps in cache file");
        }
    }

    assert(fd >= 0);
    return fd;

close_fail:
    flock(fd, LOCK_UN);
    close(fd);
    return -1;
}

/*
 * Unlock the file descriptor.
 *
 * Returns "true" on success.
 */
bool dvmUnlockCachedDexFile(int fd)
{
    LOGVV("DexOpt: unlocking cache file fd=%d", fd);
    return (flock(fd, LOCK_UN) == 0);
}


/*
 * Given a descriptor for a file with DEX data in it, produce an
 * optimized version.
 *
 * The file pointed to by "fd" is expected to be a locked shared resource
 * (or private); we make no efforts to enforce multi-process correctness
 * here.
 *
 * "fileName" is only used for debug output.  "modWhen" and "crc" are stored
 * in the dependency set.
 *
 * The "isBootstrap" flag determines how the optimizer and verifier handle
 * package-scope access checks.  When optimizing, we only load the bootstrap
 * class DEX files and the target DEX, so the flag determines whether the
 * target DEX classes are given a (synthetic) non-NULL classLoader pointer.
 * This only really matters if the target DEX contains classes that claim to
 * be in the same package as bootstrap classes.
 *
 * The optimizer will need to load every class in the target DEX file.
 * This is generally undesirable, so we start a subprocess to do the
 * work and wait for it to complete.
 *
 * Returns "true" on success.  All data will have been written to "fd".
 */
bool dvmOptimizeDexFile(int fd, off_t dexOffset, long dexLength,
    const char* fileName, u4 modWhen, u4 crc, bool isBootstrap)
{
    const char* lastPart = strrchr(fileName, '/');
    if (lastPart != NULL)
        lastPart++;
    else
        lastPart = fileName;

    ALOGD("DexOpt: --- BEGIN '%s' (bootstrap=%d) ---", lastPart, isBootstrap);

    pid_t pid;

    /*
     * This could happen if something in our bootclasspath, which we thought
     * was all optimized, got rejected.
     */
    if (gDvm.optimizing) {
        ALOGW("Rejecting recursive optimization attempt on '%s'", fileName);
        return false;
    }

    pid = fork();
    if (pid == 0) {
        static const int kUseValgrind = 0;
        static const char* kDexOptBin = "/bin/dexopt";
        static const char* kValgrinder = "/usr/bin/valgrind";
        static const int kFixedArgCount = 10;
        static const int kValgrindArgCount = 5;
        static const int kMaxIntLen = 12;   // '-'+10dig+'\0' -OR- 0x+8dig
        int bcpSize = dvmGetBootPathSize();
        int argc = kFixedArgCount + bcpSize
            + (kValgrindArgCount * kUseValgrind);
        const char* argv[argc+1];             // last entry is NULL
        char values[argc][kMaxIntLen];
        char* execFile;
        const char* androidRoot;
        int flags;

        /* change process groups, so we don't clash with ProcessManager */
        setpgid(0, 0);

        /* full path to optimizer */
        androidRoot = getenv("ANDROID_ROOT");
        if (androidRoot == NULL) {
            ALOGW("ANDROID_ROOT not set, defaulting to /system");
            androidRoot = "/system";
        }
        execFile = (char*)alloca(strlen(androidRoot) + strlen(kDexOptBin) + 1);
        strcpy(execFile, androidRoot);
        strcat(execFile, kDexOptBin);

        /*
         * Create arg vector.
         */
        int curArg = 0;

        if (kUseValgrind) {
            /* probably shouldn't ship the hard-coded path */
            argv[curArg++] = (char*)kValgrinder;
            argv[curArg++] = "--tool=memcheck";
            argv[curArg++] = "--leak-check=yes";        // check for leaks too
            argv[curArg++] = "--leak-resolution=med";   // increase from 2 to 4
            argv[curArg++] = "--num-callers=16";        // default is 12
            assert(curArg == kValgrindArgCount);
        }
        argv[curArg++] = execFile;

        argv[curArg++] = "--dex";

        sprintf(values[2], "%d", DALVIK_VM_BUILD);
        argv[curArg++] = values[2];

        sprintf(values[3], "%d", fd);
        argv[curArg++] = values[3];

        sprintf(values[4], "%d", (int) dexOffset);
        argv[curArg++] = values[4];

        sprintf(values[5], "%d", (int) dexLength);
        argv[curArg++] = values[5];

        argv[curArg++] = (char*)fileName;

        sprintf(values[7], "%d", (int) modWhen);
        argv[curArg++] = values[7];

        sprintf(values[8], "%d", (int) crc);
        argv[curArg++] = values[8];

        flags = 0;
        if (gDvm.dexOptMode != OPTIMIZE_MODE_NONE) {
            flags |= DEXOPT_OPT_ENABLED;
            if (gDvm.dexOptMode == OPTIMIZE_MODE_ALL)
                flags |= DEXOPT_OPT_ALL;
        }
        if (gDvm.classVerifyMode != VERIFY_MODE_NONE) {
            flags |= DEXOPT_VERIFY_ENABLED;
            if (gDvm.classVerifyMode == VERIFY_MODE_ALL)
                flags |= DEXOPT_VERIFY_ALL;
        }
        if (isBootstrap)
            flags |= DEXOPT_IS_BOOTSTRAP;
        if (gDvm.generateRegisterMaps)
            flags |= DEXOPT_GEN_REGISTER_MAPS;
        sprintf(values[9], "%d", flags);
        argv[curArg++] = values[9];

        assert(((!kUseValgrind && curArg == kFixedArgCount) ||
               ((kUseValgrind && curArg == kFixedArgCount+kValgrindArgCount))));

        ClassPathEntry* cpe;
        for (cpe = gDvm.bootClassPath; cpe->ptr != NULL; cpe++) {
            argv[curArg++] = cpe->fileName;
        }
        assert(curArg == argc);

        argv[curArg] = NULL;

        if (kUseValgrind)
            execv(kValgrinder, const_cast<char**>(argv));
        else
            execv(execFile, const_cast<char**>(argv));

        ALOGE("execv '%s'%s failed: %s", execFile,
            kUseValgrind ? " [valgrind]" : "", strerror(errno));
        exit(1);
    } else {
        ALOGV("DexOpt: waiting for verify+opt, pid=%d", (int) pid);
        int status;
        pid_t gotPid;

        /*
         * Wait for the optimization process to finish.  We go into VMWAIT
         * mode here so GC suspension won't have to wait for us.
         */
        ThreadStatus oldStatus = dvmChangeStatus(NULL, THREAD_VMWAIT);
        while (true) {
            gotPid = waitpid(pid, &status, 0);
            if (gotPid == -1 && errno == EINTR) {
                ALOGD("waitpid interrupted, retrying");
            } else {
                break;
            }
        }
        dvmChangeStatus(NULL, oldStatus);
        if (gotPid != pid) {
            ALOGE("waitpid failed: wanted %d, got %d: %s",
                (int) pid, (int) gotPid, strerror(errno));
            return false;
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            ALOGD("DexOpt: --- END '%s' (success) ---", lastPart);
            return true;
        } else {
            ALOGW("DexOpt: --- END '%s' --- status=0x%04x, process failed",
                lastPart, status);
            return false;
        }
    }
}

/*
 * Do the actual optimization.  This is executed in the dexopt process.
 *
 * For best use of disk/memory, we want to extract once and perform
 * optimizations in place.  If the file has to expand or contract
 * to match local structure padding/alignment expectations, we want
 * to do the rewrite as part of the extract, rather than extracting
 * into a temp file and slurping it back out.  (The structure alignment
 * is currently correct for all platforms, and this isn't expected to
 * change, so we should be okay with having it already extracted.)
 *
 * Returns "true" on success.
 */
bool dvmContinueOptimization(int fd, off_t dexOffset, long dexLength,
    const char* fileName, u4 modWhen, u4 crc, bool isBootstrap)
{
    DexClassLookup* pClassLookup = NULL;
    RegisterMapBuilder* pRegMapBuilder = NULL;

    assert(gDvm.optimizing);

    ALOGV("Continuing optimization (%s, isb=%d)", fileName, isBootstrap);

    assert(dexOffset >= 0);

    /* quick test so we don't blow up on empty file */
    if (dexLength < (int) sizeof(DexHeader)) {
        ALOGE("too small to be DEX");
        return false;
    }
    if (dexOffset < (int) sizeof(DexOptHeader)) {
        ALOGE("not enough room for opt header");
        return false;
    }

    bool result = false;

    /*
     * Drop this into a global so we don't have to pass it around.  We could
     * also add a field to DexFile, but since it only pertains to DEX
     * creation that probably doesn't make sense.
     */
    gDvm.optimizingBootstrapClass = isBootstrap;

    {
        /*
         * Map the entire file (so we don't have to worry about page
         * alignment).  The expectation is that the output file contains
         * our DEX data plus room for a small header.
         */
        bool success;
        void* mapAddr;
        mapAddr = mmap(NULL, dexOffset + dexLength, PROT_READ|PROT_WRITE,
                    MAP_SHARED, fd, 0);
        if (mapAddr == MAP_FAILED) {
            ALOGE("unable to mmap DEX cache: %s", strerror(errno));
            goto bail;
        }

        bool doVerify, doOpt;
        if (gDvm.classVerifyMode == VERIFY_MODE_NONE) {
            doVerify = false;
        } else if (gDvm.classVerifyMode == VERIFY_MODE_REMOTE) {
            doVerify = !gDvm.optimizingBootstrapClass;
        } else /*if (gDvm.classVerifyMode == VERIFY_MODE_ALL)*/ {
            doVerify = true;
        }

        if (gDvm.dexOptMode == OPTIMIZE_MODE_NONE) {
            doOpt = false;
        } else if (gDvm.dexOptMode == OPTIMIZE_MODE_VERIFIED ||
                   gDvm.dexOptMode == OPTIMIZE_MODE_FULL) {
            doOpt = doVerify;
        } else /*if (gDvm.dexOptMode == OPTIMIZE_MODE_ALL)*/ {
            doOpt = true;
        }

        /*
         * Rewrite the file.  Byte reordering, structure realigning,
         * class verification, and bytecode optimization are all performed
         * here.
         *
         * In theory the file could change size and bits could shift around.
         * In practice this would be annoying to deal with, so the file
         * layout is designed so that it can always be rewritten in place.
         *
         * This creates the class lookup table as part of doing the processing.
         */
        success = rewriteDex(((u1*) mapAddr) + dexOffset, dexLength,
                    doVerify, doOpt, &pClassLookup, NULL);

        if (success) {
            DvmDex* pDvmDex = NULL;
            u1* dexAddr = ((u1*) mapAddr) + dexOffset;

            if (dvmDexFileOpenPartial(dexAddr, dexLength, &pDvmDex) != 0) {
                ALOGE("Unable to create DexFile");
                success = false;
            } else {
                /*
                 * If configured to do so, generate register map output
                 * for all verified classes.  The register maps were
                 * generated during verification, and will now be serialized.
                 */
                if (gDvm.generateRegisterMaps) {
                    pRegMapBuilder = dvmGenerateRegisterMaps(pDvmDex);
                    if (pRegMapBuilder == NULL) {
                        ALOGE("Failed generating register maps");
                        success = false;
                    }
                }

                DexHeader* pHeader = (DexHeader*)pDvmDex->pHeader;
                updateChecksum(dexAddr, dexLength, pHeader);

                dvmDexFileFree(pDvmDex);
            }
        }

        /* unmap the read-write version, forcing writes to disk */
        if (msync(mapAddr, dexOffset + dexLength, MS_SYNC) != 0) {
            ALOGW("msync failed: %s", strerror(errno));
            // weird, but keep going
        }
#if 1
        /*
         * This causes clean shutdown to fail, because we have loaded classes
         * that point into it.  For the optimizer this isn't a problem,
         * because it's more efficient for the process to simply exit.
         * Exclude this code when doing clean shutdown for valgrind.
         */
        if (munmap(mapAddr, dexOffset + dexLength) != 0) {
            ALOGE("munmap failed: %s", strerror(errno));
            goto bail;
        }
#endif

        if (!success)
            goto bail;
    }

    /* get start offset, and adjust deps start for 64-bit alignment */
    off_t depsOffset, optOffset, endOffset, adjOffset;
    int depsLength, optLength;
    u4 optChecksum;

    depsOffset = lseek(fd, 0, SEEK_END);
    if (depsOffset < 0) {
        ALOGE("lseek to EOF failed: %s", strerror(errno));
        goto bail;
    }
    adjOffset = (depsOffset + 7) & ~(0x07);
    if (adjOffset != depsOffset) {
        ALOGV("Adjusting deps start from %d to %d",
            (int) depsOffset, (int) adjOffset);
        depsOffset = adjOffset;
        lseek(fd, depsOffset, SEEK_SET);
    }

    /*
     * Append the dependency list.
     */
    if (writeDependencies(fd, modWhen, crc) != 0) {
        ALOGW("Failed writing dependencies");
        goto bail;
    }

    /* compute deps length, then adjust opt start for 64-bit alignment */
    optOffset = lseek(fd, 0, SEEK_END);
    depsLength = optOffset - depsOffset;

    adjOffset = (optOffset + 7) & ~(0x07);
    if (adjOffset != optOffset) {
        ALOGV("Adjusting opt start from %d to %d",
            (int) optOffset, (int) adjOffset);
        optOffset = adjOffset;
        lseek(fd, optOffset, SEEK_SET);
    }

    /*
     * Append any optimized pre-computed data structures.
     */
    if (!writeOptData(fd, pClassLookup, pRegMapBuilder)) {
        ALOGW("Failed writing opt data");
        goto bail;
    }

    endOffset = lseek(fd, 0, SEEK_END);
    optLength = endOffset - optOffset;

    /* compute checksum from start of deps to end of opt area */
    if (!computeFileChecksum(fd, depsOffset,
            (optOffset+optLength) - depsOffset, &optChecksum))
    {
        goto bail;
    }

    /*
     * Output the "opt" header with all values filled in and a correct
     * magic number.
     */
    DexOptHeader optHdr;
    memset(&optHdr, 0xff, sizeof(optHdr));
    memcpy(optHdr.magic, DEX_OPT_MAGIC, 4);
    memcpy(optHdr.magic+4, DEX_OPT_MAGIC_VERS, 4);
    optHdr.dexOffset = (u4) dexOffset;
    optHdr.dexLength = (u4) dexLength;
    optHdr.depsOffset = (u4) depsOffset;
    optHdr.depsLength = (u4) depsLength;
    optHdr.optOffset = (u4) optOffset;
    optHdr.optLength = (u4) optLength;
#if __BYTE_ORDER != __LITTLE_ENDIAN
    optHdr.flags = DEX_OPT_FLAG_BIG;
#else
    optHdr.flags = 0;
#endif
    optHdr.checksum = optChecksum;

    fsync(fd);      /* ensure previous writes go before header is written */

    lseek(fd, 0, SEEK_SET);
    if (sysWriteFully(fd, &optHdr, sizeof(optHdr), "DexOpt opt header") != 0)
        goto bail;

    ALOGV("Successfully wrote DEX header");
    result = true;

    //dvmRegisterMapDumpStats();

bail:
    dvmFreeRegisterMapBuilder(pRegMapBuilder);
    free(pClassLookup);
    return result;
}

/*
 * Prepare an in-memory DEX file.
 *
 * The data was presented to the VM as a byte array rather than a file.
 * We want to do the same basic set of operations, but we can just leave
 * them in memory instead of writing them out to a cached optimized DEX file.
 */
bool dvmPrepareDexInMemory(u1* addr, size_t len, DvmDex** ppDvmDex)
{
    DexClassLookup* pClassLookup = NULL;

    /*
     * Byte-swap, realign, verify basic DEX file structure.
     *
     * We could load + verify + optimize here as well, but that's probably
     * not desirable.
     *
     * (The bulk-verification code is currently only setting the DEX
     * file's "verified" flag, not updating the ClassObject.  This would
     * also need to be changed, or we will try to verify the class twice,
     * and possibly reject it when optimized opcodes are encountered.)
     */
    if (!rewriteDex(addr, len, false, false, &pClassLookup, ppDvmDex)) {
        return false;
    }

    (*ppDvmDex)->pDexFile->pClassLookup = pClassLookup;

    return true;
}

/*
 * Perform in-place rewrites on a memory-mapped DEX file.
 *
 * If this is called from a short-lived child process (dexopt), we can
 * go nutty with loading classes and allocating memory.  When it's
 * called to prepare classes provided in a byte array, we may want to
 * be more conservative.
 *
 * If "ppClassLookup" is non-NULL, a pointer to a newly-allocated
 * DexClassLookup will be returned on success.
 *
 * If "ppDvmDex" is non-NULL, a newly-allocated DvmDex struct will be
 * returned on success.
 */
static bool rewriteDex(u1* addr, int len, bool doVerify, bool doOpt,
    DexClassLookup** ppClassLookup, DvmDex** ppDvmDex)
{
    DexClassLookup* pClassLookup = NULL;
    u8 prepWhen, loadWhen, verifyOptWhen;
    DvmDex* pDvmDex = NULL;
    bool result = false;
    const char* msgStr = "???";

    /* if the DEX is in the wrong byte order, swap it now */
    if (dexSwapAndVerify(addr, len) != 0)
        goto bail;

    /*
     * Now that the DEX file can be read directly, create a DexFile struct
     * for it.
     */
    if (dvmDexFileOpenPartial(addr, len, &pDvmDex) != 0) {
        ALOGE("Unable to create DexFile");
        goto bail;
    }

    /*
     * Create the class lookup table.  This will eventually be appended
     * to the end of the .odex.
     *
     * We create a temporary link from the DexFile for the benefit of
     * class loading, below.
     */
    pClassLookup = dexCreateClassLookup(pDvmDex->pDexFile);
    if (pClassLookup == NULL)
        goto bail;
    pDvmDex->pDexFile->pClassLookup = pClassLookup;

    /*
     * If we're not going to attempt to verify or optimize the classes,
     * there's no value in loading them, so bail out early.
     */
    if (!doVerify && !doOpt) {
        result = true;
        goto bail;
    }

    prepWhen = dvmGetRelativeTimeUsec();

    /*
     * Load all classes found in this DEX file.  If they fail to load for
     * some reason, they won't get verified (which is as it should be).
     */
    if (!loadAllClasses(pDvmDex))
        goto bail;
    loadWhen = dvmGetRelativeTimeUsec();

    /*
     * Create a data structure for use by the bytecode optimizer.
     * We need to look up methods in a few classes, so this may cause
     * a bit of class loading.  We usually do this during VM init, but
     * for dexopt on core.jar the order of operations gets a bit tricky,
     * so we defer it to here.
     */
    if (!dvmCreateInlineSubsTable())
        goto bail;

    /*
     * Verify and optimize all classes in the DEX file (command-line
     * options permitting).
     *
     * This is best-effort, so there's really no way for dexopt to
     * fail at this point.
     */
    verifyAndOptimizeClasses(pDvmDex->pDexFile, doVerify, doOpt);
    verifyOptWhen = dvmGetRelativeTimeUsec();

    if (doVerify && doOpt)
        msgStr = "verify+opt";
    else if (doVerify)
        msgStr = "verify";
    else if (doOpt)
        msgStr = "opt";
    ALOGD("DexOpt: load %dms, %s %dms, %d bytes",
        (int) (loadWhen - prepWhen) / 1000,
        msgStr,
        (int) (verifyOptWhen - loadWhen) / 1000,
        gDvm.pBootLoaderAlloc->curOffset);

    result = true;

bail:
    /*
     * On success, return the pieces that the caller asked for.
     */

    if (pDvmDex != NULL) {
        /* break link between the two */
        pDvmDex->pDexFile->pClassLookup = NULL;
    }

    if (ppDvmDex == NULL || !result) {
        dvmDexFileFree(pDvmDex);
    } else {
        *ppDvmDex = pDvmDex;
    }

    if (ppClassLookup == NULL || !result) {
        free(pClassLookup);
    } else {
        *ppClassLookup = pClassLookup;
    }

    return result;
}

/*
 * Try to load all classes in the specified DEX.  If they have some sort
 * of broken dependency, e.g. their superclass lives in a different DEX
 * that wasn't previously loaded into the bootstrap class path, loading
 * will fail.  This is the desired behavior.
 *
 * We have no notion of class loader at this point, so we load all of
 * the classes with the bootstrap class loader.  It turns out this has
 * exactly the behavior we want, and has no ill side effects because we're
 * running in a separate process and anything we load here will be forgotten.
 *
 * We set the CLASS_MULTIPLE_DEFS flag here if we see multiple definitions.
 * This works because we only call here as part of optimization / pre-verify,
 * not during verification as part of loading a class into a running VM.
 *
 * This returns "false" if the world is too screwed up to do anything
 * useful at all.
 */
static bool loadAllClasses(DvmDex* pDvmDex)
{
    u4 count = pDvmDex->pDexFile->pHeader->classDefsSize;
    u4 idx;
    int loaded = 0;

    ALOGV("DexOpt: +++ trying to load %d classes", count);

    dvmSetBootPathExtraDex(pDvmDex);

    /*
     * At this point, it is safe -- and necessary! -- to look up the
     * VM's required classes and members, even when what we are in the
     * process of processing is the core library that defines these
     * classes itself. (The reason it is necessary is that in the act
     * of initializing the class Class, below, the system will end up
     * referring to many of the class references that got set up by
     * this call.)
     */
    if (!dvmFindRequiredClassesAndMembers()) {
        return false;
    }

    /*
     * We have some circularity issues with Class and Object that are
     * most easily avoided by ensuring that Object is never the first
     * thing we try to find-and-initialize. The call to
     * dvmFindSystemClass() here takes care of that situation. (We
     * only need to do this when loading classes from the DEX file
     * that contains Object, and only when Object comes first in the
     * list, but it costs very little to do it in all cases.)
     */
    if (!dvmInitClass(gDvm.classJavaLangClass)) {
        ALOGE("ERROR: failed to initialize the class Class!");
        return false;
    }

    for (idx = 0; idx < count; idx++) {
        const DexClassDef* pClassDef;
        const char* classDescriptor;
        ClassObject* newClass;

        pClassDef = dexGetClassDef(pDvmDex->pDexFile, idx);
        classDescriptor =
            dexStringByTypeIdx(pDvmDex->pDexFile, pClassDef->classIdx);

        ALOGV("+++  loading '%s'", classDescriptor);
        //newClass = dvmDefineClass(pDexFile, classDescriptor,
        //        NULL);
        newClass = dvmFindSystemClassNoInit(classDescriptor);
        if (newClass == NULL) {
            ALOGV("DexOpt: failed loading '%s'", classDescriptor);
            dvmClearOptException(dvmThreadSelf());
        } else if (newClass->pDvmDex != pDvmDex) {
            /*
             * We don't load the new one, and we tag the first one found
             * with the "multiple def" flag so the resolver doesn't try
             * to make it available.
             */
            ALOGD("DexOpt: '%s' has an earlier definition; blocking out",
                classDescriptor);
            SET_CLASS_FLAG(newClass, CLASS_MULTIPLE_DEFS);
        } else {
            loaded++;
        }
    }
    ALOGV("DexOpt: +++ successfully loaded %d classes", loaded);

    dvmSetBootPathExtraDex(NULL);
    return true;
}

/*
 * Verify and/or optimize all classes that were successfully loaded from
 * this DEX file.
 */
static void verifyAndOptimizeClasses(DexFile* pDexFile, bool doVerify,
    bool doOpt)
{
    u4 count = pDexFile->pHeader->classDefsSize;
    u4 idx;

    for (idx = 0; idx < count; idx++) {
        const DexClassDef* pClassDef;
        const char* classDescriptor;
        ClassObject* clazz;

        pClassDef = dexGetClassDef(pDexFile, idx);
        classDescriptor = dexStringByTypeIdx(pDexFile, pClassDef->classIdx);

        /* all classes are loaded into the bootstrap class loader */
        clazz = dvmLookupClass(classDescriptor, NULL, false);
        if (clazz != NULL) {
            verifyAndOptimizeClass(pDexFile, clazz, pClassDef, doVerify, doOpt);

        } else {
            // TODO: log when in verbose mode
            ALOGV("DexOpt: not optimizing unavailable class '%s'",
                classDescriptor);
        }
    }

#ifdef VERIFIER_STATS
    ALOGI("Verifier stats:");
    ALOGI(" methods examined        : %u", gDvm.verifierStats.methodsExamined);
    ALOGI(" monitor-enter methods   : %u", gDvm.verifierStats.monEnterMethods);
    ALOGI(" instructions examined   : %u", gDvm.verifierStats.instrsExamined);
    ALOGI(" instructions re-examined: %u", gDvm.verifierStats.instrsReexamined);
    ALOGI(" copying of register sets: %u", gDvm.verifierStats.copyRegCount);
    ALOGI(" merging of register sets: %u", gDvm.verifierStats.mergeRegCount);
    ALOGI(" ...that caused changes  : %u", gDvm.verifierStats.mergeRegChanged);
    ALOGI(" uninit searches         : %u", gDvm.verifierStats.uninitSearches);
    ALOGI(" max memory required     : %u", gDvm.verifierStats.biggestAlloc);
#endif
}

/*
 * Verify and/or optimize a specific class.
 */
static void verifyAndOptimizeClass(DexFile* pDexFile, ClassObject* clazz,
    const DexClassDef* pClassDef, bool doVerify, bool doOpt)
{
    const char* classDescriptor;
    bool verified = false;

    if (clazz->pDvmDex->pDexFile != pDexFile) {
        /*
         * The current DEX file defined a class that is also present in the
         * bootstrap class path.  The class loader favored the bootstrap
         * version, which means that we have a pointer to a class that is
         * (a) not the one we want to examine, and (b) mapped read-only,
         * so we will seg fault if we try to rewrite instructions inside it.
         */
        ALOGD("DexOpt: not verifying/optimizing '%s': multiple definitions",
            clazz->descriptor);
        return;
    }

    classDescriptor = dexStringByTypeIdx(pDexFile, pClassDef->classIdx);

    /*
     * First, try to verify it.
     */
    if (doVerify) {
        if (dvmVerifyClass(clazz)) {
            /*
             * Set the "is preverified" flag in the DexClassDef.  We
             * do it here, rather than in the ClassObject structure,
             * because the DexClassDef is part of the odex file.
             */
            assert((clazz->accessFlags & JAVA_FLAGS_MASK) ==
                pClassDef->accessFlags);
            ((DexClassDef*)pClassDef)->accessFlags |= CLASS_ISPREVERIFIED;
            verified = true;
        } else {
            // TODO: log when in verbose mode
            ALOGV("DexOpt: '%s' failed verification", classDescriptor);
        }
    }

    if (doOpt) {
        bool needVerify = (gDvm.dexOptMode == OPTIMIZE_MODE_VERIFIED ||
                           gDvm.dexOptMode == OPTIMIZE_MODE_FULL);
        if (!verified && needVerify) {
            ALOGV("DexOpt: not optimizing '%s': not verified",
                classDescriptor);
        } else {
            dvmOptimizeClass(clazz, false);

            /* set the flag whether or not we actually changed anything */
            ((DexClassDef*)pClassDef)->accessFlags |= CLASS_ISOPTIMIZED;
        }
    }
}


/*
 * Get the cache file name from a ClassPathEntry.
 */
static const char* getCacheFileName(const ClassPathEntry* cpe)
{
    switch (cpe->kind) {
    case kCpeJar:
        return dvmGetJarFileCacheFileName((JarFile*) cpe->ptr);
    case kCpeDex:
        return dvmGetRawDexFileCacheFileName((RawDexFile*) cpe->ptr);
    default:
        ALOGE("DexOpt: unexpected cpe kind %d", cpe->kind);
        dvmAbort();
        return NULL;
    }
}

/*
 * Get the SHA-1 signature.
 */
static const u1* getSignature(const ClassPathEntry* cpe)
{
    DvmDex* pDvmDex;

    switch (cpe->kind) {
    case kCpeJar:
        pDvmDex = dvmGetJarFileDex((JarFile*) cpe->ptr);
        break;
    case kCpeDex:
        pDvmDex = dvmGetRawDexFileDex((RawDexFile*) cpe->ptr);
        break;
    default:
        ALOGE("unexpected cpe kind %d", cpe->kind);
        dvmAbort();
        pDvmDex = NULL;         // make gcc happy
    }

    assert(pDvmDex != NULL);
    return pDvmDex->pDexFile->pHeader->signature;
}


/*
 * Dependency layout:
 *  4b  Source file modification time, in seconds since 1970 UTC
 *  4b  CRC-32 from Zip entry, or Adler32 from source DEX header
 *  4b  Dalvik VM build number
 *  4b  Number of dependency entries that follow
 *  Dependency entries:
 *    4b  Name length (including terminating null)
 *    var Full path of cache entry (null terminated)
 *    20b SHA-1 signature from source DEX file
 *
 * If this changes, update DEX_OPT_MAGIC_VERS.
 */
static const size_t kMinDepSize = 4 * 4;
static const size_t kMaxDepSize = 4 * 4 + 2048;     // sanity check

/*
 * Read the "opt" header, verify it, then read the dependencies section
 * and verify that data as well.
 *
 * If "sourceAvail" is "true", this will verify that "modWhen" and "crc"
 * match up with what is stored in the header.  If they don't, we reject
 * the file so that it can be recreated from the updated original.  If
 * "sourceAvail" isn't set, e.g. for a .odex file, we ignore these arguments.
 *
 * On successful return, the file will be seeked immediately past the
 * "opt" header.
 */
bool dvmCheckOptHeaderAndDependencies(int fd, bool sourceAvail, u4 modWhen,
    u4 crc, bool expectVerify, bool expectOpt)
{
    DexOptHeader optHdr;
    u1* depData = NULL;
    const u1* magic;
    off_t posn;
    int result = false;
    ssize_t actual;

    /*
     * Start at the start.  The "opt" header, when present, will always be
     * the first thing in the file.
     */
    if (lseek(fd, 0, SEEK_SET) != 0) {
        ALOGE("DexOpt: failed to seek to start of file: %s", strerror(errno));
        goto bail;
    }

    /*
     * Read and do trivial verification on the opt header.  The header is
     * always in host byte order.
     */
    actual = read(fd, &optHdr, sizeof(optHdr));
    if (actual < 0) {
        ALOGE("DexOpt: failed reading opt header: %s", strerror(errno));
        goto bail;
    } else if (actual != sizeof(optHdr)) {
        ALOGE("DexOpt: failed reading opt header (got %d of %zd)",
            (int) actual, sizeof(optHdr));
        goto bail;
    }

    magic = optHdr.magic;
    if (memcmp(magic, DEX_MAGIC, 4) == 0) {
        /* somebody probably pointed us at the wrong file */
        ALOGD("DexOpt: expected optimized DEX, found unoptimized");
        goto bail;
    } else if (memcmp(magic, DEX_OPT_MAGIC, 4) != 0) {
        /* not a DEX file, or previous attempt was interrupted */
        ALOGD("DexOpt: incorrect opt magic number (0x%02x %02x %02x %02x)",
            magic[0], magic[1], magic[2], magic[3]);
        goto bail;
    }
    if (memcmp(magic+4, DEX_OPT_MAGIC_VERS, 4) != 0) {
        ALOGW("DexOpt: stale opt version (0x%02x %02x %02x %02x)",
            magic[4], magic[5], magic[6], magic[7]);
        goto bail;
    }
    if (optHdr.depsLength < kMinDepSize || optHdr.depsLength > kMaxDepSize) {
        ALOGW("DexOpt: weird deps length %d, bailing", optHdr.depsLength);
        goto bail;
    }

    /*
     * Do the header flags match up with what we want?
     *
     * The only thing we really can't handle is incorrect byte ordering.
     */
    {
        const u4 matchMask = DEX_OPT_FLAG_BIG;
        u4 expectedFlags = 0;
#if __BYTE_ORDER != __LITTLE_ENDIAN
        expectedFlags |= DEX_OPT_FLAG_BIG;
#endif
        if ((expectedFlags & matchMask) != (optHdr.flags & matchMask)) {
            ALOGI("DexOpt: header flag mismatch (0x%02x vs 0x%02x, mask=0x%02x)",
                expectedFlags, optHdr.flags, matchMask);
            goto bail;
        }
    }

    posn = lseek(fd, optHdr.depsOffset, SEEK_SET);
    if (posn < 0) {
        ALOGW("DexOpt: seek to deps failed: %s", strerror(errno));
        goto bail;
    }

    /*
     * Read all of the dependency stuff into memory.
     */
    depData = (u1*) malloc(optHdr.depsLength);
    if (depData == NULL) {
        ALOGW("DexOpt: unable to allocate %d bytes for deps",
            optHdr.depsLength);
        goto bail;
    }
    actual = read(fd, depData, optHdr.depsLength);
    if (actual < 0) {
        ALOGW("DexOpt: failed reading deps: %s", strerror(errno));
        goto bail;
    } else if (actual != (ssize_t) optHdr.depsLength) {
        ALOGW("DexOpt: failed reading deps: got %d of %d",
            (int) actual, optHdr.depsLength);
        goto bail;
    }

    /*
     * Verify simple items.
     */
    const u1* ptr;
    u4 val;

    ptr = depData;
    val = read4LE(&ptr);
    if (sourceAvail && val != modWhen) {
        ALOGI("DexOpt: source file mod time mismatch (%08x vs %08x)",
            val, modWhen);
        goto bail;
    }
    val = read4LE(&ptr);
    if (sourceAvail && val != crc) {
        ALOGI("DexOpt: source file CRC mismatch (%08x vs %08x)", val, crc);
        goto bail;
    }
    val = read4LE(&ptr);
    if (val != DALVIK_VM_BUILD) {
        ALOGD("DexOpt: VM build version mismatch (%d vs %d)",
            val, DALVIK_VM_BUILD);
        goto bail;
    }

    /*
     * Verify dependencies on other cached DEX files.  It must match
     * exactly with what is currently defined in the bootclasspath.
     */
    ClassPathEntry* cpe;
    u4 numDeps;

    numDeps = read4LE(&ptr);
    ALOGV("+++ DexOpt: numDeps = %d", numDeps);
    for (cpe = gDvm.bootClassPath; cpe->ptr != NULL; cpe++) {
        const char* cacheFileName =
            dvmPathToAbsolutePortion(getCacheFileName(cpe));
        assert(cacheFileName != NULL); /* guaranteed by Class.c */

        const u1* signature = getSignature(cpe);
        size_t len = strlen(cacheFileName) +1;
        u4 storedStrLen;

        if (numDeps == 0) {
            /* more entries in bootclasspath than in deps list */
            ALOGI("DexOpt: not all deps represented");
            goto bail;
        }

        storedStrLen = read4LE(&ptr);
        if (len != storedStrLen ||
            strcmp(cacheFileName, (const char*) ptr) != 0)
        {
            ALOGI("DexOpt: mismatch dep name: '%s' vs. '%s'",
                cacheFileName, ptr);
            goto bail;
        }

        ptr += storedStrLen;

        if (memcmp(signature, ptr, kSHA1DigestLen) != 0) {
            ALOGI("DexOpt: mismatch dep signature for '%s'", cacheFileName);
            goto bail;
        }
        ptr += kSHA1DigestLen;

        ALOGV("DexOpt: dep match on '%s'", cacheFileName);

        numDeps--;
    }

    if (numDeps != 0) {
        /* more entries in deps list than in classpath */
        ALOGI("DexOpt: Some deps went away");
        goto bail;
    }

    // consumed all data and no more?
    if (ptr != depData + optHdr.depsLength) {
        ALOGW("DexOpt: Spurious dep data? %d vs %d",
            (int) (ptr - depData), optHdr.depsLength);
        assert(false);
    }

    result = true;

bail:
    free(depData);
    return result;
}

/*
 * Write the dependency info to "fd" at the current file position.
 */
static int writeDependencies(int fd, u4 modWhen, u4 crc)
{
    u1* buf = NULL;
    int result = -1;
    ssize_t bufLen;
    ClassPathEntry* cpe;
    int numDeps;

    /*
     * Count up the number of completed entries in the bootclasspath.
     */
    numDeps = 0;
    bufLen = 0;
    for (cpe = gDvm.bootClassPath; cpe->ptr != NULL; cpe++) {
        const char* cacheFileName =
            dvmPathToAbsolutePortion(getCacheFileName(cpe));
        assert(cacheFileName != NULL); /* guaranteed by Class.c */

        ALOGV("+++ DexOpt: found dep '%s'", cacheFileName);

        numDeps++;
        bufLen += strlen(cacheFileName) +1;
    }

    bufLen += 4*4 + numDeps * (4+kSHA1DigestLen);

    buf = (u1*)malloc(bufLen);

    set4LE(buf+0, modWhen);
    set4LE(buf+4, crc);
    set4LE(buf+8, DALVIK_VM_BUILD);
    set4LE(buf+12, numDeps);

    // TODO: do we want to add dvmGetInlineOpsTableLength() here?  Won't
    // help us if somebody replaces an existing entry, but it'd catch
    // additions/removals.

    u1* ptr = buf + 4*4;
    for (cpe = gDvm.bootClassPath; cpe->ptr != NULL; cpe++) {
        const char* cacheFileName =
            dvmPathToAbsolutePortion(getCacheFileName(cpe));
        assert(cacheFileName != NULL); /* guaranteed by Class.c */

        const u1* signature = getSignature(cpe);
        int len = strlen(cacheFileName) +1;

        if (ptr + 4 + len + kSHA1DigestLen > buf + bufLen) {
            ALOGE("DexOpt: overran buffer");
            dvmAbort();
        }

        set4LE(ptr, len);
        ptr += 4;
        memcpy(ptr, cacheFileName, len);
        ptr += len;
        memcpy(ptr, signature, kSHA1DigestLen);
        ptr += kSHA1DigestLen;
    }

    assert(ptr == buf + bufLen);

    result = sysWriteFully(fd, buf, bufLen, "DexOpt dep info");

    free(buf);
    return result;
}


/*
 * Write a block of data in "chunk" format.
 *
 * The chunk header fields are always in "native" byte order.  If "size"
 * is not a multiple of 8 bytes, the data area is padded out.
 */
static bool writeChunk(int fd, u4 type, const void* data, size_t size)
{
    union {             /* save a syscall by grouping these together */
        char raw[8];
        struct {
            u4 type;
            u4 size;
        } ts;
    } header;

    assert(sizeof(header) == 8);

    ALOGV("Writing chunk, type=%.4s size=%d", (char*) &type, size);

    header.ts.type = type;
    header.ts.size = (u4) size;
    if (sysWriteFully(fd, &header, sizeof(header),
            "DexOpt opt chunk header write") != 0)
    {
        return false;
    }

    if (size > 0) {
        if (sysWriteFully(fd, data, size, "DexOpt opt chunk write") != 0)
            return false;
    }

    /* if necessary, pad to 64-bit alignment */
    if ((size & 7) != 0) {
        int padSize = 8 - (size & 7);
        ALOGV("size was %d, inserting %d pad bytes", size, padSize);
        lseek(fd, padSize, SEEK_CUR);
    }

    assert( ((int)lseek(fd, 0, SEEK_CUR) & 7) == 0);

    return true;
}

/*
 * Write opt data.
 *
 * We have different pieces, some of which may be optional.  To make the
 * most effective use of space, we use a "chunk" format, with a 4-byte
 * type and a 4-byte length.  We guarantee 64-bit alignment for the data,
 * so it can be used directly when the file is mapped for reading.
 */
static bool writeOptData(int fd, const DexClassLookup* pClassLookup,
    const RegisterMapBuilder* pRegMapBuilder)
{
    /* pre-computed class lookup hash table */
    if (!writeChunk(fd, (u4) kDexChunkClassLookup,
            pClassLookup, pClassLookup->size))
    {
        return false;
    }

    /* register maps (optional) */
    if (pRegMapBuilder != NULL) {
        if (!writeChunk(fd, (u4) kDexChunkRegisterMaps,
                pRegMapBuilder->data, pRegMapBuilder->size))
        {
            return false;
        }
    }

    /* write the end marker */
    if (!writeChunk(fd, (u4) kDexChunkEnd, NULL, 0)) {
        return false;
    }

    return true;
}

/*
 * Compute a checksum on a piece of an open file.
 *
 * File will be positioned at end of checksummed area.
 *
 * Returns "true" on success.
 */
static bool computeFileChecksum(int fd, off_t start, size_t length, u4* pSum)
{
    unsigned char readBuf[8192];
    ssize_t actual;
    uLong adler;

    if (lseek(fd, start, SEEK_SET) != start) {
        ALOGE("Unable to seek to start of checksum area (%ld): %s",
            (long) start, strerror(errno));
        return false;
    }

    adler = adler32(0L, Z_NULL, 0);

    while (length != 0) {
        size_t wanted = (length < sizeof(readBuf)) ? length : sizeof(readBuf);
        actual = read(fd, readBuf, wanted);
        if (actual <= 0) {
            ALOGE("Read failed (%d) while computing checksum (len=%zu): %s",
                (int) actual, length, strerror(errno));
            return false;
        }

        adler = adler32(adler, readBuf, actual);

        length -= actual;
    }

    *pSum = adler;
    return true;
}

/*
 * Update the Adler-32 checksum stored in the DEX file.  This covers the
 * swapped and optimized DEX data, but does not include the opt header
 * or optimized data.
 */
static void updateChecksum(u1* addr, int len, DexHeader* pHeader)
{
    /*
     * Rewrite the checksum.  We leave the SHA-1 signature alone.
     */
    uLong adler = adler32(0L, Z_NULL, 0);
    const int nonSum = sizeof(pHeader->magic) + sizeof(pHeader->checksum);

    adler = adler32(adler, addr + nonSum, len - nonSum);
    pHeader->checksum = adler;
}
