/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_INVOKE_ARG_ARRAY_BUILDER_H_
#define ART_RUNTIME_INVOKE_ARG_ARRAY_BUILDER_H_

#include "mirror/art_method.h"
#include "mirror/object.h"
#include "scoped_thread_state_change.h"

namespace art {

static inline size_t NumArgArrayBytes(const char* shorty, uint32_t shorty_len) {
  size_t num_bytes = 0;
  for (size_t i = 1; i < shorty_len; ++i) {
    char ch = shorty[i];
    if (ch == 'D' || ch == 'J') {
      num_bytes += 8;
    } else if (ch == 'L') {
      // Argument is a reference or an array.  The shorty descriptor
      // does not distinguish between these types.
      num_bytes += sizeof(mirror::Object*);
    } else {
      num_bytes += 4;
    }
  }
  return num_bytes;
}

class ArgArray {
 public:
  explicit ArgArray(const char* shorty, uint32_t shorty_len)
      : shorty_(shorty), shorty_len_(shorty_len), num_bytes_(0) {
    size_t num_slots = shorty_len + 1;  // +1 in case of receiver.
    if (LIKELY((num_slots * 2) < kSmallArgArraySize)) {
      // We can trivially use the small arg array.
      arg_array_ = small_arg_array_;
    } else {
      // Analyze shorty to see if we need the large arg array.
      for (size_t i = 1; i < shorty_len; ++i) {
        char c = shorty[i];
        if (c == 'J' || c == 'D') {
          num_slots++;
        }
      }
      if (num_slots <= kSmallArgArraySize) {
        arg_array_ = small_arg_array_;
      } else {
        large_arg_array_.reset(new uint32_t[num_slots]);
        arg_array_ = large_arg_array_.get();
      }
    }
  }

  uint32_t* GetArray() {
    return arg_array_;
  }

  uint32_t GetNumBytes() {
    return num_bytes_;
  }

  void Append(uint32_t value) {
    arg_array_[num_bytes_ / 4] = value;
    num_bytes_ += 4;
  }

  void AppendWide(uint64_t value) {
    // For ARM and MIPS portable, align wide values to 8 bytes (ArgArray starts at offset of 4).
#if defined(ART_USE_PORTABLE_COMPILER) && (defined(__arm__) || defined(__mips__))
    if (num_bytes_ % 8 == 0) {
      num_bytes_ += 4;
    }
#endif
    arg_array_[num_bytes_ / 4] = value;
    arg_array_[(num_bytes_ / 4) + 1] = value >> 32;
    num_bytes_ += 8;
  }

  void BuildArgArray(const ScopedObjectAccess& soa, mirror::Object* receiver, va_list ap)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Set receiver if non-null (method is not static)
    if (receiver != NULL) {
      Append(reinterpret_cast<int32_t>(receiver));
    }
    for (size_t i = 1; i < shorty_len_; ++i) {
      switch (shorty_[i]) {
        case 'Z':
        case 'B':
        case 'C':
        case 'S':
        case 'I':
          Append(va_arg(ap, jint));
          break;
        case 'F': {
          JValue value;
          value.SetF(va_arg(ap, jdouble));
          Append(value.GetI());
          break;
        }
        case 'L':
          Append(reinterpret_cast<int32_t>(soa.Decode<mirror::Object*>(va_arg(ap, jobject))));
          break;
        case 'D': {
          JValue value;
          value.SetD(va_arg(ap, jdouble));
          AppendWide(value.GetJ());
          break;
        }
        case 'J': {
          AppendWide(va_arg(ap, jlong));
          break;
        }
      }
    }
  }

  void BuildArgArray(const ScopedObjectAccess& soa, mirror::Object* receiver, jvalue* args)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Set receiver if non-null (method is not static)
    if (receiver != NULL) {
      Append(reinterpret_cast<int32_t>(receiver));
    }
    for (size_t i = 1, args_offset = 0; i < shorty_len_; ++i, ++args_offset) {
      switch (shorty_[i]) {
        case 'Z':
          Append(args[args_offset].z);
          break;
        case 'B':
          Append(args[args_offset].b);
          break;
        case 'C':
          Append(args[args_offset].c);
          break;
        case 'S':
          Append(args[args_offset].s);
          break;
        case 'I':
        case 'F':
          Append(args[args_offset].i);
          break;
        case 'L':
          Append(reinterpret_cast<int32_t>(soa.Decode<mirror::Object*>(args[args_offset].l)));
          break;
        case 'D':
        case 'J':
          AppendWide(args[args_offset].j);
          break;
      }
    }
  }


  void BuildArgArrayFromFrame(ShadowFrame* shadow_frame, uint32_t arg_offset)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Set receiver if non-null (method is not static)
    size_t cur_arg = arg_offset;
    if (!shadow_frame->GetMethod()->IsStatic()) {
      Append(shadow_frame->GetVReg(cur_arg));
      cur_arg++;
    }
    for (size_t i = 1; i < shorty_len_; ++i) {
      switch (shorty_[i]) {
        case 'Z':
        case 'B':
        case 'C':
        case 'S':
        case 'I':
        case 'F':
        case 'L':
          Append(shadow_frame->GetVReg(cur_arg));
          cur_arg++;
          break;
        case 'D':
        case 'J':
          AppendWide(shadow_frame->GetVRegLong(cur_arg));
          cur_arg++;
          cur_arg++;
          break;
      }
    }
  }

 private:
  enum { kSmallArgArraySize = 16 };
  const char* const shorty_;
  const uint32_t shorty_len_;
  uint32_t num_bytes_;
  uint32_t* arg_array_;
  uint32_t small_arg_array_[kSmallArgArraySize];
  UniquePtr<uint32_t[]> large_arg_array_;
};

}  // namespace art

#endif  // ART_RUNTIME_INVOKE_ARG_ARRAY_BUILDER_H_
