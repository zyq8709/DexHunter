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

#ifndef ART_RUNTIME_DEX_FILE_INL_H_
#define ART_RUNTIME_DEX_FILE_INL_H_

#include "base/logging.h"
#include "dex_file.h"
#include "leb128.h"
#include "utils.h"

namespace art {

inline int32_t DexFile::GetStringLength(const StringId& string_id) const {
  const byte* ptr = begin_ + string_id.string_data_off_;
  return DecodeUnsignedLeb128(&ptr);
}

inline const char* DexFile::GetStringDataAndLength(const StringId& string_id, uint32_t* length) const {
  DCHECK(length != NULL) << GetLocation();
  const byte* ptr = begin_ + string_id.string_data_off_;
  *length = DecodeUnsignedLeb128(&ptr);
  return reinterpret_cast<const char*>(ptr);
}

inline const DexFile::TryItem* DexFile::GetTryItems(const CodeItem& code_item, uint32_t offset) {
  const uint16_t* insns_end_ = &code_item.insns_[code_item.insns_size_in_code_units_];
  return reinterpret_cast<const TryItem*>
      (RoundUp(reinterpret_cast<uint32_t>(insns_end_), 4)) + offset;
}

}  // namespace art

#endif  // ART_RUNTIME_DEX_FILE_INL_H_
