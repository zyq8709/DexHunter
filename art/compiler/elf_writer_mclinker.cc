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

#include "elf_writer_mclinker.h"

#include <llvm/Support/TargetSelect.h>

#include <mcld/Environment.h>
#include <mcld/IRBuilder.h>
#include <mcld/Linker.h>
#include <mcld/LinkerConfig.h>
#include <mcld/MC/ZOption.h>
#include <mcld/Module.h>
#include <mcld/Support/Path.h>
#include <mcld/Support/TargetSelect.h>

#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "dex_method_iterator.h"
#include "driver/compiler_driver.h"
#include "elf_file.h"
#include "globals.h"
#include "mirror/art_method.h"
#include "mirror/art_method-inl.h"
#include "mirror/object-inl.h"
#include "oat_writer.h"
#include "scoped_thread_state_change.h"
#include "vector_output_stream.h"

namespace art {

ElfWriterMclinker::ElfWriterMclinker(const CompilerDriver& driver, File* elf_file)
  : ElfWriter(driver, elf_file), oat_input_(NULL) {}

ElfWriterMclinker::~ElfWriterMclinker() {}

bool ElfWriterMclinker::Create(File* elf_file,
                               OatWriter& oat_writer,
                               const std::vector<const DexFile*>& dex_files,
                               const std::string& android_root,
                               bool is_host,
                               const CompilerDriver& driver) {
  ElfWriterMclinker elf_writer(driver, elf_file);
  return elf_writer.Write(oat_writer, dex_files, android_root, is_host);
}

bool ElfWriterMclinker::Write(OatWriter& oat_writer,
                              const std::vector<const DexFile*>& dex_files,
                              const std::string& android_root,
                              bool is_host) {
  std::vector<uint8_t> oat_contents;
  oat_contents.reserve(oat_writer.GetSize());
  VectorOutputStream output_stream("oat contents", oat_contents);
  CHECK(oat_writer.Write(output_stream));
  CHECK_EQ(oat_writer.GetSize(), oat_contents.size());

  Init();
  AddOatInput(oat_contents);
#if defined(ART_USE_PORTABLE_COMPILER)
  AddMethodInputs(dex_files);
  AddRuntimeInputs(android_root, is_host);
#endif
  if (!Link()) {
    return false;
  }
  oat_contents.clear();
#if defined(ART_USE_PORTABLE_COMPILER)
  FixupOatMethodOffsets(dex_files);
#endif
  return true;
}

static void InitializeLLVM() {
  // TODO: this is lifted from art's compiler_llvm.cc, should be factored out
  if (kIsTargetBuild) {
    llvm::InitializeNativeTarget();
    // TODO: odd that there is no InitializeNativeTargetMC?
  } else {
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
  }
}

void ElfWriterMclinker::Init() {
  std::string target_triple;
  std::string target_cpu;
  std::string target_attr;
  CompilerDriver::InstructionSetToLLVMTarget(compiler_driver_->GetInstructionSet(),
                                             target_triple,
                                             target_cpu,
                                             target_attr);

  // Based on mclinker's llvm-mcld.cpp main() and LinkerTest
  //
  // TODO: LinkerTest uses mcld::Initialize(), but it does an
  // llvm::InitializeAllTargets, which we don't want. Basically we
  // want mcld::InitializeNative, but it doesn't exist yet, so we
  // inline the minimal we need here.
  InitializeLLVM();
  mcld::InitializeAllTargets();
  mcld::InitializeAllLinkers();
  mcld::InitializeAllEmulations();
  mcld::InitializeAllDiagnostics();

  linker_config_.reset(new mcld::LinkerConfig(target_triple));
  CHECK(linker_config_.get() != NULL);
  linker_config_->setCodeGenType(mcld::LinkerConfig::DynObj);
  linker_config_->options().setSOName(elf_file_->GetPath());

  // error on undefined symbols.
  // TODO: should this just be set if kIsDebugBuild?
  linker_config_->options().setNoUndefined(true);

  if (compiler_driver_->GetInstructionSet() == kMips) {
     // MCLinker defaults MIPS section alignment to 0x10000, not
     // 0x1000.  The ABI says this is because the max page size is
     // general is 64k but that isn't true on Android.
     mcld::ZOption z_option;
     z_option.setKind(mcld::ZOption::MaxPageSize);
     z_option.setPageSize(kPageSize);
     linker_config_->options().addZOption(z_option);
  }

  // TODO: Wire up mcld DiagnosticEngine to LOG?
  linker_config_->options().setColor(false);
  if (false) {
    // enables some tracing of input file processing
    linker_config_->options().setTrace(true);
  }

  // Based on alone::Linker::config
  module_.reset(new mcld::Module(linker_config_->options().soname()));
  CHECK(module_.get() != NULL);
  ir_builder_.reset(new mcld::IRBuilder(*module_.get(), *linker_config_.get()));
  CHECK(ir_builder_.get() != NULL);
  linker_.reset(new mcld::Linker());
  CHECK(linker_.get() != NULL);
  linker_->config(*linker_config_.get());
}

void ElfWriterMclinker::AddOatInput(std::vector<uint8_t>& oat_contents) {
  // Add an artificial memory input. Based on LinkerTest.
  UniquePtr<OatFile> oat_file(OatFile::OpenMemory(oat_contents, elf_file_->GetPath()));
  CHECK(oat_file.get() != NULL) << elf_file_->GetPath();

  const char* oat_data_start = reinterpret_cast<const char*>(&oat_file->GetOatHeader());
  const size_t oat_data_length = oat_file->GetOatHeader().GetExecutableOffset();
  const char* oat_code_start = oat_data_start + oat_data_length;
  const size_t oat_code_length = oat_file->Size() - oat_data_length;

  // TODO: ownership of oat_input?
  oat_input_ = ir_builder_->CreateInput("oat contents",
                                        mcld::sys::fs::Path("oat contents path"),
                                        mcld::Input::Object);
  CHECK(oat_input_ != NULL);

  // TODO: ownership of null_section?
  mcld::LDSection* null_section = ir_builder_->CreateELFHeader(*oat_input_,
                                                               "",
                                                               mcld::LDFileFormat::Null,
                                                               llvm::ELF::SHT_NULL,
                                                               0);
  CHECK(null_section != NULL);

  // TODO: we should split readonly data from readonly executable
  // code like .oat does.  We need to control section layout with
  // linker script like functionality to guarantee references
  // between sections maintain relative position which isn't
  // possible right now with the mclinker APIs.
  CHECK(oat_code_start != NULL);

  // we need to ensure that oatdata is page aligned so when we
  // fixup the segment load addresses, they remain page aligned.
  uint32_t alignment = kPageSize;

  // TODO: ownership of text_section?
  mcld::LDSection* text_section = ir_builder_->CreateELFHeader(*oat_input_,
                                                               ".text",
                                                               llvm::ELF::SHT_PROGBITS,
                                                               llvm::ELF::SHF_EXECINSTR
                                                               | llvm::ELF::SHF_ALLOC,
                                                               alignment);
  CHECK(text_section != NULL);

  mcld::SectionData* text_sectiondata = ir_builder_->CreateSectionData(*text_section);
  CHECK(text_sectiondata != NULL);

  // TODO: why does IRBuilder::CreateRegion take a non-const pointer?
  mcld::Fragment* text_fragment = ir_builder_->CreateRegion(const_cast<char*>(oat_data_start),
                                                            oat_file->Size());
  CHECK(text_fragment != NULL);
  ir_builder_->AppendFragment(*text_fragment, *text_sectiondata);

  ir_builder_->AddSymbol(*oat_input_,
                         "oatdata",
                         mcld::ResolveInfo::Object,
                         mcld::ResolveInfo::Define,
                         mcld::ResolveInfo::Global,
                         oat_data_length,  // size
                         0,                // offset
                         text_section);

  ir_builder_->AddSymbol(*oat_input_,
                         "oatexec",
                         mcld::ResolveInfo::Function,
                         mcld::ResolveInfo::Define,
                         mcld::ResolveInfo::Global,
                         oat_code_length,  // size
                         oat_data_length,  // offset
                         text_section);

  ir_builder_->AddSymbol(*oat_input_,
                         "oatlastword",
                         mcld::ResolveInfo::Object,
                         mcld::ResolveInfo::Define,
                         mcld::ResolveInfo::Global,
                         0,                // size
                         // subtract a word so symbol is within section
                         (oat_data_length + oat_code_length) - sizeof(uint32_t),  // offset
                         text_section);
}

#if defined(ART_USE_PORTABLE_COMPILER)
void ElfWriterMclinker::AddMethodInputs(const std::vector<const DexFile*>& dex_files) {
  DCHECK(oat_input_ != NULL);

  DexMethodIterator it(dex_files);
  while (it.HasNext()) {
    const DexFile& dex_file = it.GetDexFile();
    uint32_t method_idx = it.GetMemberIndex();
    const CompiledMethod* compiled_method =
      compiler_driver_->GetCompiledMethod(MethodReference(&dex_file, method_idx));
    if (compiled_method != NULL) {
      AddCompiledCodeInput(*compiled_method);
    }
    it.Next();
  }
  added_symbols_.clear();
}

void ElfWriterMclinker::AddCompiledCodeInput(const CompiledCode& compiled_code) {
  // Check if we've seen this compiled code before. If so skip
  // it. This can happen for reused code such as invoke stubs.
  const std::string& symbol = compiled_code.GetSymbol();
  SafeMap<const std::string*, const std::string*>::iterator it = added_symbols_.find(&symbol);
  if (it != added_symbols_.end()) {
    return;
  }
  added_symbols_.Put(&symbol, &symbol);

  // Add input to supply code for symbol
  const std::vector<uint8_t>& code = compiled_code.GetCode();
  // TODO: ownership of code_input?
  // TODO: why does IRBuilder::ReadInput take a non-const pointer?
  mcld::Input* code_input = ir_builder_->ReadInput(symbol,
                                                   const_cast<uint8_t*>(&code[0]),
                                                   code.size());
  CHECK(code_input != NULL);
}

void ElfWriterMclinker::AddRuntimeInputs(const std::string& android_root, bool is_host) {
  std::string libart_so(android_root);
  libart_so += kIsDebugBuild ? "/lib/libartd.so" : "/lib/libart.so";
  // TODO: ownership of libart_so_input?
  mcld::Input* libart_so_input = ir_builder_->ReadInput(libart_so, libart_so);
  CHECK(libart_so_input != NULL);

  std::string host_prebuilt_dir("prebuilts/gcc/linux-x86/host/i686-linux-glibc2.7-4.6");

  std::string compiler_runtime_lib;
  if (is_host) {
    compiler_runtime_lib += host_prebuilt_dir;
    compiler_runtime_lib += "/lib/gcc/i686-linux/4.6.x-google/libgcc.a";
  } else {
    compiler_runtime_lib += android_root;
    compiler_runtime_lib += "/lib/libcompiler_rt.a";
  }
  // TODO: ownership of compiler_runtime_lib_input?
  mcld::Input* compiler_runtime_lib_input = ir_builder_->ReadInput(compiler_runtime_lib,
                                                                   compiler_runtime_lib);
  CHECK(compiler_runtime_lib_input != NULL);

  std::string libc_lib;
  if (is_host) {
    libc_lib += host_prebuilt_dir;
    libc_lib += "/sysroot/usr/lib/libc.so.6";
  } else {
    libc_lib += android_root;
    libc_lib += "/lib/libc.so";
  }
  // TODO: ownership of libc_lib_input?
  mcld::Input* libc_lib_input_input = ir_builder_->ReadInput(libc_lib, libc_lib);
  CHECK(libc_lib_input_input != NULL);

  std::string libm_lib;
  if (is_host) {
    libm_lib += host_prebuilt_dir;
    libm_lib += "/sysroot/usr/lib/libm.so";
  } else {
    libm_lib += android_root;
    libm_lib += "/lib/libm.so";
  }
  // TODO: ownership of libm_lib_input?
  mcld::Input* libm_lib_input_input = ir_builder_->ReadInput(libm_lib, libm_lib);
  CHECK(libm_lib_input_input != NULL);
}
#endif

bool ElfWriterMclinker::Link() {
  // link inputs
  if (!linker_->link(*module_.get(), *ir_builder_.get())) {
    LOG(ERROR) << "Failed to link " << elf_file_->GetPath();
    return false;
  }

  // emit linked output
  // TODO: avoid dup of fd by fixing Linker::emit to not close the argument fd.
  int fd = dup(elf_file_->Fd());
  if (fd == -1) {
    PLOG(ERROR) << "Failed to dup file descriptor for " << elf_file_->GetPath();
    return false;
  }
  if (!linker_->emit(fd)) {
    LOG(ERROR) << "Failed to emit " << elf_file_->GetPath();
    return false;
  }
  mcld::Finalize();
  LOG(INFO) << "ELF file written successfully: " << elf_file_->GetPath();
  return true;
}

#if defined(ART_USE_PORTABLE_COMPILER)
void ElfWriterMclinker::FixupOatMethodOffsets(const std::vector<const DexFile*>& dex_files) {
  UniquePtr<ElfFile> elf_file(ElfFile::Open(elf_file_, true, false));
  CHECK(elf_file.get() != NULL) << elf_file_->GetPath();

  llvm::ELF::Elf32_Addr oatdata_address = GetOatDataAddress(elf_file.get());
  DexMethodIterator it(dex_files);
  while (it.HasNext()) {
    const DexFile& dex_file = it.GetDexFile();
    uint32_t method_idx = it.GetMemberIndex();
    InvokeType invoke_type = it.GetInvokeType();
    mirror::ArtMethod* method = NULL;
    if (compiler_driver_->IsImage()) {
      ClassLinker* linker = Runtime::Current()->GetClassLinker();
      mirror::DexCache* dex_cache = linker->FindDexCache(dex_file);
      // Unchecked as we hold mutator_lock_ on entry.
      ScopedObjectAccessUnchecked soa(Thread::Current());
      method = linker->ResolveMethod(dex_file, method_idx, dex_cache, NULL, NULL, invoke_type);
      CHECK(method != NULL);
    }
    const CompiledMethod* compiled_method =
      compiler_driver_->GetCompiledMethod(MethodReference(&dex_file, method_idx));
    if (compiled_method != NULL) {
      uint32_t offset = FixupCompiledCodeOffset(*elf_file.get(), oatdata_address, *compiled_method);
      // Don't overwrite static method trampoline
      if (method != NULL &&
          (!method->IsStatic() ||
           method->IsConstructor() ||
           method->GetDeclaringClass()->IsInitialized())) {
        method->SetOatCodeOffset(offset);
      }
    }
    it.Next();
  }
  symbol_to_compiled_code_offset_.clear();
}

uint32_t ElfWriterMclinker::FixupCompiledCodeOffset(ElfFile& elf_file,
                                                    llvm::ELF::Elf32_Addr oatdata_address,
                                                    const CompiledCode& compiled_code) {
  const std::string& symbol = compiled_code.GetSymbol();
  SafeMap<const std::string*, uint32_t>::iterator it = symbol_to_compiled_code_offset_.find(&symbol);
  if (it != symbol_to_compiled_code_offset_.end()) {
    return it->second;
  }

  llvm::ELF::Elf32_Addr compiled_code_address = elf_file.FindSymbolAddress(llvm::ELF::SHT_SYMTAB,
                                                                           symbol,
                                                                           true);
  CHECK_NE(0U, compiled_code_address) << symbol;
  CHECK_LT(oatdata_address, compiled_code_address) << symbol;
  uint32_t compiled_code_offset = compiled_code_address - oatdata_address;
  symbol_to_compiled_code_offset_.Put(&symbol, compiled_code_offset);

  const std::vector<uint32_t>& offsets = compiled_code.GetOatdataOffsetsToCompliledCodeOffset();
  for (uint32_t i = 0; i < offsets.size(); i++) {
    uint32_t oatdata_offset = oatdata_address + offsets[i];
    uint32_t* addr = reinterpret_cast<uint32_t*>(elf_file.Begin() + oatdata_offset);
    *addr = compiled_code_offset;
  }
  return compiled_code_offset;
}
#endif

}  // namespace art
