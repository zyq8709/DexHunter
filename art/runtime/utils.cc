/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "utils.h"

#include <pthread.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "UniquePtr.h"
#include "base/unix_file/fd_file.h"
#include "dex_file-inl.h"
#include "mirror/art_field-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/string.h"
#include "object_utils.h"
#include "os.h"
#include "utf.h"

#if !defined(HAVE_POSIX_CLOCKS)
#include <sys/time.h>
#endif

#if defined(HAVE_PRCTL)
#include <sys/prctl.h>
#endif

#if defined(__APPLE__)
#include "AvailabilityMacros.h"  // For MAC_OS_X_VERSION_MAX_ALLOWED
#include <sys/syscall.h>
#endif

#include <corkscrew/backtrace.h>  // For DumpNativeStack.
#include <corkscrew/demangle.h>  // For DumpNativeStack.

#if defined(__linux__)
#include <linux/unistd.h>
#endif

namespace art {

pid_t GetTid() {
#if defined(__APPLE__)
  uint64_t owner;
  CHECK_PTHREAD_CALL(pthread_threadid_np, (NULL, &owner), __FUNCTION__);  // Requires Mac OS 10.6
  return owner;
#else
  // Neither bionic nor glibc exposes gettid(2).
  return syscall(__NR_gettid);
#endif
}

int GetUid(){
    return syscall(__NR_getuid);
}

std::string GetThreadName(pid_t tid) {
  std::string result;
  if (ReadFileToString(StringPrintf("/proc/self/task/%d/comm", tid), &result)) {
    result.resize(result.size() - 1);  // Lose the trailing '\n'.
  } else {
    result = "<unknown>";
  }
  return result;
}

void GetThreadStack(pthread_t thread, void*& stack_base, size_t& stack_size) {
#if defined(__APPLE__)
  stack_size = pthread_get_stacksize_np(thread);
  void* stack_addr = pthread_get_stackaddr_np(thread);

  // Check whether stack_addr is the base or end of the stack.
  // (On Mac OS 10.7, it's the end.)
  int stack_variable;
  if (stack_addr > &stack_variable) {
    stack_base = reinterpret_cast<byte*>(stack_addr) - stack_size;
  } else {
    stack_base = stack_addr;
  }
#else
  pthread_attr_t attributes;
  CHECK_PTHREAD_CALL(pthread_getattr_np, (thread, &attributes), __FUNCTION__);
  CHECK_PTHREAD_CALL(pthread_attr_getstack, (&attributes, &stack_base, &stack_size), __FUNCTION__);
  CHECK_PTHREAD_CALL(pthread_attr_destroy, (&attributes), __FUNCTION__);
#endif
}

bool ReadFileToString(const std::string& file_name, std::string* result) {
  UniquePtr<File> file(new File);
  if (!file->Open(file_name, O_RDONLY)) {
    return false;
  }

  std::vector<char> buf(8 * KB);
  while (true) {
    int64_t n = TEMP_FAILURE_RETRY(read(file->Fd(), &buf[0], buf.size()));
    if (n == -1) {
      return false;
    }
    if (n == 0) {
      return true;
    }
    result->append(&buf[0], n);
  }
}

std::string GetIsoDate() {
  time_t now = time(NULL);
  tm tmbuf;
  tm* ptm = localtime_r(&now, &tmbuf);
  return StringPrintf("%04d-%02d-%02d %02d:%02d:%02d",
      ptm->tm_year + 1900, ptm->tm_mon+1, ptm->tm_mday,
      ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
}

uint64_t MilliTime() {
#if defined(HAVE_POSIX_CLOCKS)
  timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<uint64_t>(now.tv_sec) * 1000LL + now.tv_nsec / 1000000LL;
#else
  timeval now;
  gettimeofday(&now, NULL);
  return static_cast<uint64_t>(now.tv_sec) * 1000LL + now.tv_usec / 1000LL;
#endif
}

uint64_t MicroTime() {
#if defined(HAVE_POSIX_CLOCKS)
  timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<uint64_t>(now.tv_sec) * 1000000LL + now.tv_nsec / 1000LL;
#else
  timeval now;
  gettimeofday(&now, NULL);
  return static_cast<uint64_t>(now.tv_sec) * 1000000LL + now.tv_usec;
#endif
}

uint64_t NanoTime() {
#if defined(HAVE_POSIX_CLOCKS)
  timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<uint64_t>(now.tv_sec) * 1000000000LL + now.tv_nsec;
#else
  timeval now;
  gettimeofday(&now, NULL);
  return static_cast<uint64_t>(now.tv_sec) * 1000000000LL + now.tv_usec * 1000LL;
#endif
}

uint64_t ThreadCpuNanoTime() {
#if defined(HAVE_POSIX_CLOCKS)
  timespec now;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
  return static_cast<uint64_t>(now.tv_sec) * 1000000000LL + now.tv_nsec;
#else
  UNIMPLEMENTED(WARNING);
  return -1;
#endif
}

void NanoSleep(uint64_t ns) {
  timespec tm;
  tm.tv_sec = 0;
  tm.tv_nsec = ns;
  nanosleep(&tm, NULL);
}

void InitTimeSpec(bool absolute, int clock, int64_t ms, int32_t ns, timespec* ts) {
  int64_t endSec;

  if (absolute) {
#if !defined(__APPLE__)
    clock_gettime(clock, ts);
#else
    UNUSED(clock);
    timeval tv;
    gettimeofday(&tv, NULL);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
#endif
  } else {
    ts->tv_sec = 0;
    ts->tv_nsec = 0;
  }
  endSec = ts->tv_sec + ms / 1000;
  if (UNLIKELY(endSec >= 0x7fffffff)) {
    std::ostringstream ss;
    LOG(INFO) << "Note: end time exceeds epoch: " << ss.str();
    endSec = 0x7ffffffe;
  }
  ts->tv_sec = endSec;
  ts->tv_nsec = (ts->tv_nsec + (ms % 1000) * 1000000) + ns;

  // Catch rollover.
  if (ts->tv_nsec >= 1000000000L) {
    ts->tv_sec++;
    ts->tv_nsec -= 1000000000L;
  }
}

std::string PrettyDescriptor(const mirror::String* java_descriptor) {
  if (java_descriptor == NULL) {
    return "null";
  }
  return PrettyDescriptor(java_descriptor->ToModifiedUtf8());
}

std::string PrettyDescriptor(const mirror::Class* klass) {
  if (klass == NULL) {
    return "null";
  }
  return PrettyDescriptor(ClassHelper(klass).GetDescriptor());
}

std::string PrettyDescriptor(const std::string& descriptor) {
  // Count the number of '['s to get the dimensionality.
  const char* c = descriptor.c_str();
  size_t dim = 0;
  while (*c == '[') {
    dim++;
    c++;
  }

  // Reference or primitive?
  if (*c == 'L') {
    // "[[La/b/C;" -> "a.b.C[][]".
    c++;  // Skip the 'L'.
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
    case 'V': c = "void;"; break;  // Used when decoding return types.
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

std::string PrettyDescriptor(Primitive::Type type) {
  std::string descriptor_string(Primitive::Descriptor(type));
  return PrettyDescriptor(descriptor_string);
}

std::string PrettyField(const mirror::ArtField* f, bool with_type) {
  if (f == NULL) {
    return "null";
  }
  FieldHelper fh(f);
  std::string result;
  if (with_type) {
    result += PrettyDescriptor(fh.GetTypeDescriptor());
    result += ' ';
  }
  result += PrettyDescriptor(fh.GetDeclaringClassDescriptor());
  result += '.';
  result += fh.GetName();
  return result;
}

std::string PrettyField(uint32_t field_idx, const DexFile& dex_file, bool with_type) {
  if (field_idx >= dex_file.NumFieldIds()) {
    return StringPrintf("<<invalid-field-idx-%d>>", field_idx);
  }
  const DexFile::FieldId& field_id = dex_file.GetFieldId(field_idx);
  std::string result;
  if (with_type) {
    result += dex_file.GetFieldTypeDescriptor(field_id);
    result += ' ';
  }
  result += PrettyDescriptor(dex_file.GetFieldDeclaringClassDescriptor(field_id));
  result += '.';
  result += dex_file.GetFieldName(field_id);
  return result;
}

std::string PrettyType(uint32_t type_idx, const DexFile& dex_file) {
  if (type_idx >= dex_file.NumTypeIds()) {
    return StringPrintf("<<invalid-type-idx-%d>>", type_idx);
  }
  const DexFile::TypeId& type_id = dex_file.GetTypeId(type_idx);
  return PrettyDescriptor(dex_file.GetTypeDescriptor(type_id));
}

std::string PrettyArguments(const char* signature) {
  std::string result;
  result += '(';
  CHECK_EQ(*signature, '(');
  ++signature;  // Skip the '('.
  while (*signature != ')') {
    size_t argument_length = 0;
    while (signature[argument_length] == '[') {
      ++argument_length;
    }
    if (signature[argument_length] == 'L') {
      argument_length = (strchr(signature, ';') - signature + 1);
    } else {
      ++argument_length;
    }
    std::string argument_descriptor(signature, argument_length);
    result += PrettyDescriptor(argument_descriptor);
    if (signature[argument_length] != ')') {
      result += ", ";
    }
    signature += argument_length;
  }
  CHECK_EQ(*signature, ')');
  ++signature;  // Skip the ')'.
  result += ')';
  return result;
}

std::string PrettyReturnType(const char* signature) {
  const char* return_type = strchr(signature, ')');
  CHECK(return_type != NULL);
  ++return_type;  // Skip ')'.
  return PrettyDescriptor(return_type);
}

std::string PrettyMethod(const mirror::ArtMethod* m, bool with_signature) {
  if (m == NULL) {
    return "null";
  }
  MethodHelper mh(m);
  std::string result(PrettyDescriptor(mh.GetDeclaringClassDescriptor()));
  result += '.';
  result += mh.GetName();
  if (with_signature) {
    std::string signature(mh.GetSignature());
    if (signature == "<no signature>") {
      return result + signature;
    }
    result = PrettyReturnType(signature.c_str()) + " " + result + PrettyArguments(signature.c_str());
  }
  return result;
}

std::string PrettyMethod(uint32_t method_idx, const DexFile& dex_file, bool with_signature) {
  if (method_idx >= dex_file.NumMethodIds()) {
    return StringPrintf("<<invalid-method-idx-%d>>", method_idx);
  }
  const DexFile::MethodId& method_id = dex_file.GetMethodId(method_idx);
  std::string result(PrettyDescriptor(dex_file.GetMethodDeclaringClassDescriptor(method_id)));
  result += '.';
  result += dex_file.GetMethodName(method_id);
  if (with_signature) {
    std::string signature(dex_file.GetMethodSignature(method_id));
    if (signature == "<no signature>") {
      return result + signature;
    }
    result = PrettyReturnType(signature.c_str()) + " " + result + PrettyArguments(signature.c_str());
  }
  return result;
}

std::string PrettyTypeOf(const mirror::Object* obj) {
  if (obj == NULL) {
    return "null";
  }
  if (obj->GetClass() == NULL) {
    return "(raw)";
  }
  ClassHelper kh(obj->GetClass());
  std::string result(PrettyDescriptor(kh.GetDescriptor()));
  if (obj->IsClass()) {
    kh.ChangeClass(obj->AsClass());
    result += "<" + PrettyDescriptor(kh.GetDescriptor()) + ">";
  }
  return result;
}

std::string PrettyClass(const mirror::Class* c) {
  if (c == NULL) {
    return "null";
  }
  std::string result;
  result += "java.lang.Class<";
  result += PrettyDescriptor(c);
  result += ">";
  return result;
}

std::string PrettyClassAndClassLoader(const mirror::Class* c) {
  if (c == NULL) {
    return "null";
  }
  std::string result;
  result += "java.lang.Class<";
  result += PrettyDescriptor(c);
  result += ",";
  result += PrettyTypeOf(c->GetClassLoader());
  // TODO: add an identifying hash value for the loader
  result += ">";
  return result;
}

std::string PrettySize(size_t byte_count) {
  // The byte thresholds at which we display amounts.  A byte count is displayed
  // in unit U when kUnitThresholds[U] <= bytes < kUnitThresholds[U+1].
  static const size_t kUnitThresholds[] = {
    0,              // B up to...
    3*1024,         // KB up to...
    2*1024*1024,    // MB up to...
    1024*1024*1024  // GB from here.
  };
  static const size_t kBytesPerUnit[] = { 1, KB, MB, GB };
  static const char* const kUnitStrings[] = { "B", "KB", "MB", "GB" };

  int i = arraysize(kUnitThresholds);
  while (--i > 0) {
    if (byte_count >= kUnitThresholds[i]) {
      break;
    }
  }

  return StringPrintf("%zd%s", byte_count / kBytesPerUnit[i], kUnitStrings[i]);
}

std::string PrettyDuration(uint64_t nano_duration) {
  if (nano_duration == 0) {
    return "0";
  } else {
    return FormatDuration(nano_duration, GetAppropriateTimeUnit(nano_duration));
  }
}

TimeUnit GetAppropriateTimeUnit(uint64_t nano_duration) {
  const uint64_t one_sec = 1000 * 1000 * 1000;
  const uint64_t one_ms  = 1000 * 1000;
  const uint64_t one_us  = 1000;
  if (nano_duration >= one_sec) {
    return kTimeUnitSecond;
  } else if (nano_duration >= one_ms) {
    return kTimeUnitMillisecond;
  } else if (nano_duration >= one_us) {
    return kTimeUnitMicrosecond;
  } else {
    return kTimeUnitNanosecond;
  }
}

uint64_t GetNsToTimeUnitDivisor(TimeUnit time_unit) {
  const uint64_t one_sec = 1000 * 1000 * 1000;
  const uint64_t one_ms  = 1000 * 1000;
  const uint64_t one_us  = 1000;

  switch (time_unit) {
    case kTimeUnitSecond:
      return one_sec;
    case kTimeUnitMillisecond:
      return one_ms;
    case kTimeUnitMicrosecond:
      return one_us;
    case kTimeUnitNanosecond:
      return 1;
  }
  return 0;
}

std::string FormatDuration(uint64_t nano_duration, TimeUnit time_unit) {
  const char* unit = NULL;
  uint64_t divisor = GetNsToTimeUnitDivisor(time_unit);
  uint32_t zero_fill = 1;
  switch (time_unit) {
    case kTimeUnitSecond:
      unit = "s";
      zero_fill = 9;
      break;
    case kTimeUnitMillisecond:
      unit = "ms";
      zero_fill = 6;
      break;
    case kTimeUnitMicrosecond:
      unit = "us";
      zero_fill = 3;
      break;
    case kTimeUnitNanosecond:
      unit = "ns";
      zero_fill = 0;
      break;
  }

  uint64_t whole_part = nano_duration / divisor;
  uint64_t fractional_part = nano_duration % divisor;
  if (fractional_part == 0) {
    return StringPrintf("%llu%s", whole_part, unit);
  } else {
    while ((fractional_part % 1000) == 0) {
      zero_fill -= 3;
      fractional_part /= 1000;
    }
    if (zero_fill == 3) {
      return StringPrintf("%llu.%03llu%s", whole_part, fractional_part, unit);
    } else if (zero_fill == 6) {
      return StringPrintf("%llu.%06llu%s", whole_part, fractional_part, unit);
    } else {
      return StringPrintf("%llu.%09llu%s", whole_part, fractional_part, unit);
    }
  }
}

std::string PrintableString(const std::string& utf) {
  std::string result;
  result += '"';
  const char* p = utf.c_str();
  size_t char_count = CountModifiedUtf8Chars(p);
  for (size_t i = 0; i < char_count; ++i) {
    uint16_t ch = GetUtf16FromUtf8(&p);
    if (ch == '\\') {
      result += "\\\\";
    } else if (ch == '\n') {
      result += "\\n";
    } else if (ch == '\r') {
      result += "\\r";
    } else if (ch == '\t') {
      result += "\\t";
    } else if (NeedsEscaping(ch)) {
      StringAppendF(&result, "\\u%04x", ch);
    } else {
      result += ch;
    }
  }
  result += '"';
  return result;
}

// See http://java.sun.com/j2se/1.5.0/docs/guide/jni/spec/design.html#wp615 for the full rules.
std::string MangleForJni(const std::string& s) {
  std::string result;
  size_t char_count = CountModifiedUtf8Chars(s.c_str());
  const char* cp = &s[0];
  for (size_t i = 0; i < char_count; ++i) {
    uint16_t ch = GetUtf16FromUtf8(&cp);
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
      result.push_back(ch);
    } else if (ch == '.' || ch == '/') {
      result += "_";
    } else if (ch == '_') {
      result += "_1";
    } else if (ch == ';') {
      result += "_2";
    } else if (ch == '[') {
      result += "_3";
    } else {
      StringAppendF(&result, "_0%04x", ch);
    }
  }
  return result;
}

std::string DotToDescriptor(const char* class_name) {
  std::string descriptor(class_name);
  std::replace(descriptor.begin(), descriptor.end(), '.', '/');
  if (descriptor.length() > 0 && descriptor[0] != '[') {
    descriptor = "L" + descriptor + ";";
  }
  return descriptor;
}

std::string DescriptorToDot(const char* descriptor) {
  size_t length = strlen(descriptor);
  if (descriptor[0] == 'L' && descriptor[length - 1] == ';') {
    std::string result(descriptor + 1, length - 2);
    std::replace(result.begin(), result.end(), '/', '.');
    return result;
  }
  return descriptor;
}

std::string DescriptorToName(const char* descriptor) {
  size_t length = strlen(descriptor);
  if (descriptor[0] == 'L' && descriptor[length - 1] == ';') {
    std::string result(descriptor + 1, length - 2);
    return result;
  }
  return descriptor;
}

std::string JniShortName(const mirror::ArtMethod* m) {
  MethodHelper mh(m);
  std::string class_name(mh.GetDeclaringClassDescriptor());
  // Remove the leading 'L' and trailing ';'...
  CHECK_EQ(class_name[0], 'L') << class_name;
  CHECK_EQ(class_name[class_name.size() - 1], ';') << class_name;
  class_name.erase(0, 1);
  class_name.erase(class_name.size() - 1, 1);

  std::string method_name(mh.GetName());

  std::string short_name;
  short_name += "Java_";
  short_name += MangleForJni(class_name);
  short_name += "_";
  short_name += MangleForJni(method_name);
  return short_name;
}

std::string JniLongName(const mirror::ArtMethod* m) {
  std::string long_name;
  long_name += JniShortName(m);
  long_name += "__";

  std::string signature(MethodHelper(m).GetSignature());
  signature.erase(0, 1);
  signature.erase(signature.begin() + signature.find(')'), signature.end());

  long_name += MangleForJni(signature);

  return long_name;
}

// Helper for IsValidPartOfMemberNameUtf8(), a bit vector indicating valid low ascii.
uint32_t DEX_MEMBER_VALID_LOW_ASCII[4] = {
  0x00000000,  // 00..1f low control characters; nothing valid
  0x03ff2010,  // 20..3f digits and symbols; valid: '0'..'9', '$', '-'
  0x87fffffe,  // 40..5f uppercase etc.; valid: 'A'..'Z', '_'
  0x07fffffe   // 60..7f lowercase etc.; valid: 'a'..'z'
};

// Helper for IsValidPartOfMemberNameUtf8(); do not call directly.
bool IsValidPartOfMemberNameUtf8Slow(const char** pUtf8Ptr) {
  /*
   * It's a multibyte encoded character. Decode it and analyze. We
   * accept anything that isn't (a) an improperly encoded low value,
   * (b) an improper surrogate pair, (c) an encoded '\0', (d) a high
   * control character, or (e) a high space, layout, or special
   * character (U+00a0, U+2000..U+200f, U+2028..U+202f,
   * U+fff0..U+ffff). This is all specified in the dex format
   * document.
   */

  uint16_t utf16 = GetUtf16FromUtf8(pUtf8Ptr);

  // Perform follow-up tests based on the high 8 bits.
  switch (utf16 >> 8) {
  case 0x00:
    // It's only valid if it's above the ISO-8859-1 high space (0xa0).
    return (utf16 > 0x00a0);
  case 0xd8:
  case 0xd9:
  case 0xda:
  case 0xdb:
    // It's a leading surrogate. Check to see that a trailing
    // surrogate follows.
    utf16 = GetUtf16FromUtf8(pUtf8Ptr);
    return (utf16 >= 0xdc00) && (utf16 <= 0xdfff);
  case 0xdc:
  case 0xdd:
  case 0xde:
  case 0xdf:
    // It's a trailing surrogate, which is not valid at this point.
    return false;
  case 0x20:
  case 0xff:
    // It's in the range that has spaces, controls, and specials.
    switch (utf16 & 0xfff8) {
    case 0x2000:
    case 0x2008:
    case 0x2028:
    case 0xfff0:
    case 0xfff8:
      return false;
    }
    break;
  }
  return true;
}

/* Return whether the pointed-at modified-UTF-8 encoded character is
 * valid as part of a member name, updating the pointer to point past
 * the consumed character. This will consume two encoded UTF-16 code
 * points if the character is encoded as a surrogate pair. Also, if
 * this function returns false, then the given pointer may only have
 * been partially advanced.
 */
bool IsValidPartOfMemberNameUtf8(const char** pUtf8Ptr) {
  uint8_t c = (uint8_t) **pUtf8Ptr;
  if (c <= 0x7f) {
    // It's low-ascii, so check the table.
    uint32_t wordIdx = c >> 5;
    uint32_t bitIdx = c & 0x1f;
    (*pUtf8Ptr)++;
    return (DEX_MEMBER_VALID_LOW_ASCII[wordIdx] & (1 << bitIdx)) != 0;
  }

  // It's a multibyte encoded character. Call a non-inline function
  // for the heavy lifting.
  return IsValidPartOfMemberNameUtf8Slow(pUtf8Ptr);
}

bool IsValidMemberName(const char* s) {
  bool angle_name = false;

  switch (*s) {
    case '\0':
      // The empty string is not a valid name.
      return false;
    case '<':
      angle_name = true;
      s++;
      break;
  }

  while (true) {
    switch (*s) {
      case '\0':
        return !angle_name;
      case '>':
        return angle_name && s[1] == '\0';
    }

    if (!IsValidPartOfMemberNameUtf8(&s)) {
      return false;
    }
  }
}

enum ClassNameType { kName, kDescriptor };
bool IsValidClassName(const char* s, ClassNameType type, char separator) {
  int arrayCount = 0;
  while (*s == '[') {
    arrayCount++;
    s++;
  }

  if (arrayCount > 255) {
    // Arrays may have no more than 255 dimensions.
    return false;
  }

  if (arrayCount != 0) {
    /*
     * If we're looking at an array of some sort, then it doesn't
     * matter if what is being asked for is a class name; the
     * format looks the same as a type descriptor in that case, so
     * treat it as such.
     */
    type = kDescriptor;
  }

  if (type == kDescriptor) {
    /*
     * We are looking for a descriptor. Either validate it as a
     * single-character primitive type, or continue on to check the
     * embedded class name (bracketed by "L" and ";").
     */
    switch (*(s++)) {
    case 'B':
    case 'C':
    case 'D':
    case 'F':
    case 'I':
    case 'J':
    case 'S':
    case 'Z':
      // These are all single-character descriptors for primitive types.
      return (*s == '\0');
    case 'V':
      // Non-array void is valid, but you can't have an array of void.
      return (arrayCount == 0) && (*s == '\0');
    case 'L':
      // Class name: Break out and continue below.
      break;
    default:
      // Oddball descriptor character.
      return false;
    }
  }

  /*
   * We just consumed the 'L' that introduces a class name as part
   * of a type descriptor, or we are looking for an unadorned class
   * name.
   */

  bool sepOrFirst = true;  // first character or just encountered a separator.
  for (;;) {
    uint8_t c = (uint8_t) *s;
    switch (c) {
    case '\0':
      /*
       * Premature end for a type descriptor, but valid for
       * a class name as long as we haven't encountered an
       * empty component (including the degenerate case of
       * the empty string "").
       */
      return (type == kName) && !sepOrFirst;
    case ';':
      /*
       * Invalid character for a class name, but the
       * legitimate end of a type descriptor. In the latter
       * case, make sure that this is the end of the string
       * and that it doesn't end with an empty component
       * (including the degenerate case of "L;").
       */
      return (type == kDescriptor) && !sepOrFirst && (s[1] == '\0');
    case '/':
    case '.':
      if (c != separator) {
        // The wrong separator character.
        return false;
      }
      if (sepOrFirst) {
        // Separator at start or two separators in a row.
        return false;
      }
      sepOrFirst = true;
      s++;
      break;
    default:
      if (!IsValidPartOfMemberNameUtf8(&s)) {
        return false;
      }
      sepOrFirst = false;
      break;
    }
  }
}

bool IsValidBinaryClassName(const char* s) {
  return IsValidClassName(s, kName, '.');
}

bool IsValidJniClassName(const char* s) {
  return IsValidClassName(s, kName, '/');
}

bool IsValidDescriptor(const char* s) {
  return IsValidClassName(s, kDescriptor, '/');
}

void Split(const std::string& s, char separator, std::vector<std::string>& result) {
  const char* p = s.data();
  const char* end = p + s.size();
  while (p != end) {
    if (*p == separator) {
      ++p;
    } else {
      const char* start = p;
      while (++p != end && *p != separator) {
        // Skip to the next occurrence of the separator.
      }
      result.push_back(std::string(start, p - start));
    }
  }
}

template <typename StringT>
std::string Join(std::vector<StringT>& strings, char separator) {
  if (strings.empty()) {
    return "";
  }

  std::string result(strings[0]);
  for (size_t i = 1; i < strings.size(); ++i) {
    result += separator;
    result += strings[i];
  }
  return result;
}

// Explicit instantiations.
template std::string Join<std::string>(std::vector<std::string>& strings, char separator);
template std::string Join<const char*>(std::vector<const char*>& strings, char separator);
template std::string Join<char*>(std::vector<char*>& strings, char separator);

bool StartsWith(const std::string& s, const char* prefix) {
  return s.compare(0, strlen(prefix), prefix) == 0;
}

bool EndsWith(const std::string& s, const char* suffix) {
  size_t suffix_length = strlen(suffix);
  size_t string_length = s.size();
  if (suffix_length > string_length) {
    return false;
  }
  size_t offset = string_length - suffix_length;
  return s.compare(offset, suffix_length, suffix) == 0;
}

void SetThreadName(const char* thread_name) {
  int hasAt = 0;
  int hasDot = 0;
  const char* s = thread_name;
  while (*s) {
    if (*s == '.') {
      hasDot = 1;
    } else if (*s == '@') {
      hasAt = 1;
    }
    s++;
  }
  int len = s - thread_name;
  if (len < 15 || hasAt || !hasDot) {
    s = thread_name;
  } else {
    s = thread_name + len - 15;
  }
#if defined(HAVE_ANDROID_PTHREAD_SETNAME_NP)
  // pthread_setname_np fails rather than truncating long strings.
  char buf[16];       // MAX_TASK_COMM_LEN=16 is hard-coded into bionic
  strncpy(buf, s, sizeof(buf)-1);
  buf[sizeof(buf)-1] = '\0';
  errno = pthread_setname_np(pthread_self(), buf);
  if (errno != 0) {
    PLOG(WARNING) << "Unable to set the name of current thread to '" << buf << "'";
  }
#elif defined(__APPLE__) && MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
  pthread_setname_np(thread_name);
#elif defined(HAVE_PRCTL)
  prctl(PR_SET_NAME, (unsigned long) s, 0, 0, 0);  // NOLINT (unsigned long)
#else
  UNIMPLEMENTED(WARNING) << thread_name;
#endif
}

void GetTaskStats(pid_t tid, char& state, int& utime, int& stime, int& task_cpu) {
  utime = stime = task_cpu = 0;
  std::string stats;
  if (!ReadFileToString(StringPrintf("/proc/self/task/%d/stat", tid), &stats)) {
    return;
  }
  // Skip the command, which may contain spaces.
  stats = stats.substr(stats.find(')') + 2);
  // Extract the three fields we care about.
  std::vector<std::string> fields;
  Split(stats, ' ', fields);
  state = fields[0][0];
  utime = strtoull(fields[11].c_str(), NULL, 10);
  stime = strtoull(fields[12].c_str(), NULL, 10);
  task_cpu = strtoull(fields[36].c_str(), NULL, 10);
}

std::string GetSchedulerGroupName(pid_t tid) {
  // /proc/<pid>/cgroup looks like this:
  // 2:devices:/
  // 1:cpuacct,cpu:/
  // We want the third field from the line whose second field contains the "cpu" token.
  std::string cgroup_file;
  if (!ReadFileToString(StringPrintf("/proc/self/task/%d/cgroup", tid), &cgroup_file)) {
    return "";
  }
  std::vector<std::string> cgroup_lines;
  Split(cgroup_file, '\n', cgroup_lines);
  for (size_t i = 0; i < cgroup_lines.size(); ++i) {
    std::vector<std::string> cgroup_fields;
    Split(cgroup_lines[i], ':', cgroup_fields);
    std::vector<std::string> cgroups;
    Split(cgroup_fields[1], ',', cgroups);
    for (size_t i = 0; i < cgroups.size(); ++i) {
      if (cgroups[i] == "cpu") {
        return cgroup_fields[2].substr(1);  // Skip the leading slash.
      }
    }
  }
  return "";
}

static const char* CleanMapName(const backtrace_symbol_t* symbol) {
  const char* map_name = symbol->map_name;
  if (map_name == NULL) {
    map_name = "???";
  }
  // Turn "/usr/local/google/home/enh/clean-dalvik-dev/out/host/linux-x86/lib/libartd.so"
  // into "libartd.so".
  const char* last_slash = strrchr(map_name, '/');
  if (last_slash != NULL) {
    map_name = last_slash + 1;
  }
  return map_name;
}

static void FindSymbolInElf(const backtrace_frame_t* frame, const backtrace_symbol_t* symbol,
                            std::string& symbol_name, uint32_t& pc_offset) {
  symbol_table_t* symbol_table = NULL;
  if (symbol->map_name != NULL) {
    symbol_table = load_symbol_table(symbol->map_name);
  }
  const symbol_t* elf_symbol = NULL;
  bool was_relative = true;
  if (symbol_table != NULL) {
    elf_symbol = find_symbol(symbol_table, symbol->relative_pc);
    if (elf_symbol == NULL) {
      elf_symbol = find_symbol(symbol_table, frame->absolute_pc);
      was_relative = false;
    }
  }
  if (elf_symbol != NULL) {
    const char* demangled_symbol_name = demangle_symbol_name(elf_symbol->name);
    if (demangled_symbol_name != NULL) {
      symbol_name = demangled_symbol_name;
    } else {
      symbol_name = elf_symbol->name;
    }

    // TODO: is it a libcorkscrew bug that we have to do this?
    pc_offset = (was_relative ? symbol->relative_pc : frame->absolute_pc) - elf_symbol->start;
  } else {
    symbol_name = "???";
  }
  free_symbol_table(symbol_table);
}

void DumpNativeStack(std::ostream& os, pid_t tid, const char* prefix, bool include_count) {
  // Ensure libcorkscrew doesn't use a stale cache of /proc/self/maps.
  flush_my_map_info_list();

  const size_t MAX_DEPTH = 32;
  UniquePtr<backtrace_frame_t[]> frames(new backtrace_frame_t[MAX_DEPTH]);
  size_t ignore_count = 2;  // Don't include unwind_backtrace_thread or DumpNativeStack.
  ssize_t frame_count = unwind_backtrace_thread(tid, frames.get(), ignore_count, MAX_DEPTH);
  if (frame_count == -1) {
    os << prefix << "(unwind_backtrace_thread failed for thread " << tid << ")\n";
    return;
  } else if (frame_count == 0) {
    os << prefix << "(no native stack frames for thread " << tid << ")\n";
    return;
  }

  UniquePtr<backtrace_symbol_t[]> backtrace_symbols(new backtrace_symbol_t[frame_count]);
  get_backtrace_symbols(frames.get(), frame_count, backtrace_symbols.get());

  for (size_t i = 0; i < static_cast<size_t>(frame_count); ++i) {
    const backtrace_frame_t* frame = &frames[i];
    const backtrace_symbol_t* symbol = &backtrace_symbols[i];

    // We produce output like this:
    // ]    #00 unwind_backtrace_thread+536 [0x55d75bb8] (libcorkscrew.so)

    std::string symbol_name;
    uint32_t pc_offset = 0;
    if (symbol->demangled_name != NULL) {
      symbol_name = symbol->demangled_name;
      pc_offset = symbol->relative_pc - symbol->relative_symbol_addr;
    } else if (symbol->symbol_name != NULL) {
      symbol_name = symbol->symbol_name;
      pc_offset = symbol->relative_pc - symbol->relative_symbol_addr;
    } else {
      // dladdr(3) didn't find a symbol; maybe it's static? Look in the ELF file...
      FindSymbolInElf(frame, symbol, symbol_name, pc_offset);
    }

    os << prefix;
    if (include_count) {
      os << StringPrintf("#%02zd ", i);
    }
    os << symbol_name;
    if (pc_offset != 0) {
      os << "+" << pc_offset;
    }
    os << StringPrintf(" [%p] (%s)\n",
                       reinterpret_cast<void*>(frame->absolute_pc), CleanMapName(symbol));
  }

  free_backtrace_symbols(backtrace_symbols.get(), frame_count);
}

#if defined(__APPLE__)

// TODO: is there any way to get the kernel stack on Mac OS?
void DumpKernelStack(std::ostream&, pid_t, const char*, bool) {}

#else

void DumpKernelStack(std::ostream& os, pid_t tid, const char* prefix, bool include_count) {
  if (tid == GetTid()) {
    // There's no point showing that we're reading our stack out of /proc!
    return;
  }

  std::string kernel_stack_filename(StringPrintf("/proc/self/task/%d/stack", tid));
  std::string kernel_stack;
  if (!ReadFileToString(kernel_stack_filename, &kernel_stack)) {
    os << prefix << "(couldn't read " << kernel_stack_filename << ")\n";
    return;
  }

  std::vector<std::string> kernel_stack_frames;
  Split(kernel_stack, '\n', kernel_stack_frames);
  // We skip the last stack frame because it's always equivalent to "[<ffffffff>] 0xffffffff",
  // which looking at the source appears to be the kernel's way of saying "that's all, folks!".
  kernel_stack_frames.pop_back();
  for (size_t i = 0; i < kernel_stack_frames.size(); ++i) {
    // Turn "[<ffffffff8109156d>] futex_wait_queue_me+0xcd/0x110" into "futex_wait_queue_me+0xcd/0x110".
    const char* text = kernel_stack_frames[i].c_str();
    const char* close_bracket = strchr(text, ']');
    if (close_bracket != NULL) {
      text = close_bracket + 2;
    }
    os << prefix;
    if (include_count) {
      os << StringPrintf("#%02zd ", i);
    }
    os << text << "\n";
  }
}

#endif

const char* GetAndroidRoot() {
  const char* android_root = getenv("ANDROID_ROOT");
  if (android_root == NULL) {
    if (OS::DirectoryExists("/system")) {
      android_root = "/system";
    } else {
      LOG(FATAL) << "ANDROID_ROOT not set and /system does not exist";
      return "";
    }
  }
  if (!OS::DirectoryExists(android_root)) {
    LOG(FATAL) << "Failed to find ANDROID_ROOT directory " << android_root;
    return "";
  }
  return android_root;
}

const char* GetAndroidData() {
  const char* android_data = getenv("ANDROID_DATA");
  if (android_data == NULL) {
    if (OS::DirectoryExists("/data")) {
      android_data = "/data";
    } else {
      LOG(FATAL) << "ANDROID_DATA not set and /data does not exist";
      return "";
    }
  }
  if (!OS::DirectoryExists(android_data)) {
    LOG(FATAL) << "Failed to find ANDROID_DATA directory " << android_data;
    return "";
  }
  return android_data;
}

std::string GetDalvikCacheOrDie(const char* android_data) {
  std::string dalvik_cache(StringPrintf("%s/dalvik-cache", android_data));

  if (!OS::DirectoryExists(dalvik_cache.c_str())) {
    if (StartsWith(dalvik_cache, "/tmp/")) {
      int result = mkdir(dalvik_cache.c_str(), 0700);
      if (result != 0) {
        LOG(FATAL) << "Failed to create dalvik-cache directory " << dalvik_cache;
        return "";
      }
    } else {
      LOG(FATAL) << "Failed to find dalvik-cache directory " << dalvik_cache;
      return "";
    }
  }
  return dalvik_cache;
}

std::string GetDalvikCacheFilenameOrDie(const std::string& location) {
  std::string dalvik_cache(GetDalvikCacheOrDie(GetAndroidData()));
  if (location[0] != '/') {
    LOG(FATAL) << "Expected path in location to be absolute: "<< location;
  }
  std::string cache_file(location, 1);  // skip leading slash
  if (!EndsWith(location, ".dex") && !EndsWith(location, ".art")) {
    cache_file += "/";
    cache_file += DexFile::kClassesDex;
  }
  std::replace(cache_file.begin(), cache_file.end(), '/', '@');
  return dalvik_cache + "/" + cache_file;
}

bool IsZipMagic(uint32_t magic) {
  return (('P' == ((magic >> 0) & 0xff)) &&
          ('K' == ((magic >> 8) & 0xff)));
}

bool IsDexMagic(uint32_t magic) {
  return DexFile::IsMagicValid(reinterpret_cast<const byte*>(&magic));
}

bool IsOatMagic(uint32_t magic) {
  return (memcmp(reinterpret_cast<const byte*>(magic),
                 OatHeader::kOatMagic,
                 sizeof(OatHeader::kOatMagic)) == 0);
}

}  // namespace art
