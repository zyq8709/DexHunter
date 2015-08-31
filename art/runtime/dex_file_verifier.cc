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

#include "dex_file_verifier.h"

#include "base/stringprintf.h"
#include "dex_file-inl.h"
#include "leb128.h"
#include "safe_map.h"
#include "UniquePtr.h"
#include "utf.h"
#include "utils.h"
#include "zip_archive.h"

namespace art {

static uint32_t MapTypeToBitMask(uint32_t map_type) {
  switch (map_type) {
    case DexFile::kDexTypeHeaderItem:               return 1 << 0;
    case DexFile::kDexTypeStringIdItem:             return 1 << 1;
    case DexFile::kDexTypeTypeIdItem:               return 1 << 2;
    case DexFile::kDexTypeProtoIdItem:              return 1 << 3;
    case DexFile::kDexTypeFieldIdItem:              return 1 << 4;
    case DexFile::kDexTypeMethodIdItem:             return 1 << 5;
    case DexFile::kDexTypeClassDefItem:             return 1 << 6;
    case DexFile::kDexTypeMapList:                  return 1 << 7;
    case DexFile::kDexTypeTypeList:                 return 1 << 8;
    case DexFile::kDexTypeAnnotationSetRefList:     return 1 << 9;
    case DexFile::kDexTypeAnnotationSetItem:        return 1 << 10;
    case DexFile::kDexTypeClassDataItem:            return 1 << 11;
    case DexFile::kDexTypeCodeItem:                 return 1 << 12;
    case DexFile::kDexTypeStringDataItem:           return 1 << 13;
    case DexFile::kDexTypeDebugInfoItem:            return 1 << 14;
    case DexFile::kDexTypeAnnotationItem:           return 1 << 15;
    case DexFile::kDexTypeEncodedArrayItem:         return 1 << 16;
    case DexFile::kDexTypeAnnotationsDirectoryItem: return 1 << 17;
  }
  return 0;
}

static bool IsDataSectionType(uint32_t map_type) {
  switch (map_type) {
    case DexFile::kDexTypeHeaderItem:
    case DexFile::kDexTypeStringIdItem:
    case DexFile::kDexTypeTypeIdItem:
    case DexFile::kDexTypeProtoIdItem:
    case DexFile::kDexTypeFieldIdItem:
    case DexFile::kDexTypeMethodIdItem:
    case DexFile::kDexTypeClassDefItem:
      return false;
  }
  return true;
}

static bool CheckShortyDescriptorMatch(char shorty_char, const char* descriptor,
    bool is_return_type) {
  switch (shorty_char) {
    case 'V':
      if (!is_return_type) {
        LOG(ERROR) << "Invalid use of void";
        return false;
      }
      // Intentional fallthrough.
    case 'B':
    case 'C':
    case 'D':
    case 'F':
    case 'I':
    case 'J':
    case 'S':
    case 'Z':
      if ((descriptor[0] != shorty_char) || (descriptor[1] != '\0')) {
        LOG(ERROR) << StringPrintf("Shorty vs. primitive type mismatch: '%c', '%s'", shorty_char, descriptor);
        return false;
      }
      break;
    case 'L':
      if ((descriptor[0] != 'L') && (descriptor[0] != '[')) {
        LOG(ERROR) << StringPrintf("Shorty vs. type mismatch: '%c', '%s'", shorty_char, descriptor);
        return false;
      }
      break;
    default:
      LOG(ERROR) << "Bad shorty character: '" << shorty_char << "'";
      return false;
  }
  return true;
}

bool DexFileVerifier::Verify(const DexFile* dex_file, const byte* begin, size_t size) {
  UniquePtr<DexFileVerifier> verifier(new DexFileVerifier(dex_file, begin, size));
  return verifier->Verify();
}

bool DexFileVerifier::CheckPointerRange(const void* start, const void* end, const char* label) const {
  uint32_t range_start = reinterpret_cast<uint32_t>(start);
  uint32_t range_end = reinterpret_cast<uint32_t>(end);
  uint32_t file_start = reinterpret_cast<uint32_t>(begin_);
  uint32_t file_end = file_start + size_;
  if ((range_start < file_start) || (range_start > file_end) ||
      (range_end < file_start) || (range_end > file_end)) {
    LOG(ERROR) << StringPrintf("Bad range for %s: %x to %x", label,
        range_start - file_start, range_end - file_start);
    return false;
  }
  return true;
}

bool DexFileVerifier::CheckListSize(const void* start, uint32_t count,
    uint32_t element_size, const char* label) const {
  const byte* list_start = reinterpret_cast<const byte*>(start);
  return CheckPointerRange(list_start, list_start + (count * element_size), label);
}

bool DexFileVerifier::CheckIndex(uint32_t field, uint32_t limit, const char* label) const {
  if (field >= limit) {
    LOG(ERROR) << StringPrintf("Bad index for %s: %x >= %x", label, field, limit);
    return false;
  }
  return true;
}

bool DexFileVerifier::CheckHeader() const {
  // Check file size from the header.
  uint32_t expected_size = header_->file_size_;
  if (size_ != expected_size) {
    LOG(ERROR) << "Bad file size (" << size_ << ", expected " << expected_size << ")";
    return false;
  }

  // Compute and verify the checksum in the header.
  uint32_t adler_checksum = adler32(0L, Z_NULL, 0);
  const uint32_t non_sum = sizeof(header_->magic_) + sizeof(header_->checksum_);
  const byte* non_sum_ptr = reinterpret_cast<const byte*>(header_) + non_sum;
  adler_checksum = adler32(adler_checksum, non_sum_ptr, expected_size - non_sum);
  if (adler_checksum != header_->checksum_) {
    LOG(ERROR) << StringPrintf("Bad checksum (%08x, expected %08x)", adler_checksum, header_->checksum_);
    return false;
  }

  // Check the contents of the header.
  if (header_->endian_tag_ != DexFile::kDexEndianConstant) {
    LOG(ERROR) << StringPrintf("Unexpected endian_tag: %x", header_->endian_tag_);
    return false;
  }

  if (header_->header_size_ != sizeof(DexFile::Header)) {
    LOG(ERROR) << "Bad header size: " << header_->header_size_;
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckMap() const {
  const DexFile::MapList* map = reinterpret_cast<const DexFile::MapList*>(begin_ + header_->map_off_);
  const DexFile::MapItem* item = map->list_;

  uint32_t count = map->size_;
  uint32_t last_offset = 0;
  uint32_t data_item_count = 0;
  uint32_t data_items_left = header_->data_size_;
  uint32_t used_bits = 0;

  // Sanity check the size of the map list.
  if (!CheckListSize(item, count, sizeof(DexFile::MapItem), "map size")) {
    return false;
  }

  // Check the items listed in the map.
  for (uint32_t i = 0; i < count; i++) {
    if (last_offset >= item->offset_ && i != 0) {
      LOG(ERROR) << StringPrintf("Out of order map item: %x then %x", last_offset, item->offset_);
      return false;
    }
    if (item->offset_ >= header_->file_size_) {
      LOG(ERROR) << StringPrintf("Map item after end of file: %x, size %x", item->offset_, header_->file_size_);
      return false;
    }

    if (IsDataSectionType(item->type_)) {
      uint32_t icount = item->size_;
      if (icount > data_items_left) {
        LOG(ERROR) << "Too many items in data section: " << data_item_count + icount;
        return false;
      }
      data_items_left -= icount;
      data_item_count += icount;
    }

    uint32_t bit = MapTypeToBitMask(item->type_);

    if (bit == 0) {
      LOG(ERROR) << StringPrintf("Unknown map section type %x", item->type_);
      return false;
    }

    if ((used_bits & bit) != 0) {
      LOG(ERROR) << StringPrintf("Duplicate map section of type %x", item->type_);
      return false;
    }

    used_bits |= bit;
    last_offset = item->offset_;
    item++;
  }

  // Check for missing sections in the map.
  if ((used_bits & MapTypeToBitMask(DexFile::kDexTypeHeaderItem)) == 0) {
    LOG(ERROR) << "Map is missing header entry";
    return false;
  }
  if ((used_bits & MapTypeToBitMask(DexFile::kDexTypeMapList)) == 0) {
    LOG(ERROR) << "Map is missing map_list entry";
    return false;
  }
  if ((used_bits & MapTypeToBitMask(DexFile::kDexTypeStringIdItem)) == 0 &&
      ((header_->string_ids_off_ != 0) || (header_->string_ids_size_ != 0))) {
    LOG(ERROR) << "Map is missing string_ids entry";
    return false;
  }
  if ((used_bits & MapTypeToBitMask(DexFile::kDexTypeTypeIdItem)) == 0 &&
      ((header_->type_ids_off_ != 0) || (header_->type_ids_size_ != 0))) {
    LOG(ERROR) << "Map is missing type_ids entry";
    return false;
  }
  if ((used_bits & MapTypeToBitMask(DexFile::kDexTypeProtoIdItem)) == 0 &&
      ((header_->proto_ids_off_ != 0) || (header_->proto_ids_size_ != 0))) {
    LOG(ERROR) << "Map is missing proto_ids entry";
    return false;
  }
  if ((used_bits & MapTypeToBitMask(DexFile::kDexTypeFieldIdItem)) == 0 &&
      ((header_->field_ids_off_ != 0) || (header_->field_ids_size_ != 0))) {
    LOG(ERROR) << "Map is missing field_ids entry";
    return false;
  }
  if ((used_bits & MapTypeToBitMask(DexFile::kDexTypeMethodIdItem)) == 0 &&
      ((header_->method_ids_off_ != 0) || (header_->method_ids_size_ != 0))) {
    LOG(ERROR) << "Map is missing method_ids entry";
    return false;
  }
  if ((used_bits & MapTypeToBitMask(DexFile::kDexTypeClassDefItem)) == 0 &&
      ((header_->class_defs_off_ != 0) || (header_->class_defs_size_ != 0))) {
    LOG(ERROR) << "Map is missing class_defs entry";
    return false;
  }

  return true;
}

uint32_t DexFileVerifier::ReadUnsignedLittleEndian(uint32_t size) {
  uint32_t result = 0;
  if (!CheckPointerRange(ptr_, ptr_ + size, "encoded_value")) {
    return 0;
  }

  for (uint32_t i = 0; i < size; i++) {
    result |= ((uint32_t) *(ptr_++)) << (i * 8);
  }

  return result;
}

bool DexFileVerifier::CheckAndGetHandlerOffsets(const DexFile::CodeItem* code_item,
    uint32_t* handler_offsets, uint32_t handlers_size) {
  const byte* handlers_base = DexFile::GetCatchHandlerData(*code_item, 0);

  for (uint32_t i = 0; i < handlers_size; i++) {
    bool catch_all;
    uint32_t offset = reinterpret_cast<uint32_t>(ptr_) - reinterpret_cast<uint32_t>(handlers_base);
    int32_t size = DecodeSignedLeb128(&ptr_);

    if ((size < -65536) || (size > 65536)) {
      LOG(ERROR) << "Invalid exception handler size: " << size;
      return false;
    }

    if (size <= 0) {
      catch_all = true;
      size = -size;
    } else {
      catch_all = false;
    }

    handler_offsets[i] = offset;

    while (size-- > 0) {
      uint32_t type_idx = DecodeUnsignedLeb128(&ptr_);
      if (!CheckIndex(type_idx, header_->type_ids_size_, "handler type_idx")) {
        return false;
      }

      uint32_t addr = DecodeUnsignedLeb128(&ptr_);
      if (addr >= code_item->insns_size_in_code_units_) {
        LOG(ERROR) << StringPrintf("Invalid handler addr: %x", addr);
        return false;
      }
    }

    if (catch_all) {
      uint32_t addr = DecodeUnsignedLeb128(&ptr_);
      if (addr >= code_item->insns_size_in_code_units_) {
        LOG(ERROR) << StringPrintf("Invalid handler catch_all_addr: %x", addr);
        return false;
      }
    }
  }

  return true;
}

bool DexFileVerifier::CheckClassDataItemField(uint32_t idx, uint32_t access_flags,
    bool expect_static) const {
  if (!CheckIndex(idx, header_->field_ids_size_, "class_data_item field_idx")) {
    return false;
  }

  bool is_static = (access_flags & kAccStatic) != 0;
  if (is_static != expect_static) {
    LOG(ERROR) << "Static/instance field not in expected list";
    return false;
  }

  uint32_t access_field_mask = kAccPublic | kAccPrivate | kAccProtected | kAccStatic |
      kAccFinal | kAccVolatile | kAccTransient | kAccSynthetic | kAccEnum;
  if ((access_flags & ~access_field_mask) != 0) {
    LOG(ERROR) << StringPrintf("Bad class_data_item field access_flags %x", access_flags);
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckClassDataItemMethod(uint32_t idx, uint32_t access_flags,
    uint32_t code_offset, bool expect_direct) const {
  if (!CheckIndex(idx, header_->method_ids_size_, "class_data_item method_idx")) {
    return false;
  }

  bool is_direct = (access_flags & (kAccStatic | kAccPrivate | kAccConstructor)) != 0;
  bool expect_code = (access_flags & (kAccNative | kAccAbstract)) == 0;
  bool is_synchronized = (access_flags & kAccSynchronized) != 0;
  bool allow_synchronized = (access_flags & kAccNative) != 0;

  if (is_direct != expect_direct) {
    LOG(ERROR) << "Direct/virtual method not in expected list";
    return false;
  }

  uint32_t access_method_mask = kAccPublic | kAccPrivate | kAccProtected | kAccStatic |
      kAccFinal | kAccSynchronized | kAccBridge | kAccVarargs | kAccNative | kAccAbstract |
      kAccStrict | kAccSynthetic | kAccConstructor | kAccDeclaredSynchronized;
  if (((access_flags & ~access_method_mask) != 0) || (is_synchronized && !allow_synchronized)) {
    LOG(ERROR) << StringPrintf("Bad class_data_item method access_flags %x", access_flags);
    return false;
  }

  if (expect_code && code_offset == 0) {
    LOG(ERROR)<< StringPrintf("Unexpected zero value for class_data_item method code_off"
        " with access flags %x", access_flags);
    return false;
  } else if (!expect_code && code_offset != 0) {
    LOG(ERROR) << StringPrintf("Unexpected non-zero value %x for class_data_item method code_off"
        " with access flags %x", code_offset, access_flags);
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckPadding(uint32_t offset, uint32_t aligned_offset) {
  if (offset < aligned_offset) {
    if (!CheckPointerRange(begin_ + offset, begin_ + aligned_offset, "section")) {
      return false;
    }
    while (offset < aligned_offset) {
      if (*ptr_ != '\0') {
        LOG(ERROR) << StringPrintf("Non-zero padding %x before section start at %x", *ptr_, offset);
        return false;
      }
      ptr_++;
      offset++;
    }
  }
  return true;
}

bool DexFileVerifier::CheckEncodedValue() {
  if (!CheckPointerRange(ptr_, ptr_ + 1, "encoded_value header")) {
    return false;
  }

  uint8_t header_byte = *(ptr_++);
  uint32_t value_type = header_byte & DexFile::kDexAnnotationValueTypeMask;
  uint32_t value_arg = header_byte >> DexFile::kDexAnnotationValueArgShift;

  switch (value_type) {
    case DexFile::kDexAnnotationByte:
      if (value_arg != 0) {
        LOG(ERROR) << StringPrintf("Bad encoded_value byte size %x", value_arg);
        return false;
      }
      ptr_++;
      break;
    case DexFile::kDexAnnotationShort:
    case DexFile::kDexAnnotationChar:
      if (value_arg > 1) {
        LOG(ERROR) << StringPrintf("Bad encoded_value char/short size %x", value_arg);
        return false;
      }
      ptr_ += value_arg + 1;
      break;
    case DexFile::kDexAnnotationInt:
    case DexFile::kDexAnnotationFloat:
      if (value_arg > 3) {
        LOG(ERROR) << StringPrintf("Bad encoded_value int/float size %x", value_arg);
        return false;
      }
      ptr_ += value_arg + 1;
      break;
    case DexFile::kDexAnnotationLong:
    case DexFile::kDexAnnotationDouble:
      ptr_ += value_arg + 1;
      break;
    case DexFile::kDexAnnotationString: {
      if (value_arg > 3) {
        LOG(ERROR) << StringPrintf("Bad encoded_value string size %x", value_arg);
        return false;
      }
      uint32_t idx = ReadUnsignedLittleEndian(value_arg + 1);
      if (!CheckIndex(idx, header_->string_ids_size_, "encoded_value string")) {
        return false;
      }
      break;
    }
    case DexFile::kDexAnnotationType: {
      if (value_arg > 3) {
        LOG(ERROR) << StringPrintf("Bad encoded_value type size %x", value_arg);
        return false;
      }
      uint32_t idx = ReadUnsignedLittleEndian(value_arg + 1);
      if (!CheckIndex(idx, header_->type_ids_size_, "encoded_value type")) {
        return false;
      }
      break;
    }
    case DexFile::kDexAnnotationField:
    case DexFile::kDexAnnotationEnum: {
      if (value_arg > 3) {
        LOG(ERROR) << StringPrintf("Bad encoded_value field/enum size %x", value_arg);
        return false;
      }
      uint32_t idx = ReadUnsignedLittleEndian(value_arg + 1);
      if (!CheckIndex(idx, header_->field_ids_size_, "encoded_value field")) {
        return false;
      }
      break;
    }
    case DexFile::kDexAnnotationMethod: {
      if (value_arg > 3) {
        LOG(ERROR) << StringPrintf("Bad encoded_value method size %x", value_arg);
        return false;
      }
      uint32_t idx = ReadUnsignedLittleEndian(value_arg + 1);
      if (!CheckIndex(idx, header_->method_ids_size_, "encoded_value method")) {
        return false;
      }
      break;
    }
    case DexFile::kDexAnnotationArray:
      if (value_arg != 0) {
        LOG(ERROR) << StringPrintf("Bad encoded_value array value_arg %x", value_arg);
        return false;
      }
      if (!CheckEncodedArray()) {
        return false;
      }
      break;
    case DexFile::kDexAnnotationAnnotation:
      if (value_arg != 0) {
        LOG(ERROR) << StringPrintf("Bad encoded_value annotation value_arg %x", value_arg);
        return false;
      }
      if (!CheckEncodedAnnotation()) {
        return false;
      }
      break;
    case DexFile::kDexAnnotationNull:
      if (value_arg != 0) {
        LOG(ERROR) << StringPrintf("Bad encoded_value null value_arg %x", value_arg);
        return false;
      }
      break;
    case DexFile::kDexAnnotationBoolean:
      if (value_arg > 1) {
        LOG(ERROR) << StringPrintf("Bad encoded_value boolean size %x", value_arg);
        return false;
      }
      break;
    default:
      LOG(ERROR) << StringPrintf("Bogus encoded_value value_type %x", value_type);
      return false;
  }

  return true;
}

bool DexFileVerifier::CheckEncodedArray() {
  uint32_t size = DecodeUnsignedLeb128(&ptr_);

  while (size--) {
    if (!CheckEncodedValue()) {
      LOG(ERROR) << "Bad encoded_array value";
      return false;
    }
  }
  return true;
}

bool DexFileVerifier::CheckEncodedAnnotation() {
  uint32_t idx = DecodeUnsignedLeb128(&ptr_);
  if (!CheckIndex(idx, header_->type_ids_size_, "encoded_annotation type_idx")) {
    return false;
  }

  uint32_t size = DecodeUnsignedLeb128(&ptr_);
  uint32_t last_idx = 0;

  for (uint32_t i = 0; i < size; i++) {
    idx = DecodeUnsignedLeb128(&ptr_);
    if (!CheckIndex(idx, header_->string_ids_size_, "annotation_element name_idx")) {
      return false;
    }

    if (last_idx >= idx && i != 0) {
      LOG(ERROR) << StringPrintf("Out-of-order annotation_element name_idx: %x then %x",
          last_idx, idx);
      return false;
    }

    if (!CheckEncodedValue()) {
      return false;
    }

    last_idx = idx;
  }
  return true;
}

bool DexFileVerifier::CheckIntraClassDataItem() {
  ClassDataItemIterator it(*dex_file_, ptr_);

  for (; it.HasNextStaticField(); it.Next()) {
    if (!CheckClassDataItemField(it.GetMemberIndex(), it.GetMemberAccessFlags(), true)) {
      return false;
    }
  }
  for (; it.HasNextInstanceField(); it.Next()) {
    if (!CheckClassDataItemField(it.GetMemberIndex(), it.GetMemberAccessFlags(), false)) {
      return false;
    }
  }
  for (; it.HasNextDirectMethod(); it.Next()) {
    if (!CheckClassDataItemMethod(it.GetMemberIndex(), it.GetMemberAccessFlags(),
        it.GetMethodCodeItemOffset(), true)) {
      return false;
    }
  }
  for (; it.HasNextVirtualMethod(); it.Next()) {
    if (!CheckClassDataItemMethod(it.GetMemberIndex(), it.GetMemberAccessFlags(),
        it.GetMethodCodeItemOffset(), false)) {
      return false;
    }
  }

  ptr_ = it.EndDataPointer();
  return true;
}

bool DexFileVerifier::CheckIntraCodeItem() {
  const DexFile::CodeItem* code_item = reinterpret_cast<const DexFile::CodeItem*>(ptr_);
  if (!CheckPointerRange(code_item, code_item + 1, "code")) {
    return false;
  }

  if (code_item->ins_size_ > code_item->registers_size_) {
    LOG(ERROR) << "ins_size (" << code_item->ins_size_ << ") > registers_size ("
               << code_item->registers_size_ << ")";
    return false;
  }

  if ((code_item->outs_size_ > 5) && (code_item->outs_size_ > code_item->registers_size_)) {
    /*
     * outs_size can be up to 5, even if registers_size is smaller, since the
     * short forms of method invocation allow repetitions of a register multiple
     * times within a single parameter list. However, longer parameter lists
     * need to be represented in-order in the register file.
     */
    LOG(ERROR) << "outs_size (" << code_item->outs_size_ << ") > registers_size ("
               << code_item->registers_size_ << ")";
    return false;
  }

  const uint16_t* insns = code_item->insns_;
  uint32_t insns_size = code_item->insns_size_in_code_units_;
  if (!CheckListSize(insns, insns_size, sizeof(uint16_t), "insns size")) {
    return false;
  }

  // Grab the end of the insns if there are no try_items.
  uint32_t try_items_size = code_item->tries_size_;
  if (try_items_size == 0) {
    ptr_ = reinterpret_cast<const byte*>(&insns[insns_size]);
    return true;
  }

  // try_items are 4-byte aligned. Verify the spacer is 0.
  if ((((uint32_t) &insns[insns_size] & 3) != 0) && (insns[insns_size] != 0)) {
    LOG(ERROR) << StringPrintf("Non-zero padding: %x", insns[insns_size]);
    return false;
  }

  const DexFile::TryItem* try_items = DexFile::GetTryItems(*code_item, 0);
  ptr_ = DexFile::GetCatchHandlerData(*code_item, 0);
  uint32_t handlers_size = DecodeUnsignedLeb128(&ptr_);

  if (!CheckListSize(try_items, try_items_size, sizeof(DexFile::TryItem), "try_items size")) {
    return false;
  }

  if ((handlers_size == 0) || (handlers_size >= 65536)) {
    LOG(ERROR) << "Invalid handlers_size: " << handlers_size;
    return false;
  }

  UniquePtr<uint32_t[]> handler_offsets(new uint32_t[handlers_size]);
  if (!CheckAndGetHandlerOffsets(code_item, &handler_offsets[0], handlers_size)) {
    return false;
  }

  uint32_t last_addr = 0;
  while (try_items_size--) {
    if (try_items->start_addr_ < last_addr) {
      LOG(ERROR) << StringPrintf("Out-of_order try_item with start_addr: %x",
          try_items->start_addr_);
      return false;
    }

    if (try_items->start_addr_ >= insns_size) {
      LOG(ERROR) << StringPrintf("Invalid try_item start_addr: %x", try_items->start_addr_);
      return false;
    }

    uint32_t i;
    for (i = 0; i < handlers_size; i++) {
      if (try_items->handler_off_ == handler_offsets[i]) {
        break;
      }
    }

    if (i == handlers_size) {
      LOG(ERROR) << StringPrintf("Bogus handler offset: %x", try_items->handler_off_);
      return false;
    }

    last_addr = try_items->start_addr_ + try_items->insn_count_;
    if (last_addr > insns_size) {
      LOG(ERROR) << StringPrintf("Invalid try_item insn_count: %x", try_items->insn_count_);
      return false;
    }

    try_items++;
  }

  return true;
}

bool DexFileVerifier::CheckIntraStringDataItem() {
  uint32_t size = DecodeUnsignedLeb128(&ptr_);
  const byte* file_end = begin_ + size_;

  for (uint32_t i = 0; i < size; i++) {
    if (ptr_ >= file_end) {
      LOG(ERROR) << "String data would go beyond end-of-file";
      return false;
    }

    uint8_t byte = *(ptr_++);

    // Switch on the high 4 bits.
    switch (byte >> 4) {
      case 0x00:
        // Special case of bit pattern 0xxx.
        if (byte == 0) {
          LOG(ERROR) << StringPrintf("String data shorter than indicated utf16_size %x", size);
          return false;
        }
        break;
      case 0x01:
      case 0x02:
      case 0x03:
      case 0x04:
      case 0x05:
      case 0x06:
      case 0x07:
        // No extra checks necessary for bit pattern 0xxx.
        break;
      case 0x08:
      case 0x09:
      case 0x0a:
      case 0x0b:
      case 0x0f:
        // Illegal bit patterns 10xx or 1111.
        // Note: 1111 is valid for normal UTF-8, but not here.
        LOG(ERROR) << StringPrintf("Illegal start byte %x in string data", byte);
        return false;
      case 0x0c:
      case 0x0d: {
        // Bit pattern 110x has an additional byte.
        uint8_t byte2 = *(ptr_++);
        if ((byte2 & 0xc0) != 0x80) {
          LOG(ERROR) << StringPrintf("Illegal continuation byte %x in string data", byte2);
          return false;
        }
        uint16_t value = ((byte & 0x1f) << 6) | (byte2 & 0x3f);
        if ((value != 0) && (value < 0x80)) {
          LOG(ERROR) << StringPrintf("Illegal representation for value %x in string data", value);
          return false;
        }
        break;
      }
      case 0x0e: {
        // Bit pattern 1110 has 2 additional bytes.
        uint8_t byte2 = *(ptr_++);
        if ((byte2 & 0xc0) != 0x80) {
          LOG(ERROR) << StringPrintf("Illegal continuation byte %x in string data", byte2);
          return false;
        }
        uint8_t byte3 = *(ptr_++);
        if ((byte3 & 0xc0) != 0x80) {
          LOG(ERROR) << StringPrintf("Illegal continuation byte %x in string data", byte3);
          return false;
        }
        uint16_t value = ((byte & 0x0f) << 12) | ((byte2 & 0x3f) << 6) | (byte3 & 0x3f);
        if (value < 0x800) {
          LOG(ERROR) << StringPrintf("Illegal representation for value %x in string data", value);
          return false;
        }
        break;
      }
    }
  }

  if (*(ptr_++) != '\0') {
    LOG(ERROR) << StringPrintf("String longer than indicated size %x", size);
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckIntraDebugInfoItem() {
  DecodeUnsignedLeb128(&ptr_);
  uint32_t parameters_size = DecodeUnsignedLeb128(&ptr_);
  if (parameters_size > 65536) {
    LOG(ERROR) << StringPrintf("Invalid parameters_size: %x", parameters_size);
    return false;
  }

  for (uint32_t j = 0; j < parameters_size; j++) {
    uint32_t parameter_name = DecodeUnsignedLeb128(&ptr_);
    if (parameter_name != 0) {
      parameter_name--;
      if (!CheckIndex(parameter_name, header_->string_ids_size_, "debug_info_item parameter_name")) {
        return false;
      }
    }
  }

  while (true) {
    uint8_t opcode = *(ptr_++);
    switch (opcode) {
      case DexFile::DBG_END_SEQUENCE: {
        return true;
      }
      case DexFile::DBG_ADVANCE_PC: {
        DecodeUnsignedLeb128(&ptr_);
        break;
      }
      case DexFile::DBG_ADVANCE_LINE: {
        DecodeSignedLeb128(&ptr_);
        break;
      }
      case DexFile::DBG_START_LOCAL: {
        uint32_t reg_num = DecodeUnsignedLeb128(&ptr_);
        if (reg_num >= 65536) {
          LOG(ERROR) << StringPrintf("Bad reg_num for opcode %x", opcode);
          return false;
        }
        uint32_t name_idx = DecodeUnsignedLeb128(&ptr_);
        if (name_idx != 0) {
          name_idx--;
          if (!CheckIndex(name_idx, header_->string_ids_size_, "DBG_START_LOCAL name_idx")) {
            return false;
          }
        }
        uint32_t type_idx = DecodeUnsignedLeb128(&ptr_);
        if (type_idx != 0) {
          type_idx--;
          if (!CheckIndex(type_idx, header_->string_ids_size_, "DBG_START_LOCAL type_idx")) {
            return false;
          }
        }
        break;
      }
      case DexFile::DBG_END_LOCAL:
      case DexFile::DBG_RESTART_LOCAL: {
        uint32_t reg_num = DecodeUnsignedLeb128(&ptr_);
        if (reg_num >= 65536) {
          LOG(ERROR) << StringPrintf("Bad reg_num for opcode %x", opcode);
          return false;
        }
        break;
      }
      case DexFile::DBG_START_LOCAL_EXTENDED: {
        uint32_t reg_num = DecodeUnsignedLeb128(&ptr_);
        if (reg_num >= 65536) {
          LOG(ERROR) << StringPrintf("Bad reg_num for opcode %x", opcode);
          return false;
        }
        uint32_t name_idx = DecodeUnsignedLeb128(&ptr_);
        if (name_idx != 0) {
          name_idx--;
          if (!CheckIndex(name_idx, header_->string_ids_size_, "DBG_START_LOCAL_EXTENDED name_idx")) {
            return false;
          }
        }
        uint32_t type_idx = DecodeUnsignedLeb128(&ptr_);
        if (type_idx != 0) {
          type_idx--;
          if (!CheckIndex(type_idx, header_->string_ids_size_, "DBG_START_LOCAL_EXTENDED type_idx")) {
            return false;
          }
        }
        uint32_t sig_idx = DecodeUnsignedLeb128(&ptr_);
        if (sig_idx != 0) {
          sig_idx--;
          if (!CheckIndex(sig_idx, header_->string_ids_size_, "DBG_START_LOCAL_EXTENDED sig_idx")) {
            return false;
          }
        }
        break;
      }
      case DexFile::DBG_SET_FILE: {
        uint32_t name_idx = DecodeUnsignedLeb128(&ptr_);
        if (name_idx != 0) {
          name_idx--;
          if (!CheckIndex(name_idx, header_->string_ids_size_, "DBG_SET_FILE name_idx")) {
            return false;
          }
        }
        break;
      }
    }
  }
}

bool DexFileVerifier::CheckIntraAnnotationItem() {
  if (!CheckPointerRange(ptr_, ptr_ + 1, "annotation visibility")) {
    return false;
  }

  // Check visibility
  switch (*(ptr_++)) {
    case DexFile::kDexVisibilityBuild:
    case DexFile::kDexVisibilityRuntime:
    case DexFile::kDexVisibilitySystem:
      break;
    default:
      LOG(ERROR) << StringPrintf("Bad annotation visibility: %x", *ptr_);
      return false;
  }

  if (!CheckEncodedAnnotation()) {
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckIntraAnnotationsDirectoryItem() {
  const DexFile::AnnotationsDirectoryItem* item =
      reinterpret_cast<const DexFile::AnnotationsDirectoryItem*>(ptr_);
  if (!CheckPointerRange(item, item + 1, "annotations_directory")) {
    return false;
  }

  // Field annotations follow immediately after the annotations directory.
  const DexFile::FieldAnnotationsItem* field_item =
      reinterpret_cast<const DexFile::FieldAnnotationsItem*>(item + 1);
  uint32_t field_count = item->fields_size_;
  if (!CheckListSize(field_item, field_count, sizeof(DexFile::FieldAnnotationsItem), "field_annotations list")) {
    return false;
  }

  uint32_t last_idx = 0;
  for (uint32_t i = 0; i < field_count; i++) {
    if (last_idx >= field_item->field_idx_ && i != 0) {
      LOG(ERROR) << StringPrintf("Out-of-order field_idx for annotation: %x then %x", last_idx, field_item->field_idx_);
      return false;
    }
    last_idx = field_item->field_idx_;
    field_item++;
  }

  // Method annotations follow immediately after field annotations.
  const DexFile::MethodAnnotationsItem* method_item =
      reinterpret_cast<const DexFile::MethodAnnotationsItem*>(field_item);
  uint32_t method_count = item->methods_size_;
  if (!CheckListSize(method_item, method_count, sizeof(DexFile::MethodAnnotationsItem), "method_annotations list")) {
    return false;
  }

  last_idx = 0;
  for (uint32_t i = 0; i < method_count; i++) {
    if (last_idx >= method_item->method_idx_ && i != 0) {
      LOG(ERROR) << StringPrintf("Out-of-order method_idx for annotation: %x then %x",
          last_idx, method_item->method_idx_);
      return false;
    }
    last_idx = method_item->method_idx_;
    method_item++;
  }

  // Parameter annotations follow immediately after method annotations.
  const DexFile::ParameterAnnotationsItem* parameter_item =
      reinterpret_cast<const DexFile::ParameterAnnotationsItem*>(method_item);
  uint32_t parameter_count = item->parameters_size_;
  if (!CheckListSize(parameter_item, parameter_count, sizeof(DexFile::ParameterAnnotationsItem),
      "parameter_annotations list")) {
    return false;
  }

  last_idx = 0;
  for (uint32_t i = 0; i < parameter_count; i++) {
    if (last_idx >= parameter_item->method_idx_ && i != 0) {
      LOG(ERROR) << StringPrintf("Out-of-order method_idx for annotation: %x then %x",
          last_idx, parameter_item->method_idx_);
      return false;
    }
    last_idx = parameter_item->method_idx_;
    parameter_item++;
  }

  // Return a pointer to the end of the annotations.
  ptr_ = reinterpret_cast<const byte*>(parameter_item);
  return true;
}

bool DexFileVerifier::CheckIntraSectionIterate(uint32_t offset, uint32_t count, uint16_t type) {
  // Get the right alignment mask for the type of section.
  uint32_t alignment_mask;
  switch (type) {
    case DexFile::kDexTypeClassDataItem:
    case DexFile::kDexTypeStringDataItem:
    case DexFile::kDexTypeDebugInfoItem:
    case DexFile::kDexTypeAnnotationItem:
    case DexFile::kDexTypeEncodedArrayItem:
      alignment_mask = sizeof(uint8_t) - 1;
      break;
    default:
      alignment_mask = sizeof(uint32_t) - 1;
      break;
  }

  // Iterate through the items in the section.
  for (uint32_t i = 0; i < count; i++) {
    uint32_t aligned_offset = (offset + alignment_mask) & ~alignment_mask;

    // Check the padding between items.
    if (!CheckPadding(offset, aligned_offset)) {
      return false;
    }

    // Check depending on the section type.
    switch (type) {
      case DexFile::kDexTypeStringIdItem: {
        if (!CheckPointerRange(ptr_, ptr_ + sizeof(DexFile::StringId), "string_ids")) {
          return false;
        }
        ptr_ += sizeof(DexFile::StringId);
        break;
      }
      case DexFile::kDexTypeTypeIdItem: {
        if (!CheckPointerRange(ptr_, ptr_ + sizeof(DexFile::TypeId), "type_ids")) {
          return false;
        }
        ptr_ += sizeof(DexFile::TypeId);
        break;
      }
      case DexFile::kDexTypeProtoIdItem: {
        if (!CheckPointerRange(ptr_, ptr_ + sizeof(DexFile::ProtoId), "proto_ids")) {
          return false;
        }
        ptr_ += sizeof(DexFile::ProtoId);
        break;
      }
      case DexFile::kDexTypeFieldIdItem: {
        if (!CheckPointerRange(ptr_, ptr_ + sizeof(DexFile::FieldId), "field_ids")) {
          return false;
        }
        ptr_ += sizeof(DexFile::FieldId);
        break;
      }
      case DexFile::kDexTypeMethodIdItem: {
        if (!CheckPointerRange(ptr_, ptr_ + sizeof(DexFile::MethodId), "method_ids")) {
          return false;
        }
        ptr_ += sizeof(DexFile::MethodId);
        break;
      }
      case DexFile::kDexTypeClassDefItem: {
        if (!CheckPointerRange(ptr_, ptr_ + sizeof(DexFile::ClassDef), "class_defs")) {
          return false;
        }
        ptr_ += sizeof(DexFile::ClassDef);
        break;
      }
      case DexFile::kDexTypeTypeList: {
        const DexFile::TypeList* list = reinterpret_cast<const DexFile::TypeList*>(ptr_);
        const DexFile::TypeItem* item = &list->GetTypeItem(0);
        uint32_t count = list->Size();

        if (!CheckPointerRange(list, list + 1, "type_list") ||
            !CheckListSize(item, count, sizeof(DexFile::TypeItem), "type_list size")) {
          return false;
        }
        ptr_ = reinterpret_cast<const byte*>(item + count);
        break;
      }
      case DexFile::kDexTypeAnnotationSetRefList: {
        const DexFile::AnnotationSetRefList* list =
            reinterpret_cast<const DexFile::AnnotationSetRefList*>(ptr_);
        const DexFile::AnnotationSetRefItem* item = list->list_;
        uint32_t count = list->size_;

        if (!CheckPointerRange(list, list + 1, "annotation_set_ref_list") ||
            !CheckListSize(item, count, sizeof(DexFile::AnnotationSetRefItem),
                "annotation_set_ref_list size")) {
          return false;
        }
        ptr_ = reinterpret_cast<const byte*>(item + count);
        break;
      }
      case DexFile::kDexTypeAnnotationSetItem: {
        const DexFile::AnnotationSetItem* set =
            reinterpret_cast<const DexFile::AnnotationSetItem*>(ptr_);
        const uint32_t* item = set->entries_;
        uint32_t count = set->size_;

        if (!CheckPointerRange(set, set + 1, "annotation_set_item") ||
            !CheckListSize(item, count, sizeof(uint32_t), "annotation_set_item size")) {
          return false;
        }
        ptr_ = reinterpret_cast<const byte*>(item + count);
        break;
      }
      case DexFile::kDexTypeClassDataItem: {
        if (!CheckIntraClassDataItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeCodeItem: {
        if (!CheckIntraCodeItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeStringDataItem: {
        if (!CheckIntraStringDataItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeDebugInfoItem: {
        if (!CheckIntraDebugInfoItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationItem: {
        if (!CheckIntraAnnotationItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeEncodedArrayItem: {
        if (!CheckEncodedArray()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationsDirectoryItem: {
        if (!CheckIntraAnnotationsDirectoryItem()) {
          return false;
        }
        break;
      }
      default:
        LOG(ERROR) << StringPrintf("Unknown map item type %x", type);
        return false;
    }

    if (IsDataSectionType(type)) {
      offset_to_type_map_.Put(aligned_offset, type);
    }

    aligned_offset = reinterpret_cast<uint32_t>(ptr_) - reinterpret_cast<uint32_t>(begin_);
    if (aligned_offset > size_) {
      LOG(ERROR) << StringPrintf("Item %d at ends out of bounds", i);
      return false;
    }

    offset = aligned_offset;
  }

  return true;
}

bool DexFileVerifier::CheckIntraIdSection(uint32_t offset, uint32_t count, uint16_t type) {
  uint32_t expected_offset;
  uint32_t expected_size;

  // Get the expected offset and size from the header.
  switch (type) {
    case DexFile::kDexTypeStringIdItem:
      expected_offset = header_->string_ids_off_;
      expected_size = header_->string_ids_size_;
      break;
    case DexFile::kDexTypeTypeIdItem:
      expected_offset = header_->type_ids_off_;
      expected_size = header_->type_ids_size_;
      break;
    case DexFile::kDexTypeProtoIdItem:
      expected_offset = header_->proto_ids_off_;
      expected_size = header_->proto_ids_size_;
      break;
    case DexFile::kDexTypeFieldIdItem:
      expected_offset = header_->field_ids_off_;
      expected_size = header_->field_ids_size_;
      break;
    case DexFile::kDexTypeMethodIdItem:
      expected_offset = header_->method_ids_off_;
      expected_size = header_->method_ids_size_;
      break;
    case DexFile::kDexTypeClassDefItem:
      expected_offset = header_->class_defs_off_;
      expected_size = header_->class_defs_size_;
      break;
    default:
      LOG(ERROR) << StringPrintf("Bad type for id section: %x", type);
      return false;
  }

  // Check that the offset and size are what were expected from the header.
  if (offset != expected_offset) {
    LOG(ERROR) << StringPrintf("Bad offset for section: got %x, expected %x", offset, expected_offset);
    return false;
  }
  if (count != expected_size) {
    LOG(ERROR) << StringPrintf("Bad size for section: got %x, expected %x", count, expected_size);
    return false;
  }

  return CheckIntraSectionIterate(offset, count, type);
}

bool DexFileVerifier::CheckIntraDataSection(uint32_t offset, uint32_t count, uint16_t type) {
  uint32_t data_start = header_->data_off_;
  uint32_t data_end = data_start + header_->data_size_;

  // Sanity check the offset of the section.
  if ((offset < data_start) || (offset > data_end)) {
    LOG(ERROR) << StringPrintf("Bad offset for data subsection: %x", offset);
    return false;
  }

  if (!CheckIntraSectionIterate(offset, count, type)) {
    return false;
  }

  uint32_t next_offset = reinterpret_cast<uint32_t>(ptr_) - reinterpret_cast<uint32_t>(begin_);
  if (next_offset > data_end) {
    LOG(ERROR) << StringPrintf("Out-of-bounds end of data subsection: %x", next_offset);
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckIntraSection() {
  const DexFile::MapList* map = reinterpret_cast<const DexFile::MapList*>(begin_ + header_->map_off_);
  const DexFile::MapItem* item = map->list_;

  uint32_t count = map->size_;
  uint32_t offset = 0;
  ptr_ = begin_;

  // Check the items listed in the map.
  while (count--) {
    uint32_t section_offset = item->offset_;
    uint32_t section_count = item->size_;
    uint16_t type = item->type_;

    // Check for padding and overlap between items.
    if (!CheckPadding(offset, section_offset)) {
      return false;
    } else if (offset > section_offset) {
      LOG(ERROR) << StringPrintf("Section overlap or out-of-order map: %x, %x", offset, section_offset);
      return false;
    }

    // Check each item based on its type.
    switch (type) {
      case DexFile::kDexTypeHeaderItem:
        if (section_count != 1) {
          LOG(ERROR) << "Multiple header items";
          return false;
        }
        if (section_offset != 0) {
          LOG(ERROR) << StringPrintf("Header at %x, not at start of file", section_offset);
          return false;
        }
        ptr_ = begin_ + header_->header_size_;
        offset = header_->header_size_;
        break;
      case DexFile::kDexTypeStringIdItem:
      case DexFile::kDexTypeTypeIdItem:
      case DexFile::kDexTypeProtoIdItem:
      case DexFile::kDexTypeFieldIdItem:
      case DexFile::kDexTypeMethodIdItem:
      case DexFile::kDexTypeClassDefItem:
        if (!CheckIntraIdSection(section_offset, section_count, type)) {
          return false;
        }
        offset = reinterpret_cast<uint32_t>(ptr_) - reinterpret_cast<uint32_t>(begin_);
        break;
      case DexFile::kDexTypeMapList:
        if (section_count != 1) {
          LOG(ERROR) << "Multiple map list items";
          return false;
        }
        if (section_offset != header_->map_off_) {
          LOG(ERROR) << StringPrintf("Map not at header-defined offset: %x, expected %x",
              section_offset, header_->map_off_);
          return false;
        }
        ptr_ += sizeof(uint32_t) + (map->size_ * sizeof(DexFile::MapItem));
        offset = section_offset + sizeof(uint32_t) + (map->size_ * sizeof(DexFile::MapItem));
        break;
      case DexFile::kDexTypeTypeList:
      case DexFile::kDexTypeAnnotationSetRefList:
      case DexFile::kDexTypeAnnotationSetItem:
      case DexFile::kDexTypeClassDataItem:
      case DexFile::kDexTypeCodeItem:
      case DexFile::kDexTypeStringDataItem:
      case DexFile::kDexTypeDebugInfoItem:
      case DexFile::kDexTypeAnnotationItem:
      case DexFile::kDexTypeEncodedArrayItem:
      case DexFile::kDexTypeAnnotationsDirectoryItem:
        if (!CheckIntraDataSection(section_offset, section_count, type)) {
          return false;
        }
        offset = reinterpret_cast<uint32_t>(ptr_) - reinterpret_cast<uint32_t>(begin_);
        break;
      default:
        LOG(ERROR) << StringPrintf("Unknown map item type %x", type);
        return false;
    }

    item++;
  }

  return true;
}

bool DexFileVerifier::CheckOffsetToTypeMap(uint32_t offset, uint16_t type) {
  auto it = offset_to_type_map_.find(offset);
  if (it == offset_to_type_map_.end()) {
    LOG(ERROR) << StringPrintf("No data map entry found @ %x; expected %x", offset, type);
    return false;
  }
  if (it->second != type) {
    LOG(ERROR) << StringPrintf("Unexpected data map entry @ %x; expected %x, found %x",
        offset, type, it->second);
    return false;
  }
  return true;
}

uint16_t DexFileVerifier::FindFirstClassDataDefiner(const byte* ptr) const {
  ClassDataItemIterator it(*dex_file_, ptr);

  if (it.HasNextStaticField() || it.HasNextInstanceField()) {
    const DexFile::FieldId& field = dex_file_->GetFieldId(it.GetMemberIndex());
    return field.class_idx_;
  }

  if (it.HasNextDirectMethod() || it.HasNextVirtualMethod()) {
    const DexFile::MethodId& method = dex_file_->GetMethodId(it.GetMemberIndex());
    return method.class_idx_;
  }

  return DexFile::kDexNoIndex16;
}

uint16_t DexFileVerifier::FindFirstAnnotationsDirectoryDefiner(const byte* ptr) const {
  const DexFile::AnnotationsDirectoryItem* item =
      reinterpret_cast<const DexFile::AnnotationsDirectoryItem*>(ptr);
  if (item->fields_size_ != 0) {
    DexFile::FieldAnnotationsItem* field_items = (DexFile::FieldAnnotationsItem*) (item + 1);
    const DexFile::FieldId& field = dex_file_->GetFieldId(field_items[0].field_idx_);
    return field.class_idx_;
  }

  if (item->methods_size_ != 0) {
    DexFile::MethodAnnotationsItem* method_items = (DexFile::MethodAnnotationsItem*) (item + 1);
    const DexFile::MethodId& method = dex_file_->GetMethodId(method_items[0].method_idx_);
    return method.class_idx_;
  }

  if (item->parameters_size_ != 0) {
    DexFile::ParameterAnnotationsItem* parameter_items = (DexFile::ParameterAnnotationsItem*) (item + 1);
    const DexFile::MethodId& method = dex_file_->GetMethodId(parameter_items[0].method_idx_);
    return method.class_idx_;
  }

  return DexFile::kDexNoIndex16;
}

bool DexFileVerifier::CheckInterStringIdItem() {
  const DexFile::StringId* item = reinterpret_cast<const DexFile::StringId*>(ptr_);

  // Check the map to make sure it has the right offset->type.
  if (!CheckOffsetToTypeMap(item->string_data_off_, DexFile::kDexTypeStringDataItem)) {
    return false;
  }

  // Check ordering between items.
  if (previous_item_ != NULL) {
    const DexFile::StringId* prev_item = reinterpret_cast<const DexFile::StringId*>(previous_item_);
    const char* prev_str = dex_file_->GetStringData(*prev_item);
    const char* str = dex_file_->GetStringData(*item);
    if (CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(prev_str, str) >= 0) {
      LOG(ERROR) << StringPrintf("Out-of-order string_ids: '%s' then '%s'", prev_str, str);
      return false;
    }
  }

  ptr_ += sizeof(DexFile::StringId);
  return true;
}

bool DexFileVerifier::CheckInterTypeIdItem() {
  const DexFile::TypeId* item = reinterpret_cast<const DexFile::TypeId*>(ptr_);
  const char* descriptor = dex_file_->StringDataByIdx(item->descriptor_idx_);

  // Check that the descriptor is a valid type.
  if (!IsValidDescriptor(descriptor)) {
    LOG(ERROR) << StringPrintf("Invalid type descriptor: '%s'", descriptor);
    return false;
  }

  // Check ordering between items.
  if (previous_item_ != NULL) {
    const DexFile::TypeId* prev_item = reinterpret_cast<const DexFile::TypeId*>(previous_item_);
    if (prev_item->descriptor_idx_ >= item->descriptor_idx_) {
      LOG(ERROR) << StringPrintf("Out-of-order type_ids: %x then %x",
          prev_item->descriptor_idx_, item->descriptor_idx_);
      return false;
    }
  }

  ptr_ += sizeof(DexFile::TypeId);
  return true;
}

bool DexFileVerifier::CheckInterProtoIdItem() {
  const DexFile::ProtoId* item = reinterpret_cast<const DexFile::ProtoId*>(ptr_);
  const char* shorty = dex_file_->StringDataByIdx(item->shorty_idx_);
  if (item->parameters_off_ != 0 &&
      !CheckOffsetToTypeMap(item->parameters_off_, DexFile::kDexTypeTypeList)) {
    return false;
  }

  // Check the return type and advance the shorty.
  if (!CheckShortyDescriptorMatch(*shorty, dex_file_->StringByTypeIdx(item->return_type_idx_), true)) {
    return false;
  }
  shorty++;

  DexFileParameterIterator it(*dex_file_, *item);
  while (it.HasNext() && *shorty != '\0') {
    const char* descriptor = it.GetDescriptor();
    if (!CheckShortyDescriptorMatch(*shorty, descriptor, false)) {
      return false;
    }
    it.Next();
    shorty++;
  }
  if (it.HasNext() || *shorty != '\0') {
    LOG(ERROR) << "Mismatched length for parameters and shorty";
    return false;
  }

  // Check ordering between items. This relies on type_ids being in order.
  if (previous_item_ != NULL) {
    const DexFile::ProtoId* prev = reinterpret_cast<const DexFile::ProtoId*>(previous_item_);
    if (prev->return_type_idx_ > item->return_type_idx_) {
      LOG(ERROR) << "Out-of-order proto_id return types";
      return false;
    } else if (prev->return_type_idx_ == item->return_type_idx_) {
      DexFileParameterIterator curr_it(*dex_file_, *item);
      DexFileParameterIterator prev_it(*dex_file_, *prev);

      while (curr_it.HasNext() && prev_it.HasNext()) {
        uint16_t prev_idx = prev_it.GetTypeIdx();
        uint16_t curr_idx = curr_it.GetTypeIdx();
        if (prev_idx == DexFile::kDexNoIndex16) {
          break;
        }
        if (curr_idx == DexFile::kDexNoIndex16) {
          LOG(ERROR) << "Out-of-order proto_id arguments";
          return false;
        }

        if (prev_idx < curr_idx) {
          break;
        } else if (prev_idx > curr_idx) {
          LOG(ERROR) << "Out-of-order proto_id arguments";
          return false;
        }

        prev_it.Next();
        curr_it.Next();
      }
    }
  }

  ptr_ += sizeof(DexFile::ProtoId);
  return true;
}

bool DexFileVerifier::CheckInterFieldIdItem() {
  const DexFile::FieldId* item = reinterpret_cast<const DexFile::FieldId*>(ptr_);

  // Check that the class descriptor is valid.
  const char* descriptor = dex_file_->StringByTypeIdx(item->class_idx_);
  if (!IsValidDescriptor(descriptor) || descriptor[0] != 'L') {
    LOG(ERROR) << "Invalid descriptor for class_idx: '" << descriptor << '"';
    return false;
  }

  // Check that the type descriptor is a valid field name.
  descriptor = dex_file_->StringByTypeIdx(item->type_idx_);
  if (!IsValidDescriptor(descriptor) || descriptor[0] == 'V') {
    LOG(ERROR) << "Invalid descriptor for type_idx: '" << descriptor << '"';
    return false;
  }

  // Check that the name is valid.
  descriptor = dex_file_->StringDataByIdx(item->name_idx_);
  if (!IsValidMemberName(descriptor)) {
    LOG(ERROR) << "Invalid field name: '" << descriptor << '"';
    return false;
  }

  // Check ordering between items. This relies on the other sections being in order.
  if (previous_item_ != NULL) {
    const DexFile::FieldId* prev_item = reinterpret_cast<const DexFile::FieldId*>(previous_item_);
    if (prev_item->class_idx_ > item->class_idx_) {
      LOG(ERROR) << "Out-of-order field_ids";
      return false;
    } else if (prev_item->class_idx_ == item->class_idx_) {
      if (prev_item->name_idx_ > item->name_idx_) {
        LOG(ERROR) << "Out-of-order field_ids";
        return false;
      } else if (prev_item->name_idx_ == item->name_idx_) {
        if (prev_item->type_idx_ >= item->type_idx_) {
          LOG(ERROR) << "Out-of-order field_ids";
          return false;
        }
      }
    }
  }

  ptr_ += sizeof(DexFile::FieldId);
  return true;
}

bool DexFileVerifier::CheckInterMethodIdItem() {
  const DexFile::MethodId* item = reinterpret_cast<const DexFile::MethodId*>(ptr_);

  // Check that the class descriptor is a valid reference name.
  const char* descriptor = dex_file_->StringByTypeIdx(item->class_idx_);
  if (!IsValidDescriptor(descriptor) || (descriptor[0] != 'L' && descriptor[0] != '[')) {
    LOG(ERROR) << "Invalid descriptor for class_idx: '" << descriptor << '"';
    return false;
  }

  // Check that the name is valid.
  descriptor = dex_file_->StringDataByIdx(item->name_idx_);
  if (!IsValidMemberName(descriptor)) {
    LOG(ERROR) << "Invalid method name: '" << descriptor << '"';
    return false;
  }

  // Check ordering between items. This relies on the other sections being in order.
  if (previous_item_ != NULL) {
    const DexFile::MethodId* prev_item = reinterpret_cast<const DexFile::MethodId*>(previous_item_);
    if (prev_item->class_idx_ > item->class_idx_) {
      LOG(ERROR) << "Out-of-order method_ids";
      return false;
    } else if (prev_item->class_idx_ == item->class_idx_) {
      if (prev_item->name_idx_ > item->name_idx_) {
        LOG(ERROR) << "Out-of-order method_ids";
        return false;
      } else if (prev_item->name_idx_ == item->name_idx_) {
        if (prev_item->proto_idx_ >= item->proto_idx_) {
          LOG(ERROR) << "Out-of-order method_ids";
          return false;
        }
      }
    }
  }

  ptr_ += sizeof(DexFile::MethodId);
  return true;
}

bool DexFileVerifier::CheckInterClassDefItem() {
  const DexFile::ClassDef* item = reinterpret_cast<const DexFile::ClassDef*>(ptr_);
  uint32_t class_idx = item->class_idx_;
  const char* descriptor = dex_file_->StringByTypeIdx(class_idx);

  if (!IsValidDescriptor(descriptor) || descriptor[0] != 'L') {
    LOG(ERROR) << "Invalid class descriptor: '" << descriptor << "'";
    return false;
  }

  if (item->interfaces_off_ != 0 &&
      !CheckOffsetToTypeMap(item->interfaces_off_, DexFile::kDexTypeTypeList)) {
    return false;
  }
  if (item->annotations_off_ != 0 &&
      !CheckOffsetToTypeMap(item->annotations_off_, DexFile::kDexTypeAnnotationsDirectoryItem)) {
    return false;
  }
  if (item->class_data_off_ != 0 &&
      !CheckOffsetToTypeMap(item->class_data_off_, DexFile::kDexTypeClassDataItem)) {
    return false;
  }
  if (item->static_values_off_ != 0 &&
      !CheckOffsetToTypeMap(item->static_values_off_, DexFile::kDexTypeEncodedArrayItem)) {
    return false;
  }

  if (item->superclass_idx_ != DexFile::kDexNoIndex16) {
    descriptor = dex_file_->StringByTypeIdx(item->superclass_idx_);
    if (!IsValidDescriptor(descriptor) || descriptor[0] != 'L') {
      LOG(ERROR) << "Invalid superclass: '" << descriptor << "'";
      return false;
    }
  }

  const DexFile::TypeList* interfaces = dex_file_->GetInterfacesList(*item);
  if (interfaces != NULL) {
    uint32_t size = interfaces->Size();

    // Ensure that all interfaces refer to classes (not arrays or primitives).
    for (uint32_t i = 0; i < size; i++) {
      descriptor = dex_file_->StringByTypeIdx(interfaces->GetTypeItem(i).type_idx_);
      if (!IsValidDescriptor(descriptor) || descriptor[0] != 'L') {
        LOG(ERROR) << "Invalid interface: '" << descriptor << "'";
        return false;
      }
    }

    /*
     * Ensure that there are no duplicates. This is an O(N^2) test, but in
     * practice the number of interfaces implemented by any given class is low.
     */
    for (uint32_t i = 1; i < size; i++) {
      uint32_t idx1 = interfaces->GetTypeItem(i).type_idx_;
      for (uint32_t j =0; j < i; j++) {
        uint32_t idx2 = interfaces->GetTypeItem(j).type_idx_;
        if (idx1 == idx2) {
          LOG(ERROR) << "Duplicate interface: '" << dex_file_->StringByTypeIdx(idx1) << "'";
          return false;
        }
      }
    }
  }

  // Check that references in class_data_item are to the right class.
  if (item->class_data_off_ != 0) {
    const byte* data = begin_ + item->class_data_off_;
    uint16_t data_definer = FindFirstClassDataDefiner(data);
    if ((data_definer != item->class_idx_) && (data_definer != DexFile::kDexNoIndex16)) {
      LOG(ERROR) << "Invalid class_data_item";
      return false;
    }
  }

  // Check that references in annotations_directory_item are to right class.
  if (item->annotations_off_ != 0) {
    const byte* data = begin_ + item->annotations_off_;
    uint16_t annotations_definer = FindFirstAnnotationsDirectoryDefiner(data);
    if ((annotations_definer != item->class_idx_) && (annotations_definer != DexFile::kDexNoIndex16)) {
      LOG(ERROR) << "Invalid annotations_directory_item";
      return false;
    }
  }

  ptr_ += sizeof(DexFile::ClassDef);
  return true;
}

bool DexFileVerifier::CheckInterAnnotationSetRefList() {
  const DexFile::AnnotationSetRefList* list =
      reinterpret_cast<const DexFile::AnnotationSetRefList*>(ptr_);
  const DexFile::AnnotationSetRefItem* item = list->list_;
  uint32_t count = list->size_;

  while (count--) {
    if (item->annotations_off_ != 0 &&
        !CheckOffsetToTypeMap(item->annotations_off_, DexFile::kDexTypeAnnotationSetItem)) {
      return false;
    }
    item++;
  }

  ptr_ = reinterpret_cast<const byte*>(item);
  return true;
}

bool DexFileVerifier::CheckInterAnnotationSetItem() {
  const DexFile::AnnotationSetItem* set = reinterpret_cast<const DexFile::AnnotationSetItem*>(ptr_);
  const uint32_t* offsets = set->entries_;
  uint32_t count = set->size_;
  uint32_t last_idx = 0;

  for (uint32_t i = 0; i < count; i++) {
    if (*offsets != 0 && !CheckOffsetToTypeMap(*offsets, DexFile::kDexTypeAnnotationItem)) {
      return false;
    }

    // Get the annotation from the offset and the type index for the annotation.
    const DexFile::AnnotationItem* annotation =
        reinterpret_cast<const DexFile::AnnotationItem*>(begin_ + *offsets);
    const uint8_t* data = annotation->annotation_;
    uint32_t idx = DecodeUnsignedLeb128(&data);

    if (last_idx >= idx && i != 0) {
      LOG(ERROR) << StringPrintf("Out-of-order entry types: %x then %x", last_idx, idx);
      return false;
    }

    last_idx = idx;
    offsets++;
  }

  ptr_ = reinterpret_cast<const byte*>(offsets);
  return true;
}

bool DexFileVerifier::CheckInterClassDataItem() {
  ClassDataItemIterator it(*dex_file_, ptr_);
  uint16_t defining_class = FindFirstClassDataDefiner(ptr_);

  for (; it.HasNextStaticField() || it.HasNextInstanceField(); it.Next()) {
    const DexFile::FieldId& field = dex_file_->GetFieldId(it.GetMemberIndex());
    if (field.class_idx_ != defining_class) {
      LOG(ERROR) << "Mismatched defining class for class_data_item field";
      return false;
    }
  }
  for (; it.HasNextDirectMethod() || it.HasNextVirtualMethod(); it.Next()) {
    uint32_t code_off = it.GetMethodCodeItemOffset();
    if (code_off != 0 && !CheckOffsetToTypeMap(code_off, DexFile::kDexTypeCodeItem)) {
      return false;
    }
    const DexFile::MethodId& method = dex_file_->GetMethodId(it.GetMemberIndex());
    if (method.class_idx_ != defining_class) {
      LOG(ERROR) << "Mismatched defining class for class_data_item method";
      return false;
    }
  }

  ptr_ = it.EndDataPointer();
  return true;
}

bool DexFileVerifier::CheckInterAnnotationsDirectoryItem() {
  const DexFile::AnnotationsDirectoryItem* item =
      reinterpret_cast<const DexFile::AnnotationsDirectoryItem*>(ptr_);
  uint16_t defining_class = FindFirstAnnotationsDirectoryDefiner(ptr_);

  if (item->class_annotations_off_ != 0 &&
      !CheckOffsetToTypeMap(item->class_annotations_off_, DexFile::kDexTypeAnnotationSetItem)) {
    return false;
  }

  // Field annotations follow immediately after the annotations directory.
  const DexFile::FieldAnnotationsItem* field_item =
      reinterpret_cast<const DexFile::FieldAnnotationsItem*>(item + 1);
  uint32_t field_count = item->fields_size_;
  for (uint32_t i = 0; i < field_count; i++) {
    const DexFile::FieldId& field = dex_file_->GetFieldId(field_item->field_idx_);
    if (field.class_idx_ != defining_class) {
      LOG(ERROR) << "Mismatched defining class for field_annotation";
      return false;
    }
    if (!CheckOffsetToTypeMap(field_item->annotations_off_, DexFile::kDexTypeAnnotationSetItem)) {
      return false;
    }
    field_item++;
  }

  // Method annotations follow immediately after field annotations.
  const DexFile::MethodAnnotationsItem* method_item =
      reinterpret_cast<const DexFile::MethodAnnotationsItem*>(field_item);
  uint32_t method_count = item->methods_size_;
  for (uint32_t i = 0; i < method_count; i++) {
    const DexFile::MethodId& method = dex_file_->GetMethodId(method_item->method_idx_);
    if (method.class_idx_ != defining_class) {
      LOG(ERROR) << "Mismatched defining class for method_annotation";
      return false;
    }
    if (!CheckOffsetToTypeMap(method_item->annotations_off_, DexFile::kDexTypeAnnotationSetItem)) {
      return false;
    }
    method_item++;
  }

  // Parameter annotations follow immediately after method annotations.
  const DexFile::ParameterAnnotationsItem* parameter_item =
      reinterpret_cast<const DexFile::ParameterAnnotationsItem*>(method_item);
  uint32_t parameter_count = item->parameters_size_;
  for (uint32_t i = 0; i < parameter_count; i++) {
    const DexFile::MethodId& parameter_method = dex_file_->GetMethodId(parameter_item->method_idx_);
    if (parameter_method.class_idx_ != defining_class) {
      LOG(ERROR) << "Mismatched defining class for parameter_annotation";
      return false;
    }
    if (!CheckOffsetToTypeMap(parameter_item->annotations_off_,
        DexFile::kDexTypeAnnotationSetRefList)) {
      return false;
    }
    parameter_item++;
  }

  ptr_ = reinterpret_cast<const byte*>(parameter_item);
  return true;
}

bool DexFileVerifier::CheckInterSectionIterate(uint32_t offset, uint32_t count, uint16_t type) {
  // Get the right alignment mask for the type of section.
  uint32_t alignment_mask;
  switch (type) {
    case DexFile::kDexTypeClassDataItem:
      alignment_mask = sizeof(uint8_t) - 1;
      break;
    default:
      alignment_mask = sizeof(uint32_t) - 1;
      break;
  }

  // Iterate through the items in the section.
  previous_item_ = NULL;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t new_offset = (offset + alignment_mask) & ~alignment_mask;
    ptr_ = begin_ + new_offset;
    const byte* prev_ptr = ptr_;

    // Check depending on the section type.
    switch (type) {
      case DexFile::kDexTypeStringIdItem: {
        if (!CheckInterStringIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeTypeIdItem: {
        if (!CheckInterTypeIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeProtoIdItem: {
        if (!CheckInterProtoIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeFieldIdItem: {
        if (!CheckInterFieldIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeMethodIdItem: {
        if (!CheckInterMethodIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeClassDefItem: {
        if (!CheckInterClassDefItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationSetRefList: {
        if (!CheckInterAnnotationSetRefList()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationSetItem: {
        if (!CheckInterAnnotationSetItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeClassDataItem: {
        if (!CheckInterClassDataItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationsDirectoryItem: {
        if (!CheckInterAnnotationsDirectoryItem()) {
          return false;
        }
        break;
      }
      default:
        LOG(ERROR) << StringPrintf("Unknown map item type %x", type);
        return false;
    }

    previous_item_ = prev_ptr;
    offset = reinterpret_cast<uint32_t>(ptr_) - reinterpret_cast<uint32_t>(begin_);
  }

  return true;
}

bool DexFileVerifier::CheckInterSection() {
  const DexFile::MapList* map = reinterpret_cast<const DexFile::MapList*>(begin_ + header_->map_off_);
  const DexFile::MapItem* item = map->list_;
  uint32_t count = map->size_;

  // Cross check the items listed in the map.
  while (count--) {
    uint32_t section_offset = item->offset_;
    uint32_t section_count = item->size_;
    uint16_t type = item->type_;

    switch (type) {
      case DexFile::kDexTypeHeaderItem:
      case DexFile::kDexTypeMapList:
      case DexFile::kDexTypeTypeList:
      case DexFile::kDexTypeCodeItem:
      case DexFile::kDexTypeStringDataItem:
      case DexFile::kDexTypeDebugInfoItem:
      case DexFile::kDexTypeAnnotationItem:
      case DexFile::kDexTypeEncodedArrayItem:
        break;
      case DexFile::kDexTypeStringIdItem:
      case DexFile::kDexTypeTypeIdItem:
      case DexFile::kDexTypeProtoIdItem:
      case DexFile::kDexTypeFieldIdItem:
      case DexFile::kDexTypeMethodIdItem:
      case DexFile::kDexTypeClassDefItem:
      case DexFile::kDexTypeAnnotationSetRefList:
      case DexFile::kDexTypeAnnotationSetItem:
      case DexFile::kDexTypeClassDataItem:
      case DexFile::kDexTypeAnnotationsDirectoryItem: {
        if (!CheckInterSectionIterate(section_offset, section_count, type)) {
          return false;
        }
        break;
      }
      default:
        LOG(ERROR) << StringPrintf("Unknown map item type %x", type);
        return false;
    }

    item++;
  }

  return true;
}

bool DexFileVerifier::Verify() {
  // Check the header.
  if (!CheckHeader()) {
    return false;
  }

  // Check the map section.
  if (!CheckMap()) {
    return false;
  }

  // Check structure within remaining sections.
  if (!CheckIntraSection()) {
    return false;
  }

  // Check references from one section to another.
  if (!CheckInterSection()) {
    return false;
  }

  return true;
}

}  // namespace art
