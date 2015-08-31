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

#include <string>
#include <llvm/PassManager.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include "base/logging.h"
#include "driver/compiler_driver.h"
#include "sea_ir/ir/sea.h"
#include "sea_ir/code_gen/code_gen.h"


namespace sea_ir {
std::string CodeGenData::GetElf(art::InstructionSet instruction_set) {
  std::string elf;
  ::llvm::raw_string_ostream out_stream(elf);
  // Lookup the LLVM target
  std::string target_triple;
  std::string target_cpu;
  std::string target_attr;
  art::CompilerDriver::InstructionSetToLLVMTarget(instruction_set,
      target_triple, target_cpu, target_attr);

  std::string errmsg;
  const ::llvm::Target* target =
    ::llvm::TargetRegistry::lookupTarget(target_triple, errmsg);

  CHECK(target != NULL) << errmsg;

  // Target options
  ::llvm::TargetOptions target_options;
  target_options.FloatABIType = ::llvm::FloatABI::Soft;
  target_options.NoFramePointerElim = true;
  target_options.NoFramePointerElimNonLeaf = true;
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
  ::llvm::FunctionPassManager fpm(&module_);
  fpm.add(new ::llvm::DataLayout(*data_layout));

  // Add optimization pass
  ::llvm::PassManagerBuilder pm_builder;
  // TODO: Use inliner after we can do IPO.
  pm_builder.Inliner = NULL;
  // pm_builder.Inliner = ::llvm::createFunctionInliningPass();
  // pm_builder.Inliner = ::llvm::createAlwaysInlinerPass();
  // pm_builder.Inliner = ::llvm::createPartialInliningPass();
  pm_builder.OptLevel = 3;
  pm_builder.DisableSimplifyLibCalls = 1;
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
    }

    // Run the code generation passes
    pm.run(module_);
  }
  return elf;
}
}  // namespace sea_ir
