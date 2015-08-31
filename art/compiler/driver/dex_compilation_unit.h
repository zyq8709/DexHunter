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

#ifndef ART_COMPILER_DRIVER_DEX_COMPILATION_UNIT_H_
#define ART_COMPILER_DRIVER_DEX_COMPILATION_UNIT_H_

#include <stdint.h>

#include "dex_file.h"
#include "jni.h"

namespace art {
namespace mirror {
class ClassLoader;
class DexCache;
}  // namespace mirror
class ClassLinker;
struct CompilationUnit;

class DexCompilationUnit {
 public:
  explicit DexCompilationUnit(CompilationUnit* cu);

  DexCompilationUnit(CompilationUnit* cu, jobject class_loader, ClassLinker* class_linker,
                     const DexFile& dex_file, const DexFile::CodeItem* code_item,
                     uint16_t class_def_idx, uint32_t method_idx, uint32_t access_flags);

  CompilationUnit* GetCompilationUnit() const {
    return cu_;
  }

  jobject GetClassLoader() const {
    return class_loader_;
  }

  ClassLinker* GetClassLinker() const {
    return class_linker_;
  }

  const DexFile* GetDexFile() const {
    return dex_file_;
  }

  uint16_t GetClassDefIndex() const {
    return class_def_idx_;
  }

  uint32_t GetDexMethodIndex() const {
    return dex_method_idx_;
  }

  const DexFile::CodeItem* GetCodeItem() const {
    return code_item_;
  }

  const char* GetShorty() const {
    const DexFile::MethodId& method_id = dex_file_->GetMethodId(dex_method_idx_);
    return dex_file_->GetMethodShorty(method_id);
  }

  const char* GetShorty(uint32_t* shorty_len) const {
    const DexFile::MethodId& method_id = dex_file_->GetMethodId(dex_method_idx_);
    return dex_file_->GetMethodShorty(method_id, shorty_len);
  }

  uint32_t GetAccessFlags() const {
    return access_flags_;
  }

  bool IsConstructor() const {
    return ((access_flags_ & kAccConstructor) != 0);
  }

  bool IsNative() const {
    return ((access_flags_ & kAccNative) != 0);
  }

  bool IsStatic() const {
    return ((access_flags_ & kAccStatic) != 0);
  }

  bool IsSynchronized() const {
    return ((access_flags_ & kAccSynchronized) != 0);
  }

  const std::string& GetSymbol();

 private:
  CompilationUnit* const cu_;

  const jobject class_loader_;

  ClassLinker* const class_linker_;

  const DexFile* const dex_file_;

  const DexFile::CodeItem* const code_item_;
  const uint16_t class_def_idx_;
  const uint32_t dex_method_idx_;
  const uint32_t access_flags_;

  std::string symbol_;
};

}  // namespace art

#endif  // ART_COMPILER_DRIVER_DEX_COMPILATION_UNIT_H_
