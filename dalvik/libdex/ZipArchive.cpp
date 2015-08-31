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
 * Read-only access to Zip archives, with minimal heap allocation.
 */
#include "ZipArchive.h"

#include <zlib.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <JNIHelp.h>        // TEMP_FAILURE_RETRY may or may not be in unistd
#include <utils/Compat.h>   // For off64_t and lseek64 on Mac

#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
 * Zip file constants.
 */
#define kEOCDSignature       0x06054b50
#define kEOCDLen             22
#define kEOCDDiskNumber      4               // number of the current disk
#define kEOCDDiskNumberForCD 6               // disk number with the Central Directory
#define kEOCDNumEntries      8               // offset to #of entries in file
#define kEOCDTotalNumEntries 10              // offset to total #of entries in spanned archives
#define kEOCDSize            12              // size of the central directory
#define kEOCDFileOffset      16              // offset to central directory
#define kEOCDCommentSize     20              // offset to the length of the file comment

#define kMaxCommentLen       65535           // longest possible in ushort
#define kMaxEOCDSearch       (kMaxCommentLen + kEOCDLen)

#define kLFHSignature        0x04034b50
#define kLFHLen              30              // excluding variable-len fields
#define kLFHGPBFlags          6              // offset to GPB flags
#define kLFHNameLen          26              // offset to filename length
#define kLFHExtraLen         28              // offset to extra length

#define kCDESignature        0x02014b50
#define kCDELen              46              // excluding variable-len fields
#define kCDEGPBFlags          8              // offset to GPB flags
#define kCDEMethod           10              // offset to compression method
#define kCDEModWhen          12              // offset to modification timestamp
#define kCDECRC              16              // offset to entry CRC
#define kCDECompLen          20              // offset to compressed length
#define kCDEUncompLen        24              // offset to uncompressed length
#define kCDENameLen          28              // offset to filename length
#define kCDEExtraLen         30              // offset to extra length
#define kCDECommentLen       32              // offset to comment length
#define kCDELocalOffset      42              // offset to local hdr

/* General Purpose Bit Flag */
#define kGPFEncryptedFlag    (1 << 0)
#define kGPFUnsupportedMask  (kGPFEncryptedFlag)

/*
 * The values we return for ZipEntryRO use 0 as an invalid value, so we
 * want to adjust the hash table index by a fixed amount.  Using a large
 * value helps insure that people don't mix & match arguments, e.g. to
 * findEntryByIndex().
 */
#define kZipEntryAdj        10000

/*
 * Convert a ZipEntry to a hash table index, verifying that it's in a
 * valid range.
 */
static int entryToIndex(const ZipArchive* pArchive, const ZipEntry entry)
{
    long ent = ((long) entry) - kZipEntryAdj;
    if (ent < 0 || ent >= pArchive->mHashTableSize ||
        pArchive->mHashTable[ent].name == NULL)
    {
        ALOGW("Zip: invalid ZipEntry %p (%ld)", entry, ent);
        return -1;
    }
    return ent;
}

/*
 * Simple string hash function for non-null-terminated strings.
 */
static unsigned int computeHash(const char* str, int len)
{
    unsigned int hash = 0;

    while (len--)
        hash = hash * 31 + *str++;

    return hash;
}

/*
 * Add a new entry to the hash table.
 */
static void addToHash(ZipArchive* pArchive, const char* str, int strLen,
    unsigned int hash)
{
    const int hashTableSize = pArchive->mHashTableSize;
    int ent = hash & (hashTableSize - 1);

    /*
     * We over-allocated the table, so we're guaranteed to find an empty slot.
     */
    while (pArchive->mHashTable[ent].name != NULL)
        ent = (ent + 1) & (hashTableSize-1);

    pArchive->mHashTable[ent].name = str;
    pArchive->mHashTable[ent].nameLen = strLen;
}

/*
 * Get 2 little-endian bytes.
 */
static u2 get2LE(unsigned char const* pSrc)
{
    return pSrc[0] | (pSrc[1] << 8);
}

/*
 * Get 4 little-endian bytes.
 */
static u4 get4LE(unsigned char const* pSrc)
{
    u4 result;

    result = pSrc[0];
    result |= pSrc[1] << 8;
    result |= pSrc[2] << 16;
    result |= pSrc[3] << 24;

    return result;
}

static int mapCentralDirectory0(int fd, const char* debugFileName,
        ZipArchive* pArchive, off64_t fileLength, size_t readAmount, u1* scanBuf)
{
    /*
     * Make sure this is a Zip archive.
     */
    if (lseek64(pArchive->mFd, 0, SEEK_SET) != 0) {
        ALOGW("seek to start failed: %s", strerror(errno));
        return false;
    }

    ssize_t actual = TEMP_FAILURE_RETRY(read(pArchive->mFd, scanBuf, sizeof(int32_t)));
    if (actual != (ssize_t) sizeof(int32_t)) {
        ALOGI("couldn't read first signature from zip archive: %s", strerror(errno));
        return false;
    }

    unsigned int header = get4LE(scanBuf);
    if (header != kLFHSignature) {
        ALOGV("Not a Zip archive (found 0x%08x)\n", header);
        return false;
    }

    /*
     * Perform the traditional EOCD snipe hunt.
     *
     * We're searching for the End of Central Directory magic number,
     * which appears at the start of the EOCD block.  It's followed by
     * 18 bytes of EOCD stuff and up to 64KB of archive comment.  We
     * need to read the last part of the file into a buffer, dig through
     * it to find the magic number, parse some values out, and use those
     * to determine the extent of the CD.
     *
     * We start by pulling in the last part of the file.
     */
    off64_t searchStart = fileLength - readAmount;

    if (lseek64(pArchive->mFd, searchStart, SEEK_SET) != searchStart) {
        ALOGW("seek %ld failed: %s\n",  (long) searchStart, strerror(errno));
        return false;
    }
    actual = TEMP_FAILURE_RETRY(read(pArchive->mFd, scanBuf, readAmount));
    if (actual != (ssize_t) readAmount) {
        ALOGW("Zip: read %zd, expected %zd. Failed: %s\n",
            actual, readAmount, strerror(errno));
        return false;
    }


    /*
     * Scan backward for the EOCD magic.  In an archive without a trailing
     * comment, we'll find it on the first try.  (We may want to consider
     * doing an initial minimal read; if we don't find it, retry with a
     * second read as above.)
     */
    int i;
    for (i = readAmount - kEOCDLen; i >= 0; i--) {
        if (scanBuf[i] == 0x50 && get4LE(&scanBuf[i]) == kEOCDSignature) {
            ALOGV("+++ Found EOCD at buf+%d", i);
            break;
        }
    }
    if (i < 0) {
        ALOGD("Zip: EOCD not found, %s is not zip", debugFileName);
        return -1;
    }

    off64_t eocdOffset = searchStart + i;
    const u1* eocdPtr = scanBuf + i;

    assert(eocdOffset < fileLength);

    /*
     * Grab the CD offset and size, and the number of entries in the
     * archive.  Verify that they look reasonable.
     */
    u4 diskNumber = get2LE(eocdPtr + kEOCDDiskNumber);
    u4 diskWithCentralDir = get2LE(eocdPtr + kEOCDDiskNumberForCD);
    u4 numEntries = get2LE(eocdPtr + kEOCDNumEntries);
    u4 totalNumEntries = get2LE(eocdPtr + kEOCDTotalNumEntries);
    u4 centralDirSize = get4LE(eocdPtr + kEOCDSize);
    u4 centralDirOffset = get4LE(eocdPtr + kEOCDFileOffset);
    u4 commentSize = get2LE(eocdPtr + kEOCDCommentSize);

    // Verify that they look reasonable.
    if ((long long) centralDirOffset + (long long) centralDirSize > (long long) eocdOffset) {
        ALOGW("bad offsets (dir %ld, size %u, eocd %ld)\n",
            (long) centralDirOffset, centralDirSize, (long) eocdOffset);
        return false;
    }
    if (numEntries == 0) {
        ALOGW("empty archive?\n");
        return false;
    } else if (numEntries != totalNumEntries || diskNumber != 0 || diskWithCentralDir != 0) {
        ALOGW("spanned archives not supported");
        return false;
    }

    // Check to see if comment is a sane size
    if (((size_t) commentSize > (fileLength - kEOCDLen))
            || (eocdOffset > (fileLength - kEOCDLen) - commentSize)) {
        ALOGW("comment size runs off end of file");
        return false;
    }

    ALOGV("+++ numEntries=%d dirSize=%d dirOffset=%d\n",
        numEntries, centralDirSize, centralDirOffset);

    /*
     * It all looks good.  Create a mapping for the CD, and set the fields
     * in pArchive.
     */
    if (sysMapFileSegmentInShmem(fd, centralDirOffset, centralDirSize,
            &pArchive->mDirectoryMap) != 0)
    {
        ALOGW("Zip: cd map failed");
        return -1;
    }

    pArchive->mNumEntries = numEntries;
    pArchive->mDirectoryOffset = centralDirOffset;

    return 0;
}

/*
 * Find the zip Central Directory and memory-map it.
 *
 * On success, returns 0 after populating fields from the EOCD area:
 *   mDirectoryOffset
 *   mDirectoryMap
 *   mNumEntries
 */
static int mapCentralDirectory(int fd, const char* debugFileName,
    ZipArchive* pArchive)
{
    /*
     * Get and test file length.
     */
    off64_t fileLength = lseek64(fd, 0, SEEK_END);
    if (fileLength < kEOCDLen) {
        ALOGV("Zip: length %ld is too small to be zip", (long) fileLength);
        return -1;
    }

    /*
     * Perform the traditional EOCD snipe hunt.
     *
     * We're searching for the End of Central Directory magic number,
     * which appears at the start of the EOCD block.  It's followed by
     * 18 bytes of EOCD stuff and up to 64KB of archive comment.  We
     * need to read the last part of the file into a buffer, dig through
     * it to find the magic number, parse some values out, and use those
     * to determine the extent of the CD.
     *
     * We start by pulling in the last part of the file.
     */
    size_t readAmount = kMaxEOCDSearch;
    if (fileLength < off_t(readAmount))
        readAmount = fileLength;

    u1* scanBuf = (u1*) malloc(readAmount);
    if (scanBuf == NULL) {
        return -1;
    }

    int result = mapCentralDirectory0(fd, debugFileName, pArchive,
            fileLength, readAmount, scanBuf);

    free(scanBuf);
    return result;
}

/*
 * Parses the Zip archive's Central Directory.  Allocates and populates the
 * hash table.
 *
 * Returns 0 on success.
 */
static int parseZipArchive(ZipArchive* pArchive)
{
    int result = -1;
    const u1* cdPtr = (const u1*)pArchive->mDirectoryMap.addr;
    size_t cdLength = pArchive->mDirectoryMap.length;
    int numEntries = pArchive->mNumEntries;

    /*
     * Create hash table.  We have a minimum 75% load factor, possibly as
     * low as 50% after we round off to a power of 2.  There must be at
     * least one unused entry to avoid an infinite loop during creation.
     */
    pArchive->mHashTableSize = dexRoundUpPower2(1 + (numEntries * 4) / 3);
    pArchive->mHashTable = (ZipHashEntry*)
            calloc(pArchive->mHashTableSize, sizeof(ZipHashEntry));

    /*
     * Walk through the central directory, adding entries to the hash
     * table and verifying values.
     */
    const u1* ptr = cdPtr;
    int i;
    for (i = 0; i < numEntries; i++) {
        if (get4LE(ptr) != kCDESignature) {
            ALOGW("Zip: missed a central dir sig (at %d)", i);
            goto bail;
        }
        if (ptr + kCDELen > cdPtr + cdLength) {
            ALOGW("Zip: ran off the end (at %d)", i);
            goto bail;
        }

        long localHdrOffset = (long) get4LE(ptr + kCDELocalOffset);
        if (localHdrOffset >= pArchive->mDirectoryOffset) {
            ALOGW("Zip: bad LFH offset %ld at entry %d", localHdrOffset, i);
            goto bail;
        }

        unsigned int gpbf = get2LE(ptr + kCDEGPBFlags);
        if ((gpbf & kGPFUnsupportedMask) != 0) {
            ALOGW("Invalid General Purpose Bit Flag: %d", gpbf);
            goto bail;
        }

        unsigned int nameLen, extraLen, commentLen, hash;
        nameLen = get2LE(ptr + kCDENameLen);
        extraLen = get2LE(ptr + kCDEExtraLen);
        commentLen = get2LE(ptr + kCDECommentLen);

        const char *name = (const char *) ptr + kCDELen;

        /* Check name for NULL characters */
        if (memchr(name, 0, nameLen) != NULL) {
            ALOGW("Filename contains NUL byte");
            goto bail;
        }

        /* add the CDE filename to the hash table */
        hash = computeHash(name, nameLen);
        addToHash(pArchive, name, nameLen, hash);

        /* We don't care about the comment or extra data. */
        ptr += kCDELen + nameLen + extraLen + commentLen;
        if ((size_t)(ptr - cdPtr) > cdLength) {
            ALOGW("Zip: bad CD advance (%d vs %zd) at entry %d",
                (int) (ptr - cdPtr), cdLength, i);
            goto bail;
        }
    }
    ALOGV("+++ zip good scan %d entries", numEntries);

    result = 0;

bail:
    return result;
}

/*
 * Open the specified file read-only.  We examine the contents and verify
 * that it appears to be a valid zip file.
 *
 * This will be called on non-Zip files, especially during VM startup, so
 * we don't want to be too noisy about certain types of failure.  (Do
 * we want a "quiet" flag?)
 *
 * On success, we fill out the contents of "pArchive" and return 0.  On
 * failure we return the errno value.
 */
int dexZipOpenArchive(const char* fileName, ZipArchive* pArchive)
{
    int fd, err;

    ALOGV("Opening as zip '%s' %p", fileName, pArchive);

    memset(pArchive, 0, sizeof(ZipArchive));

    fd = open(fileName, O_RDONLY | O_BINARY, 0);
    if (fd < 0) {
        err = errno ? errno : -1;
        ALOGV("Unable to open '%s': %s", fileName, strerror(err));
        return err;
    }

    return dexZipPrepArchive(fd, fileName, pArchive);
}

/*
 * Prepare to access a ZipArchive through an open file descriptor.
 *
 * On success, we fill out the contents of "pArchive" and return 0.
 */
int dexZipPrepArchive(int fd, const char* debugFileName, ZipArchive* pArchive)
{
    int result = -1;

    memset(pArchive, 0, sizeof(*pArchive));
    pArchive->mFd = fd;

    if (mapCentralDirectory(fd, debugFileName, pArchive) != 0)
        goto bail;

    if (parseZipArchive(pArchive) != 0) {
        ALOGV("Zip: parsing '%s' failed", debugFileName);
        goto bail;
    }

    /* success */
    result = 0;

bail:
    if (result != 0)
        dexZipCloseArchive(pArchive);
    return result;
}


/*
 * Close a ZipArchive, closing the file and freeing the contents.
 *
 * NOTE: the ZipArchive may not have been fully created.
 */
void dexZipCloseArchive(ZipArchive* pArchive)
{
    ALOGV("Closing archive %p", pArchive);

    if (pArchive->mFd >= 0)
        close(pArchive->mFd);

    sysReleaseShmem(&pArchive->mDirectoryMap);

    free(pArchive->mHashTable);

    /* ensure nobody tries to use the ZipArchive after it's closed */
    pArchive->mDirectoryOffset = -1;
    pArchive->mFd = -1;
    pArchive->mNumEntries = -1;
    pArchive->mHashTableSize = -1;
    pArchive->mHashTable = NULL;
}


/*
 * Find a matching entry.
 *
 * Returns 0 if not found.
 */
ZipEntry dexZipFindEntry(const ZipArchive* pArchive, const char* entryName)
{
    int nameLen = strlen(entryName);
    unsigned int hash = computeHash(entryName, nameLen);
    const int hashTableSize = pArchive->mHashTableSize;
    int ent = hash & (hashTableSize-1);

    while (pArchive->mHashTable[ent].name != NULL) {
        if (pArchive->mHashTable[ent].nameLen == nameLen &&
            memcmp(pArchive->mHashTable[ent].name, entryName, nameLen) == 0)
        {
            /* match */
            return (ZipEntry)(long)(ent + kZipEntryAdj);
        }

        ent = (ent + 1) & (hashTableSize-1);
    }

    return NULL;
}

#if 0
/*
 * Find the Nth entry.
 *
 * This currently involves walking through the sparse hash table, counting
 * non-empty entries.  If we need to speed this up we can either allocate
 * a parallel lookup table or (perhaps better) provide an iterator interface.
 */
ZipEntry findEntryByIndex(ZipArchive* pArchive, int idx)
{
    if (idx < 0 || idx >= pArchive->mNumEntries) {
        ALOGW("Invalid index %d", idx);
        return NULL;
    }

    int ent;
    for (ent = 0; ent < pArchive->mHashTableSize; ent++) {
        if (pArchive->mHashTable[ent].name != NULL) {
            if (idx-- == 0)
                return (ZipEntry) (ent + kZipEntryAdj);
        }
    }

    return NULL;
}
#endif

/*
 * Get the useful fields from the zip entry.
 *
 * Returns non-zero if the contents of the fields (particularly the data
 * offset) appear to be bogus.
 */
int dexZipGetEntryInfo(const ZipArchive* pArchive, ZipEntry entry,
    int* pMethod, size_t* pUncompLen, size_t* pCompLen, off_t* pOffset,
    long* pModWhen, long* pCrc32)
{
    int ent = entryToIndex(pArchive, entry);
    if (ent < 0)
        return -1;

    /*
     * Recover the start of the central directory entry from the filename
     * pointer.  The filename is the first entry past the fixed-size data,
     * so we can just subtract back from that.
     */
    const unsigned char* basePtr = (const unsigned char*)
        pArchive->mDirectoryMap.addr;
    const unsigned char* ptr = (const unsigned char*)
        pArchive->mHashTable[ent].name;
    off_t cdOffset = pArchive->mDirectoryOffset;

    ptr -= kCDELen;

    int method = get2LE(ptr + kCDEMethod);
    if (pMethod != NULL)
        *pMethod = method;

    if (pModWhen != NULL)
        *pModWhen = get4LE(ptr + kCDEModWhen);
    if (pCrc32 != NULL)
        *pCrc32 = get4LE(ptr + kCDECRC);

    size_t compLen = get4LE(ptr + kCDECompLen);
    if (pCompLen != NULL)
        *pCompLen = compLen;
    size_t uncompLen = get4LE(ptr + kCDEUncompLen);
    if (pUncompLen != NULL)
        *pUncompLen = uncompLen;

    /*
     * If requested, determine the offset of the start of the data.  All we
     * have is the offset to the Local File Header, which is variable size,
     * so we have to read the contents of the struct to figure out where
     * the actual data starts.
     *
     * We also need to make sure that the lengths are not so large that
     * somebody trying to map the compressed or uncompressed data runs
     * off the end of the mapped region.
     *
     * Note we don't verify compLen/uncompLen if they don't request the
     * dataOffset, because dataOffset is expensive to determine.  However,
     * if they don't have the file offset, they're not likely to be doing
     * anything with the contents.
     */
    if (pOffset != NULL) {
        long localHdrOffset = (long) get4LE(ptr + kCDELocalOffset);
        if (localHdrOffset + kLFHLen >= cdOffset) {
            ALOGW("Zip: bad local hdr offset in zip");
            return -1;
        }

        u1 lfhBuf[kLFHLen];
        if (lseek(pArchive->mFd, localHdrOffset, SEEK_SET) != localHdrOffset) {
            ALOGW("Zip: failed seeking to lfh at offset %ld", localHdrOffset);
            return -1;
        }
        ssize_t actual =
            TEMP_FAILURE_RETRY(read(pArchive->mFd, lfhBuf, sizeof(lfhBuf)));
        if (actual != sizeof(lfhBuf)) {
            ALOGW("Zip: failed reading lfh from offset %ld", localHdrOffset);
            return -1;
        }

        if (get4LE(lfhBuf) != kLFHSignature) {
            ALOGW("Zip: didn't find signature at start of lfh, offset=%ld",
                localHdrOffset);
            return -1;
        }

        u4 gpbf = get2LE(lfhBuf + kLFHGPBFlags);
        if ((gpbf & kGPFUnsupportedMask) != 0) {
            ALOGW("Invalid General Purpose Bit Flag: %d", gpbf);
            return -1;
        }

        off64_t dataOffset = localHdrOffset + kLFHLen
            + get2LE(lfhBuf + kLFHNameLen) + get2LE(lfhBuf + kLFHExtraLen);
        if (dataOffset >= cdOffset) {
            ALOGW("Zip: bad data offset %ld in zip", (long) dataOffset);
            return -1;
        }

        /* check lengths */
        if ((off_t)(dataOffset + compLen) > cdOffset) {
            ALOGW("Zip: bad compressed length in zip (%ld + %zd > %ld)",
                (long) dataOffset, compLen, (long) cdOffset);
            return -1;
        }

        if (method == kCompressStored &&
            (off_t)(dataOffset + uncompLen) > cdOffset)
        {
            ALOGW("Zip: bad uncompressed length in zip (%ld + %zd > %ld)",
                (long) dataOffset, uncompLen, (long) cdOffset);
            return -1;
        }

        *pOffset = dataOffset;
    }
    return 0;
}

/*
 * Uncompress "deflate" data from the archive's file to an open file
 * descriptor.
 */
static int inflateToFile(int outFd, int inFd, size_t uncompLen, size_t compLen)
{
    int result = -1;
    const size_t kBufSize = 32768;
    unsigned char* readBuf = (unsigned char*) malloc(kBufSize);
    unsigned char* writeBuf = (unsigned char*) malloc(kBufSize);
    z_stream zstream;
    int zerr;

    if (readBuf == NULL || writeBuf == NULL)
        goto bail;

    /*
     * Initialize the zlib stream struct.
     */
    memset(&zstream, 0, sizeof(zstream));
    zstream.zalloc = Z_NULL;
    zstream.zfree = Z_NULL;
    zstream.opaque = Z_NULL;
    zstream.next_in = NULL;
    zstream.avail_in = 0;
    zstream.next_out = (Bytef*) writeBuf;
    zstream.avail_out = kBufSize;
    zstream.data_type = Z_UNKNOWN;

    /*
     * Use the undocumented "negative window bits" feature to tell zlib
     * that there's no zlib header waiting for it.
     */
    zerr = inflateInit2(&zstream, -MAX_WBITS);
    if (zerr != Z_OK) {
        if (zerr == Z_VERSION_ERROR) {
            ALOGE("Installed zlib is not compatible with linked version (%s)",
                ZLIB_VERSION);
        } else {
            ALOGW("Call to inflateInit2 failed (zerr=%d)", zerr);
        }
        goto bail;
    }

    /*
     * Loop while we have more to do.
     */
    do {
        /* read as much as we can */
        if (zstream.avail_in == 0) {
            size_t getSize = (compLen > kBufSize) ? kBufSize : compLen;

            ssize_t actual = TEMP_FAILURE_RETRY(read(inFd, readBuf, getSize));
            if (actual != (ssize_t) getSize) {
                ALOGW("Zip: inflate read failed (%d vs %zd)",
                    (int)actual, getSize);
                goto z_bail;
            }

            compLen -= getSize;

            zstream.next_in = readBuf;
            zstream.avail_in = getSize;
        }

        /* uncompress the data */
        zerr = inflate(&zstream, Z_NO_FLUSH);
        if (zerr != Z_OK && zerr != Z_STREAM_END) {
            ALOGW("Zip: inflate zerr=%d (nIn=%p aIn=%u nOut=%p aOut=%u)",
                zerr, zstream.next_in, zstream.avail_in,
                zstream.next_out, zstream.avail_out);
            goto z_bail;
        }

        /* write when we're full or when we're done */
        if (zstream.avail_out == 0 ||
            (zerr == Z_STREAM_END && zstream.avail_out != kBufSize))
        {
            size_t writeSize = zstream.next_out - writeBuf;
            if (sysWriteFully(outFd, writeBuf, writeSize, "Zip inflate") != 0)
                goto z_bail;

            zstream.next_out = writeBuf;
            zstream.avail_out = kBufSize;
        }
    } while (zerr == Z_OK);

    assert(zerr == Z_STREAM_END);       /* other errors should've been caught */

    /* paranoia */
    if (zstream.total_out != uncompLen) {
        ALOGW("Zip: size mismatch on inflated file (%ld vs %zd)",
            zstream.total_out, uncompLen);
        goto z_bail;
    }

    result = 0;

z_bail:
    inflateEnd(&zstream);        /* free up any allocated structures */

bail:
    free(readBuf);
    free(writeBuf);
    return result;
}

/*
 * Uncompress an entry, in its entirety, to an open file descriptor.
 *
 * TODO: this doesn't verify the data's CRC, but probably should (especially
 * for uncompressed data).
 */
int dexZipExtractEntryToFile(const ZipArchive* pArchive,
    const ZipEntry entry, int fd)
{
    int result = -1;
    int ent = entryToIndex(pArchive, entry);
    if (ent < 0) {
        ALOGW("Zip: extract can't find entry %p", entry);
        goto bail;
    }

    int method;
    size_t uncompLen, compLen;
    off_t dataOffset;

    if (dexZipGetEntryInfo(pArchive, entry, &method, &uncompLen, &compLen,
            &dataOffset, NULL, NULL) != 0)
    {
        goto bail;
    }
    if (lseek(pArchive->mFd, dataOffset, SEEK_SET) != dataOffset) {
        ALOGW("Zip: lseek to data at %ld failed", (long) dataOffset);
        goto bail;
    }

    if (method == kCompressStored) {
        if (sysCopyFileToFile(fd, pArchive->mFd, uncompLen) != 0)
            goto bail;
    } else {
        if (inflateToFile(fd, pArchive->mFd, uncompLen, compLen) != 0)
            goto bail;
    }

    result = 0;

bail:
    return result;
}
