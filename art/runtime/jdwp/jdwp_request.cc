/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "jdwp/jdwp.h"

#include "base/stringprintf.h"
#include "jdwp/jdwp_priv.h"

namespace art {

namespace JDWP {

Request::Request(const uint8_t* bytes, uint32_t available) : p_(bytes) {
  byte_count_ = Read4BE();
  end_ =  bytes + byte_count_;
  CHECK_LE(byte_count_, available);

  id_ = Read4BE();
  int8_t flags = Read1();
  if ((flags & kJDWPFlagReply) != 0) {
    LOG(FATAL) << "reply?!";
  }

  command_set_ = Read1();
  command_ = Read1();
}

Request::~Request() {
}

void Request::CheckConsumed() {
  if (p_ < end_) {
    CHECK(p_ == end_) << "read too few bytes: " << (end_ - p_);
  } else if (p_ > end_) {
    CHECK(p_ == end_) << "read too many bytes: " << (p_ - end_);
  }
}

std::string Request::ReadUtf8String() {
  uint32_t length = Read4BE();
  std::string s;
  s.resize(length);
  memcpy(&s[0], p_, length);
  p_ += length;
  VLOG(jdwp) << "    string \"" << s << "\"";
  return s;
}

// Helper function: read a variable-width value from the input buffer.
uint64_t Request::ReadValue(size_t width) {
  uint64_t value = -1;
  switch (width) {
    case 1: value = Read1(); break;
    case 2: value = Read2BE(); break;
    case 4: value = Read4BE(); break;
    case 8: value = Read8BE(); break;
    default: LOG(FATAL) << width; break;
  }
  return value;
}

int32_t Request::ReadSigned32(const char* what) {
  int32_t value = static_cast<int32_t>(Read4BE());
  VLOG(jdwp) << "    " << what << " " << value;
  return value;
}

uint32_t Request::ReadUnsigned32(const char* what) {
  uint32_t value = Read4BE();
  VLOG(jdwp) << "    " << what << " " << value;
  return value;
}

FieldId Request::ReadFieldId() {
  FieldId id = Read4BE();
  VLOG(jdwp) << "    field id " << DescribeField(id);
  return id;
}

MethodId Request::ReadMethodId() {
  MethodId id = Read4BE();
  VLOG(jdwp) << "    method id " << DescribeMethod(id);
  return id;
}

ObjectId Request::ReadObjectId(const char* specific_kind) {
  ObjectId id = Read8BE();
  VLOG(jdwp) << StringPrintf("    %s id %#llx", specific_kind, id);
  return id;
}

ObjectId Request::ReadArrayId() {
  return ReadObjectId("array");
}

ObjectId Request::ReadObjectId() {
  return ReadObjectId("object");
}

ObjectId Request::ReadThreadId() {
  return ReadObjectId("thread");
}

ObjectId Request::ReadThreadGroupId() {
  return ReadObjectId("thread group");
}

RefTypeId Request::ReadRefTypeId() {
  RefTypeId id = Read8BE();
  VLOG(jdwp) << "    ref type id " << DescribeRefTypeId(id);
  return id;
}

FrameId Request::ReadFrameId() {
  FrameId id = Read8BE();
  VLOG(jdwp) << "    frame id " << id;
  return id;
}

JdwpTag Request::ReadTag() {
  return ReadEnum1<JdwpTag>("tag");
}

JdwpTypeTag Request::ReadTypeTag() {
  return ReadEnum1<JdwpTypeTag>("type tag");
}

JdwpLocation Request::ReadLocation() {
  JdwpLocation location;
  memset(&location, 0, sizeof(location));  // Allows memcmp(3) later.
  location.type_tag = ReadTypeTag();
  location.class_id = ReadObjectId("class");
  location.method_id = ReadMethodId();
  location.dex_pc = Read8BE();
  VLOG(jdwp) << "    location " << location;
  return location;
}

JdwpModKind Request::ReadModKind() {
  return ReadEnum1<JdwpModKind>("mod kind");
}

uint8_t Request::Read1() {
  return *p_++;
}

uint16_t Request::Read2BE() {
  uint16_t result = p_[0] << 8 | p_[1];
  p_ += 2;
  return result;
}

uint32_t Request::Read4BE() {
  uint32_t result = p_[0] << 24;
  result |= p_[1] << 16;
  result |= p_[2] << 8;
  result |= p_[3];
  p_ += 4;
  return result;
}

uint64_t Request::Read8BE() {
  uint64_t high = Read4BE();
  uint64_t low = Read4BE();
  return (high << 32) | low;
}

}  // namespace JDWP

}  // namespace art
