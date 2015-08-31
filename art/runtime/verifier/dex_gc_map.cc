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

#include "verifier/dex_gc_map.h"

#include "base/logging.h"

namespace art {
namespace verifier {

const uint8_t* DexPcToReferenceMap::FindBitMap(uint16_t dex_pc, bool error_if_not_present) const {
  size_t num_entries = NumEntries();
  // Do linear or binary search?
  static const size_t kSearchThreshold = 8;
  if (num_entries < kSearchThreshold) {
    for (size_t i = 0; i < num_entries; i++)  {
      if (GetDexPc(i) == dex_pc) {
        return GetBitMap(i);
      }
    }
  } else {
    int lo = 0;
    int hi = num_entries -1;
    while (hi >= lo) {
      int mid = (hi + lo) / 2;
      int mid_pc = GetDexPc(mid);
      if (dex_pc > mid_pc) {
        lo = mid + 1;
      } else if (dex_pc < mid_pc) {
        hi = mid - 1;
      } else {
        return GetBitMap(mid);
      }
    }
  }
  if (error_if_not_present) {
    LOG(ERROR) << "Didn't find reference bit map for dex_pc " << dex_pc;
  }
  return NULL;
}

}  // namespace verifier
}  // namespace art
