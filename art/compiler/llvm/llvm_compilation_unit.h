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

#ifndef ART_COMPILER_LLVM_LLVM_COMPILATION_UNIT_H_
#define ART_COMPILER_LLVM_LLVM_COMPILATION_UNIT_H_

#include "base/logging.h"
#include "base/mutex.h"
#include "dex/compiler_internals.h"
#include "driver/compiler_driver.h"
#include "driver/dex_compilation_unit.h"
#include "globals.h"
#include "instruction_set.h"
#include "runtime_support_builder.h"
#include "runtime_support_llvm_func.h"
#include "safe_map.h"

#include <UniquePtr.h>
#include <string>
#include <vector>

namespace art {
  class CompiledMethod;
}

namespace llvm {
  class Function;
  class LLVMContext;
  class Module;
  class raw_ostream;
}

namespace art {
namespace llvm {

class CompilerLLVM;
class IRBuilder;

class LlvmCompilationUnit {
 public:
  ~LlvmCompilationUnit();

  uint32_t GetCompilationUnitId() const {
    return cunit_id_;
  }

  InstructionSet GetInstructionSet() const;

  ::llvm::LLVMContext* GetLLVMContext() const {
    return context_.get();
  }

  ::llvm::Module* GetModule() const {
    return module_;
  }

  IRBuilder* GetIRBuilder() const {
    return irb_.get();
  }

  void SetBitcodeFileName(const std::string& bitcode_filename) {
    bitcode_filename_ = bitcode_filename;
  }

  LLVMInfo* GetQuickContext() const {
    return llvm_info_.get();
  }
  void SetCompilerDriver(CompilerDriver* driver) {
    driver_ = driver;
  }
  DexCompilationUnit* GetDexCompilationUnit() {
    return dex_compilation_unit_;
  }
  void SetDexCompilationUnit(DexCompilationUnit* dex_compilation_unit) {
    dex_compilation_unit_ = dex_compilation_unit;
  }

  bool Materialize();

  bool IsMaterialized() const {
    return !elf_object_.empty();
  }

  const std::string& GetElfObject() const {
    DCHECK(IsMaterialized());
    return elf_object_;
  }

 private:
  LlvmCompilationUnit(const CompilerLLVM* compiler_llvm,
                      uint32_t cunit_id);

  const CompilerLLVM* compiler_llvm_;
  const uint32_t cunit_id_;

  UniquePtr< ::llvm::LLVMContext> context_;
  UniquePtr<IRBuilder> irb_;
  UniquePtr<RuntimeSupportBuilder> runtime_support_;
  ::llvm::Module* module_;  // Managed by context_
  UniquePtr<IntrinsicHelper> intrinsic_helper_;
  UniquePtr<LLVMInfo> llvm_info_;
  CompilerDriver* driver_;
  DexCompilationUnit* dex_compilation_unit_;

  std::string bitcode_filename_;

  std::string elf_object_;

  SafeMap<const ::llvm::Function*, CompiledMethod*> compiled_methods_map_;

  void CheckCodeAlign(uint32_t offset) const;

  void DumpBitcodeToFile();
  void DumpBitcodeToString(std::string& str_buffer);

  bool MaterializeToString(std::string& str_buffer);
  bool MaterializeToRawOStream(::llvm::raw_ostream& out_stream);

  friend class CompilerLLVM;  // For LlvmCompilationUnit constructor
};

}  // namespace llvm
}  // namespace art

#endif  // ART_COMPILER_LLVM_LLVM_COMPILATION_UNIT_H_
