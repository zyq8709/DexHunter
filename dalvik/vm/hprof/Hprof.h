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
#ifndef DALVIK_HPROF_HPROF_H_
#define DALVIK_HPROF_HPROF_H_

#include "Dalvik.h"

#define HPROF_ID_SIZE (sizeof (u4))

#define UNIQUE_ERROR() \
    -((((uintptr_t)__func__) << 16 | __LINE__) & (0x7fffffff))

#define HPROF_TIME 0
#define HPROF_NULL_STACK_TRACE   0
#define HPROF_NULL_THREAD        0

typedef u4 hprof_id;
typedef hprof_id hprof_string_id;
typedef hprof_id hprof_object_id;
typedef hprof_id hprof_class_object_id;

enum hprof_basic_type {
    hprof_basic_object = 2,
    hprof_basic_boolean = 4,
    hprof_basic_char = 5,
    hprof_basic_float = 6,
    hprof_basic_double = 7,
    hprof_basic_byte = 8,
    hprof_basic_short = 9,
    hprof_basic_int = 10,
    hprof_basic_long = 11,
};

enum hprof_tag_t {
    HPROF_TAG_STRING = 0x01,
    HPROF_TAG_LOAD_CLASS = 0x02,
    HPROF_TAG_UNLOAD_CLASS = 0x03,
    HPROF_TAG_STACK_FRAME = 0x04,
    HPROF_TAG_STACK_TRACE = 0x05,
    HPROF_TAG_ALLOC_SITES = 0x06,
    HPROF_TAG_HEAP_SUMMARY = 0x07,
    HPROF_TAG_START_THREAD = 0x0A,
    HPROF_TAG_END_THREAD = 0x0B,
    HPROF_TAG_HEAP_DUMP = 0x0C,
    HPROF_TAG_HEAP_DUMP_SEGMENT = 0x1C,
    HPROF_TAG_HEAP_DUMP_END = 0x2C,
    HPROF_TAG_CPU_SAMPLES = 0x0D,
    HPROF_TAG_CONTROL_SETTINGS = 0x0E,
};

/* Values for the first byte of
 * HEAP_DUMP and HEAP_DUMP_SEGMENT
 * records:
 */
enum hprof_heap_tag_t {
    /* standard */
    HPROF_ROOT_UNKNOWN = 0xFF,
    HPROF_ROOT_JNI_GLOBAL = 0x01,
    HPROF_ROOT_JNI_LOCAL = 0x02,
    HPROF_ROOT_JAVA_FRAME = 0x03,
    HPROF_ROOT_NATIVE_STACK = 0x04,
    HPROF_ROOT_STICKY_CLASS = 0x05,
    HPROF_ROOT_THREAD_BLOCK = 0x06,
    HPROF_ROOT_MONITOR_USED = 0x07,
    HPROF_ROOT_THREAD_OBJECT = 0x08,
    HPROF_CLASS_DUMP = 0x20,
    HPROF_INSTANCE_DUMP = 0x21,
    HPROF_OBJECT_ARRAY_DUMP = 0x22,
    HPROF_PRIMITIVE_ARRAY_DUMP = 0x23,

    /* Android */
    HPROF_HEAP_DUMP_INFO = 0xfe,
    HPROF_ROOT_INTERNED_STRING = 0x89,
    HPROF_ROOT_FINALIZING = 0x8a,  /* obsolete */
    HPROF_ROOT_DEBUGGER = 0x8b,
    HPROF_ROOT_REFERENCE_CLEANUP = 0x8c,  /* obsolete */
    HPROF_ROOT_VM_INTERNAL = 0x8d,
    HPROF_ROOT_JNI_MONITOR = 0x8e,
    HPROF_UNREACHABLE = 0x90,  /* obsolete */
    HPROF_PRIMITIVE_ARRAY_NODATA_DUMP = 0xc3,
};

/* Represents a top-level hprof record, whose serialized
 * format is:
 *
 *     u1     TAG: denoting the type of the record
 *     u4     TIME: number of microseconds since the time stamp in the header
 *     u4     LENGTH: number of bytes that follow this u4 field
 *                    and belong to this record
 *     [u1]*  BODY: as many bytes as specified in the above u4 field
 */
struct hprof_record_t {
    unsigned char *body;
    u4 time;
    u4 length;
    size_t allocLen;
    u1 tag;
    bool dirty;
};

enum HprofHeapId {
    HPROF_HEAP_DEFAULT = 0,
    HPROF_HEAP_ZYGOTE = 'Z',
    HPROF_HEAP_APP = 'A'
};

struct hprof_context_t {
    /* curRec *must* be first so that we
     * can cast from a context to a record.
     */
    hprof_record_t curRec;

    u4 gcThreadSerialNumber;
    u1 gcScanState;
    HprofHeapId currentHeap;    // which heap we're currently emitting
    u4 stackTraceSerialNumber;
    size_t objectsInSegment;

    /*
     * If directToDdms is set, "fileName" and "fd" will be ignored.
     * Otherwise, "fileName" must be valid, though if "fd" >= 0 it will
     * only be used for debug messages.
     */
    bool directToDdms;
    char *fileName;
    char *fileDataPtr;          // for open_memstream
    size_t fileDataSize;        // for open_memstream
    FILE *memFp;
    int fd;
};


/*
 * HprofString.cpp functions
 */

hprof_string_id hprofLookupStringId(const char *str);

int hprofDumpStrings(hprof_context_t *ctx);

int hprofStartup_String(void);
int hprofShutdown_String(void);


/*
 * HprofClass.cpp functions
 */

hprof_class_object_id hprofLookupClassId(const ClassObject *clazz);

int hprofDumpClasses(hprof_context_t *ctx);

int hprofStartup_Class(void);
int hprofShutdown_Class(void);


/*
 * HprofHeap.cpp functions
 */

int hprofStartHeapDump(hprof_context_t *ctx);
int hprofFinishHeapDump(hprof_context_t *ctx);

int hprofSetGcScanState(hprof_context_t *ctx,
                        hprof_heap_tag_t state, u4 threadSerialNumber);
void hprofMarkRootObject(hprof_context_t *ctx,
                         const Object *obj, jobject jniObj);

int hprofDumpHeapObject(hprof_context_t *ctx, const Object *obj);

/*
 * HprofOutput.cpp functions
 */

void hprofContextInit(hprof_context_t *ctx, char *fileName, int fd,
                      bool writeHeader, bool directToDdms);

int hprofFlushRecord(hprof_record_t *rec, FILE *fp);
int hprofFlushCurrentRecord(hprof_context_t *ctx);
int hprofStartNewRecord(hprof_context_t *ctx, u1 tag, u4 time);

int hprofAddU1ToRecord(hprof_record_t *rec, u1 value);
int hprofAddU1ListToRecord(hprof_record_t *rec,
                           const u1 *values, size_t numValues);

int hprofAddUtf8StringToRecord(hprof_record_t *rec, const char *str);

int hprofAddU2ToRecord(hprof_record_t *rec, u2 value);
int hprofAddU2ListToRecord(hprof_record_t *rec,
                           const u2 *values, size_t numValues);

int hprofAddU4ToRecord(hprof_record_t *rec, u4 value);
int hprofAddU4ListToRecord(hprof_record_t *rec,
                           const u4 *values, size_t numValues);

int hprofAddU8ToRecord(hprof_record_t *rec, u8 value);
int hprofAddU8ListToRecord(hprof_record_t *rec,
                           const u8 *values, size_t numValues);

#define hprofAddIdToRecord(rec, id) hprofAddU4ToRecord((rec), (u4)(id))
#define hprofAddIdListToRecord(rec, values, numValues) \
            hprofAddU4ListToRecord((rec), (const u4 *)(values), (numValues))

/*
 * Hprof.cpp functions
 */

hprof_context_t* hprofStartup(const char *outputFileName, int fd,
    bool directToDdms);
bool hprofShutdown(hprof_context_t *ctx);
void hprofFreeContext(hprof_context_t *ctx);
int hprofDumpHeap(const char* fileName, int fd, bool directToDdms);

#endif  // DALVIK_HPROF_HPROF_H_
