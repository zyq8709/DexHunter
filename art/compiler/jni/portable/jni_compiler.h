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

#ifndef ART_COMPILER_JNI_PORTABLE_JNI_COMPILER_H_
#define ART_COMPILER_JNI_PORTABLE_JNI_COMPILER_H_

#include <stdint.h>

#include <string>

namespace art {
  class ClassLinker;
  class CompiledMethod;
  class CompilerDriver;
  class DexFile;
  class DexCompilationUnit;
  namespace mirror {
    class ArtMethod;
    class ClassLoader;
    class DexCache;
  }  // namespace mirror
}  // namespace art

namespace llvm {
  class AllocaInst;
  class Function;
  class FunctionType;
  class BasicBlock;
  class LLVMContext;
  class Module;
  class Type;
  class Value;
}  // namespace llvm

namespace art {
namespace llvm {

class LlvmCompilationUnit;
class IRBuilder;

class JniCompiler {
 public:
  JniCompiler(LlvmCompilationUnit* cunit,
              CompilerDriver& driver,
              const DexCompilationUnit* dex_compilation_unit);

  CompiledMethod* Compile();

 private:
  void CreateFunction(const std::string& symbol);

  ::llvm::FunctionType* GetFunctionType(uint32_t method_idx,
                                        bool is_static, bool is_target_function);

 private:
  LlvmCompilationUnit* cunit_;
  CompilerDriver* driver_;

  ::llvm::Module* module_;
  ::llvm::LLVMContext* context_;
  IRBuilder& irb_;

  const DexCompilationUnit* const dex_compilation_unit_;

  ::llvm::Function* func_;
  uint16_t elf_func_idx_;
};


}  // namespace llvm
}  // namespace art


#endif  // ART_COMPILER_JNI_PORTABLE_JNI_COMPILER_H_
