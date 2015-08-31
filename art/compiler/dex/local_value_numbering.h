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

#ifndef ART_COMPILER_DEX_LOCAL_VALUE_NUMBERING_H_
#define ART_COMPILER_DEX_LOCAL_VALUE_NUMBERING_H_

#include "compiler_internals.h"

#define NO_VALUE 0xffff
#define ARRAY_REF 0xfffe

namespace art {

// Key is s_reg, value is value name.
typedef SafeMap<uint16_t, uint16_t> SregValueMap;
// Key is concatenation of quad, value is value name.
typedef SafeMap<uint64_t, uint16_t> ValueMap;
// Key represents a memory address, value is generation.
typedef SafeMap<uint32_t, uint16_t> MemoryVersionMap;

class LocalValueNumbering {
 public:
  explicit LocalValueNumbering(CompilationUnit* cu) : cu_(cu) {}

  static uint64_t BuildKey(uint16_t op, uint16_t operand1, uint16_t operand2, uint16_t modifier) {
    return (static_cast<uint64_t>(op) << 48 | static_cast<uint64_t>(operand1) << 32 |
            static_cast<uint64_t>(operand2) << 16 | static_cast<uint64_t>(modifier));
  };

  uint16_t LookupValue(uint16_t op, uint16_t operand1, uint16_t operand2, uint16_t modifier) {
    uint16_t res;
    uint64_t key = BuildKey(op, operand1, operand2, modifier);
    ValueMap::iterator it = value_map_.find(key);
    if (it != value_map_.end()) {
      res = it->second;
    } else {
      res = value_map_.size() + 1;
      value_map_.Put(key, res);
    }
    return res;
  };

  bool ValueExists(uint16_t op, uint16_t operand1, uint16_t operand2, uint16_t modifier) const {
    uint64_t key = BuildKey(op, operand1, operand2, modifier);
    ValueMap::const_iterator it = value_map_.find(key);
    return (it != value_map_.end());
  };

  uint16_t GetMemoryVersion(uint16_t base, uint16_t field) {
    uint32_t key = (base << 16) | field;
    uint16_t res;
    MemoryVersionMap::iterator it = memory_version_map_.find(key);
    if (it == memory_version_map_.end()) {
      res = 0;
      memory_version_map_.Put(key, res);
    } else {
      res = it->second;
    }
    return res;
  };

  void AdvanceMemoryVersion(uint16_t base, uint16_t field) {
    uint32_t key = (base << 16) | field;
    MemoryVersionMap::iterator it = memory_version_map_.find(key);
    if (it == memory_version_map_.end()) {
      memory_version_map_.Put(key, 0);
    } else {
      it->second++;
    }
  };

  void SetOperandValue(uint16_t s_reg, uint16_t value) {
    SregValueMap::iterator it = sreg_value_map_.find(s_reg);
    if (it != sreg_value_map_.end()) {
      DCHECK_EQ(it->second, value);
    } else {
      sreg_value_map_.Put(s_reg, value);
    }
  };

  uint16_t GetOperandValue(int s_reg) {
    uint16_t res = NO_VALUE;
    SregValueMap::iterator it = sreg_value_map_.find(s_reg);
    if (it != sreg_value_map_.end()) {
      res = it->second;
    } else {
      // First use
      res = LookupValue(NO_VALUE, s_reg, NO_VALUE, NO_VALUE);
      sreg_value_map_.Put(s_reg, res);
    }
    return res;
  };

  void SetOperandValueWide(uint16_t s_reg, uint16_t value) {
    SregValueMap::iterator it = sreg_wide_value_map_.find(s_reg);
    if (it != sreg_wide_value_map_.end()) {
      DCHECK_EQ(it->second, value);
    } else {
      sreg_wide_value_map_.Put(s_reg, value);
    }
  };

  uint16_t GetOperandValueWide(int s_reg) {
    uint16_t res = NO_VALUE;
    SregValueMap::iterator it = sreg_wide_value_map_.find(s_reg);
    if (it != sreg_wide_value_map_.end()) {
      res = it->second;
    } else {
      // First use
      res = LookupValue(NO_VALUE, s_reg, NO_VALUE, NO_VALUE);
      sreg_wide_value_map_.Put(s_reg, res);
    }
    return res;
  };

  uint16_t GetValueNumber(MIR* mir);

 private:
  CompilationUnit* const cu_;
  SregValueMap sreg_value_map_;
  SregValueMap sreg_wide_value_map_;
  ValueMap value_map_;
  MemoryVersionMap memory_version_map_;
  std::set<uint16_t> null_checked_;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_LOCAL_VALUE_NUMBERING_H_
