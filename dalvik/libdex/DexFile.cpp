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
 * Access the contents of a .dex file.
 */

#include "DexFile.h"
#include "DexOptData.h"
#include "DexProto.h"
#include "DexCatch.h"
#include "Leb128.h"
#include "sha1.h"
#include "ZipArchive.h"

#include <zlib.h>

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>


/*
 * Verifying checksums is good, but it slows things down and causes us to
 * touch every page.  In the "optimized" world, it doesn't work at all,
 * because we rewrite the contents.
 */
static const bool kVerifyChecksum = false;
static const bool kVerifySignature = false;

/* (documented in header) */
char dexGetPrimitiveTypeDescriptorChar(PrimitiveType type) {
    const char* string = dexGetPrimitiveTypeDescriptor(type);

    return (string == NULL) ? '\0' : string[0];
}

/* (documented in header) */
const char* dexGetPrimitiveTypeDescriptor(PrimitiveType type) {
    switch (type) {
        case PRIM_VOID:    return "V";
        case PRIM_BOOLEAN: return "Z";
        case PRIM_BYTE:    return "B";
        case PRIM_SHORT:   return "S";
        case PRIM_CHAR:    return "C";
        case PRIM_INT:     return "I";
        case PRIM_LONG:    return "J";
        case PRIM_FLOAT:   return "F";
        case PRIM_DOUBLE:  return "D";
        default:           return NULL;
    }

    return NULL;
}

/* (documented in header) */
const char* dexGetBoxedTypeDescriptor(PrimitiveType type) {
    switch (type) {
        case PRIM_VOID:    return NULL;
        case PRIM_BOOLEAN: return "Ljava/lang/Boolean;";
        case PRIM_BYTE:    return "Ljava/lang/Byte;";
        case PRIM_SHORT:   return "Ljava/lang/Short;";
        case PRIM_CHAR:    return "Ljava/lang/Character;";
        case PRIM_INT:     return "Ljava/lang/Integer;";
        case PRIM_LONG:    return "Ljava/lang/Long;";
        case PRIM_FLOAT:   return "Ljava/lang/Float;";
        case PRIM_DOUBLE:  return "Ljava/lang/Double;";
        default:           return NULL;
    }
}

/* (documented in header) */
PrimitiveType dexGetPrimitiveTypeFromDescriptorChar(char descriptorChar) {
    switch (descriptorChar) {
        case 'V': return PRIM_VOID;
        case 'Z': return PRIM_BOOLEAN;
        case 'B': return PRIM_BYTE;
        case 'S': return PRIM_SHORT;
        case 'C': return PRIM_CHAR;
        case 'I': return PRIM_INT;
        case 'J': return PRIM_LONG;
        case 'F': return PRIM_FLOAT;
        case 'D': return PRIM_DOUBLE;
        default:  return PRIM_NOT;
    }
}

/* Return the UTF-8 encoded string with the specified string_id index,
 * also filling in the UTF-16 size (number of 16-bit code points).*/
const char* dexStringAndSizeById(const DexFile* pDexFile, u4 idx,
        u4* utf16Size) {
    const DexStringId* pStringId = dexGetStringId(pDexFile, idx);
    const u1* ptr = pDexFile->baseAddr + pStringId->stringDataOff;

    *utf16Size = readUnsignedLeb128(&ptr);
    return (const char*) ptr;
}

/*
 * Format an SHA-1 digest for printing.  tmpBuf must be able to hold at
 * least kSHA1DigestOutputLen bytes.
 */
const char* dvmSHA1DigestToStr(const unsigned char digest[], char* tmpBuf);

/*
 * Compute a SHA-1 digest on a range of bytes.
 */
static void dexComputeSHA1Digest(const unsigned char* data, size_t length,
    unsigned char digest[])
{
    SHA1_CTX context;
    SHA1Init(&context);
    SHA1Update(&context, data, length);
    SHA1Final(digest, &context);
}

/*
 * Format the SHA-1 digest into the buffer, which must be able to hold at
 * least kSHA1DigestOutputLen bytes.  Returns a pointer to the buffer,
 */
static const char* dexSHA1DigestToStr(const unsigned char digest[],char* tmpBuf)
{
    static const char hexDigit[] = "0123456789abcdef";
    char* cp;
    int i;

    cp = tmpBuf;
    for (i = 0; i < kSHA1DigestLen; i++) {
        *cp++ = hexDigit[digest[i] >> 4];
        *cp++ = hexDigit[digest[i] & 0x0f];
    }
    *cp++ = '\0';

    assert(cp == tmpBuf + kSHA1DigestOutputLen);

    return tmpBuf;
}

/*
 * Compute a hash code on a UTF-8 string, for use with internal hash tables.
 *
 * This may or may not be compatible with UTF-8 hash functions used inside
 * the Dalvik VM.
 *
 * The basic "multiply by 31 and add" approach does better on class names
 * than most other things tried (e.g. adler32).
 */
static u4 classDescriptorHash(const char* str)
{
    u4 hash = 1;

    while (*str != '\0')
        hash = hash * 31 + *str++;

    return hash;
}

/*
 * Add an entry to the class lookup table.  We hash the string and probe
 * until we find an open slot.
 */
static void classLookupAdd(DexFile* pDexFile, DexClassLookup* pLookup,
    int stringOff, int classDefOff, int* pNumProbes)
{
    const char* classDescriptor =
        (const char*) (pDexFile->baseAddr + stringOff);
    const DexClassDef* pClassDef =
        (const DexClassDef*) (pDexFile->baseAddr + classDefOff);
    u4 hash = classDescriptorHash(classDescriptor);
    int mask = pLookup->numEntries-1;
    int idx = hash & mask;

    /*
     * Find the first empty slot.  We oversized the table, so this is
     * guaranteed to finish.
     */
    int probes = 0;
    while (pLookup->table[idx].classDescriptorOffset != 0) {
        idx = (idx + 1) & mask;
        probes++;
    }
    //if (probes > 1)
    //    ALOGW("classLookupAdd: probes=%d", probes);

    pLookup->table[idx].classDescriptorHash = hash;
    pLookup->table[idx].classDescriptorOffset = stringOff;
    pLookup->table[idx].classDefOffset = classDefOff;
    *pNumProbes = probes;
}

/*
 * Create the class lookup hash table.
 *
 * Returns newly-allocated storage.
 */
DexClassLookup* dexCreateClassLookup(DexFile* pDexFile)
{
    DexClassLookup* pLookup;
    int allocSize;
    int i, numEntries;
    int numProbes, totalProbes, maxProbes;

    numProbes = totalProbes = maxProbes = 0;

    assert(pDexFile != NULL);

    /*
     * Using a factor of 3 results in far less probing than a factor of 2,
     * but almost doubles the flash storage requirements for the bootstrap
     * DEX files.  The overall impact on class loading performance seems
     * to be minor.  We could probably get some performance improvement by
     * using a secondary hash.
     */
    numEntries = dexRoundUpPower2(pDexFile->pHeader->classDefsSize * 2);
    allocSize = offsetof(DexClassLookup, table)
                    + numEntries * sizeof(pLookup->table[0]);

    pLookup = (DexClassLookup*) calloc(1, allocSize);
    if (pLookup == NULL)
        return NULL;
    pLookup->size = allocSize;
    pLookup->numEntries = numEntries;

    for (i = 0; i < (int)pDexFile->pHeader->classDefsSize; i++) {
        const DexClassDef* pClassDef;
        const char* pString;

        pClassDef = dexGetClassDef(pDexFile, i);
        pString = dexStringByTypeIdx(pDexFile, pClassDef->classIdx);

        classLookupAdd(pDexFile, pLookup,
            (u1*)pString - pDexFile->baseAddr,
            (u1*)pClassDef - pDexFile->baseAddr, &numProbes);

        if (numProbes > maxProbes)
            maxProbes = numProbes;
        totalProbes += numProbes;
    }

    ALOGV("Class lookup: classes=%d slots=%d (%d%% occ) alloc=%d"
         " total=%d max=%d",
        pDexFile->pHeader->classDefsSize, numEntries,
        (100 * pDexFile->pHeader->classDefsSize) / numEntries,
        allocSize, totalProbes, maxProbes);

    return pLookup;
}


/*
 * Set up the basic raw data pointers of a DexFile. This function isn't
 * meant for general use.
 */
void dexFileSetupBasicPointers(DexFile* pDexFile, const u1* data) {
    DexHeader *pHeader = (DexHeader*) data;

    pDexFile->baseAddr = data;
    pDexFile->pHeader = pHeader;
    pDexFile->pStringIds = (const DexStringId*) (data + pHeader->stringIdsOff);
    pDexFile->pTypeIds = (const DexTypeId*) (data + pHeader->typeIdsOff);
    pDexFile->pFieldIds = (const DexFieldId*) (data + pHeader->fieldIdsOff);
    pDexFile->pMethodIds = (const DexMethodId*) (data + pHeader->methodIdsOff);
    pDexFile->pProtoIds = (const DexProtoId*) (data + pHeader->protoIdsOff);
    pDexFile->pClassDefs = (const DexClassDef*) (data + pHeader->classDefsOff);
    pDexFile->pLinkData = (const DexLink*) (data + pHeader->linkOff);
}

/*
 * Parse an optimized or unoptimized .dex file sitting in memory.  This is
 * called after the byte-ordering and structure alignment has been fixed up.
 *
 * On success, return a newly-allocated DexFile.
 */
DexFile* dexFileParse(const u1* data, size_t length, int flags)
{
    DexFile* pDexFile = NULL;
    const DexHeader* pHeader;
    const u1* magic;
    int result = -1;

    if (length < sizeof(DexHeader)) {
        ALOGE("too short to be a valid .dex");
        goto bail;      /* bad file format */
    }

    pDexFile = (DexFile*) malloc(sizeof(DexFile));
    if (pDexFile == NULL)
        goto bail;      /* alloc failure */
    memset(pDexFile, 0, sizeof(DexFile));

    /*
     * Peel off the optimized header.
     */
    if (memcmp(data, DEX_OPT_MAGIC, 4) == 0) {
        magic = data;
        if (memcmp(magic+4, DEX_OPT_MAGIC_VERS, 4) != 0) {
            ALOGE("bad opt version (0x%02x %02x %02x %02x)",
                 magic[4], magic[5], magic[6], magic[7]);
            goto bail;
        }

        pDexFile->pOptHeader = (const DexOptHeader*) data;
        ALOGV("Good opt header, DEX offset is %d, flags=0x%02x",
            pDexFile->pOptHeader->dexOffset, pDexFile->pOptHeader->flags);

        /* parse the optimized dex file tables */
        if (!dexParseOptData(data, length, pDexFile))
            goto bail;

        /* ignore the opt header and appended data from here on out */
        data += pDexFile->pOptHeader->dexOffset;
        length -= pDexFile->pOptHeader->dexOffset;
        if (pDexFile->pOptHeader->dexLength > length) {
            ALOGE("File truncated? stored len=%d, rem len=%d",
                pDexFile->pOptHeader->dexLength, (int) length);
            goto bail;
        }
        length = pDexFile->pOptHeader->dexLength;
    }

    dexFileSetupBasicPointers(pDexFile, data);
    pHeader = pDexFile->pHeader;

    if (!dexHasValidMagic(pHeader)) {
        goto bail;
    }

    /*
     * Verify the checksum(s).  This is reasonably quick, but does require
     * touching every byte in the DEX file.  The base checksum changes after
     * byte-swapping and DEX optimization.
     */
    if (flags & kDexParseVerifyChecksum) {
        u4 adler = dexComputeChecksum(pHeader);
        if (adler != pHeader->checksum) {
            ALOGE("ERROR: bad checksum (%08x vs %08x)",
                adler, pHeader->checksum);
            if (!(flags & kDexParseContinueOnError))
                goto bail;
        } else {
            ALOGV("+++ adler32 checksum (%08x) verified", adler);
        }

        const DexOptHeader* pOptHeader = pDexFile->pOptHeader;
        if (pOptHeader != NULL) {
            adler = dexComputeOptChecksum(pOptHeader);
            if (adler != pOptHeader->checksum) {
                ALOGE("ERROR: bad opt checksum (%08x vs %08x)",
                    adler, pOptHeader->checksum);
                if (!(flags & kDexParseContinueOnError))
                    goto bail;
            } else {
                ALOGV("+++ adler32 opt checksum (%08x) verified", adler);
            }
        }
    }

    /*
     * Verify the SHA-1 digest.  (Normally we don't want to do this --
     * the digest is used to uniquely identify the original DEX file, and
     * can't be computed for verification after the DEX is byte-swapped
     * and optimized.)
     */
    if (kVerifySignature) {
        unsigned char sha1Digest[kSHA1DigestLen];
        const int nonSum = sizeof(pHeader->magic) + sizeof(pHeader->checksum) +
                            kSHA1DigestLen;

        dexComputeSHA1Digest(data + nonSum, length - nonSum, sha1Digest);
        if (memcmp(sha1Digest, pHeader->signature, kSHA1DigestLen) != 0) {
            char tmpBuf1[kSHA1DigestOutputLen];
            char tmpBuf2[kSHA1DigestOutputLen];
            ALOGE("ERROR: bad SHA1 digest (%s vs %s)",
                dexSHA1DigestToStr(sha1Digest, tmpBuf1),
                dexSHA1DigestToStr(pHeader->signature, tmpBuf2));
            if (!(flags & kDexParseContinueOnError))
                goto bail;
        } else {
            ALOGV("+++ sha1 digest verified");
        }
    }

    if (pHeader->fileSize != length) {
        ALOGE("ERROR: stored file size (%d) != expected (%d)",
            (int) pHeader->fileSize, (int) length);
        if (!(flags & kDexParseContinueOnError))
            goto bail;
    }

    if (pHeader->classDefsSize == 0) {
        ALOGE("ERROR: DEX file has no classes in it, failing");
        goto bail;
    }

    /*
     * Success!
     */
    result = 0;

bail:
    if (result != 0 && pDexFile != NULL) {
        dexFileFree(pDexFile);
        pDexFile = NULL;
    }
    return pDexFile;
}

/*
 * Free up the DexFile and any associated data structures.
 *
 * Note we may be called with a partially-initialized DexFile.
 */
void dexFileFree(DexFile* pDexFile)
{
    if (pDexFile == NULL)
        return;

    free(pDexFile);
}

/*
 * Look up a class definition entry by descriptor.
 *
 * "descriptor" should look like "Landroid/debug/Stuff;".
 */
const DexClassDef* dexFindClass(const DexFile* pDexFile,
    const char* descriptor)
{
    const DexClassLookup* pLookup = pDexFile->pClassLookup;
    u4 hash;
    int idx, mask;

    hash = classDescriptorHash(descriptor);
    mask = pLookup->numEntries - 1;
    idx = hash & mask;

    /*
     * Search until we find a matching entry or an empty slot.
     */
    while (true) {
        int offset;

        offset = pLookup->table[idx].classDescriptorOffset;
        if (offset == 0)
            return NULL;

        if (pLookup->table[idx].classDescriptorHash == hash) {
            const char* str;

            str = (const char*) (pDexFile->baseAddr + offset);
            if (strcmp(str, descriptor) == 0) {
                return (const DexClassDef*)
                    (pDexFile->baseAddr + pLookup->table[idx].classDefOffset);
            }
        }

        idx = (idx + 1) & mask;
    }
}


/*
 * Compute the DEX file checksum for a memory-mapped DEX file.
 */
u4 dexComputeChecksum(const DexHeader* pHeader)
{
    const u1* start = (const u1*) pHeader;

    uLong adler = adler32(0L, Z_NULL, 0);
    const int nonSum = sizeof(pHeader->magic) + sizeof(pHeader->checksum);

    return (u4) adler32(adler, start + nonSum, pHeader->fileSize - nonSum);
}

/*
 * Compute the size, in bytes, of a DexCode.
 */
size_t dexGetDexCodeSize(const DexCode* pCode)
{
    /*
     * The catch handler data is the last entry.  It has a variable number
     * of variable-size pieces, so we need to create an iterator.
     */
    u4 handlersSize;
    u4 offset;
    u4 ui;

    if (pCode->triesSize != 0) {
        handlersSize = dexGetHandlersSize(pCode);
        offset = dexGetFirstHandlerOffset(pCode);
    } else {
        handlersSize = 0;
        offset = 0;
    }

    for (ui = 0; ui < handlersSize; ui++) {
        DexCatchIterator iterator;
        dexCatchIteratorInit(&iterator, pCode, offset);
        offset = dexCatchIteratorGetEndOffset(&iterator, pCode);
    }

    const u1* handlerData = dexGetCatchHandlerData(pCode);

    //ALOGD("+++ pCode=%p handlerData=%p last offset=%d",
    //    pCode, handlerData, offset);

    /* return the size of the catch handler + everything before it */
    return (handlerData - (u1*) pCode) + offset;
}

/*
 * Round up to the next highest power of 2.
 *
 * Found on http://graphics.stanford.edu/~seander/bithacks.html.
 */
u4 dexRoundUpPower2(u4 val)
{
    val--;
    val |= val >> 1;
    val |= val >> 2;
    val |= val >> 4;
    val |= val >> 8;
    val |= val >> 16;
    val++;

    return val;
}
