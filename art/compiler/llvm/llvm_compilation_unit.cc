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

// TODO: TargetLibraryInfo is included before sys/... because on Android bionic does #define tricks like:
//
// #define  stat64    stat
// #define  fstat64   fstat
// #define  lstat64   lstat
//
// which causes grief. bionic probably should not do that.
#include <llvm/Target/TargetLibraryInfo.h>

#include "llvm_compilation_unit.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

#include <llvm/ADT/OwningPtr.h>
#include <llvm/ADT/StringSet.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/CallGraphSCCPass.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/Analysis/RegionPass.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/CodeGen/MachineFrameInfo.h>
#include <llvm/CodeGen/MachineFunction.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/DebugInfo.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/PassManager.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/ELF.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/PassNameParser.h>
#include <llvm/Support/PluginLoader.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/SystemUtils.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/system_error.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Scalar.h>

#include "base/logging.h"
#include "base/unix_file/fd_file.h"
#include "compiled_method.h"
#include "compiler_llvm.h"
#include "instruction_set.h"
#include "ir_builder.h"
#include "os.h"
#include "runtime_support_builder_arm.h"
#include "runtime_support_builder_thumb2.h"
#include "runtime_support_builder_x86.h"
#include "utils_llvm.h"

namespace art {
namespace llvm {

::llvm::FunctionPass*
CreateGBCExpanderPass(const IntrinsicHelper& intrinsic_helper, IRBuilder& irb,
                      CompilerDriver* compiler, const DexCompilationUnit* dex_compilation_unit);

::llvm::Module* makeLLVMModuleContents(::llvm::Module* module);


LlvmCompilationUnit::LlvmCompilationUnit(const CompilerLLVM* compiler_llvm, size_t cunit_id)
    : compiler_llvm_(compiler_llvm), cunit_id_(cunit_id) {
  driver_ = NULL;
  dex_compilation_unit_ = NULL;
  llvm_info_.reset(new LLVMInfo());
  context_.reset(llvm_info_->GetLLVMContext());
  module_ = llvm_info_->GetLLVMModule();

  // Include the runtime function declaration
  makeLLVMModuleContents(module_);


  intrinsic_helper_.reset(new IntrinsicHelper(*context_, *module_));

  // Create IRBuilder
  irb_.reset(new IRBuilder(*context_, *module_, *intrinsic_helper_));

  // We always need a switch case, so just use a normal function.
  switch (GetInstructionSet()) {
  default:
    runtime_support_.reset(new RuntimeSupportBuilder(*context_, *module_, *irb_));
    break;
  case kArm:
    runtime_support_.reset(new RuntimeSupportBuilderARM(*context_, *module_, *irb_));
    break;
  case kThumb2:
    runtime_support_.reset(new RuntimeSupportBuilderThumb2(*context_, *module_, *irb_));
    break;
  case kX86:
    runtime_support_.reset(new RuntimeSupportBuilderX86(*context_, *module_, *irb_));
    break;
  }

  irb_->SetRuntimeSupport(runtime_support_.get());
}


LlvmCompilationUnit::~LlvmCompilationUnit() {
  ::llvm::LLVMContext* llvm_context = context_.release();  // Managed by llvm_info_
  CHECK(llvm_context != NULL);
}


InstructionSet LlvmCompilationUnit::GetInstructionSet() const {
  return compiler_llvm_->GetInstructionSet();
}


static std::string DumpDirectory() {
  if (kIsTargetBuild) {
    return GetDalvikCacheOrDie(GetAndroidData());
  }
  return "/tmp";
}

void LlvmCompilationUnit::DumpBitcodeToFile() {
  std::string bitcode;
  DumpBitcodeToString(bitcode);
  std::string filename(StringPrintf("%s/Art%u.bc", DumpDirectory().c_str(), cunit_id_));
  UniquePtr<File> output(OS::CreateEmptyFile(filename.c_str()));
  output->WriteFully(bitcode.data(), bitcode.size());
  LOG(INFO) << ".bc file written successfully: " << filename;
}

void LlvmCompilationUnit::DumpBitcodeToString(std::string& str_buffer) {
  ::llvm::raw_string_ostream str_os(str_buffer);
  ::llvm::WriteBitcodeToFile(module_, str_os);
}

bool LlvmCompilationUnit::Materialize() {
  const bool kDumpBitcode = false;
  if (kDumpBitcode) {
    // Dump the bitcode for debugging
    DumpBitcodeToFile();
  }

  // Compile and prelink ::llvm::Module
  if (!MaterializeToString(elf_object_)) {
    LOG(ERROR) << "Failed to materialize compilation unit " << cunit_id_;
    return false;
  }

  const bool kDumpELF = false;
  if (kDumpELF) {
    // Dump the ELF image for debugging
    std::string filename(StringPrintf("%s/Art%u.o", DumpDirectory().c_str(), cunit_id_));
    UniquePtr<File> output(OS::CreateEmptyFile(filename.c_str()));
    output->WriteFully(elf_object_.data(), elf_object_.size());
    LOG(INFO) << ".o file written successfully: " << filename;
  }

  return true;
}


bool LlvmCompilationUnit::MaterializeToString(std::string& str_buffer) {
  ::llvm::raw_string_ostream str_os(str_buffer);
  return MaterializeToRawOStream(str_os);
}


bool LlvmCompilationUnit::MaterializeToRawOStream(::llvm::raw_ostream& out_stream) {
  // Lookup the LLVM target
  std::string target_triple;
  std::string target_cpu;
  std::string target_attr;
  CompilerDriver::InstructionSetToLLVMTarget(GetInstructionSet(), target_triple, target_cpu, target_attr);

  std::string errmsg;
  const ::llvm::Target* target =
    ::llvm::TargetRegistry::lookupTarget(target_triple, errmsg);

  CHECK(target != NULL) << errmsg;

  // Target options
  ::llvm::TargetOptions target_options;
  target_options.FloatABIType = ::llvm::FloatABI::Soft;
  target_options.NoFramePointerElim = true;
  target_options.UseSoftFloat = false;
  target_options.EnableFastISel = false;

  // Create the ::llvm::TargetMachine
  ::llvm::OwningPtr< ::llvm::TargetMachine> target_machine(
    target->createTargetMachine(target_triple, target_cpu, target_attr, target_options,
                                ::llvm::Reloc::Static, ::llvm::CodeModel::Small,
                                ::llvm::CodeGenOpt::Aggressive));

  CHECK(target_machine.get() != NULL) << "Failed to create target machine";

  // Add target data
  const ::llvm::DataLayout* data_layout = target_machine->getDataLayout();

  // PassManager for code generation passes
  ::llvm::PassManager pm;
  pm.add(new ::llvm::DataLayout(*data_layout));

  // FunctionPassManager for optimization pass
  ::llvm::FunctionPassManager fpm(module_);
  fpm.add(new ::llvm::DataLayout(*data_layout));

  if (bitcode_filename_.empty()) {
    // If we don't need write the bitcode to file, add the AddSuspendCheckToLoopLatchPass to the
    // regular FunctionPass.
    fpm.add(CreateGBCExpanderPass(*llvm_info_->GetIntrinsicHelper(), *irb_.get(),
                                  driver_, dex_compilation_unit_));
  } else {
    ::llvm::FunctionPassManager fpm2(module_);
    fpm2.add(CreateGBCExpanderPass(*llvm_info_->GetIntrinsicHelper(), *irb_.get(),
                                   driver_, dex_compilation_unit_));
    fpm2.doInitialization();
    for (::llvm::Module::iterator F = module_->begin(), E = module_->end();
         F != E; ++F) {
      fpm2.run(*F);
    }
    fpm2.doFinalization();

    // Write bitcode to file
    std::string errmsg;

    ::llvm::OwningPtr< ::llvm::tool_output_file> out_file(
      new ::llvm::tool_output_file(bitcode_filename_.c_str(), errmsg,
                                 ::llvm::sys::fs::F_Binary));


    if (!errmsg.empty()) {
      LOG(ERROR) << "Failed to create bitcode output file: " << errmsg;
      return false;
    }

    ::llvm::WriteBitcodeToFile(module_, out_file->os());
    out_file->keep();
  }

  // Add optimization pass
  ::llvm::PassManagerBuilder pm_builder;
  // TODO: Use inliner after we can do IPO.
  pm_builder.Inliner = NULL;
  // pm_builder.Inliner = ::llvm::createFunctionInliningPass();
  // pm_builder.Inliner = ::llvm::createAlwaysInlinerPass();
  // pm_builder.Inliner = ::llvm::createPartialInliningPass();
  pm_builder.OptLevel = 3;
  pm_builder.DisableUnitAtATime = 1;
  pm_builder.populateFunctionPassManager(fpm);
  pm_builder.populateModulePassManager(pm);
  pm.add(::llvm::createStripDeadPrototypesPass());

  // Add passes to emit ELF image
  {
    ::llvm::formatted_raw_ostream formatted_os(out_stream, false);

    // Ask the target to add backend passes as necessary.
    if (target_machine->addPassesToEmitFile(pm,
                                            formatted_os,
                                            ::llvm::TargetMachine::CGFT_ObjectFile,
                                            true)) {
      LOG(FATAL) << "Unable to generate ELF for this target";
      return false;
    }

    // Run the per-function optimization
    fpm.doInitialization();
    for (::llvm::Module::iterator F = module_->begin(), E = module_->end();
         F != E; ++F) {
      fpm.run(*F);
    }
    fpm.doFinalization();

    // Run the code generation passes
    pm.run(*module_);
  }

  return true;
}

// Check whether the align is less than or equal to the code alignment of
// that architecture.  Since the Oat writer only guarantee that the compiled
// method being aligned to kArchAlignment, we have no way to align the ELf
// section if the section alignment is greater than kArchAlignment.
void LlvmCompilationUnit::CheckCodeAlign(uint32_t align) const {
  InstructionSet insn_set = GetInstructionSet();
  switch (insn_set) {
  case kThumb2:
  case kArm:
    CHECK_LE(align, static_cast<uint32_t>(kArmAlignment));
    break;

  case kX86:
    CHECK_LE(align, static_cast<uint32_t>(kX86Alignment));
    break;

  case kMips:
    CHECK_LE(align, static_cast<uint32_t>(kMipsAlignment));
    break;

  default:
    LOG(FATAL) << "Unknown instruction set: " << insn_set;
  }
}


}  // namespace llvm
}  // namespace art
