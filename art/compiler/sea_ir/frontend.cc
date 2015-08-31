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

#ifdef ART_SEA_IR_MODE
#include <llvm/Support/Threading.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Bitcode/ReaderWriter.h>

#include "base/logging.h"
#include "llvm/llvm_compilation_unit.h"
#include "dex/portable/mir_to_gbc.h"
#include "driver/compiler_driver.h"
#include "verifier/method_verifier.h"
#include "mirror/object.h"
#include "utils.h"

#include "runtime.h"
#include "safe_map.h"

#include "sea_ir/ir/sea.h"
#include "sea_ir/debug/dot_gen.h"
#include "sea_ir/types/types.h"
#include "sea_ir/code_gen/code_gen.h"

namespace art {

static CompiledMethod* CompileMethodWithSeaIr(CompilerDriver& compiler,
                                     const CompilerBackend compiler_backend,
                                     const DexFile::CodeItem* code_item,
                                     uint32_t method_access_flags, InvokeType invoke_type,
                                     uint16_t class_def_idx, uint32_t method_idx,
                                     jobject class_loader, const DexFile& dex_file
#if defined(ART_USE_PORTABLE_COMPILER)
                                     , llvm::LlvmCompilationUnit* llvm_compilation_unit
#endif
) {
  LOG(INFO) << "Compiling " << PrettyMethod(method_idx, dex_file) << ".";
  sea_ir::SeaGraph* ir_graph = sea_ir::SeaGraph::GetGraph(dex_file);
  std::string symbol = "dex_" + MangleForJni(PrettyMethod(method_idx, dex_file));
  sea_ir::CodeGenData* llvm_data = ir_graph->CompileMethod(symbol,
          code_item, class_def_idx, method_idx, method_access_flags, dex_file);
  sea_ir::DotConversion dc;
  SafeMap<int, const sea_ir::Type*>*  types = ir_graph->ti_->GetTypeMap();
  dc.DumpSea(ir_graph, "/tmp/temp.dot", types);
  MethodReference mref(&dex_file, method_idx);
  std::string llvm_code = llvm_data->GetElf(compiler.GetInstructionSet());
  CompiledMethod* compiled_method =
      new CompiledMethod(compiler, compiler.GetInstructionSet(), llvm_code,
                         *verifier::MethodVerifier::GetDexGcMap(mref), symbol);
  LOG(INFO) << "Compiled SEA IR method " << PrettyMethod(method_idx, dex_file) << ".";
  return compiled_method;
}

CompiledMethod* SeaIrCompileOneMethod(CompilerDriver& compiler,
                                 const CompilerBackend backend,
                                 const DexFile::CodeItem* code_item,
                                 uint32_t method_access_flags,
                                 InvokeType invoke_type,
                                 uint16_t class_def_idx,
                                 uint32_t method_idx,
                                 jobject class_loader,
                                 const DexFile& dex_file,
                                 llvm::LlvmCompilationUnit* llvm_compilation_unit) {
  return CompileMethodWithSeaIr(compiler, backend, code_item, method_access_flags, invoke_type,
      class_def_idx, method_idx, class_loader, dex_file
#if defined(ART_USE_PORTABLE_COMPILER)
                       , llvm_compilation_unit
#endif
                       );  // NOLINT
}

extern "C" art::CompiledMethod*
    SeaIrCompileMethod(art::CompilerDriver& compiler,
                          const art::DexFile::CodeItem* code_item,
                          uint32_t method_access_flags, art::InvokeType invoke_type,
                          uint16_t class_def_idx, uint32_t method_idx, jobject class_loader,
                          const art::DexFile& dex_file) {
  // TODO: Check method fingerprint here to determine appropriate backend type.
  //       Until then, use build default
  art::CompilerBackend backend = compiler.GetCompilerBackend();
  return art::SeaIrCompileOneMethod(compiler, backend, code_item, method_access_flags, invoke_type,
                               class_def_idx, method_idx, class_loader, dex_file,
                               NULL /* use thread llvm_info */);
}
#endif

}  // namespace art
