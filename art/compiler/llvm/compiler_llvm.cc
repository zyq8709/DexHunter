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

#include "compiler_llvm.h"

#include "backend_options.h"
#include "base/stl_util.h"
#include "class_linker.h"
#include "compiled_method.h"
#include "driver/compiler_driver.h"
#include "driver/dex_compilation_unit.h"
#include "globals.h"
#include "ir_builder.h"
#include "jni/portable/jni_compiler.h"
#include "llvm_compilation_unit.h"
#include "utils_llvm.h"
#include "verifier/method_verifier.h"

#include <llvm/LinkAllPasses.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Threading.h>

namespace art {
void CompileOneMethod(CompilerDriver& driver,
                      const CompilerBackend compilerBackend,
                      const DexFile::CodeItem* code_item,
                      uint32_t access_flags, InvokeType invoke_type,
                      uint16_t class_def_idx, uint32_t method_idx, jobject class_loader,
                      const DexFile& dex_file,
                      llvm::LlvmCompilationUnit* llvm_info);
}

namespace llvm {
  extern bool TimePassesIsEnabled;
}

namespace {

pthread_once_t llvm_initialized = PTHREAD_ONCE_INIT;

void InitializeLLVM() {
  // Initialize LLVM internal data structure for multithreading
  llvm::llvm_start_multithreaded();

  // NOTE: Uncomment following line to show the time consumption of LLVM passes
  // llvm::TimePassesIsEnabled = true;

  // Initialize LLVM target-specific options.
  art::llvm::InitialBackendOptions();

  // Initialize LLVM target, MC subsystem, asm printer, and asm parser.
  if (art::kIsTargetBuild) {
    // Don't initialize all targets on device. Just initialize the device's native target
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
  } else {
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();
  }

  // Initialize LLVM optimization passes
  llvm::PassRegistry &registry = *llvm::PassRegistry::getPassRegistry();

  llvm::initializeCore(registry);
  llvm::initializeScalarOpts(registry);
  llvm::initializeIPO(registry);
  llvm::initializeAnalysis(registry);
  llvm::initializeIPA(registry);
  llvm::initializeTransformUtils(registry);
  llvm::initializeInstCombine(registry);
  llvm::initializeInstrumentation(registry);
  llvm::initializeTarget(registry);
}

// The Guard to Shutdown LLVM
// llvm::llvm_shutdown_obj llvm_guard;
// TODO: We are commenting out this line because this will cause SEGV from
// time to time.
// Two reasons: (1) the order of the destruction of static objects, or
//              (2) dlopen/dlclose side-effect on static objects.

}  // anonymous namespace


namespace art {
namespace llvm {


::llvm::Module* makeLLVMModuleContents(::llvm::Module* module);


CompilerLLVM::CompilerLLVM(CompilerDriver* driver, InstructionSet insn_set)
    : compiler_driver_(driver), insn_set_(insn_set),
      next_cunit_id_lock_("compilation unit id lock"), next_cunit_id_(1) {

  // Initialize LLVM libraries
  pthread_once(&llvm_initialized, InitializeLLVM);
}


CompilerLLVM::~CompilerLLVM() {
}


LlvmCompilationUnit* CompilerLLVM::AllocateCompilationUnit() {
  MutexLock GUARD(Thread::Current(), next_cunit_id_lock_);
  LlvmCompilationUnit* cunit = new LlvmCompilationUnit(this, next_cunit_id_++);
  if (!bitcode_filename_.empty()) {
    cunit->SetBitcodeFileName(StringPrintf("%s-%zu",
                                           bitcode_filename_.c_str(),
                                           cunit->GetCompilationUnitId()));
  }
  return cunit;
}


CompiledMethod* CompilerLLVM::
CompileDexMethod(DexCompilationUnit* dex_compilation_unit, InvokeType invoke_type) {
  UniquePtr<LlvmCompilationUnit> cunit(AllocateCompilationUnit());

  cunit->SetDexCompilationUnit(dex_compilation_unit);
  cunit->SetCompilerDriver(compiler_driver_);
  // TODO: consolidate ArtCompileMethods
  CompileOneMethod(*compiler_driver_,
                   kPortable,
                   dex_compilation_unit->GetCodeItem(),
                   dex_compilation_unit->GetAccessFlags(),
                   invoke_type,
                   dex_compilation_unit->GetClassDefIndex(),
                   dex_compilation_unit->GetDexMethodIndex(),
                   dex_compilation_unit->GetClassLoader(),
                   *dex_compilation_unit->GetDexFile(),
                   cunit.get());

  cunit->Materialize();

  MethodReference mref(dex_compilation_unit->GetDexFile(),
                       dex_compilation_unit->GetDexMethodIndex());
  return new CompiledMethod(*compiler_driver_, compiler_driver_->GetInstructionSet(),
                            cunit->GetElfObject(), *verifier::MethodVerifier::GetDexGcMap(mref),
                            cunit->GetDexCompilationUnit()->GetSymbol());
}


CompiledMethod* CompilerLLVM::
CompileNativeMethod(DexCompilationUnit* dex_compilation_unit) {
  UniquePtr<LlvmCompilationUnit> cunit(AllocateCompilationUnit());

  UniquePtr<JniCompiler> jni_compiler(
      new JniCompiler(cunit.get(), *compiler_driver_, dex_compilation_unit));

  return jni_compiler->Compile();
}


}  // namespace llvm
}  // namespace art

inline static art::llvm::CompilerLLVM* ContextOf(art::CompilerDriver& driver) {
  void *compiler_context = driver.GetCompilerContext();
  CHECK(compiler_context != NULL);
  return reinterpret_cast<art::llvm::CompilerLLVM*>(compiler_context);
}

inline static const art::llvm::CompilerLLVM* ContextOf(const art::CompilerDriver& driver) {
  void *compiler_context = driver.GetCompilerContext();
  CHECK(compiler_context != NULL);
  return reinterpret_cast<const art::llvm::CompilerLLVM*>(compiler_context);
}

extern "C" void ArtInitCompilerContext(art::CompilerDriver& driver) {
  CHECK(driver.GetCompilerContext() == NULL);

  art::llvm::CompilerLLVM* compiler_llvm = new art::llvm::CompilerLLVM(&driver,
                                                                       driver.GetInstructionSet());

  driver.SetCompilerContext(compiler_llvm);
}

extern "C" void ArtUnInitCompilerContext(art::CompilerDriver& driver) {
  delete ContextOf(driver);
  driver.SetCompilerContext(NULL);
}
extern "C" art::CompiledMethod* ArtCompileMethod(art::CompilerDriver& driver,
                                                 const art::DexFile::CodeItem* code_item,
                                                 uint32_t access_flags,
                                                 art::InvokeType invoke_type,
                                                 uint16_t class_def_idx,
                                                 uint32_t method_idx,
                                                 jobject class_loader,
                                                 const art::DexFile& dex_file) {
  UNUSED(class_def_idx);  // TODO: this is used with Compiler::RequiresConstructorBarrier.
  art::ClassLinker *class_linker = art::Runtime::Current()->GetClassLinker();

  art::DexCompilationUnit dex_compilation_unit(
    NULL, class_loader, class_linker, dex_file, code_item,
    class_def_idx, method_idx, access_flags);
  art::llvm::CompilerLLVM* compiler_llvm = ContextOf(driver);
  art::CompiledMethod* result = compiler_llvm->CompileDexMethod(&dex_compilation_unit, invoke_type);
  return result;
}

extern "C" art::CompiledMethod* ArtLLVMJniCompileMethod(art::CompilerDriver& driver,
                                                        uint32_t access_flags, uint32_t method_idx,
                                                        const art::DexFile& dex_file) {
  art::ClassLinker *class_linker = art::Runtime::Current()->GetClassLinker();

  art::DexCompilationUnit dex_compilation_unit(
    NULL, NULL, class_linker, dex_file, NULL,
    0, method_idx, access_flags);

  art::llvm::CompilerLLVM* compiler_llvm = ContextOf(driver);
  art::CompiledMethod* result = compiler_llvm->CompileNativeMethod(&dex_compilation_unit);
  return result;
}

extern "C" void compilerLLVMSetBitcodeFileName(art::CompilerDriver& driver,
                                               std::string const& filename) {
  ContextOf(driver)->SetBitcodeFileName(filename);
}
