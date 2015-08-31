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

#ifndef ART_COMPILER_DEX_ARENA_BIT_VECTOR_H_
#define ART_COMPILER_DEX_ARENA_BIT_VECTOR_H_

#include <stdint.h>
#include <stddef.h>
#include "compiler_enums.h"
#include "arena_allocator.h"

namespace art {

/*
 * Expanding bitmap, used for tracking resources.  Bits are numbered starting
 * from zero.  All operations on a BitVector are unsynchronized.
 */
class ArenaBitVector {
  public:
    class Iterator {
      public:
        explicit Iterator(ArenaBitVector* bit_vector)
          : p_bits_(bit_vector),
            bit_storage_(bit_vector->GetRawStorage()),
            bit_index_(0),
            bit_size_(p_bits_->storage_size_ * sizeof(uint32_t) * 8) {}

        // Return the position of the next set bit.  -1 means end-of-element reached.
        int Next() {
          // Did anything obviously change since we started?
          DCHECK_EQ(bit_size_, p_bits_->GetStorageSize() * sizeof(uint32_t) * 8);
          DCHECK_EQ(bit_storage_, p_bits_->GetRawStorage());

          if (bit_index_ >= bit_size_) return -1;

          uint32_t word_index = bit_index_ / 32;
          uint32_t word = bit_storage_[word_index];
          // Mask out any bits in the first word we've already considered.
          word >>= bit_index_ & 0x1f;
          if (word == 0) {
            bit_index_ &= ~0x1f;
            do {
              word_index++;
              if ((word_index * 32) >= bit_size_) {
                bit_index_ = bit_size_;
                return -1;
              }
              word = bit_storage_[word_index];
              bit_index_ += 32;
            } while (word == 0);
          }
          bit_index_ += CTZ(word) + 1;
          return bit_index_ - 1;
        }

        static void* operator new(size_t size, ArenaAllocator* arena) {
          return arena->Alloc(sizeof(ArenaBitVector::Iterator),
                              ArenaAllocator::kAllocGrowableBitMap);
        };
        static void operator delete(void* p) {}  // Nop.

      private:
        ArenaBitVector* const p_bits_;
        uint32_t* const bit_storage_;
        uint32_t bit_index_;              // Current index (size in bits).
        const uint32_t bit_size_;       // Size of vector in bits.
    };

    ArenaBitVector(ArenaAllocator* arena, unsigned int start_bits, bool expandable,
                   OatBitMapKind kind = kBitMapMisc);
    ~ArenaBitVector() {}

    static void* operator new(size_t size, ArenaAllocator* arena) {
      return arena->Alloc(sizeof(ArenaBitVector), ArenaAllocator::kAllocGrowableBitMap);
    }
    static void operator delete(void* p) {}  // Nop.

    void SetBit(unsigned int num);
    void ClearBit(unsigned int num);
    void MarkAllBits(bool set);
    void DebugBitVector(char* msg, int length);
    bool IsBitSet(unsigned int num);
    void ClearAllBits();
    void SetInitialBits(unsigned int num_bits);
    void Copy(ArenaBitVector* src);
    void Intersect(const ArenaBitVector* src2);
    void Union(const ArenaBitVector* src);
    // Are we equal to another bit vector?  Note: expandability attributes must also match.
    bool Equal(const ArenaBitVector* src) {
      return (storage_size_ == src->GetStorageSize()) &&
        (expandable_ == src->IsExpandable()) &&
        (memcmp(storage_, src->GetRawStorage(), storage_size_ * 4) == 0);
    }
    int NumSetBits();

    uint32_t GetStorageSize() const { return storage_size_; }
    bool IsExpandable() const { return expandable_; }
    uint32_t GetRawStorageWord(size_t idx) const { return storage_[idx]; }
    uint32_t* GetRawStorage() { return storage_; }
    const uint32_t* GetRawStorage() const { return storage_; }

  private:
    ArenaAllocator* const arena_;
    const bool expandable_;         // expand bitmap if we run out?
    const OatBitMapKind kind_;      // for memory use tuning.
    uint32_t   storage_size_;       // current size, in 32-bit words.
    uint32_t*  storage_;
};


}  // namespace art

#endif  // ART_COMPILER_DEX_ARENA_BIT_VECTOR_H_
