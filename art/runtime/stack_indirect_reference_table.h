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

#ifndef ART_RUNTIME_STACK_INDIRECT_REFERENCE_TABLE_H_
#define ART_RUNTIME_STACK_INDIRECT_REFERENCE_TABLE_H_

#include "base/logging.h"
#include "base/macros.h"

namespace art {
namespace mirror {
class Object;
}
class Thread;

// Stack allocated indirect reference table. It can allocated within
// the bridge frame between managed and native code backed by stack
// storage or manually allocated by SirtRef to hold one reference.
class StackIndirectReferenceTable {
 public:
  explicit StackIndirectReferenceTable(mirror::Object* object) :
      number_of_references_(1), link_(NULL) {
    references_[0] = object;
  }

  ~StackIndirectReferenceTable() {}

  // Number of references contained within this SIRT
  size_t NumberOfReferences() const {
    return number_of_references_;
  }

  // Link to previous SIRT or NULL
  StackIndirectReferenceTable* GetLink() const {
    return link_;
  }

  void SetLink(StackIndirectReferenceTable* sirt) {
    DCHECK_NE(this, sirt);
    link_ = sirt;
  }

  mirror::Object* GetReference(size_t i) const {
    DCHECK_LT(i, number_of_references_);
    return references_[i];
  }

  void SetReference(size_t i, mirror::Object* object) {
    DCHECK_LT(i, number_of_references_);
    references_[i] = object;
  }

  bool Contains(mirror::Object** sirt_entry) const {
    // A SIRT should always contain something. One created by the
    // jni_compiler should have a jobject/jclass as a native method is
    // passed in a this pointer or a class
    DCHECK_GT(number_of_references_, 0U);
    return ((&references_[0] <= sirt_entry)
            && (sirt_entry <= (&references_[number_of_references_ - 1])));
  }

  // Offset of length within SIRT, used by generated code
  static size_t NumberOfReferencesOffset() {
    return OFFSETOF_MEMBER(StackIndirectReferenceTable, number_of_references_);
  }

  // Offset of link within SIRT, used by generated code
  static size_t LinkOffset() {
    return OFFSETOF_MEMBER(StackIndirectReferenceTable, link_);
  }

 private:
  StackIndirectReferenceTable() {}

  size_t number_of_references_;
  StackIndirectReferenceTable* link_;

  // number_of_references_ are available if this is allocated and filled in by jni_compiler.
  mirror::Object* references_[1];

  DISALLOW_COPY_AND_ASSIGN(StackIndirectReferenceTable);
};

}  // namespace art

#endif  // ART_RUNTIME_STACK_INDIRECT_REFERENCE_TABLE_H_
