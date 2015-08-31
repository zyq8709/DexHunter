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

#ifndef ART_COMPILER_LLVM_COMPILER_LLVM_H_
#define ART_COMPILER_LLVM_COMPILER_LLVM_H_

#include "base/macros.h"
#include "dex_file.h"
#include "driver/compiler_driver.h"
#include "instruction_set.h"
#include "mirror/object.h"

#include <UniquePtr.h>

#include <string>
#include <utility>
#include <vector>

namespace art {
  class CompiledMethod;
  class CompilerDriver;
  class DexCompilationUnit;
  namespace mirror {
    class ArtMethod;
    class ClassLoader;
  }  // namespace mirror
}  // namespace art


namespace llvm {
  class Function;
  class LLVMContext;
  class Module;
  class PointerType;
  class StructType;
  class Type;
}  // namespace llvm


namespace art {
namespace llvm {

class LlvmCompilationUnit;
class IRBuilder;

class CompilerLLVM {
 public:
  CompilerLLVM(CompilerDriver* driver, InstructionSet insn_set);

  ~CompilerLLVM();

  CompilerDriver* GetCompiler() const {
    return compiler_driver_;
  }

  InstructionSet GetInstructionSet() const {
    return insn_set_;
  }

  void SetBitcodeFileName(std::string const& filename) {
    bitcode_filename_ = filename;
  }

  CompiledMethod* CompileDexMethod(DexCompilationUnit* dex_compilation_unit,
                                   InvokeType invoke_type);

  CompiledMethod* CompileGBCMethod(DexCompilationUnit* dex_compilation_unit, std::string* func);

  CompiledMethod* CompileNativeMethod(DexCompilationUnit* dex_compilation_unit);

 private:
  LlvmCompilationUnit* AllocateCompilationUnit();

  CompilerDriver* const compiler_driver_;

  const InstructionSet insn_set_;

  Mutex next_cunit_id_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  size_t next_cunit_id_ GUARDED_BY(next_cunit_id_lock_);

  std::string bitcode_filename_;

  DISALLOW_COPY_AND_ASSIGN(CompilerLLVM);
};


}  // namespace llvm
}  // namespace art

#endif  // ART_COMPILER_LLVM_COMPILER_LLVM_H_
