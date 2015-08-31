/*
 * Copyright (C) 2009 The Android Open Source Project
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
 * Strip Android-specific records out of hprof data, back-converting from
 * 1.0.3 to 1.0.2.  This removes some useful information, but allows
 * Android hprof data to be handled by widely-available tools (like "jhat").
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

//#define VERBOSE_DEBUG
#ifdef VERBOSE_DEBUG
# define DBUG(...) fprintf(stderr, __VA_ARGS__)
#else
# define DBUG(...)
#endif

#ifndef FALSE
# define FALSE 0
# define TRUE (!FALSE)
#endif

typedef enum HprofBasicType {
    HPROF_BASIC_OBJECT = 2,
    HPROF_BASIC_BOOLEAN = 4,
    HPROF_BASIC_CHAR = 5,
    HPROF_BASIC_FLOAT = 6,
    HPROF_BASIC_DOUBLE = 7,
    HPROF_BASIC_BYTE = 8,
    HPROF_BASIC_SHORT = 9,
    HPROF_BASIC_INT = 10,
    HPROF_BASIC_LONG = 11,
} HprofBasicType;

typedef enum HprofTag {
    /* tags we must handle specially */
    HPROF_TAG_HEAP_DUMP                 = 0x0c,
    HPROF_TAG_HEAP_DUMP_SEGMENT         = 0x1c,
} HprofTag;

typedef enum HprofHeapTag {
    /* 1.0.2 tags */
    HPROF_ROOT_UNKNOWN                  = 0xff,
    HPROF_ROOT_JNI_GLOBAL               = 0x01,
    HPROF_ROOT_JNI_LOCAL                = 0x02,
    HPROF_ROOT_JAVA_FRAME               = 0x03,
    HPROF_ROOT_NATIVE_STACK             = 0x04,
    HPROF_ROOT_STICKY_CLASS             = 0x05,
    HPROF_ROOT_THREAD_BLOCK             = 0x06,
    HPROF_ROOT_MONITOR_USED             = 0x07,
    HPROF_ROOT_THREAD_OBJECT            = 0x08,
    HPROF_CLASS_DUMP                    = 0x20,
    HPROF_INSTANCE_DUMP                 = 0x21,
    HPROF_OBJECT_ARRAY_DUMP             = 0x22,
    HPROF_PRIMITIVE_ARRAY_DUMP          = 0x23,

    /* Android 1.0.3 tags */
    HPROF_HEAP_DUMP_INFO                = 0xfe,
    HPROF_ROOT_INTERNED_STRING          = 0x89,
    HPROF_ROOT_FINALIZING               = 0x8a,
    HPROF_ROOT_DEBUGGER                 = 0x8b,
    HPROF_ROOT_REFERENCE_CLEANUP        = 0x8c,
    HPROF_ROOT_VM_INTERNAL              = 0x8d,
    HPROF_ROOT_JNI_MONITOR              = 0x8e,
    HPROF_UNREACHABLE                   = 0x90,  /* deprecated */
    HPROF_PRIMITIVE_ARRAY_NODATA_DUMP   = 0xc3,
} HprofHeapTag;

#define kIdentSize  4
#define kRecHdrLen  9


/*
 * ===========================================================================
 *      Expanding buffer
 * ===========================================================================
 */

/* simple struct */
typedef struct {
    unsigned char* storage;
    size_t curLen;
    size_t maxLen;
} ExpandBuf;

/*
 * Create an ExpandBuf.
 */
static ExpandBuf* ebAlloc(void)
{
    static const int kInitialSize = 64;

    ExpandBuf* newBuf = (ExpandBuf*) malloc(sizeof(ExpandBuf));
    if (newBuf == NULL)
        return NULL;
    newBuf->storage = (unsigned char*) malloc(kInitialSize);
    newBuf->curLen = 0;
    newBuf->maxLen = kInitialSize;

    return newBuf;
}

/*
 * Release the storage associated with an ExpandBuf.
 */
static void ebFree(ExpandBuf* pBuf)
{
    if (pBuf != NULL) {
        free(pBuf->storage);
        free(pBuf);
    }
}

/*
 * Return a pointer to the data buffer.
 *
 * The pointer may change as data is added to the buffer, so this value
 * should not be cached.
 */
static inline unsigned char* ebGetBuffer(ExpandBuf* pBuf)
{
    return pBuf->storage;
}

/*
 * Get the amount of data currently in the buffer.
 */
static inline size_t ebGetLength(ExpandBuf* pBuf)
{
    return pBuf->curLen;
}

/*
 * Empty the buffer.
 */
static void ebClear(ExpandBuf* pBuf)
{
    pBuf->curLen = 0;
}

/*
 * Ensure that the buffer can hold at least "size" additional bytes.
 */
static int ebEnsureCapacity(ExpandBuf* pBuf, int size)
{
    assert(size > 0);

    if (pBuf->curLen + size > pBuf->maxLen) {
        int newSize = pBuf->curLen + size + 128;    /* oversize slightly */
        unsigned char* newStorage = realloc(pBuf->storage, newSize);
        if (newStorage == NULL) {
            fprintf(stderr, "ERROR: realloc failed on size=%d\n", newSize);
            return -1;
        }

        pBuf->storage = newStorage;
        pBuf->maxLen = newSize;
    }

    assert(pBuf->curLen + size <= pBuf->maxLen);
    return 0;
}

/*
 * Add data to the buffer after ensuring it can hold it.
 */
static int ebAddData(ExpandBuf* pBuf, const void* data, size_t count)
{
    ebEnsureCapacity(pBuf, count);
    memcpy(pBuf->storage + pBuf->curLen, data, count);
    pBuf->curLen += count;
    return 0;
}

/*
 * Read a NULL-terminated string from the input.
 */
static int ebReadString(ExpandBuf* pBuf, FILE* in)
{
    int ic;

    do {
        ebEnsureCapacity(pBuf, 1);

        ic = getc(in);
        if (feof(in) || ferror(in)) {
            fprintf(stderr, "ERROR: failed reading input\n");
            return -1;
        }

        pBuf->storage[pBuf->curLen++] = (unsigned char) ic;
    } while (ic != 0);

    return 0;
}

/*
 * Read some data, adding it to the expanding buffer.
 *
 * This will ensure that the buffer has enough space to hold the new data
 * (plus the previous contents).
 */
static int ebReadData(ExpandBuf* pBuf, FILE* in, size_t count, int eofExpected)
{
    size_t actual;

    assert(count > 0);

    ebEnsureCapacity(pBuf, count);
    actual = fread(pBuf->storage + pBuf->curLen, 1, count, in);
    if (actual != count) {
        if (eofExpected && feof(in) && !ferror(in)) {
            /* return without reporting an error */
        } else {
            fprintf(stderr, "ERROR: read %d of %d bytes\n", actual, count);
            return -1;
        }
    }

    pBuf->curLen += count;
    assert(pBuf->curLen <= pBuf->maxLen);

    return 0;
}

/*
 * Write the data from the buffer.  Resets the data count to zero.
 */
static int ebWriteData(ExpandBuf* pBuf, FILE* out)
{
    size_t actual;

    assert(pBuf->curLen > 0);
    assert(pBuf->curLen <= pBuf->maxLen);

    actual = fwrite(pBuf->storage, 1, pBuf->curLen, out);
    if (actual != pBuf->curLen) {
        fprintf(stderr, "ERROR: write %d of %d bytes\n", actual, pBuf->curLen);
        return -1;
    }

    pBuf->curLen = 0;

    return 0;
}


/*
 * ===========================================================================
 *      Hprof stuff
 * ===========================================================================
 */

/*
 * Get a 2-byte value, in big-endian order, from memory.
 */
static uint16_t get2BE(const unsigned char* buf)
{
    uint16_t val;

    val = (buf[0] << 8) | buf[1];
    return val;
}

/*
 * Get a 4-byte value, in big-endian order, from memory.
 */
static uint32_t get4BE(const unsigned char* buf)
{
    uint32_t val;

    val = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    return val;
}

/*
 * Set a 4-byte value, in big-endian order.
 */
static void set4BE(unsigned char* buf, uint32_t val)
{
    buf[0] = val >> 24;
    buf[1] = val >> 16;
    buf[2] = val >> 8;
    buf[3] = val;
}

/*
 * Get the size, in bytes, of one of the "basic types".
 */
static int computeBasicLen(HprofBasicType basicType)
{
    static const int sizes[] = { -1, -1, 4, -1, 1, 2, 4, 8, 1, 2, 4, 8  };
    static const size_t maxSize = sizeof(sizes) / sizeof(sizes[0]);

    assert(basicType >= 0);
    if (basicType >= maxSize)
        return -1;
    return sizes[basicType];
}

/*
 * Compute the length of a HPROF_CLASS_DUMP block.
 */
static int computeClassDumpLen(const unsigned char* origBuf, int len)
{
    const unsigned char* buf = origBuf;
    int blockLen = 0;
    int i, count;

    blockLen += kIdentSize * 7 + 8;
    buf += blockLen;
    len -= blockLen;

    if (len < 0)
        return -1;

    count = get2BE(buf);
    buf += 2;
    len -= 2;
    DBUG("CDL: 1st count is %d\n", count);
    for (i = 0; i < count; i++) {
        HprofBasicType basicType;
        int basicLen;

        basicType = buf[2];
        basicLen = computeBasicLen(basicType);
        if (basicLen < 0) {
            DBUG("ERROR: invalid basicType %d\n", basicType);
            return -1;
        }

        buf += 2 + 1 + basicLen;
        len -= 2 + 1 + basicLen;
        if (len < 0)
            return -1;
    }

    count = get2BE(buf);
    buf += 2;
    len -= 2;
    DBUG("CDL: 2nd count is %d\n", count);
    for (i = 0; i < count; i++) {
        HprofBasicType basicType;
        int basicLen;

        basicType = buf[kIdentSize];
        basicLen = computeBasicLen(basicType);
        if (basicLen < 0) {
            fprintf(stderr, "ERROR: invalid basicType %d\n", basicType);
            return -1;
        }

        buf += kIdentSize + 1 + basicLen;
        len -= kIdentSize + 1 + basicLen;
        if (len < 0)
            return -1;
    }

    count = get2BE(buf);
    buf += 2;
    len -= 2;
    DBUG("CDL: 3rd count is %d\n", count);
    for (i = 0; i < count; i++) {
        buf += kIdentSize + 1;
        len -= kIdentSize + 1;
        if (len < 0)
            return -1;
    }

    DBUG("Total class dump len: %d\n", buf - origBuf);
    return buf - origBuf;
}

/*
 * Compute the length of a HPROF_INSTANCE_DUMP block.
 */
static int computeInstanceDumpLen(const unsigned char* origBuf, int len)
{
    int extraCount = get4BE(origBuf + kIdentSize * 2 + 4);
    return kIdentSize * 2 + 8 + extraCount;
}

/*
 * Compute the length of a HPROF_OBJECT_ARRAY_DUMP block.
 */
static int computeObjectArrayDumpLen(const unsigned char* origBuf, int len)
{
    int arrayCount = get4BE(origBuf + kIdentSize + 4);
    return kIdentSize * 2 + 8 + arrayCount * kIdentSize;
}

/*
 * Compute the length of a HPROF_PRIMITIVE_ARRAY_DUMP block.
 */
static int computePrimitiveArrayDumpLen(const unsigned char* origBuf, int len)
{
    int arrayCount = get4BE(origBuf + kIdentSize + 4);
    HprofBasicType basicType = origBuf[kIdentSize + 8];
    int basicLen = computeBasicLen(basicType);

    return kIdentSize + 9 + arrayCount * basicLen;
}

/*
 * Crunch through a heap dump record, writing the original or converted
 * data to "out".
 */
static int processHeapDump(ExpandBuf* pBuf, FILE* out)
{
    ExpandBuf* pOutBuf = ebAlloc();
    unsigned char* origBuf = ebGetBuffer(pBuf);
    unsigned char* buf = origBuf;
    int len = ebGetLength(pBuf);
    int result = -1;

    pBuf = NULL;        /* we just use the raw pointer from here forward */

    /* copy the original header to the output buffer */
    if (ebAddData(pOutBuf, buf, kRecHdrLen) != 0)
        goto bail;

    buf += kRecHdrLen;      /* skip past record header */
    len -= kRecHdrLen;

    while (len > 0) {
        unsigned char subType = buf[0];
        int justCopy = TRUE;
        int subLen;

        DBUG("--- 0x%02x  ", subType);
        switch (subType) {
        /* 1.0.2 types */
        case HPROF_ROOT_UNKNOWN:
            subLen = kIdentSize;
            break;
        case HPROF_ROOT_JNI_GLOBAL:
            subLen = kIdentSize * 2;
            break;
        case HPROF_ROOT_JNI_LOCAL:
            subLen = kIdentSize + 8;
            break;
        case HPROF_ROOT_JAVA_FRAME:
            subLen = kIdentSize + 8;
            break;
        case HPROF_ROOT_NATIVE_STACK:
            subLen = kIdentSize + 4;
            break;
        case HPROF_ROOT_STICKY_CLASS:
            subLen = kIdentSize;
            break;
        case HPROF_ROOT_THREAD_BLOCK:
            subLen = kIdentSize + 4;
            break;
        case HPROF_ROOT_MONITOR_USED:
            subLen = kIdentSize;
            break;
        case HPROF_ROOT_THREAD_OBJECT:
            subLen = kIdentSize + 8;
            break;
        case HPROF_CLASS_DUMP:
            subLen = computeClassDumpLen(buf+1, len-1);
            break;
        case HPROF_INSTANCE_DUMP:
            subLen = computeInstanceDumpLen(buf+1, len-1);
            break;
        case HPROF_OBJECT_ARRAY_DUMP:
            subLen = computeObjectArrayDumpLen(buf+1, len-1);
            break;
        case HPROF_PRIMITIVE_ARRAY_DUMP:
            subLen = computePrimitiveArrayDumpLen(buf+1, len-1);
            break;

        /* these were added for Android in 1.0.3 */
        case HPROF_HEAP_DUMP_INFO:
            justCopy = FALSE;
            subLen = kIdentSize + 4;
            // no 1.0.2 equivalent for this
            break;
        case HPROF_ROOT_INTERNED_STRING:
            buf[0] = HPROF_ROOT_UNKNOWN;
            subLen = kIdentSize;
            break;
        case HPROF_ROOT_FINALIZING:
            buf[0] = HPROF_ROOT_UNKNOWN;
            subLen = kIdentSize;
            break;
        case HPROF_ROOT_DEBUGGER:
            buf[0] = HPROF_ROOT_UNKNOWN;
            subLen = kIdentSize;
            break;
        case HPROF_ROOT_REFERENCE_CLEANUP:
            buf[0] = HPROF_ROOT_UNKNOWN;
            subLen = kIdentSize;
            break;
        case HPROF_ROOT_VM_INTERNAL:
            buf[0] = HPROF_ROOT_UNKNOWN;
            subLen = kIdentSize;
            break;
        case HPROF_ROOT_JNI_MONITOR:
            /* keep the ident, drop the next 8 bytes */
            buf[0] = HPROF_ROOT_UNKNOWN;
            justCopy = FALSE;
            ebAddData(pOutBuf, buf, 1 + kIdentSize);
            subLen = kIdentSize + 8;
            break;
        case HPROF_UNREACHABLE:
            buf[0] = HPROF_ROOT_UNKNOWN;
            subLen = kIdentSize;
            break;
        case HPROF_PRIMITIVE_ARRAY_NODATA_DUMP:
            buf[0] = HPROF_PRIMITIVE_ARRAY_DUMP;
            buf[5] = buf[6] = buf[7] = buf[8] = 0;  /* set array len to 0 */
            subLen = kIdentSize + 9;
            break;

        /* shouldn't get here */
        default:
            fprintf(stderr, "ERROR: unexpected subtype 0x%02x at offset %d\n",
                subType, buf - origBuf);
            goto bail;
        }

        if (justCopy) {
            /* copy source data */
            DBUG("(%d)\n", 1 + subLen);
            ebAddData(pOutBuf, buf, 1 + subLen);
        } else {
            /* other data has been written, or the sub-record omitted */
            DBUG("(adv %d)\n", 1 + subLen);
        }

        /* advance to next entry */
        buf += 1 + subLen;
        len -= 1 + subLen;
    }

    /*
     * Update the record length.
     */
    set4BE(ebGetBuffer(pOutBuf) + 5, ebGetLength(pOutBuf) - kRecHdrLen);

    if (ebWriteData(pOutBuf, out) != 0)
        goto bail;

    result = 0;

bail:
    ebFree(pOutBuf);
    return result;
}

/*
 * Filter an hprof data file.
 */
static int filterData(FILE* in, FILE* out)
{
    const char *magicString;
    ExpandBuf* pBuf;
    int result = -1;

    pBuf = ebAlloc();
    if (pBuf == NULL)
        goto bail;

    /*
     * Start with the header.
     */
    if (ebReadString(pBuf, in) != 0)
        goto bail;

    magicString = (const char*)ebGetBuffer(pBuf);
    if (strcmp(magicString, "JAVA PROFILE 1.0.3") != 0) {
        if (strcmp(magicString, "JAVA PROFILE 1.0.2") == 0) {
            fprintf(stderr, "ERROR: HPROF file already in 1.0.2 format.\n");
        } else {
            fprintf(stderr, "ERROR: expecting HPROF file format 1.0.3\n");
        }
        goto bail;
    }

    /* downgrade to 1.0.2 */
    (ebGetBuffer(pBuf))[17] = '2';
    if (ebWriteData(pBuf, out) != 0)
        goto bail;

    /*
     * Copy:
     * (4b) identifier size, always 4
     * (8b) file creation date
     */
    if (ebReadData(pBuf, in, 12, FALSE) != 0)
        goto bail;
    if (ebWriteData(pBuf, out) != 0)
        goto bail;

    /*
     * Read records until we hit EOF.  Each record begins with:
     * (1b) type
     * (4b) timestamp
     * (4b) length of data that follows
     */
    while (1) {
        assert(ebGetLength(pBuf) == 0);

        /* read type char */
        if (ebReadData(pBuf, in, 1, TRUE) != 0)
            goto bail;
        if (feof(in))
            break;

        /* read the rest of the header */
        if (ebReadData(pBuf, in, kRecHdrLen-1, FALSE) != 0)
            goto bail;

        unsigned char* buf = ebGetBuffer(pBuf);
        unsigned char type;
        unsigned int timestamp, length;

        type = buf[0];
        timestamp = get4BE(buf + 1);
        length = get4BE(buf + 5);
        buf = NULL;     /* ptr invalid after next read op */

        /* read the record data */
        if (length != 0) {
            if (ebReadData(pBuf, in, length, FALSE) != 0)
                goto bail;
        }

        if (type == HPROF_TAG_HEAP_DUMP ||
            type == HPROF_TAG_HEAP_DUMP_SEGMENT)
        {
            DBUG("Processing heap dump 0x%02x (%d bytes)\n",
                type, length);
            if (processHeapDump(pBuf, out) != 0)
                goto bail;
            ebClear(pBuf);
        } else {
            /* keep */
            DBUG("Keeping 0x%02x (%d bytes)\n", type, length);
            if (ebWriteData(pBuf, out) != 0)
                goto bail;
        }
    }

    result = 0;

bail:
    ebFree(pBuf);
    return result;
}

/*
 * Get args.
 */
int main(int argc, char** argv)
{
    FILE* in = stdin;
    FILE* out = stdout;
    int cc;

    if (argc != 3) {
        fprintf(stderr, "Usage: hprof-conf infile outfile\n\n");
        fprintf(stderr,
            "Specify '-' for either or both to use stdin/stdout.\n\n");

        fprintf(stderr,
            "Copyright (C) 2009 The Android Open Source Project\n\n"
            "This software is built from source code licensed under the "
            "Apache License,\n"
            "Version 2.0 (the \"License\"). You may obtain a copy of the "
            "License at\n\n"
            "     http://www.apache.org/licenses/LICENSE-2.0\n\n"
            "See the associated NOTICE file for this software for further "
            "details.\n");

        return 2;
    }

    if (strcmp(argv[1], "-") != 0) {
        in = fopen(argv[1], "rb");
        if (in == NULL) {
            fprintf(stderr, "ERROR: failed to open input '%s': %s\n",
                argv[1], strerror(errno));
            return 1;
        }
    }
    if (strcmp(argv[2], "-") != 0) {
        out = fopen(argv[2], "wb");
        if (out == NULL) {
            fprintf(stderr, "ERROR: failed to open output '%s': %s\n",
                argv[2], strerror(errno));
            if (in != stdin)
                fclose(in);
            return 1;
        }
    }

    cc = filterData(in, out);

    if (in != stdin)
        fclose(in);
    if (out != stdout)
        fclose(out);
    return (cc != 0);
}
