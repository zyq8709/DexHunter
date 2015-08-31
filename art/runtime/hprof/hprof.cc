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
 * Preparation and completion of hprof data generation.  The output is
 * written into two files and then combined.  This is necessary because
 * we generate some of the data (strings and classes) while we dump the
 * heap, and some analysis tools require that the class and string data
 * appear first.
 */

#include "hprof.h"

#include <cutils/open_memstream.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <time.h>
#include <time.h>
#include <unistd.h>

#include <set>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "common_throws.h"
#include "debugger.h"
#include "dex_file-inl.h"
#include "gc/accounting/heap_bitmap.h"
#include "gc/heap.h"
#include "gc/space/space.h"
#include "globals.h"
#include "mirror/art_field-inl.h"
#include "mirror/class.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "object_utils.h"
#include "os.h"
#include "safe_map.h"
#include "scoped_thread_state_change.h"
#include "thread_list.h"

namespace art {

namespace hprof {

#define UNIQUE_ERROR -((((uintptr_t)__func__) << 16 | __LINE__) & (0x7fffffff))

#define HPROF_TIME 0
#define HPROF_NULL_STACK_TRACE   0
#define HPROF_NULL_THREAD        0

#define U2_TO_BUF_BE(buf, offset, value) \
    do { \
      unsigned char* buf_ = (unsigned char*)(buf); \
      int offset_ = static_cast<int>(offset); \
      uint16_t value_ = (uint16_t)(value); \
      buf_[offset_ + 0] = (unsigned char)(value_ >>  8); \
      buf_[offset_ + 1] = (unsigned char)(value_      ); \
    } while (0)

#define U4_TO_BUF_BE(buf, offset, value) \
    do { \
      unsigned char* buf_ = (unsigned char*)(buf); \
      int offset_ = static_cast<int>(offset); \
      uint32_t value_ = (uint32_t)(value); \
      buf_[offset_ + 0] = (unsigned char)(value_ >> 24); \
      buf_[offset_ + 1] = (unsigned char)(value_ >> 16); \
      buf_[offset_ + 2] = (unsigned char)(value_ >>  8); \
      buf_[offset_ + 3] = (unsigned char)(value_      ); \
    } while (0)

#define U8_TO_BUF_BE(buf, offset, value) \
    do { \
      unsigned char* buf_ = (unsigned char*)(buf); \
      int offset_ = static_cast<int>(offset); \
      uint64_t value_ = (uint64_t)(value); \
      buf_[offset_ + 0] = (unsigned char)(value_ >> 56); \
      buf_[offset_ + 1] = (unsigned char)(value_ >> 48); \
      buf_[offset_ + 2] = (unsigned char)(value_ >> 40); \
      buf_[offset_ + 3] = (unsigned char)(value_ >> 32); \
      buf_[offset_ + 4] = (unsigned char)(value_ >> 24); \
      buf_[offset_ + 5] = (unsigned char)(value_ >> 16); \
      buf_[offset_ + 6] = (unsigned char)(value_ >>  8); \
      buf_[offset_ + 7] = (unsigned char)(value_      ); \
    } while (0)

enum HprofTag {
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

// Values for the first byte of HEAP_DUMP and HEAP_DUMP_SEGMENT records:
enum HprofHeapTag {
  // Traditional.
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

  // Android.
  HPROF_HEAP_DUMP_INFO = 0xfe,
  HPROF_ROOT_INTERNED_STRING = 0x89,
  HPROF_ROOT_FINALIZING = 0x8a,  // Obsolete.
  HPROF_ROOT_DEBUGGER = 0x8b,
  HPROF_ROOT_REFERENCE_CLEANUP = 0x8c,  // Obsolete.
  HPROF_ROOT_VM_INTERNAL = 0x8d,
  HPROF_ROOT_JNI_MONITOR = 0x8e,
  HPROF_UNREACHABLE = 0x90,  // Obsolete.
  HPROF_PRIMITIVE_ARRAY_NODATA_DUMP = 0xc3,  // Obsolete.
};

enum HprofHeapId {
  HPROF_HEAP_DEFAULT = 0,
  HPROF_HEAP_ZYGOTE = 'Z',
  HPROF_HEAP_APP = 'A'
};

enum HprofBasicType {
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

typedef uint32_t HprofId;
typedef HprofId HprofStringId;
typedef HprofId HprofObjectId;
typedef HprofId HprofClassObjectId;
typedef std::set<mirror::Class*> ClassSet;
typedef std::set<mirror::Class*>::iterator ClassSetIterator;
typedef SafeMap<std::string, size_t> StringMap;
typedef SafeMap<std::string, size_t>::iterator StringMapIterator;

// Represents a top-level hprof record, whose serialized format is:
// U1  TAG: denoting the type of the record
// U4  TIME: number of microseconds since the time stamp in the header
// U4  LENGTH: number of bytes that follow this uint32_t field and belong to this record
// U1* BODY: as many bytes as specified in the above uint32_t field
class HprofRecord {
 public:
  HprofRecord() {
    dirty_ = false;
    alloc_length_ = 128;
    body_ = reinterpret_cast<unsigned char*>(malloc(alloc_length_));
    fp_ = NULL;
  }

  ~HprofRecord() {
    free(body_);
  }

  int StartNewRecord(FILE* fp, uint8_t tag, uint32_t time) {
    int rc = Flush();
    if (rc != 0) {
      return rc;
    }

    fp_ = fp;
    tag_ = tag;
    time_ = time;
    length_ = 0;
    dirty_ = true;
    return 0;
  }

  int Flush() {
    if (dirty_) {
      unsigned char headBuf[sizeof(uint8_t) + 2 * sizeof(uint32_t)];

      headBuf[0] = tag_;
      U4_TO_BUF_BE(headBuf, 1, time_);
      U4_TO_BUF_BE(headBuf, 5, length_);

      int nb = fwrite(headBuf, 1, sizeof(headBuf), fp_);
      if (nb != sizeof(headBuf)) {
        return UNIQUE_ERROR;
      }
      nb = fwrite(body_, 1, length_, fp_);
      if (nb != static_cast<int>(length_)) {
        return UNIQUE_ERROR;
      }

      dirty_ = false;
    }
    // TODO if we used less than half (or whatever) of allocLen, shrink the buffer.
    return 0;
  }

  int AddU1(uint8_t value) {
    int err = GuaranteeRecordAppend(1);
    if (err != 0) {
      return err;
    }

    body_[length_++] = value;
    return 0;
  }

  int AddU2(uint16_t value) {
    return AddU2List(&value, 1);
  }

  int AddU4(uint32_t value) {
    return AddU4List(&value, 1);
  }

  int AddU8(uint64_t value) {
    return AddU8List(&value, 1);
  }

  int AddId(HprofObjectId value) {
    return AddU4((uint32_t) value);
  }

  int AddU1List(const uint8_t* values, size_t numValues) {
    int err = GuaranteeRecordAppend(numValues);
    if (err != 0) {
      return err;
    }

    memcpy(body_ + length_, values, numValues);
    length_ += numValues;
    return 0;
  }

  int AddU2List(const uint16_t* values, size_t numValues) {
    int err = GuaranteeRecordAppend(numValues * 2);
    if (err != 0) {
      return err;
    }

    unsigned char* insert = body_ + length_;
    for (size_t i = 0; i < numValues; ++i) {
      U2_TO_BUF_BE(insert, 0, *values++);
      insert += sizeof(*values);
    }
    length_ += numValues * 2;
    return 0;
  }

  int AddU4List(const uint32_t* values, size_t numValues) {
    int err = GuaranteeRecordAppend(numValues * 4);
    if (err != 0) {
      return err;
    }

    unsigned char* insert = body_ + length_;
    for (size_t i = 0; i < numValues; ++i) {
      U4_TO_BUF_BE(insert, 0, *values++);
      insert += sizeof(*values);
    }
    length_ += numValues * 4;
    return 0;
  }

  void UpdateU4(size_t offset, uint32_t new_value) {
    U4_TO_BUF_BE(body_, offset, new_value);
  }

  int AddU8List(const uint64_t* values, size_t numValues) {
    int err = GuaranteeRecordAppend(numValues * 8);
    if (err != 0) {
      return err;
    }

    unsigned char* insert = body_ + length_;
    for (size_t i = 0; i < numValues; ++i) {
      U8_TO_BUF_BE(insert, 0, *values++);
      insert += sizeof(*values);
    }
    length_ += numValues * 8;
    return 0;
  }

  int AddIdList(const HprofObjectId* values, size_t numValues) {
    return AddU4List((const uint32_t*) values, numValues);
  }

  int AddUtf8String(const char* str) {
    // The terminating NUL character is NOT written.
    return AddU1List((const uint8_t*)str, strlen(str));
  }

  size_t Size() const {
    return length_;
  }

 private:
  int GuaranteeRecordAppend(size_t nmore) {
    size_t minSize = length_ + nmore;
    if (minSize > alloc_length_) {
      size_t newAllocLen = alloc_length_ * 2;
      if (newAllocLen < minSize) {
        newAllocLen = alloc_length_ + nmore + nmore/2;
      }
      unsigned char* newBody = (unsigned char*)realloc(body_, newAllocLen);
      if (newBody != NULL) {
        body_ = newBody;
        alloc_length_ = newAllocLen;
      } else {
        // TODO: set an error flag so future ops will fail
        return UNIQUE_ERROR;
      }
    }

    CHECK_LE(length_ + nmore, alloc_length_);
    return 0;
  }

  size_t alloc_length_;
  unsigned char* body_;

  FILE* fp_;
  uint8_t tag_;
  uint32_t time_;
  size_t length_;
  bool dirty_;

  DISALLOW_COPY_AND_ASSIGN(HprofRecord);
};

class Hprof {
 public:
  Hprof(const char* output_filename, int fd, bool direct_to_ddms)
      : filename_(output_filename),
        fd_(fd),
        direct_to_ddms_(direct_to_ddms),
        start_ns_(NanoTime()),
        current_record_(),
        gc_thread_serial_number_(0),
        gc_scan_state_(0),
        current_heap_(HPROF_HEAP_DEFAULT),
        objects_in_segment_(0),
        header_fp_(NULL),
        header_data_ptr_(NULL),
        header_data_size_(0),
        body_fp_(NULL),
        body_data_ptr_(NULL),
        body_data_size_(0),
        next_string_id_(0x400000) {
    LOG(INFO) << "hprof: heap dump \"" << filename_ << "\" starting...";

    header_fp_ = open_memstream(&header_data_ptr_, &header_data_size_);
    if (header_fp_ == NULL) {
      PLOG(FATAL) << "header open_memstream failed";
    }

    body_fp_ = open_memstream(&body_data_ptr_, &body_data_size_);
    if (body_fp_ == NULL) {
      PLOG(FATAL) << "body open_memstream failed";
    }
  }

  ~Hprof() {
    if (header_fp_ != NULL) {
      fclose(header_fp_);
    }
    if (body_fp_ != NULL) {
      fclose(body_fp_);
    }
    free(header_data_ptr_);
    free(body_data_ptr_);
  }

  void Dump()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::heap_bitmap_lock_) {
    // Walk the roots and the heap.
    current_record_.StartNewRecord(body_fp_, HPROF_TAG_HEAP_DUMP_SEGMENT, HPROF_TIME);
    Runtime::Current()->VisitRoots(RootVisitor, this, false, false);
    Thread* self = Thread::Current();
    {
      WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
      Runtime::Current()->GetHeap()->FlushAllocStack();
    }
    {
      ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
      Runtime::Current()->GetHeap()->GetLiveBitmap()->Walk(HeapBitmapCallback, this);
    }
    current_record_.StartNewRecord(body_fp_, HPROF_TAG_HEAP_DUMP_END, HPROF_TIME);
    current_record_.Flush();
    fflush(body_fp_);

    // Write the header.
    WriteFixedHeader();
    // Write the string and class tables, and any stack traces, to the header.
    // (jhat requires that these appear before any of the data in the body that refers to them.)
    WriteStringTable();
    WriteClassTable();
    WriteStackTraces();
    current_record_.Flush();
    fflush(header_fp_);

    bool okay = true;
    if (direct_to_ddms_) {
      // Send the data off to DDMS.
      iovec iov[2];
      iov[0].iov_base = header_data_ptr_;
      iov[0].iov_len = header_data_size_;
      iov[1].iov_base = body_data_ptr_;
      iov[1].iov_len = body_data_size_;
      Dbg::DdmSendChunkV(CHUNK_TYPE("HPDS"), iov, 2);
    } else {
      // Where exactly are we writing to?
      int out_fd;
      if (fd_ >= 0) {
        out_fd = dup(fd_);
        if (out_fd < 0) {
          ThrowRuntimeException("Couldn't dump heap; dup(%d) failed: %s", fd_, strerror(errno));
          return;
        }
      } else {
        out_fd = open(filename_.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if (out_fd < 0) {
          ThrowRuntimeException("Couldn't dump heap; open(\"%s\") failed: %s", filename_.c_str(),
                                strerror(errno));
          return;
        }
      }

      UniquePtr<File> file(new File(out_fd, filename_));
      okay = file->WriteFully(header_data_ptr_, header_data_size_) &&
          file->WriteFully(body_data_ptr_, body_data_size_);
      if (!okay) {
        std::string msg(StringPrintf("Couldn't dump heap; writing \"%s\" failed: %s",
                                     filename_.c_str(), strerror(errno)));
        ThrowRuntimeException("%s", msg.c_str());
        LOG(ERROR) << msg;
      }
    }

    // Throw out a log message for the benefit of "runhat".
    if (okay) {
      uint64_t duration = NanoTime() - start_ns_;
      LOG(INFO) << "hprof: heap dump completed ("
          << PrettySize(header_data_size_ + body_data_size_ + 1023)
          << ") in " << PrettyDuration(duration);
    }
  }

 private:
  static void RootVisitor(const mirror::Object* obj, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    CHECK(arg != NULL);
    Hprof* hprof = reinterpret_cast<Hprof*>(arg);
    hprof->VisitRoot(obj);
  }

  static void HeapBitmapCallback(mirror::Object* obj, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    CHECK(obj != NULL);
    CHECK(arg != NULL);
    Hprof* hprof = reinterpret_cast<Hprof*>(arg);
    hprof->DumpHeapObject(obj);
  }

  void VisitRoot(const mirror::Object* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  int DumpHeapObject(mirror::Object* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void Finish() {
  }

  int WriteClassTable() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    HprofRecord* rec = &current_record_;
    uint32_t nextSerialNumber = 1;

    for (ClassSetIterator it = classes_.begin(); it != classes_.end(); ++it) {
      const mirror::Class* c = *it;
      CHECK(c != NULL);

      int err = current_record_.StartNewRecord(header_fp_, HPROF_TAG_LOAD_CLASS, HPROF_TIME);
      if (err != 0) {
        return err;
      }

      // LOAD CLASS format:
      // U4: class serial number (always > 0)
      // ID: class object ID. We use the address of the class object structure as its ID.
      // U4: stack trace serial number
      // ID: class name string ID
      rec->AddU4(nextSerialNumber++);
      rec->AddId((HprofClassObjectId) c);
      rec->AddU4(HPROF_NULL_STACK_TRACE);
      rec->AddId(LookupClassNameId(c));
    }

    return 0;
  }

  int WriteStringTable() {
    HprofRecord* rec = &current_record_;

    for (StringMapIterator it = strings_.begin(); it != strings_.end(); ++it) {
      std::string string((*it).first);
      size_t id = (*it).second;

      int err = current_record_.StartNewRecord(header_fp_, HPROF_TAG_STRING, HPROF_TIME);
      if (err != 0) {
        return err;
      }

      // STRING format:
      // ID:  ID for this string
      // U1*: UTF8 characters for string (NOT NULL terminated)
      //      (the record format encodes the length)
      err = rec->AddU4(id);
      if (err != 0) {
        return err;
      }
      err = rec->AddUtf8String(string.c_str());
      if (err != 0) {
        return err;
      }
    }

    return 0;
  }

  void StartNewHeapDumpSegment() {
    // This flushes the old segment and starts a new one.
    current_record_.StartNewRecord(body_fp_, HPROF_TAG_HEAP_DUMP_SEGMENT, HPROF_TIME);
    objects_in_segment_ = 0;

    // Starting a new HEAP_DUMP resets the heap to default.
    current_heap_ = HPROF_HEAP_DEFAULT;
  }

  int MarkRootObject(const mirror::Object* obj, jobject jniObj);

  HprofClassObjectId LookupClassId(mirror::Class* c)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (c == NULL) {
      // c is the superclass of java.lang.Object or a primitive
      return (HprofClassObjectId)0;
    }

    std::pair<ClassSetIterator, bool> result = classes_.insert(c);
    const mirror::Class* present = *result.first;

    // Make sure that we've assigned a string ID for this class' name
    LookupClassNameId(c);

    CHECK_EQ(present, c);
    return (HprofStringId) present;
  }

  HprofStringId LookupStringId(mirror::String* string) {
    return LookupStringId(string->ToModifiedUtf8());
  }

  HprofStringId LookupStringId(const char* string) {
    return LookupStringId(std::string(string));
  }

  HprofStringId LookupStringId(const std::string& string) {
    StringMapIterator it = strings_.find(string);
    if (it != strings_.end()) {
      return it->second;
    }
    HprofStringId id = next_string_id_++;
    strings_.Put(string, id);
    return id;
  }

  HprofStringId LookupClassNameId(const mirror::Class* c)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return LookupStringId(PrettyDescriptor(c));
  }

  void WriteFixedHeader() {
    char magic[] = "JAVA PROFILE 1.0.3";
    unsigned char buf[4];

    // Write the file header.
    // U1: NUL-terminated magic string.
    fwrite(magic, 1, sizeof(magic), header_fp_);

    // U4: size of identifiers.  We're using addresses as IDs, so make sure a pointer fits.
    U4_TO_BUF_BE(buf, 0, sizeof(void*));
    fwrite(buf, 1, sizeof(uint32_t), header_fp_);

    // The current time, in milliseconds since 0:00 GMT, 1/1/70.
    timeval now;
    uint64_t nowMs;
    if (gettimeofday(&now, NULL) < 0) {
      nowMs = 0;
    } else {
      nowMs = (uint64_t)now.tv_sec * 1000 + now.tv_usec / 1000;
    }

    // U4: high word of the 64-bit time.
    U4_TO_BUF_BE(buf, 0, (uint32_t)(nowMs >> 32));
    fwrite(buf, 1, sizeof(uint32_t), header_fp_);

    // U4: low word of the 64-bit time.
    U4_TO_BUF_BE(buf, 0, (uint32_t)(nowMs & 0xffffffffULL));
    fwrite(buf, 1, sizeof(uint32_t), header_fp_);  // xxx fix the time
  }

  void WriteStackTraces() {
    // Write a dummy stack trace record so the analysis tools don't freak out.
    current_record_.StartNewRecord(header_fp_, HPROF_TAG_STACK_TRACE, HPROF_TIME);
    current_record_.AddU4(HPROF_NULL_STACK_TRACE);
    current_record_.AddU4(HPROF_NULL_THREAD);
    current_record_.AddU4(0);    // no frames
  }

  // If direct_to_ddms_ is set, "filename_" and "fd" will be ignored.
  // Otherwise, "filename_" must be valid, though if "fd" >= 0 it will
  // only be used for debug messages.
  std::string filename_;
  int fd_;
  bool direct_to_ddms_;

  uint64_t start_ns_;

  HprofRecord current_record_;

  uint32_t gc_thread_serial_number_;
  uint8_t gc_scan_state_;
  HprofHeapId current_heap_;  // Which heap we're currently dumping.
  size_t objects_in_segment_;

  FILE* header_fp_;
  char* header_data_ptr_;
  size_t header_data_size_;

  FILE* body_fp_;
  char* body_data_ptr_;
  size_t body_data_size_;

  ClassSet classes_;
  size_t next_string_id_;
  StringMap strings_;

  DISALLOW_COPY_AND_ASSIGN(Hprof);
};

#define OBJECTS_PER_SEGMENT     ((size_t)128)
#define BYTES_PER_SEGMENT       ((size_t)4096)

// The static field-name for the synthetic object generated to account
// for class static overhead.
#define STATIC_OVERHEAD_NAME    "$staticOverhead"
// The ID for the synthetic object generated to account for class static overhead.
#define CLASS_STATICS_ID(c) ((HprofObjectId)(((uint32_t)(c)) | 1))

static HprofBasicType SignatureToBasicTypeAndSize(const char* sig, size_t* sizeOut) {
  char c = sig[0];
  HprofBasicType ret;
  size_t size;

  switch (c) {
  case '[':
  case 'L': ret = hprof_basic_object;  size = 4; break;
  case 'Z': ret = hprof_basic_boolean; size = 1; break;
  case 'C': ret = hprof_basic_char;    size = 2; break;
  case 'F': ret = hprof_basic_float;   size = 4; break;
  case 'D': ret = hprof_basic_double;  size = 8; break;
  case 'B': ret = hprof_basic_byte;    size = 1; break;
  case 'S': ret = hprof_basic_short;   size = 2; break;
  default: CHECK(false);
  case 'I': ret = hprof_basic_int;     size = 4; break;
  case 'J': ret = hprof_basic_long;    size = 8; break;
  }

  if (sizeOut != NULL) {
    *sizeOut = size;
  }

  return ret;
}

static HprofBasicType PrimitiveToBasicTypeAndSize(Primitive::Type prim, size_t* sizeOut) {
  HprofBasicType ret;
  size_t size;

  switch (prim) {
  case Primitive::kPrimBoolean: ret = hprof_basic_boolean; size = 1; break;
  case Primitive::kPrimChar:    ret = hprof_basic_char;    size = 2; break;
  case Primitive::kPrimFloat:   ret = hprof_basic_float;   size = 4; break;
  case Primitive::kPrimDouble:  ret = hprof_basic_double;  size = 8; break;
  case Primitive::kPrimByte:    ret = hprof_basic_byte;    size = 1; break;
  case Primitive::kPrimShort:   ret = hprof_basic_short;   size = 2; break;
  default: CHECK(false);
  case Primitive::kPrimInt:     ret = hprof_basic_int;     size = 4; break;
  case Primitive::kPrimLong:    ret = hprof_basic_long;    size = 8; break;
  }

  if (sizeOut != NULL) {
    *sizeOut = size;
  }

  return ret;
}

// Always called when marking objects, but only does
// something when ctx->gc_scan_state_ is non-zero, which is usually
// only true when marking the root set or unreachable
// objects.  Used to add rootset references to obj.
int Hprof::MarkRootObject(const mirror::Object* obj, jobject jniObj) {
  HprofRecord* rec = &current_record_;
  HprofHeapTag heapTag = (HprofHeapTag)gc_scan_state_;

  if (heapTag == 0) {
    return 0;
  }

  if (objects_in_segment_ >= OBJECTS_PER_SEGMENT || rec->Size() >= BYTES_PER_SEGMENT) {
    StartNewHeapDumpSegment();
  }

  switch (heapTag) {
  // ID: object ID
  case HPROF_ROOT_UNKNOWN:
  case HPROF_ROOT_STICKY_CLASS:
  case HPROF_ROOT_MONITOR_USED:
  case HPROF_ROOT_INTERNED_STRING:
  case HPROF_ROOT_DEBUGGER:
  case HPROF_ROOT_VM_INTERNAL:
    rec->AddU1(heapTag);
    rec->AddId((HprofObjectId)obj);
    break;

  // ID: object ID
  // ID: JNI global ref ID
  case HPROF_ROOT_JNI_GLOBAL:
    rec->AddU1(heapTag);
    rec->AddId((HprofObjectId)obj);
    rec->AddId((HprofId)jniObj);
    break;

  // ID: object ID
  // U4: thread serial number
  // U4: frame number in stack trace (-1 for empty)
  case HPROF_ROOT_JNI_LOCAL:
  case HPROF_ROOT_JNI_MONITOR:
  case HPROF_ROOT_JAVA_FRAME:
    rec->AddU1(heapTag);
    rec->AddId((HprofObjectId)obj);
    rec->AddU4(gc_thread_serial_number_);
    rec->AddU4((uint32_t)-1);
    break;

  // ID: object ID
  // U4: thread serial number
  case HPROF_ROOT_NATIVE_STACK:
  case HPROF_ROOT_THREAD_BLOCK:
    rec->AddU1(heapTag);
    rec->AddId((HprofObjectId)obj);
    rec->AddU4(gc_thread_serial_number_);
    break;

  // ID: thread object ID
  // U4: thread serial number
  // U4: stack trace serial number
  case HPROF_ROOT_THREAD_OBJECT:
    rec->AddU1(heapTag);
    rec->AddId((HprofObjectId)obj);
    rec->AddU4(gc_thread_serial_number_);
    rec->AddU4((uint32_t)-1);    // xxx
    break;

  case HPROF_CLASS_DUMP:
  case HPROF_INSTANCE_DUMP:
  case HPROF_OBJECT_ARRAY_DUMP:
  case HPROF_PRIMITIVE_ARRAY_DUMP:
  case HPROF_HEAP_DUMP_INFO:
  case HPROF_PRIMITIVE_ARRAY_NODATA_DUMP:
    // Ignored.
    break;

  case HPROF_ROOT_FINALIZING:
  case HPROF_ROOT_REFERENCE_CLEANUP:
  case HPROF_UNREACHABLE:
    LOG(FATAL) << "obsolete tag " << static_cast<int>(heapTag);
    break;
  }

  ++objects_in_segment_;
  return 0;
}

static int StackTraceSerialNumber(const mirror::Object* /*obj*/) {
  return HPROF_NULL_STACK_TRACE;
}

int Hprof::DumpHeapObject(mirror::Object* obj) {
  HprofRecord* rec = &current_record_;
  HprofHeapId desiredHeap = false ? HPROF_HEAP_ZYGOTE : HPROF_HEAP_APP;  // TODO: zygote objects?

  if (objects_in_segment_ >= OBJECTS_PER_SEGMENT || rec->Size() >= BYTES_PER_SEGMENT) {
    StartNewHeapDumpSegment();
  }

  if (desiredHeap != current_heap_) {
    HprofStringId nameId;

    // This object is in a different heap than the current one.
    // Emit a HEAP_DUMP_INFO tag to change heaps.
    rec->AddU1(HPROF_HEAP_DUMP_INFO);
    rec->AddU4((uint32_t)desiredHeap);   // uint32_t: heap id
    switch (desiredHeap) {
    case HPROF_HEAP_APP:
      nameId = LookupStringId("app");
      break;
    case HPROF_HEAP_ZYGOTE:
      nameId = LookupStringId("zygote");
      break;
    default:
      // Internal error
      LOG(ERROR) << "Unexpected desiredHeap";
      nameId = LookupStringId("<ILLEGAL>");
      break;
    }
    rec->AddId(nameId);
    current_heap_ = desiredHeap;
  }

  mirror::Class* c = obj->GetClass();
  if (c == NULL) {
    // This object will bother HprofReader, because it has a NULL
    // class, so just don't dump it. It could be
    // gDvm.unlinkedJavaLangClass or it could be an object just
    // allocated which hasn't been initialized yet.
  } else {
    if (obj->IsClass()) {
      mirror::Class* thisClass = obj->AsClass();
      // obj is a ClassObject.
      size_t sFieldCount = thisClass->NumStaticFields();
      if (sFieldCount != 0) {
        int byteLength = sFieldCount*sizeof(JValue);  // TODO bogus; fields are packed
        // Create a byte array to reflect the allocation of the
        // StaticField array at the end of this class.
        rec->AddU1(HPROF_PRIMITIVE_ARRAY_DUMP);
        rec->AddId(CLASS_STATICS_ID(obj));
        rec->AddU4(StackTraceSerialNumber(obj));
        rec->AddU4(byteLength);
        rec->AddU1(hprof_basic_byte);
        for (int i = 0; i < byteLength; ++i) {
          rec->AddU1(0);
        }
      }

      rec->AddU1(HPROF_CLASS_DUMP);
      rec->AddId(LookupClassId(thisClass));
      rec->AddU4(StackTraceSerialNumber(thisClass));
      rec->AddId(LookupClassId(thisClass->GetSuperClass()));
      rec->AddId((HprofObjectId)thisClass->GetClassLoader());
      rec->AddId((HprofObjectId)0);    // no signer
      rec->AddId((HprofObjectId)0);    // no prot domain
      rec->AddId((HprofId)0);           // reserved
      rec->AddId((HprofId)0);           // reserved
      if (thisClass->IsClassClass()) {
        // ClassObjects have their static fields appended, so aren't all the same size.
        // But they're at least this size.
        rec->AddU4(sizeof(mirror::Class));  // instance size
      } else if (thisClass->IsArrayClass() || thisClass->IsPrimitive()) {
        rec->AddU4(0);
      } else {
        rec->AddU4(thisClass->GetObjectSize());  // instance size
      }

      rec->AddU2(0);  // empty const pool

      FieldHelper fh;

      // Static fields
      if (sFieldCount == 0) {
        rec->AddU2((uint16_t)0);
      } else {
        rec->AddU2((uint16_t)(sFieldCount+1));
        rec->AddId(LookupStringId(STATIC_OVERHEAD_NAME));
        rec->AddU1(hprof_basic_object);
        rec->AddId(CLASS_STATICS_ID(obj));

        for (size_t i = 0; i < sFieldCount; ++i) {
          mirror::ArtField* f = thisClass->GetStaticField(i);
          fh.ChangeField(f);

          size_t size;
          HprofBasicType t = SignatureToBasicTypeAndSize(fh.GetTypeDescriptor(), &size);
          rec->AddId(LookupStringId(fh.GetName()));
          rec->AddU1(t);
          if (size == 1) {
            rec->AddU1(static_cast<uint8_t>(f->Get32(thisClass)));
          } else if (size == 2) {
            rec->AddU2(static_cast<uint16_t>(f->Get32(thisClass)));
          } else if (size == 4) {
            rec->AddU4(f->Get32(thisClass));
          } else if (size == 8) {
            rec->AddU8(f->Get64(thisClass));
          } else {
            CHECK(false);
          }
        }
      }

      // Instance fields for this class (no superclass fields)
      int iFieldCount = thisClass->IsObjectClass() ? 0 : thisClass->NumInstanceFields();
      rec->AddU2((uint16_t)iFieldCount);
      for (int i = 0; i < iFieldCount; ++i) {
        mirror::ArtField* f = thisClass->GetInstanceField(i);
        fh.ChangeField(f);
        HprofBasicType t = SignatureToBasicTypeAndSize(fh.GetTypeDescriptor(), NULL);
        rec->AddId(LookupStringId(fh.GetName()));
        rec->AddU1(t);
      }
    } else if (c->IsArrayClass()) {
      const mirror::Array* aobj = obj->AsArray();
      uint32_t length = aobj->GetLength();

      if (obj->IsObjectArray()) {
        // obj is an object array.
        rec->AddU1(HPROF_OBJECT_ARRAY_DUMP);

        rec->AddId((HprofObjectId)obj);
        rec->AddU4(StackTraceSerialNumber(obj));
        rec->AddU4(length);
        rec->AddId(LookupClassId(c));

        // Dump the elements, which are always objects or NULL.
        rec->AddIdList((const HprofObjectId*)aobj->GetRawData(sizeof(mirror::Object*)), length);
      } else {
        size_t size;
        HprofBasicType t = PrimitiveToBasicTypeAndSize(c->GetComponentType()->GetPrimitiveType(), &size);

        // obj is a primitive array.
        rec->AddU1(HPROF_PRIMITIVE_ARRAY_DUMP);

        rec->AddId((HprofObjectId)obj);
        rec->AddU4(StackTraceSerialNumber(obj));
        rec->AddU4(length);
        rec->AddU1(t);

        // Dump the raw, packed element values.
        if (size == 1) {
          rec->AddU1List((const uint8_t*)aobj->GetRawData(sizeof(uint8_t)), length);
        } else if (size == 2) {
          rec->AddU2List((const uint16_t*)aobj->GetRawData(sizeof(uint16_t)), length);
        } else if (size == 4) {
          rec->AddU4List((const uint32_t*)aobj->GetRawData(sizeof(uint32_t)), length);
        } else if (size == 8) {
          rec->AddU8List((const uint64_t*)aobj->GetRawData(sizeof(uint64_t)), length);
        }
      }
    } else {
      // obj is an instance object.
      rec->AddU1(HPROF_INSTANCE_DUMP);
      rec->AddId((HprofObjectId)obj);
      rec->AddU4(StackTraceSerialNumber(obj));
      rec->AddId(LookupClassId(c));

      // Reserve some space for the length of the instance data, which we won't
      // know until we're done writing it.
      size_t size_patch_offset = rec->Size();
      rec->AddU4(0x77777777);

      // Write the instance data;  fields for this class, followed by super class fields,
      // and so on. Don't write the klass or monitor fields of Object.class.
      const mirror::Class* sclass = c;
      FieldHelper fh;
      while (!sclass->IsObjectClass()) {
        int ifieldCount = sclass->NumInstanceFields();
        for (int i = 0; i < ifieldCount; ++i) {
          mirror::ArtField* f = sclass->GetInstanceField(i);
          fh.ChangeField(f);
          size_t size;
          SignatureToBasicTypeAndSize(fh.GetTypeDescriptor(), &size);
          if (size == 1) {
            rec->AddU1(f->Get32(obj));
          } else if (size == 2) {
            rec->AddU2(f->Get32(obj));
          } else if (size == 4) {
            rec->AddU4(f->Get32(obj));
          } else if (size == 8) {
            rec->AddU8(f->Get64(obj));
          } else {
            CHECK(false);
          }
        }

        sclass = sclass->GetSuperClass();
      }

      // Patch the instance field length.
      rec->UpdateU4(size_patch_offset, rec->Size() - (size_patch_offset + 4));
    }
  }

  ++objects_in_segment_;
  return 0;
}

void Hprof::VisitRoot(const mirror::Object* obj) {
  uint32_t threadId = 0;  // TODO
  /*RootType*/ size_t type = 0;  // TODO

  static const HprofHeapTag xlate[] = {
    HPROF_ROOT_UNKNOWN,
    HPROF_ROOT_JNI_GLOBAL,
    HPROF_ROOT_JNI_LOCAL,
    HPROF_ROOT_JAVA_FRAME,
    HPROF_ROOT_NATIVE_STACK,
    HPROF_ROOT_STICKY_CLASS,
    HPROF_ROOT_THREAD_BLOCK,
    HPROF_ROOT_MONITOR_USED,
    HPROF_ROOT_THREAD_OBJECT,
    HPROF_ROOT_INTERNED_STRING,
    HPROF_ROOT_FINALIZING,
    HPROF_ROOT_DEBUGGER,
    HPROF_ROOT_REFERENCE_CLEANUP,
    HPROF_ROOT_VM_INTERNAL,
    HPROF_ROOT_JNI_MONITOR,
  };

  CHECK_LT(type, sizeof(xlate) / sizeof(HprofHeapTag));
  if (obj == NULL) {
    return;
  }
  gc_scan_state_ = xlate[type];
  gc_thread_serial_number_ = threadId;
  MarkRootObject(obj, 0);
  gc_scan_state_ = 0;
  gc_thread_serial_number_ = 0;
}

// If "direct_to_ddms" is true, the other arguments are ignored, and data is
// sent directly to DDMS.
// If "fd" is >= 0, the output will be written to that file descriptor.
// Otherwise, "filename" is used to create an output file.
void DumpHeap(const char* filename, int fd, bool direct_to_ddms) {
  CHECK(filename != NULL);

  Runtime::Current()->GetThreadList()->SuspendAll();
  Hprof hprof(filename, fd, direct_to_ddms);
  hprof.Dump();
  Runtime::Current()->GetThreadList()->ResumeAll();
}

}  // namespace hprof

}  // namespace art
