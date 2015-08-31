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

#ifndef ART_RUNTIME_VMAP_TABLE_H_
#define ART_RUNTIME_VMAP_TABLE_H_

#include "base/logging.h"
#include "leb128.h"
#include "stack.h"

namespace art {

class VmapTable {
 public:
  explicit VmapTable(const uint8_t* table) : table_(table) {
  }

  // Look up nth entry, not called from performance critical code.
  uint16_t operator[](size_t n) const {
    const uint8_t* table = table_;
    size_t size = DecodeUnsignedLeb128(&table);
    CHECK_LT(n, size);
    uint16_t entry = DecodeUnsignedLeb128(&table);
    for (size_t i = 0; i < n; ++i) {
      entry = DecodeUnsignedLeb128(&table);
    }
    return entry;
  }

  size_t Size() const {
    const uint8_t* table = table_;
    return DecodeUnsignedLeb128(&table);
  }

  // Is the dex register 'vreg' in the context or on the stack? Should not be called when the
  // 'kind' is unknown or constant.
  bool IsInContext(size_t vreg, VRegKind kind, uint32_t* vmap_offset) const {
    DCHECK(kind == kReferenceVReg || kind == kIntVReg || kind == kFloatVReg ||
           kind == kLongLoVReg || kind == kLongHiVReg || kind == kDoubleLoVReg ||
           kind == kDoubleHiVReg || kind == kImpreciseConstant);
    *vmap_offset = 0xEBAD0FF5;
    // TODO: take advantage of the registers being ordered
    // TODO: we treat kImpreciseConstant as an integer below, need to ensure that such values
    //       are never promoted to floating point registers.
    bool is_float = (kind == kFloatVReg) || (kind == kDoubleLoVReg) || (kind == kDoubleHiVReg);
    bool in_floats = false;
    const uint8_t* table = table_;
    size_t end = DecodeUnsignedLeb128(&table);
    for (size_t i = 0; i < end; ++i) {
      // Stop if we find what we are are looking for.
      uint16_t entry = DecodeUnsignedLeb128(&table);
      if ((entry == vreg) && (in_floats == is_float)) {
        *vmap_offset = i;
        return true;
      }
      // 0xffff is the marker for LR (return PC on x86), following it are spilled float registers.
      if (entry == 0xffff) {
        in_floats = true;
      }
    }
    return false;
  }

  // Compute the register number that corresponds to the entry in the vmap (vmap_offset, computed
  // by IsInContext above). If the kind is floating point then the result will be a floating point
  // register number, otherwise it will be an integer register number.
  uint32_t ComputeRegister(uint32_t spill_mask, uint32_t vmap_offset, VRegKind kind) const {
    // Compute the register we need to load from the context.
    DCHECK(kind == kReferenceVReg || kind == kIntVReg || kind == kFloatVReg ||
           kind == kLongLoVReg || kind == kLongHiVReg || kind == kDoubleLoVReg ||
           kind == kDoubleHiVReg || kind == kImpreciseConstant);
    // TODO: we treat kImpreciseConstant as an integer below, need to ensure that such values
    //       are never promoted to floating point registers.
    bool is_float = (kind == kFloatVReg) || (kind == kDoubleLoVReg) || (kind == kDoubleHiVReg);
    uint32_t matches = 0;
    if (UNLIKELY(is_float)) {
      const uint8_t* table = table_;
      DecodeUnsignedLeb128(&table);  // Skip size.
      while (DecodeUnsignedLeb128(&table) != 0xffff) {
        matches++;
      }
      matches++;
    }
    CHECK_LT(vmap_offset - matches, static_cast<uint32_t>(__builtin_popcount(spill_mask)));
    uint32_t spill_shifts = 0;
    while (matches != (vmap_offset + 1)) {
      DCHECK_NE(spill_mask, 0u);
      matches += spill_mask & 1;  // Add 1 if the low bit is set
      spill_mask >>= 1;
      spill_shifts++;
    }
    spill_shifts--;  // wind back one as we want the last match
    return spill_shifts;
  }

 private:
  const uint8_t* const table_;
};

}  // namespace art

#endif  // ART_RUNTIME_VMAP_TABLE_H_
