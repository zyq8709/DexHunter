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

#include "compiler_driver.h"

#define ATRACE_TAG ATRACE_TAG_DALVIK
#include <utils/Trace.h>

#include <vector>
#include <unistd.h>

#include "base/stl_util.h"
#include "base/timing_logger.h"
#include "class_linker.h"
#include "dex_compilation_unit.h"
#include "dex_file-inl.h"
#include "jni_internal.h"
#include "object_utils.h"
#include "runtime.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/heap_bitmap.h"
#include "gc/space/space.h"
#include "mirror/art_field-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class_loader.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/throwable.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "thread.h"
#include "thread_pool.h"
#include "trampolines/trampoline_compiler.h"
#include "verifier/method_verifier.h"

#if defined(ART_USE_PORTABLE_COMPILER)
#include "elf_writer_mclinker.h"
#else
#include "elf_writer_quick.h"
#endif

namespace art {

static double Percentage(size_t x, size_t y) {
  return 100.0 * (static_cast<double>(x)) / (static_cast<double>(x + y));
}

static void DumpStat(size_t x, size_t y, const char* str) {
  if (x == 0 && y == 0) {
    return;
  }
  VLOG(compiler) << Percentage(x, y) << "% of " << str << " for " << (x + y) << " cases";
}

class AOTCompilationStats {
 public:
  AOTCompilationStats()
      : stats_lock_("AOT compilation statistics lock"),
        types_in_dex_cache_(0), types_not_in_dex_cache_(0),
        strings_in_dex_cache_(0), strings_not_in_dex_cache_(0),
        resolved_types_(0), unresolved_types_(0),
        resolved_instance_fields_(0), unresolved_instance_fields_(0),
        resolved_local_static_fields_(0), resolved_static_fields_(0), unresolved_static_fields_(0),
        type_based_devirtualization_(0),
        safe_casts_(0), not_safe_casts_(0) {
    for (size_t i = 0; i <= kMaxInvokeType; i++) {
      resolved_methods_[i] = 0;
      unresolved_methods_[i] = 0;
      virtual_made_direct_[i] = 0;
      direct_calls_to_boot_[i] = 0;
      direct_methods_to_boot_[i] = 0;
    }
  }

  void Dump() {
    DumpStat(types_in_dex_cache_, types_not_in_dex_cache_, "types known to be in dex cache");
    DumpStat(strings_in_dex_cache_, strings_not_in_dex_cache_, "strings known to be in dex cache");
    DumpStat(resolved_types_, unresolved_types_, "types resolved");
    DumpStat(resolved_instance_fields_, unresolved_instance_fields_, "instance fields resolved");
    DumpStat(resolved_local_static_fields_ + resolved_static_fields_, unresolved_static_fields_,
             "static fields resolved");
    DumpStat(resolved_local_static_fields_, resolved_static_fields_ + unresolved_static_fields_,
             "static fields local to a class");
    DumpStat(safe_casts_, not_safe_casts_, "check-casts removed based on type information");
    // Note, the code below subtracts the stat value so that when added to the stat value we have
    // 100% of samples. TODO: clean this up.
    DumpStat(type_based_devirtualization_,
             resolved_methods_[kVirtual] + unresolved_methods_[kVirtual] +
             resolved_methods_[kInterface] + unresolved_methods_[kInterface] -
             type_based_devirtualization_,
             "virtual/interface calls made direct based on type information");

    for (size_t i = 0; i <= kMaxInvokeType; i++) {
      std::ostringstream oss;
      oss << static_cast<InvokeType>(i) << " methods were AOT resolved";
      DumpStat(resolved_methods_[i], unresolved_methods_[i], oss.str().c_str());
      if (virtual_made_direct_[i] > 0) {
        std::ostringstream oss2;
        oss2 << static_cast<InvokeType>(i) << " methods made direct";
        DumpStat(virtual_made_direct_[i],
                 resolved_methods_[i] + unresolved_methods_[i] - virtual_made_direct_[i],
                 oss2.str().c_str());
      }
      if (direct_calls_to_boot_[i] > 0) {
        std::ostringstream oss2;
        oss2 << static_cast<InvokeType>(i) << " method calls are direct into boot";
        DumpStat(direct_calls_to_boot_[i],
                 resolved_methods_[i] + unresolved_methods_[i] - direct_calls_to_boot_[i],
                 oss2.str().c_str());
      }
      if (direct_methods_to_boot_[i] > 0) {
        std::ostringstream oss2;
        oss2 << static_cast<InvokeType>(i) << " method calls have methods in boot";
        DumpStat(direct_methods_to_boot_[i],
                 resolved_methods_[i] + unresolved_methods_[i] - direct_methods_to_boot_[i],
                 oss2.str().c_str());
      }
    }
  }

// Allow lossy statistics in non-debug builds.
#ifndef NDEBUG
#define STATS_LOCK() MutexLock mu(Thread::Current(), stats_lock_)
#else
#define STATS_LOCK()
#endif

  void TypeInDexCache() {
    STATS_LOCK();
    types_in_dex_cache_++;
  }

  void TypeNotInDexCache() {
    STATS_LOCK();
    types_not_in_dex_cache_++;
  }

  void StringInDexCache() {
    STATS_LOCK();
    strings_in_dex_cache_++;
  }

  void StringNotInDexCache() {
    STATS_LOCK();
    strings_not_in_dex_cache_++;
  }

  void TypeDoesntNeedAccessCheck() {
    STATS_LOCK();
    resolved_types_++;
  }

  void TypeNeedsAccessCheck() {
    STATS_LOCK();
    unresolved_types_++;
  }

  void ResolvedInstanceField() {
    STATS_LOCK();
    resolved_instance_fields_++;
  }

  void UnresolvedInstanceField() {
    STATS_LOCK();
    unresolved_instance_fields_++;
  }

  void ResolvedLocalStaticField() {
    STATS_LOCK();
    resolved_local_static_fields_++;
  }

  void ResolvedStaticField() {
    STATS_LOCK();
    resolved_static_fields_++;
  }

  void UnresolvedStaticField() {
    STATS_LOCK();
    unresolved_static_fields_++;
  }

  // Indicate that type information from the verifier led to devirtualization.
  void PreciseTypeDevirtualization() {
    STATS_LOCK();
    type_based_devirtualization_++;
  }

  // Indicate that a method of the given type was resolved at compile time.
  void ResolvedMethod(InvokeType type) {
    DCHECK_LE(type, kMaxInvokeType);
    STATS_LOCK();
    resolved_methods_[type]++;
  }

  // Indicate that a method of the given type was unresolved at compile time as it was in an
  // unknown dex file.
  void UnresolvedMethod(InvokeType type) {
    DCHECK_LE(type, kMaxInvokeType);
    STATS_LOCK();
    unresolved_methods_[type]++;
  }

  // Indicate that a type of virtual method dispatch has been converted into a direct method
  // dispatch.
  void VirtualMadeDirect(InvokeType type) {
    DCHECK(type == kVirtual || type == kInterface || type == kSuper);
    STATS_LOCK();
    virtual_made_direct_[type]++;
  }

  // Indicate that a method of the given type was able to call directly into boot.
  void DirectCallsToBoot(InvokeType type) {
    DCHECK_LE(type, kMaxInvokeType);
    STATS_LOCK();
    direct_calls_to_boot_[type]++;
  }

  // Indicate that a method of the given type was able to be resolved directly from boot.
  void DirectMethodsToBoot(InvokeType type) {
    DCHECK_LE(type, kMaxInvokeType);
    STATS_LOCK();
    direct_methods_to_boot_[type]++;
  }

  // A check-cast could be eliminated due to verifier type analysis.
  void SafeCast() {
    STATS_LOCK();
    safe_casts_++;
  }

  // A check-cast couldn't be eliminated due to verifier type analysis.
  void NotASafeCast() {
    STATS_LOCK();
    not_safe_casts_++;
  }

 private:
  Mutex stats_lock_;

  size_t types_in_dex_cache_;
  size_t types_not_in_dex_cache_;

  size_t strings_in_dex_cache_;
  size_t strings_not_in_dex_cache_;

  size_t resolved_types_;
  size_t unresolved_types_;

  size_t resolved_instance_fields_;
  size_t unresolved_instance_fields_;

  size_t resolved_local_static_fields_;
  size_t resolved_static_fields_;
  size_t unresolved_static_fields_;
  // Type based devirtualization for invoke interface and virtual.
  size_t type_based_devirtualization_;

  size_t resolved_methods_[kMaxInvokeType + 1];
  size_t unresolved_methods_[kMaxInvokeType + 1];
  size_t virtual_made_direct_[kMaxInvokeType + 1];
  size_t direct_calls_to_boot_[kMaxInvokeType + 1];
  size_t direct_methods_to_boot_[kMaxInvokeType + 1];

  size_t safe_casts_;
  size_t not_safe_casts_;

  DISALLOW_COPY_AND_ASSIGN(AOTCompilationStats);
};

extern "C" void ArtInitCompilerContext(art::CompilerDriver& driver);
extern "C" void ArtInitQuickCompilerContext(art::CompilerDriver& compiler);

extern "C" void ArtUnInitCompilerContext(art::CompilerDriver& driver);
extern "C" void ArtUnInitQuickCompilerContext(art::CompilerDriver& compiler);

extern "C" art::CompiledMethod* ArtCompileMethod(art::CompilerDriver& driver,
                                                 const art::DexFile::CodeItem* code_item,
                                                 uint32_t access_flags,
                                                 art::InvokeType invoke_type,
                                                 uint16_t class_def_idx,
                                                 uint32_t method_idx,
                                                 jobject class_loader,
                                                 const art::DexFile& dex_file);
extern "C" art::CompiledMethod* ArtQuickCompileMethod(art::CompilerDriver& compiler,
                                                      const art::DexFile::CodeItem* code_item,
                                                      uint32_t access_flags,
                                                      art::InvokeType invoke_type,
                                                      uint16_t class_def_idx,
                                                      uint32_t method_idx,
                                                      jobject class_loader,
                                                      const art::DexFile& dex_file);

extern "C" art::CompiledMethod* ArtCompileDEX(art::CompilerDriver& compiler,
                                              const art::DexFile::CodeItem* code_item,
                                              uint32_t access_flags,
                                              art::InvokeType invoke_type,
                                              uint16_t class_def_idx,
                                              uint32_t method_idx,
                                              jobject class_loader,
                                              const art::DexFile& dex_file);
#ifdef ART_SEA_IR_MODE
extern "C" art::CompiledMethod* SeaIrCompileMethod(art::CompilerDriver& compiler,
                                                   const art::DexFile::CodeItem* code_item,
                                                   uint32_t access_flags,
                                                   art::InvokeType invoke_type,
                                                   uint16_t class_def_idx,
                                                   uint32_t method_idx,
                                                   jobject class_loader,
                                                   const art::DexFile& dex_file);
#endif
extern "C" art::CompiledMethod* ArtLLVMJniCompileMethod(art::CompilerDriver& driver,
                                                        uint32_t access_flags, uint32_t method_idx,
                                                        const art::DexFile& dex_file);

extern "C" art::CompiledMethod* ArtQuickJniCompileMethod(art::CompilerDriver& compiler,
                                                         uint32_t access_flags, uint32_t method_idx,
                                                         const art::DexFile& dex_file);

extern "C" void compilerLLVMSetBitcodeFileName(art::CompilerDriver& driver,
                                               std::string const& filename);

CompilerDriver::CompilerDriver(CompilerBackend compiler_backend, InstructionSet instruction_set,
                               bool image, DescriptorSet* image_classes, size_t thread_count,
                               bool dump_stats)
    : compiler_backend_(compiler_backend),
      instruction_set_(instruction_set),
      freezing_constructor_lock_("freezing constructor lock"),
      compiled_classes_lock_("compiled classes lock"),
      compiled_methods_lock_("compiled method lock"),
      image_(image),
      image_classes_(image_classes),
      thread_count_(thread_count),
      start_ns_(0),
      stats_(new AOTCompilationStats),
      dump_stats_(dump_stats),
      compiler_library_(NULL),
      compiler_(NULL),
      compiler_context_(NULL),
      jni_compiler_(NULL),
      compiler_enable_auto_elf_loading_(NULL),
      compiler_get_method_code_addr_(NULL),
      support_boot_image_fixup_(true) {

  CHECK_PTHREAD_CALL(pthread_key_create, (&tls_key_, NULL), "compiler tls key");

  // TODO: more work needed to combine initializations and allow per-method backend selection
  typedef void (*InitCompilerContextFn)(CompilerDriver&);
  InitCompilerContextFn init_compiler_context;
  if (compiler_backend_ == kPortable) {
    // Initialize compiler_context_
    init_compiler_context = reinterpret_cast<void (*)(CompilerDriver&)>(ArtInitCompilerContext);
    compiler_ = reinterpret_cast<CompilerFn>(ArtCompileMethod);
  } else {
    init_compiler_context = reinterpret_cast<void (*)(CompilerDriver&)>(ArtInitQuickCompilerContext);
    compiler_ = reinterpret_cast<CompilerFn>(ArtQuickCompileMethod);
  }

  dex_to_dex_compiler_ = reinterpret_cast<DexToDexCompilerFn>(ArtCompileDEX);

#ifdef ART_SEA_IR_MODE
  sea_ir_compiler_ = NULL;
  if (Runtime::Current()->IsSeaIRMode()) {
    sea_ir_compiler_ = reinterpret_cast<CompilerFn>(SeaIrCompileMethod);
  }
#endif

  init_compiler_context(*this);

  if (compiler_backend_ == kPortable) {
    jni_compiler_ = reinterpret_cast<JniCompilerFn>(ArtLLVMJniCompileMethod);
  } else {
    jni_compiler_ = reinterpret_cast<JniCompilerFn>(ArtQuickJniCompileMethod);
  }

  CHECK(!Runtime::Current()->IsStarted());
  if (!image_) {
    CHECK(image_classes_.get() == NULL);
  }
}

std::vector<uint8_t>* CompilerDriver::DeduplicateCode(const std::vector<uint8_t>& code) {
  return dedupe_code_.Add(Thread::Current(), code);
}

std::vector<uint8_t>* CompilerDriver::DeduplicateMappingTable(const std::vector<uint8_t>& code) {
  return dedupe_mapping_table_.Add(Thread::Current(), code);
}

std::vector<uint8_t>* CompilerDriver::DeduplicateVMapTable(const std::vector<uint8_t>& code) {
  return dedupe_vmap_table_.Add(Thread::Current(), code);
}

std::vector<uint8_t>* CompilerDriver::DeduplicateGCMap(const std::vector<uint8_t>& code) {
  return dedupe_gc_map_.Add(Thread::Current(), code);
}

CompilerDriver::~CompilerDriver() {
  Thread* self = Thread::Current();
  {
    MutexLock mu(self, compiled_classes_lock_);
    STLDeleteValues(&compiled_classes_);
  }
  {
    MutexLock mu(self, compiled_methods_lock_);
    STLDeleteValues(&compiled_methods_);
  }
  {
    MutexLock mu(self, compiled_methods_lock_);
    STLDeleteElements(&code_to_patch_);
  }
  {
    MutexLock mu(self, compiled_methods_lock_);
    STLDeleteElements(&methods_to_patch_);
  }
  CHECK_PTHREAD_CALL(pthread_key_delete, (tls_key_), "delete tls key");
  typedef void (*UninitCompilerContextFn)(CompilerDriver&);
  UninitCompilerContextFn uninit_compiler_context;
  // Uninitialize compiler_context_
  // TODO: rework to combine initialization/uninitialization
  if (compiler_backend_ == kPortable) {
    uninit_compiler_context = reinterpret_cast<void (*)(CompilerDriver&)>(ArtUnInitCompilerContext);
  } else {
    uninit_compiler_context = reinterpret_cast<void (*)(CompilerDriver&)>(ArtUnInitQuickCompilerContext);
  }
  uninit_compiler_context(*this);
}

CompilerTls* CompilerDriver::GetTls() {
  // Lazily create thread-local storage
  CompilerTls* res = static_cast<CompilerTls*>(pthread_getspecific(tls_key_));
  if (res == NULL) {
    res = new CompilerTls();
    CHECK_PTHREAD_CALL(pthread_setspecific, (tls_key_, res), "compiler tls");
  }
  return res;
}

const std::vector<uint8_t>* CompilerDriver::CreateInterpreterToInterpreterBridge() const {
  return CreateTrampoline(instruction_set_, kInterpreterAbi,
                          INTERPRETER_ENTRYPOINT_OFFSET(pInterpreterToInterpreterBridge));
}

const std::vector<uint8_t>* CompilerDriver::CreateInterpreterToCompiledCodeBridge() const {
  return CreateTrampoline(instruction_set_, kInterpreterAbi,
                          INTERPRETER_ENTRYPOINT_OFFSET(pInterpreterToCompiledCodeBridge));
}

const std::vector<uint8_t>* CompilerDriver::CreateJniDlsymLookup() const {
  return CreateTrampoline(instruction_set_, kJniAbi, JNI_ENTRYPOINT_OFFSET(pDlsymLookup));
}

const std::vector<uint8_t>* CompilerDriver::CreatePortableResolutionTrampoline() const {
  return CreateTrampoline(instruction_set_, kPortableAbi,
                          PORTABLE_ENTRYPOINT_OFFSET(pPortableResolutionTrampoline));
}

const std::vector<uint8_t>* CompilerDriver::CreatePortableToInterpreterBridge() const {
  return CreateTrampoline(instruction_set_, kPortableAbi,
                          PORTABLE_ENTRYPOINT_OFFSET(pPortableToInterpreterBridge));
}

const std::vector<uint8_t>* CompilerDriver::CreateQuickResolutionTrampoline() const {
  return CreateTrampoline(instruction_set_, kQuickAbi,
                          QUICK_ENTRYPOINT_OFFSET(pQuickResolutionTrampoline));
}

const std::vector<uint8_t>* CompilerDriver::CreateQuickToInterpreterBridge() const {
  return CreateTrampoline(instruction_set_, kQuickAbi,
                          QUICK_ENTRYPOINT_OFFSET(pQuickToInterpreterBridge));
}

void CompilerDriver::CompileAll(jobject class_loader,
                                const std::vector<const DexFile*>& dex_files,
                                base::TimingLogger& timings) {
  DCHECK(!Runtime::Current()->IsStarted());
  UniquePtr<ThreadPool> thread_pool(new ThreadPool(thread_count_ - 1));
  PreCompile(class_loader, dex_files, *thread_pool.get(), timings);
  Compile(class_loader, dex_files, *thread_pool.get(), timings);
  if (dump_stats_) {
    stats_->Dump();
  }
}

static DexToDexCompilationLevel GetDexToDexCompilationlevel(mirror::ClassLoader* class_loader,
                                                            const DexFile& dex_file,
                                                            const DexFile::ClassDef& class_def)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  const char* descriptor = dex_file.GetClassDescriptor(class_def);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  mirror::Class* klass = class_linker->FindClass(descriptor, class_loader);
  if (klass == NULL) {
    Thread* self = Thread::Current();
    CHECK(self->IsExceptionPending());
    self->ClearException();
    return kDontDexToDexCompile;
  }
  // The verifier can only run on "quick" instructions at runtime (see usage of
  // FindAccessedFieldAtDexPc and FindInvokedMethodAtDexPc in ThrowNullPointerExceptionFromDexPC
  // function). Since image classes can be verified again while compiling an application,
  // we must prevent the DEX-to-DEX compiler from introducing them.
  // TODO: find a way to enable "quick" instructions for image classes and remove this check.
  bool compiling_image_classes = (class_loader == NULL);
  if (compiling_image_classes) {
    return kRequired;
  } else if (klass->IsVerified()) {
    // Class is verified so we can enable DEX-to-DEX compilation for performance.
    return kOptimize;
  } else if (klass->IsCompileTimeVerified()) {
    // Class verification has soft-failed. Anyway, ensure at least correctness.
    DCHECK_EQ(klass->GetStatus(), mirror::Class::kStatusRetryVerificationAtRuntime);
    return kRequired;
  } else {
    // Class verification has failed: do not run DEX-to-DEX compilation.
    return kDontDexToDexCompile;
  }
}

void CompilerDriver::CompileOne(const mirror::ArtMethod* method, base::TimingLogger& timings) {
  DCHECK(!Runtime::Current()->IsStarted());
  Thread* self = Thread::Current();
  jobject jclass_loader;
  const DexFile* dex_file;
  uint16_t class_def_idx;
  {
    ScopedObjectAccessUnchecked soa(self);
    ScopedLocalRef<jobject>
      local_class_loader(soa.Env(),
                    soa.AddLocalReference<jobject>(method->GetDeclaringClass()->GetClassLoader()));
    jclass_loader = soa.Env()->NewGlobalRef(local_class_loader.get());
    // Find the dex_file
    MethodHelper mh(method);
    dex_file = &mh.GetDexFile();
    class_def_idx = mh.GetClassDefIndex();
  }
  self->TransitionFromRunnableToSuspended(kNative);

  std::vector<const DexFile*> dex_files;
  dex_files.push_back(dex_file);

  UniquePtr<ThreadPool> thread_pool(new ThreadPool(0U));
  PreCompile(jclass_loader, dex_files, *thread_pool.get(), timings);

  uint32_t method_idx = method->GetDexMethodIndex();
  const DexFile::CodeItem* code_item = dex_file->GetCodeItem(method->GetCodeItemOffset());
  // Can we run DEX-to-DEX compiler on this class ?
  DexToDexCompilationLevel dex_to_dex_compilation_level = kDontDexToDexCompile;
  {
    ScopedObjectAccess soa(Thread::Current());
    const DexFile::ClassDef& class_def = dex_file->GetClassDef(class_def_idx);
    mirror::ClassLoader* class_loader = soa.Decode<mirror::ClassLoader*>(jclass_loader);
    dex_to_dex_compilation_level = GetDexToDexCompilationlevel(class_loader, *dex_file, class_def);
  }
  CompileMethod(code_item, method->GetAccessFlags(), method->GetInvokeType(),
                class_def_idx, method_idx, jclass_loader, *dex_file, dex_to_dex_compilation_level);

  self->GetJniEnv()->DeleteGlobalRef(jclass_loader);

  self->TransitionFromSuspendedToRunnable();
}

void CompilerDriver::Resolve(jobject class_loader, const std::vector<const DexFile*>& dex_files,
                             ThreadPool& thread_pool, base::TimingLogger& timings) {
  for (size_t i = 0; i != dex_files.size(); ++i) {
    const DexFile* dex_file = dex_files[i];
    CHECK(dex_file != NULL);
    ResolveDexFile(class_loader, *dex_file, thread_pool, timings);
  }
}

void CompilerDriver::PreCompile(jobject class_loader, const std::vector<const DexFile*>& dex_files,
                                ThreadPool& thread_pool, base::TimingLogger& timings) {
  LoadImageClasses(timings);

  Resolve(class_loader, dex_files, thread_pool, timings);

  Verify(class_loader, dex_files, thread_pool, timings);

  InitializeClasses(class_loader, dex_files, thread_pool, timings);

  UpdateImageClasses(timings);
}

bool CompilerDriver::IsImageClass(const char* descriptor) const {
  DCHECK(descriptor != NULL);
  if (!IsImage()) {
    return true;
  } else {
    return image_classes_->find(descriptor) != image_classes_->end();
  }
}

static void ResolveExceptionsForMethod(MethodHelper* mh,
    std::set<std::pair<uint16_t, const DexFile*> >& exceptions_to_resolve)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  const DexFile::CodeItem* code_item = mh->GetCodeItem();
  if (code_item == NULL) {
    return;  // native or abstract method
  }
  if (code_item->tries_size_ == 0) {
    return;  // nothing to process
  }
  const byte* encoded_catch_handler_list = DexFile::GetCatchHandlerData(*code_item, 0);
  size_t num_encoded_catch_handlers = DecodeUnsignedLeb128(&encoded_catch_handler_list);
  for (size_t i = 0; i < num_encoded_catch_handlers; i++) {
    int32_t encoded_catch_handler_size = DecodeSignedLeb128(&encoded_catch_handler_list);
    bool has_catch_all = false;
    if (encoded_catch_handler_size <= 0) {
      encoded_catch_handler_size = -encoded_catch_handler_size;
      has_catch_all = true;
    }
    for (int32_t j = 0; j < encoded_catch_handler_size; j++) {
      uint16_t encoded_catch_handler_handlers_type_idx =
          DecodeUnsignedLeb128(&encoded_catch_handler_list);
      // Add to set of types to resolve if not already in the dex cache resolved types
      if (!mh->IsResolvedTypeIdx(encoded_catch_handler_handlers_type_idx)) {
        exceptions_to_resolve.insert(
            std::pair<uint16_t, const DexFile*>(encoded_catch_handler_handlers_type_idx,
                                                &mh->GetDexFile()));
      }
      // ignore address associated with catch handler
      DecodeUnsignedLeb128(&encoded_catch_handler_list);
    }
    if (has_catch_all) {
      // ignore catch all address
      DecodeUnsignedLeb128(&encoded_catch_handler_list);
    }
  }
}

static bool ResolveCatchBlockExceptionsClassVisitor(mirror::Class* c, void* arg)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::set<std::pair<uint16_t, const DexFile*> >* exceptions_to_resolve =
      reinterpret_cast<std::set<std::pair<uint16_t, const DexFile*> >*>(arg);
  MethodHelper mh;
  for (size_t i = 0; i < c->NumVirtualMethods(); ++i) {
    mirror::ArtMethod* m = c->GetVirtualMethod(i);
    mh.ChangeMethod(m);
    ResolveExceptionsForMethod(&mh, *exceptions_to_resolve);
  }
  for (size_t i = 0; i < c->NumDirectMethods(); ++i) {
    mirror::ArtMethod* m = c->GetDirectMethod(i);
    mh.ChangeMethod(m);
    ResolveExceptionsForMethod(&mh, *exceptions_to_resolve);
  }
  return true;
}

static bool RecordImageClassesVisitor(mirror::Class* klass, void* arg)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  CompilerDriver::DescriptorSet* image_classes =
      reinterpret_cast<CompilerDriver::DescriptorSet*>(arg);
  image_classes->insert(ClassHelper(klass).GetDescriptor());
  return true;
}

// Make a list of descriptors for classes to include in the image
void CompilerDriver::LoadImageClasses(base::TimingLogger& timings)
      LOCKS_EXCLUDED(Locks::mutator_lock_) {
  if (!IsImage()) {
    return;
  }

  timings.NewSplit("LoadImageClasses");
  // Make a first class to load all classes explicitly listed in the file
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  for (auto it = image_classes_->begin(), end = image_classes_->end(); it != end;) {
    std::string descriptor(*it);
    SirtRef<mirror::Class> klass(self, class_linker->FindSystemClass(descriptor.c_str()));
    if (klass.get() == NULL) {
      image_classes_->erase(it++);
      VLOG(compiler) << "Failed to find class " << descriptor;
      self->ClearException();
    } else {
      ++it;
    }
  }

  // Resolve exception classes referenced by the loaded classes. The catch logic assumes
  // exceptions are resolved by the verifier when there is a catch block in an interested method.
  // Do this here so that exception classes appear to have been specified image classes.
  std::set<std::pair<uint16_t, const DexFile*> > unresolved_exception_types;
  SirtRef<mirror::Class> java_lang_Throwable(self,
                                     class_linker->FindSystemClass("Ljava/lang/Throwable;"));
  do {
    unresolved_exception_types.clear();
    class_linker->VisitClasses(ResolveCatchBlockExceptionsClassVisitor,
                               &unresolved_exception_types);
    for (const std::pair<uint16_t, const DexFile*>& exception_type : unresolved_exception_types) {
      uint16_t exception_type_idx = exception_type.first;
      const DexFile* dex_file = exception_type.second;
      mirror::DexCache* dex_cache = class_linker->FindDexCache(*dex_file);
      mirror:: ClassLoader* class_loader = NULL;
      SirtRef<mirror::Class> klass(self, class_linker->ResolveType(*dex_file, exception_type_idx,
                                                                   dex_cache, class_loader));
      if (klass.get() == NULL) {
        const DexFile::TypeId& type_id = dex_file->GetTypeId(exception_type_idx);
        const char* descriptor = dex_file->GetTypeDescriptor(type_id);
        LOG(FATAL) << "Failed to resolve class " << descriptor;
      }
      DCHECK(java_lang_Throwable->IsAssignableFrom(klass.get()));
    }
    // Resolving exceptions may load classes that reference more exceptions, iterate until no
    // more are found
  } while (!unresolved_exception_types.empty());

  // We walk the roots looking for classes so that we'll pick up the
  // above classes plus any classes them depend on such super
  // classes, interfaces, and the required ClassLinker roots.
  class_linker->VisitClasses(RecordImageClassesVisitor, image_classes_.get());

  CHECK_NE(image_classes_->size(), 0U);
}

static void MaybeAddToImageClasses(mirror::Class* klass, CompilerDriver::DescriptorSet* image_classes)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  while (!klass->IsObjectClass()) {
    ClassHelper kh(klass);
    const char* descriptor = kh.GetDescriptor();
    std::pair<CompilerDriver::DescriptorSet::iterator, bool> result =
        image_classes->insert(descriptor);
    if (result.second) {
        VLOG(compiler) << "Adding " << descriptor << " to image classes";
    } else {
      return;
    }
    for (size_t i = 0; i < kh.NumDirectInterfaces(); ++i) {
      MaybeAddToImageClasses(kh.GetDirectInterface(i), image_classes);
    }
    if (klass->IsArrayClass()) {
      MaybeAddToImageClasses(klass->GetComponentType(), image_classes);
    }
    klass = klass->GetSuperClass();
  }
}

void CompilerDriver::FindClinitImageClassesCallback(mirror::Object* object, void* arg) {
  DCHECK(object != NULL);
  DCHECK(arg != NULL);
  CompilerDriver* compiler_driver = reinterpret_cast<CompilerDriver*>(arg);
  MaybeAddToImageClasses(object->GetClass(), compiler_driver->image_classes_.get());
}

void CompilerDriver::UpdateImageClasses(base::TimingLogger& timings) {
  if (IsImage()) {
    timings.NewSplit("UpdateImageClasses");

    // Update image_classes_ with classes for objects created by <clinit> methods.
    Thread* self = Thread::Current();
    const char* old_cause = self->StartAssertNoThreadSuspension("ImageWriter");
    gc::Heap* heap = Runtime::Current()->GetHeap();
    // TODO: Image spaces only?
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
    heap->FlushAllocStack();
    heap->GetLiveBitmap()->Walk(FindClinitImageClassesCallback, this);
    self->EndAssertNoThreadSuspension(old_cause);
  }
}

bool CompilerDriver::CanAssumeTypeIsPresentInDexCache(const DexFile& dex_file,
                                                      uint32_t type_idx) {
  if (IsImage() && IsImageClass(dex_file.GetTypeDescriptor(dex_file.GetTypeId(type_idx)))) {
    if (kIsDebugBuild) {
      ScopedObjectAccess soa(Thread::Current());
      mirror::DexCache* dex_cache = Runtime::Current()->GetClassLinker()->FindDexCache(dex_file);
      mirror::Class* resolved_class = dex_cache->GetResolvedType(type_idx);
      CHECK(resolved_class != NULL);
    }
    stats_->TypeInDexCache();
    return true;
  } else {
    stats_->TypeNotInDexCache();
    return false;
  }
}

bool CompilerDriver::CanAssumeStringIsPresentInDexCache(const DexFile& dex_file,
                                                        uint32_t string_idx) {
  // See also Compiler::ResolveDexFile

  bool result = false;
  if (IsImage()) {
    // We resolve all const-string strings when building for the image.
    ScopedObjectAccess soa(Thread::Current());
    mirror::DexCache* dex_cache = Runtime::Current()->GetClassLinker()->FindDexCache(dex_file);
    Runtime::Current()->GetClassLinker()->ResolveString(dex_file, string_idx, dex_cache);
    result = true;
  }
  if (result) {
    stats_->StringInDexCache();
  } else {
    stats_->StringNotInDexCache();
  }
  return result;
}

bool CompilerDriver::CanAccessTypeWithoutChecks(uint32_t referrer_idx, const DexFile& dex_file,
                                                uint32_t type_idx,
                                                bool* type_known_final, bool* type_known_abstract,
                                                bool* equals_referrers_class) {
  if (type_known_final != NULL) {
    *type_known_final = false;
  }
  if (type_known_abstract != NULL) {
    *type_known_abstract = false;
  }
  if (equals_referrers_class != NULL) {
    *equals_referrers_class = false;
  }
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = Runtime::Current()->GetClassLinker()->FindDexCache(dex_file);
  // Get type from dex cache assuming it was populated by the verifier
  mirror::Class* resolved_class = dex_cache->GetResolvedType(type_idx);
  if (resolved_class == NULL) {
    stats_->TypeNeedsAccessCheck();
    return false;  // Unknown class needs access checks.
  }
  const DexFile::MethodId& method_id = dex_file.GetMethodId(referrer_idx);
  if (equals_referrers_class != NULL) {
    *equals_referrers_class = (method_id.class_idx_ == type_idx);
  }
  mirror::Class* referrer_class = dex_cache->GetResolvedType(method_id.class_idx_);
  if (referrer_class == NULL) {
    stats_->TypeNeedsAccessCheck();
    return false;  // Incomplete referrer knowledge needs access check.
  }
  // Perform access check, will return true if access is ok or false if we're going to have to
  // check this at runtime (for example for class loaders).
  bool result = referrer_class->CanAccess(resolved_class);
  if (result) {
    stats_->TypeDoesntNeedAccessCheck();
    if (type_known_final != NULL) {
      *type_known_final = resolved_class->IsFinal() && !resolved_class->IsArrayClass();
    }
    if (type_known_abstract != NULL) {
      *type_known_abstract = resolved_class->IsAbstract() && !resolved_class->IsArrayClass();
    }
  } else {
    stats_->TypeNeedsAccessCheck();
  }
  return result;
}

bool CompilerDriver::CanAccessInstantiableTypeWithoutChecks(uint32_t referrer_idx,
                                                            const DexFile& dex_file,
                                                            uint32_t type_idx) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = Runtime::Current()->GetClassLinker()->FindDexCache(dex_file);
  // Get type from dex cache assuming it was populated by the verifier.
  mirror::Class* resolved_class = dex_cache->GetResolvedType(type_idx);
  if (resolved_class == NULL) {
    stats_->TypeNeedsAccessCheck();
    return false;  // Unknown class needs access checks.
  }
  const DexFile::MethodId& method_id = dex_file.GetMethodId(referrer_idx);
  mirror::Class* referrer_class = dex_cache->GetResolvedType(method_id.class_idx_);
  if (referrer_class == NULL) {
    stats_->TypeNeedsAccessCheck();
    return false;  // Incomplete referrer knowledge needs access check.
  }
  // Perform access and instantiable checks, will return true if access is ok or false if we're
  // going to have to check this at runtime (for example for class loaders).
  bool result = referrer_class->CanAccess(resolved_class) && resolved_class->IsInstantiable();
  if (result) {
    stats_->TypeDoesntNeedAccessCheck();
  } else {
    stats_->TypeNeedsAccessCheck();
  }
  return result;
}

static mirror::Class* ComputeCompilingMethodsClass(ScopedObjectAccess& soa,
                                                   mirror::DexCache* dex_cache,
                                                   const DexCompilationUnit* mUnit)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // The passed dex_cache is a hint, sanity check before asking the class linker that will take a
  // lock.
  if (dex_cache->GetDexFile() != mUnit->GetDexFile()) {
    dex_cache = mUnit->GetClassLinker()->FindDexCache(*mUnit->GetDexFile());
  }
  mirror::ClassLoader* class_loader = soa.Decode<mirror::ClassLoader*>(mUnit->GetClassLoader());
  const DexFile::MethodId& referrer_method_id = mUnit->GetDexFile()->GetMethodId(mUnit->GetDexMethodIndex());
  return mUnit->GetClassLinker()->ResolveType(*mUnit->GetDexFile(), referrer_method_id.class_idx_,
                                              dex_cache, class_loader);
}

static mirror::ArtField* ComputeFieldReferencedFromCompilingMethod(ScopedObjectAccess& soa,
                                                                const DexCompilationUnit* mUnit,
                                                                uint32_t field_idx)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::DexCache* dex_cache = mUnit->GetClassLinker()->FindDexCache(*mUnit->GetDexFile());
  mirror::ClassLoader* class_loader = soa.Decode<mirror::ClassLoader*>(mUnit->GetClassLoader());
  return mUnit->GetClassLinker()->ResolveField(*mUnit->GetDexFile(), field_idx, dex_cache,
                                               class_loader, false);
}

static mirror::ArtMethod* ComputeMethodReferencedFromCompilingMethod(ScopedObjectAccess& soa,
                                                                          const DexCompilationUnit* mUnit,
                                                                          uint32_t method_idx,
                                                                          InvokeType type)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::DexCache* dex_cache = mUnit->GetClassLinker()->FindDexCache(*mUnit->GetDexFile());
  mirror::ClassLoader* class_loader = soa.Decode<mirror::ClassLoader*>(mUnit->GetClassLoader());
  return mUnit->GetClassLinker()->ResolveMethod(*mUnit->GetDexFile(), method_idx, dex_cache,
                                                class_loader, NULL, type);
}

bool CompilerDriver::ComputeInstanceFieldInfo(uint32_t field_idx, const DexCompilationUnit* mUnit,
                                              int& field_offset, bool& is_volatile, bool is_put) {
  ScopedObjectAccess soa(Thread::Current());
  // Conservative defaults.
  field_offset = -1;
  is_volatile = true;
  // Try to resolve field and ignore if an Incompatible Class Change Error (ie is static).
  mirror::ArtField* resolved_field = ComputeFieldReferencedFromCompilingMethod(soa, mUnit, field_idx);
  if (resolved_field != NULL && !resolved_field->IsStatic()) {
    mirror::Class* referrer_class =
        ComputeCompilingMethodsClass(soa, resolved_field->GetDeclaringClass()->GetDexCache(),
                                     mUnit);
    if (referrer_class != NULL) {
      mirror::Class* fields_class = resolved_field->GetDeclaringClass();
      bool access_ok = referrer_class->CanAccess(fields_class) &&
                       referrer_class->CanAccessMember(fields_class,
                                                       resolved_field->GetAccessFlags());
      if (!access_ok) {
        // The referring class can't access the resolved field, this may occur as a result of a
        // protected field being made public by a sub-class. Resort to the dex file to determine
        // the correct class for the access check.
        const DexFile& dex_file = *referrer_class->GetDexCache()->GetDexFile();
        mirror::Class* dex_fields_class = mUnit->GetClassLinker()->ResolveType(dex_file,
                                                         dex_file.GetFieldId(field_idx).class_idx_,
                                                         referrer_class);
        access_ok = referrer_class->CanAccess(dex_fields_class) &&
                    referrer_class->CanAccessMember(dex_fields_class,
                                                    resolved_field->GetAccessFlags());
      }
      bool is_write_to_final_from_wrong_class = is_put && resolved_field->IsFinal() &&
          fields_class != referrer_class;
      if (access_ok && !is_write_to_final_from_wrong_class) {
        field_offset = resolved_field->GetOffset().Int32Value();
        is_volatile = resolved_field->IsVolatile();
        stats_->ResolvedInstanceField();
        return true;  // Fast path.
      }
    }
  }
  // Clean up any exception left by field/type resolution
  if (soa.Self()->IsExceptionPending()) {
    soa.Self()->ClearException();
  }
  stats_->UnresolvedInstanceField();
  return false;  // Incomplete knowledge needs slow path.
}

bool CompilerDriver::ComputeStaticFieldInfo(uint32_t field_idx, const DexCompilationUnit* mUnit,
                                            int& field_offset, int& ssb_index,
                                            bool& is_referrers_class, bool& is_volatile,
                                            bool is_put) {
  ScopedObjectAccess soa(Thread::Current());
  // Conservative defaults.
  field_offset = -1;
  ssb_index = -1;
  is_referrers_class = false;
  is_volatile = true;
  // Try to resolve field and ignore if an Incompatible Class Change Error (ie isn't static).
  mirror::ArtField* resolved_field = ComputeFieldReferencedFromCompilingMethod(soa, mUnit, field_idx);
  if (resolved_field != NULL && resolved_field->IsStatic()) {
    mirror::Class* referrer_class =
        ComputeCompilingMethodsClass(soa, resolved_field->GetDeclaringClass()->GetDexCache(),
                                     mUnit);
    if (referrer_class != NULL) {
      mirror::Class* fields_class = resolved_field->GetDeclaringClass();
      if (fields_class == referrer_class) {
        is_referrers_class = true;  // implies no worrying about class initialization
        field_offset = resolved_field->GetOffset().Int32Value();
        is_volatile = resolved_field->IsVolatile();
        stats_->ResolvedLocalStaticField();
        return true;  // fast path
      } else {
        bool access_ok = referrer_class->CanAccess(fields_class) &&
                         referrer_class->CanAccessMember(fields_class,
                                                         resolved_field->GetAccessFlags());
        if (!access_ok) {
          // The referring class can't access the resolved field, this may occur as a result of a
          // protected field being made public by a sub-class. Resort to the dex file to determine
          // the correct class for the access check. Don't change the field's class as that is
          // used to identify the SSB.
          const DexFile& dex_file = *referrer_class->GetDexCache()->GetDexFile();
          mirror::Class* dex_fields_class =
              mUnit->GetClassLinker()->ResolveType(dex_file,
                                                   dex_file.GetFieldId(field_idx).class_idx_,
                                                   referrer_class);
          access_ok = referrer_class->CanAccess(dex_fields_class) &&
                      referrer_class->CanAccessMember(dex_fields_class,
                                                      resolved_field->GetAccessFlags());
        }
        bool is_write_to_final_from_wrong_class = is_put && resolved_field->IsFinal();
        if (access_ok && !is_write_to_final_from_wrong_class) {
          // We have the resolved field, we must make it into a ssbIndex for the referrer
          // in its static storage base (which may fail if it doesn't have a slot for it)
          // TODO: for images we can elide the static storage base null check
          // if we know there's a non-null entry in the image
          mirror::DexCache* dex_cache = mUnit->GetClassLinker()->FindDexCache(*mUnit->GetDexFile());
          if (fields_class->GetDexCache() == dex_cache) {
            // common case where the dex cache of both the referrer and the field are the same,
            // no need to search the dex file
            ssb_index = fields_class->GetDexTypeIndex();
            field_offset = resolved_field->GetOffset().Int32Value();
            is_volatile = resolved_field->IsVolatile();
            stats_->ResolvedStaticField();
            return true;
          }
          // Search dex file for localized ssb index, may fail if field's class is a parent
          // of the class mentioned in the dex file and there is no dex cache entry.
          const DexFile::StringId* string_id =
              mUnit->GetDexFile()->FindStringId(FieldHelper(resolved_field).GetDeclaringClassDescriptor());
          if (string_id != NULL) {
            const DexFile::TypeId* type_id =
               mUnit->GetDexFile()->FindTypeId(mUnit->GetDexFile()->GetIndexForStringId(*string_id));
            if (type_id != NULL) {
              // medium path, needs check of static storage base being initialized
              ssb_index = mUnit->GetDexFile()->GetIndexForTypeId(*type_id);
              field_offset = resolved_field->GetOffset().Int32Value();
              is_volatile = resolved_field->IsVolatile();
              stats_->ResolvedStaticField();
              return true;
            }
          }
        }
      }
    }
  }
  // Clean up any exception left by field/type resolution
  if (soa.Self()->IsExceptionPending()) {
    soa.Self()->ClearException();
  }
  stats_->UnresolvedStaticField();
  return false;  // Incomplete knowledge needs slow path.
}

void CompilerDriver::GetCodeAndMethodForDirectCall(InvokeType type, InvokeType sharp_type,
                                                   mirror::Class* referrer_class,
                                                   mirror::ArtMethod* method,
                                                   uintptr_t& direct_code,
                                                   uintptr_t& direct_method,
                                                   bool update_stats) {
  // For direct and static methods compute possible direct_code and direct_method values, ie
  // an address for the Method* being invoked and an address of the code for that Method*.
  // For interface calls compute a value for direct_method that is the interface method being
  // invoked, so this can be passed to the out-of-line runtime support code.
  direct_code = 0;
  direct_method = 0;
  if (compiler_backend_ == kPortable) {
    if (sharp_type != kStatic && sharp_type != kDirect) {
      return;
    }
  } else {
    if (sharp_type != kStatic && sharp_type != kDirect && sharp_type != kInterface) {
      return;
    }
  }
  bool method_code_in_boot = method->GetDeclaringClass()->GetClassLoader() == NULL;
  if (!method_code_in_boot) {
    return;
  }
  bool has_clinit_trampoline = method->IsStatic() && !method->GetDeclaringClass()->IsInitialized();
  if (has_clinit_trampoline && (method->GetDeclaringClass() != referrer_class)) {
    // Ensure we run the clinit trampoline unless we are invoking a static method in the same class.
    return;
  }
  if (update_stats) {
    if (sharp_type != kInterface) {  // Interfaces always go via a trampoline.
      stats_->DirectCallsToBoot(type);
    }
    stats_->DirectMethodsToBoot(type);
  }
  bool compiling_boot = Runtime::Current()->GetHeap()->GetContinuousSpaces().size() == 1;
  if (compiling_boot) {
    if (support_boot_image_fixup_) {
      MethodHelper mh(method);
      if (IsImageClass(mh.GetDeclaringClassDescriptor())) {
        // We can only branch directly to Methods that are resolved in the DexCache.
        // Otherwise we won't invoke the resolution trampoline.
        direct_method = -1;
        direct_code = -1;
      }
    }
  } else {
    if (Runtime::Current()->GetHeap()->FindSpaceFromObject(method, false)->IsImageSpace()) {
      direct_method = reinterpret_cast<uintptr_t>(method);
    }
    direct_code = reinterpret_cast<uintptr_t>(method->GetEntryPointFromCompiledCode());
  }
}

bool CompilerDriver::ComputeInvokeInfo(const DexCompilationUnit* mUnit, const uint32_t dex_pc,
                                       InvokeType& invoke_type,
                                       MethodReference& target_method,
                                       int& vtable_idx,
                                       uintptr_t& direct_code, uintptr_t& direct_method,
                                       bool update_stats) {
  ScopedObjectAccess soa(Thread::Current());
  vtable_idx = -1;
  direct_code = 0;
  direct_method = 0;
  mirror::ArtMethod* resolved_method =
      ComputeMethodReferencedFromCompilingMethod(soa, mUnit, target_method.dex_method_index,
                                                 invoke_type);
  if (resolved_method != NULL) {
    // Don't try to fast-path if we don't understand the caller's class or this appears to be an
    // Incompatible Class Change Error.
    mirror::Class* referrer_class =
        ComputeCompilingMethodsClass(soa, resolved_method->GetDeclaringClass()->GetDexCache(),
                                     mUnit);
    bool icce = resolved_method->CheckIncompatibleClassChange(invoke_type);
    if (referrer_class != NULL && !icce) {
      mirror::Class* methods_class = resolved_method->GetDeclaringClass();
      if (!referrer_class->CanAccess(methods_class) ||
          !referrer_class->CanAccessMember(methods_class,
                                           resolved_method->GetAccessFlags())) {
        // The referring class can't access the resolved method, this may occur as a result of a
        // protected method being made public by implementing an interface that re-declares the
        // method public. Resort to the dex file to determine the correct class for the access
        // check.
        uint16_t class_idx =
            target_method.dex_file->GetMethodId(target_method.dex_method_index).class_idx_;
        methods_class = mUnit->GetClassLinker()->ResolveType(*target_method.dex_file,
                                                             class_idx, referrer_class);
      }
      if (referrer_class->CanAccess(methods_class) &&
          referrer_class->CanAccessMember(methods_class, resolved_method->GetAccessFlags())) {
        const bool kEnableFinalBasedSharpening = true;
        // Sharpen a virtual call into a direct call when the target is known not to have been
        // overridden (ie is final).
        bool can_sharpen_virtual_based_on_type =
            (invoke_type == kVirtual) && (resolved_method->IsFinal() || methods_class->IsFinal());
        // For invoke-super, ensure the vtable index will be correct to dispatch in the vtable of
        // the super class.
        bool can_sharpen_super_based_on_type = (invoke_type == kSuper) &&
            (referrer_class != methods_class) && referrer_class->IsSubClass(methods_class) &&
            resolved_method->GetMethodIndex() < methods_class->GetVTable()->GetLength() &&
            (methods_class->GetVTable()->Get(resolved_method->GetMethodIndex()) == resolved_method);

        if (kEnableFinalBasedSharpening && (can_sharpen_virtual_based_on_type ||
                                            can_sharpen_super_based_on_type)) {
          // Sharpen a virtual call into a direct call. The method_idx is into referrer's
          // dex cache, check that this resolved method is where we expect it.
          CHECK(referrer_class->GetDexCache()->GetResolvedMethod(target_method.dex_method_index) ==
                resolved_method) << PrettyMethod(resolved_method);
          if (update_stats) {
            stats_->ResolvedMethod(invoke_type);
            stats_->VirtualMadeDirect(invoke_type);
          }
          GetCodeAndMethodForDirectCall(invoke_type, kDirect, referrer_class, resolved_method,
                                        direct_code, direct_method, update_stats);
          invoke_type = kDirect;
          return true;
        }
        const bool kEnableVerifierBasedSharpening = true;
        if (kEnableVerifierBasedSharpening && (invoke_type == kVirtual ||
                                               invoke_type == kInterface)) {
          // Did the verifier record a more precise invoke target based on its type information?
          const MethodReference caller_method(mUnit->GetDexFile(), mUnit->GetDexMethodIndex());
          const MethodReference* devirt_map_target =
              verifier::MethodVerifier::GetDevirtMap(caller_method, dex_pc);
          if (devirt_map_target != NULL) {
            mirror::DexCache* target_dex_cache =
                mUnit->GetClassLinker()->FindDexCache(*devirt_map_target->dex_file);
            mirror::ClassLoader* class_loader =
                soa.Decode<mirror::ClassLoader*>(mUnit->GetClassLoader());
            mirror::ArtMethod* called_method =
                mUnit->GetClassLinker()->ResolveMethod(*devirt_map_target->dex_file,
                                                       devirt_map_target->dex_method_index,
                                                       target_dex_cache, class_loader, NULL,
                                                       kVirtual);
            CHECK(called_method != NULL);
            CHECK(!called_method->IsAbstract());
            GetCodeAndMethodForDirectCall(invoke_type, kDirect, referrer_class, called_method,
                                          direct_code, direct_method, update_stats);
            bool compiler_needs_dex_cache =
                (GetCompilerBackend() == kPortable) ||
                (GetCompilerBackend() == kQuick && instruction_set_ != kThumb2) ||
                (direct_code == 0) || (direct_code == static_cast<unsigned int>(-1)) ||
                (direct_method == 0) || (direct_method == static_cast<unsigned int>(-1));
            if ((devirt_map_target->dex_file != target_method.dex_file) &&
                compiler_needs_dex_cache) {
              // We need to use the dex cache to find either the method or code, and the dex file
              // containing the method isn't the one expected for the target method. Try to find
              // the method within the expected target dex file.
              // TODO: the -1 could be handled as direct code if the patching new the target dex
              //       file.
              // TODO: quick only supports direct pointers with Thumb2.
              // TODO: the following should be factored into a common helper routine to find
              //       one dex file's method within another.
              const DexFile* dexfile = target_method.dex_file;
              const DexFile* cm_dexfile =
                  called_method->GetDeclaringClass()->GetDexCache()->GetDexFile();
              const DexFile::MethodId& cm_method_id =
                  cm_dexfile->GetMethodId(called_method->GetDexMethodIndex());
              const char* cm_descriptor = cm_dexfile->StringByTypeIdx(cm_method_id.class_idx_);
              const DexFile::StringId* descriptor = dexfile->FindStringId(cm_descriptor);
              if (descriptor != NULL) {
                const DexFile::TypeId* type_id =
                    dexfile->FindTypeId(dexfile->GetIndexForStringId(*descriptor));
                if (type_id != NULL) {
                  const char* cm_name = cm_dexfile->GetMethodName(cm_method_id);
                  const DexFile::StringId* name = dexfile->FindStringId(cm_name);
                  if (name != NULL) {
                    uint16_t return_type_idx;
                    std::vector<uint16_t> param_type_idxs;
                    bool success = dexfile->CreateTypeList(&return_type_idx, &param_type_idxs,
                                                           cm_dexfile->GetMethodSignature(cm_method_id));
                    if (success) {
                      const DexFile::ProtoId* sig =
                          dexfile->FindProtoId(return_type_idx, param_type_idxs);
                      if (sig != NULL) {
                        const  DexFile::MethodId* method_id = dexfile->FindMethodId(*type_id,
                                                                                    *name, *sig);
                        if (method_id != NULL) {
                          if (update_stats) {
                            stats_->ResolvedMethod(invoke_type);
                            stats_->VirtualMadeDirect(invoke_type);
                            stats_->PreciseTypeDevirtualization();
                          }
                          target_method.dex_method_index = dexfile->GetIndexForMethodId(*method_id);
                          invoke_type = kDirect;
                          return true;
                        }
                      }
                    }
                  }
                }
              }
              // TODO: the stats for direct code and method are off as we failed to find the direct
              //       method in the referring method's dex cache/file.
            } else {
              if (update_stats) {
                stats_->ResolvedMethod(invoke_type);
                stats_->VirtualMadeDirect(invoke_type);
                stats_->PreciseTypeDevirtualization();
              }
              target_method = *devirt_map_target;
              invoke_type = kDirect;
              return true;
            }
          }
        }
        if (invoke_type == kSuper) {
          // Unsharpened super calls are suspicious so go slow-path.
        } else {
          // Sharpening failed so generate a regular resolved method dispatch.
          if (update_stats) {
            stats_->ResolvedMethod(invoke_type);
          }
          if (invoke_type == kVirtual || invoke_type == kSuper) {
            vtable_idx = resolved_method->GetMethodIndex();
          }
          GetCodeAndMethodForDirectCall(invoke_type, invoke_type, referrer_class, resolved_method,
                                        direct_code, direct_method, update_stats);
          return true;
        }
      }
    }
  }
  // Clean up any exception left by method/invoke_type resolution
  if (soa.Self()->IsExceptionPending()) {
      soa.Self()->ClearException();
  }
  if (update_stats) {
    stats_->UnresolvedMethod(invoke_type);
  }
  return false;  // Incomplete knowledge needs slow path.
}

bool CompilerDriver::IsSafeCast(const MethodReference& mr, uint32_t dex_pc) {
  bool result = verifier::MethodVerifier::IsSafeCast(mr, dex_pc);
  if (result) {
    stats_->SafeCast();
  } else {
    stats_->NotASafeCast();
  }
  return result;
}


void CompilerDriver::AddCodePatch(const DexFile* dex_file,
                                  uint16_t referrer_class_def_idx,
                                  uint32_t referrer_method_idx,
                                  InvokeType referrer_invoke_type,
                                  uint32_t target_method_idx,
                                  InvokeType target_invoke_type,
                                  size_t literal_offset) {
  MutexLock mu(Thread::Current(), compiled_methods_lock_);
  code_to_patch_.push_back(new PatchInformation(dex_file,
                                                referrer_class_def_idx,
                                                referrer_method_idx,
                                                referrer_invoke_type,
                                                target_method_idx,
                                                target_invoke_type,
                                                literal_offset));
}
void CompilerDriver::AddMethodPatch(const DexFile* dex_file,
                                    uint16_t referrer_class_def_idx,
                                    uint32_t referrer_method_idx,
                                    InvokeType referrer_invoke_type,
                                    uint32_t target_method_idx,
                                    InvokeType target_invoke_type,
                                    size_t literal_offset) {
  MutexLock mu(Thread::Current(), compiled_methods_lock_);
  methods_to_patch_.push_back(new PatchInformation(dex_file,
                                                   referrer_class_def_idx,
                                                   referrer_method_idx,
                                                   referrer_invoke_type,
                                                   target_method_idx,
                                                   target_invoke_type,
                                                   literal_offset));
}

class ParallelCompilationManager {
 public:
  typedef void Callback(const ParallelCompilationManager* manager, size_t index);

  ParallelCompilationManager(ClassLinker* class_linker,
                             jobject class_loader,
                             CompilerDriver* compiler,
                             const DexFile* dex_file,
                             ThreadPool& thread_pool)
    : index_(0),
      class_linker_(class_linker),
      class_loader_(class_loader),
      compiler_(compiler),
      dex_file_(dex_file),
      thread_pool_(&thread_pool) {}

  ClassLinker* GetClassLinker() const {
    CHECK(class_linker_ != NULL);
    return class_linker_;
  }

  jobject GetClassLoader() const {
    return class_loader_;
  }

  CompilerDriver* GetCompiler() const {
    CHECK(compiler_ != NULL);
    return compiler_;
  }

  const DexFile* GetDexFile() const {
    CHECK(dex_file_ != NULL);
    return dex_file_;
  }

  void ForAll(size_t begin, size_t end, Callback callback, size_t work_units) {
    Thread* self = Thread::Current();
    self->AssertNoPendingException();
    CHECK_GT(work_units, 0U);

    std::vector<ForAllClosure*> closures(work_units);
    index_ = begin;
    for (size_t i = 0; i < work_units; ++i) {
      closures[i] = new ForAllClosure(this, end, callback);
      thread_pool_->AddTask(self, closures[i]);
    }
    thread_pool_->StartWorkers(self);

    // Ensure we're suspended while we're blocked waiting for the other threads to finish (worker
    // thread destructor's called below perform join).
    CHECK_NE(self->GetState(), kRunnable);

    // Wait for all the worker threads to finish.
    thread_pool_->Wait(self, true, false);
  }

  size_t NextIndex() {
    return index_.fetch_add(1);
  }

 private:
  class ForAllClosure : public Task {
   public:
    ForAllClosure(ParallelCompilationManager* manager, size_t end, Callback* callback)
        : manager_(manager),
          end_(end),
          callback_(callback) {}

    virtual void Run(Thread* self) {
      while (true) {
        const size_t index = manager_->NextIndex();
        if (UNLIKELY(index >= end_)) {
          break;
        }
        callback_(manager_, index);
        self->AssertNoPendingException();
      }
    }

    virtual void Finalize() {
      delete this;
    }

   private:
    ParallelCompilationManager* const manager_;
    const size_t end_;
    const Callback* const callback_;
  };

  AtomicInteger index_;
  ClassLinker* const class_linker_;
  const jobject class_loader_;
  CompilerDriver* const compiler_;
  const DexFile* const dex_file_;
  ThreadPool* const thread_pool_;

  DISALLOW_COPY_AND_ASSIGN(ParallelCompilationManager);
};

// Return true if the class should be skipped during compilation.
//
// The first case where we skip is for redundant class definitions in
// the boot classpath. We skip all but the first definition in that case.
//
// The second case where we skip is when an app bundles classes found
// in the boot classpath. Since at runtime we will select the class from
// the boot classpath, we ignore the one from the app.
static bool SkipClass(ClassLinker* class_linker, jobject class_loader, const DexFile& dex_file,
                      const DexFile::ClassDef& class_def) {
  const char* descriptor = dex_file.GetClassDescriptor(class_def);
  if (class_loader == NULL) {
    DexFile::ClassPathEntry pair = DexFile::FindInClassPath(descriptor, class_linker->GetBootClassPath());
    CHECK(pair.second != NULL);
    if (pair.first != &dex_file) {
      LOG(WARNING) << "Skipping class " << descriptor << " from " << dex_file.GetLocation()
                   << " previously found in " << pair.first->GetLocation();
      return true;
    }
    return false;
  }
  return class_linker->IsInBootClassPath(descriptor);
}

// A fast version of SkipClass above if the class pointer is available
// that avoids the expensive FindInClassPath search.
static bool SkipClass(jobject class_loader, const DexFile& dex_file, mirror::Class* klass)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(klass != NULL);
  const DexFile& original_dex_file = *klass->GetDexCache()->GetDexFile();
  if (&dex_file != &original_dex_file) {
    if (class_loader == NULL) {
      LOG(WARNING) << "Skipping class " << PrettyDescriptor(klass) << " from "
                   << dex_file.GetLocation() << " previously found in "
                   << original_dex_file.GetLocation();
    }
    return true;
  }
  return false;
}

static void ResolveClassFieldsAndMethods(const ParallelCompilationManager* manager,
                                         size_t class_def_index)
    LOCKS_EXCLUDED(Locks::mutator_lock_) {
  ATRACE_CALL();
  Thread* self = Thread::Current();
  jobject jclass_loader = manager->GetClassLoader();
  const DexFile& dex_file = *manager->GetDexFile();
  ClassLinker* class_linker = manager->GetClassLinker();

  // If an instance field is final then we need to have a barrier on the return, static final
  // fields are assigned within the lock held for class initialization. Conservatively assume
  // constructor barriers are always required.
  bool requires_constructor_barrier = true;

  // Method and Field are the worst. We can't resolve without either
  // context from the code use (to disambiguate virtual vs direct
  // method and instance vs static field) or from class
  // definitions. While the compiler will resolve what it can as it
  // needs it, here we try to resolve fields and methods used in class
  // definitions, since many of them many never be referenced by
  // generated code.
  const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
  if (!SkipClass(class_linker, jclass_loader, dex_file, class_def)) {
    ScopedObjectAccess soa(self);
    mirror::ClassLoader* class_loader = soa.Decode<mirror::ClassLoader*>(jclass_loader);
    mirror::DexCache* dex_cache = class_linker->FindDexCache(dex_file);

    // Resolve the class.
    mirror::Class* klass = class_linker->ResolveType(dex_file, class_def.class_idx_, dex_cache,
                                                     class_loader);

    bool resolve_fields_and_methods;
    if (klass == NULL) {
      // Class couldn't be resolved, for example, super-class is in a different dex file. Don't
      // attempt to resolve methods and fields when there is no declaring class.
      CHECK(soa.Self()->IsExceptionPending());
      soa.Self()->ClearException();
      resolve_fields_and_methods = false;
    } else {
      resolve_fields_and_methods = manager->GetCompiler()->IsImage();
    }
    // Note the class_data pointer advances through the headers,
    // static fields, instance fields, direct methods, and virtual
    // methods.
    const byte* class_data = dex_file.GetClassData(class_def);
    if (class_data == NULL) {
      // Empty class such as a marker interface.
      requires_constructor_barrier = false;
    } else {
      ClassDataItemIterator it(dex_file, class_data);
      while (it.HasNextStaticField()) {
        if (resolve_fields_and_methods) {
          mirror::ArtField* field = class_linker->ResolveField(dex_file, it.GetMemberIndex(),
                                                               dex_cache, class_loader, true);
          if (field == NULL) {
            CHECK(soa.Self()->IsExceptionPending());
            soa.Self()->ClearException();
          }
        }
        it.Next();
      }
      // We require a constructor barrier if there are final instance fields.
      requires_constructor_barrier = false;
      while (it.HasNextInstanceField()) {
        if ((it.GetMemberAccessFlags() & kAccFinal) != 0) {
          requires_constructor_barrier = true;
        }
        if (resolve_fields_and_methods) {
          mirror::ArtField* field = class_linker->ResolveField(dex_file, it.GetMemberIndex(),
                                                               dex_cache, class_loader, false);
          if (field == NULL) {
            CHECK(soa.Self()->IsExceptionPending());
            soa.Self()->ClearException();
          }
        }
        it.Next();
      }
      if (resolve_fields_and_methods) {
        while (it.HasNextDirectMethod()) {
          mirror::ArtMethod* method = class_linker->ResolveMethod(dex_file, it.GetMemberIndex(),
                                                                  dex_cache, class_loader, NULL,
                                                                  it.GetMethodInvokeType(class_def));
          if (method == NULL) {
            CHECK(soa.Self()->IsExceptionPending());
            soa.Self()->ClearException();
          }
          it.Next();
        }
        while (it.HasNextVirtualMethod()) {
          mirror::ArtMethod* method = class_linker->ResolveMethod(dex_file, it.GetMemberIndex(),
                                                                  dex_cache, class_loader, NULL,
                                                                  it.GetMethodInvokeType(class_def));
          if (method == NULL) {
            CHECK(soa.Self()->IsExceptionPending());
            soa.Self()->ClearException();
          }
          it.Next();
        }
        DCHECK(!it.HasNext());
      }
    }
  }
  if (requires_constructor_barrier) {
    manager->GetCompiler()->AddRequiresConstructorBarrier(self, &dex_file, class_def_index);
  }
}

static void ResolveType(const ParallelCompilationManager* manager, size_t type_idx)
    LOCKS_EXCLUDED(Locks::mutator_lock_) {
  // Class derived values are more complicated, they require the linker and loader.
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* class_linker = manager->GetClassLinker();
  const DexFile& dex_file = *manager->GetDexFile();
  mirror::DexCache* dex_cache = class_linker->FindDexCache(dex_file);
  mirror::ClassLoader* class_loader = soa.Decode<mirror::ClassLoader*>(manager->GetClassLoader());
  mirror::Class* klass = class_linker->ResolveType(dex_file, type_idx, dex_cache, class_loader);

  if (klass == NULL) {
    CHECK(soa.Self()->IsExceptionPending());
    mirror::Throwable* exception = soa.Self()->GetException(NULL);
    VLOG(compiler) << "Exception during type resolution: " << exception->Dump();
    if (strcmp(ClassHelper(exception->GetClass()).GetDescriptor(),
               "Ljava/lang/OutOfMemoryError;") == 0) {
      // There's little point continuing compilation if the heap is exhausted.
      LOG(FATAL) << "Out of memory during type resolution for compilation";
    }
    soa.Self()->ClearException();
  }
}

void CompilerDriver::ResolveDexFile(jobject class_loader, const DexFile& dex_file,
                                    ThreadPool& thread_pool, base::TimingLogger& timings) {
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();

  // TODO: we could resolve strings here, although the string table is largely filled with class
  //       and method names.

  ParallelCompilationManager context(class_linker, class_loader, this, &dex_file, thread_pool);
  if (IsImage()) {
    // For images we resolve all types, such as array, whereas for applications just those with
    // classdefs are resolved by ResolveClassFieldsAndMethods.
    // TODO: strdup memory leak.
    timings.NewSplit(strdup(("Resolve " + dex_file.GetLocation() + " Types").c_str()));
    context.ForAll(0, dex_file.NumTypeIds(), ResolveType, thread_count_);
  }

  // TODO: strdup memory leak.
  timings.NewSplit(strdup(("Resolve " + dex_file.GetLocation() + " MethodsAndFields").c_str()));
  context.ForAll(0, dex_file.NumClassDefs(), ResolveClassFieldsAndMethods, thread_count_);
}

void CompilerDriver::Verify(jobject class_loader, const std::vector<const DexFile*>& dex_files,
                            ThreadPool& thread_pool, base::TimingLogger& timings) {
  for (size_t i = 0; i != dex_files.size(); ++i) {
    const DexFile* dex_file = dex_files[i];
    CHECK(dex_file != NULL);
    VerifyDexFile(class_loader, *dex_file, thread_pool, timings);
  }
}

static void VerifyClass(const ParallelCompilationManager* manager, size_t class_def_index)
    LOCKS_EXCLUDED(Locks::mutator_lock_) {
  ATRACE_CALL();
  ScopedObjectAccess soa(Thread::Current());
  const DexFile& dex_file = *manager->GetDexFile();
  const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
  const char* descriptor = dex_file.GetClassDescriptor(class_def);
  ClassLinker* class_linker = manager->GetClassLinker();
  jobject jclass_loader = manager->GetClassLoader();
  mirror::Class* klass = class_linker->FindClass(descriptor,
                                                 soa.Decode<mirror::ClassLoader*>(jclass_loader));
  if (klass == NULL) {
    CHECK(soa.Self()->IsExceptionPending());
    soa.Self()->ClearException();

    /*
     * At compile time, we can still structurally verify the class even if FindClass fails.
     * This is to ensure the class is structurally sound for compilation. An unsound class
     * will be rejected by the verifier and later skipped during compilation in the compiler.
     */
    mirror::DexCache* dex_cache = class_linker->FindDexCache(dex_file);
    std::string error_msg;
    if (verifier::MethodVerifier::VerifyClass(&dex_file, dex_cache,
                                              soa.Decode<mirror::ClassLoader*>(jclass_loader),
                                              &class_def, true, &error_msg) ==
                                                  verifier::MethodVerifier::kHardFailure) {
      LOG(ERROR) << "Verification failed on class " << PrettyDescriptor(descriptor)
                 << " because: " << error_msg;
    }
  } else if (!SkipClass(jclass_loader, dex_file, klass)) {
    CHECK(klass->IsResolved()) << PrettyClass(klass);
    class_linker->VerifyClass(klass);

    if (klass->IsErroneous()) {
      // ClassLinker::VerifyClass throws, which isn't useful in the compiler.
      CHECK(soa.Self()->IsExceptionPending());
      soa.Self()->ClearException();
    }

    CHECK(klass->IsCompileTimeVerified() || klass->IsErroneous())
        << PrettyDescriptor(klass) << ": state=" << klass->GetStatus();
  }
  soa.Self()->AssertNoPendingException();
}

void CompilerDriver::VerifyDexFile(jobject class_loader, const DexFile& dex_file,
                                   ThreadPool& thread_pool, base::TimingLogger& timings) {
  // TODO: strdup memory leak.
  timings.NewSplit(strdup(("Verify " + dex_file.GetLocation()).c_str()));
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ParallelCompilationManager context(class_linker, class_loader, this, &dex_file, thread_pool);
  context.ForAll(0, dex_file.NumClassDefs(), VerifyClass, thread_count_);
}

static const char* class_initializer_black_list[] = {
  "Landroid/app/ActivityThread;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/bluetooth/BluetoothAudioGateway;",  // Calls android.bluetooth.BluetoothAudioGateway.classInitNative().
  "Landroid/bluetooth/HeadsetBase;",  // Calls android.bluetooth.HeadsetBase.classInitNative().
  "Landroid/content/res/CompatibilityInfo;",  // Requires android.util.DisplayMetrics -..-> android.os.SystemProperties.native_get_int.
  "Landroid/content/res/CompatibilityInfo$1;",  // Requires android.util.DisplayMetrics -..-> android.os.SystemProperties.native_get_int.
  "Landroid/content/UriMatcher;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/database/CursorWindow;",  // Requires android.util.DisplayMetrics -..-> android.os.SystemProperties.native_get_int.
  "Landroid/database/sqlite/SQLiteConnection;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/database/sqlite/SQLiteConnection$Operation;",  // Requires SimpleDateFormat -> java.util.Locale.
  "Landroid/database/sqlite/SQLiteDatabaseConfiguration;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/database/sqlite/SQLiteDebug;",  // Calls android.util.Log.isLoggable.
  "Landroid/database/sqlite/SQLiteOpenHelper;",  // Calls Class.getSimpleName -> Class.isAnonymousClass -> Class.getDex.
  "Landroid/database/sqlite/SQLiteQueryBuilder;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/drm/DrmManagerClient;",  // Calls System.loadLibrary.
  "Landroid/graphics/drawable/AnimatedRotateDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/AnimationDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/BitmapDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/ClipDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/ColorDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/Drawable;",  // Requires android.graphics.Rect.
  "Landroid/graphics/drawable/DrawableContainer;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/GradientDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/LayerDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/NinePatchDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/RotateDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/ScaleDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/ShapeDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/StateListDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/drawable/TransitionDrawable;",  // Sub-class of Drawable.
  "Landroid/graphics/Matrix;",  // Calls android.graphics.Matrix.native_create.
  "Landroid/graphics/Matrix$1;",  // Requires Matrix.
  "Landroid/graphics/PixelFormat;",  // Calls android.graphics.PixelFormat.nativeClassInit().
  "Landroid/graphics/Rect;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/graphics/SurfaceTexture;",  // Calls android.graphics.SurfaceTexture.nativeClassInit().
  "Landroid/graphics/Typeface;",  // Calls android.graphics.Typeface.nativeCreate.
  "Landroid/inputmethodservice/ExtractEditText;",  // Requires android.widget.TextView.
  "Landroid/media/AmrInputStream;",  // Calls OsConstants.initConstants.
  "Landroid/media/CamcorderProfile;",  // Calls OsConstants.initConstants.
  "Landroid/media/CameraProfile;",  // Calls System.loadLibrary.
  "Landroid/media/DecoderCapabilities;",  // Calls System.loadLibrary.
  "Landroid/media/EncoderCapabilities;",  // Calls OsConstants.initConstants.
  "Landroid/media/ExifInterface;",  // Calls OsConstants.initConstants.
  "Landroid/media/MediaCodec;",  // Calls OsConstants.initConstants.
  "Landroid/media/MediaCodecList;",  // Calls OsConstants.initConstants.
  "Landroid/media/MediaCrypto;",  // Calls OsConstants.initConstants.
  "Landroid/media/MediaDrm;",  // Calls OsConstants.initConstants.
  "Landroid/media/MediaExtractor;",  // Calls OsConstants.initConstants.
  "Landroid/media/MediaFile;",  // Requires DecoderCapabilities.
  "Landroid/media/MediaMetadataRetriever;",  // Calls OsConstants.initConstants.
  "Landroid/media/MediaMuxer;",  // Calls OsConstants.initConstants.
  "Landroid/media/MediaPlayer;",  // Calls System.loadLibrary.
  "Landroid/media/MediaRecorder;",  // Calls System.loadLibrary.
  "Landroid/media/MediaScanner;",  // Calls System.loadLibrary.
  "Landroid/media/ResampleInputStream;",  // Calls OsConstants.initConstants.
  "Landroid/media/SoundPool;",  // Calls OsConstants.initConstants.
  "Landroid/media/videoeditor/MediaArtistNativeHelper;",  // Calls OsConstants.initConstants.
  "Landroid/media/videoeditor/VideoEditorProfile;",  // Calls OsConstants.initConstants.
  "Landroid/mtp/MtpDatabase;",  // Calls OsConstants.initConstants.
  "Landroid/mtp/MtpDevice;",  // Calls OsConstants.initConstants.
  "Landroid/mtp/MtpServer;",  // Calls OsConstants.initConstants.
  "Landroid/net/NetworkInfo;",  // Calls java.util.EnumMap.<init> -> java.lang.Enum.getSharedConstants -> System.identityHashCode.
  "Landroid/net/Proxy;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/net/SSLCertificateSocketFactory;",  // Requires javax.net.ssl.HttpsURLConnection.
  "Landroid/net/Uri$AbstractHierarchicalUri;",  // Requires Uri.
  "Landroid/net/Uri$HierarchicalUri;",  // Requires Uri.
  "Landroid/net/Uri$OpaqueUri;",  // Requires Uri.
  "Landroid/net/Uri$StringUri;",  // Requires Uri.
  "Landroid/net/Uri;",  // Calls Class.getSimpleName -> Class.isAnonymousClass -> Class.getDex.
  "Landroid/net/WebAddress;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/net/wifi/WifiNative;",  // Calls new LocalLog -> new Time -> TimeZone -> Pattern.compile.
  "Landroid/nfc/NdefRecord;",  // Calls String.getBytes -> java.nio.charset.Charset.
  "Landroid/opengl/EGL14;",  // Calls android.opengl.EGL14._nativeClassInit.
  "Landroid/opengl/GLES10;",  // Calls android.opengl.GLES10._nativeClassInit.
  "Landroid/opengl/GLES10Ext;",  // Calls android.opengl.GLES10Ext._nativeClassInit.
  "Landroid/opengl/GLES11;",  // Requires GLES10.
  "Landroid/opengl/GLES11Ext;",  // Calls android.opengl.GLES11Ext._nativeClassInit.
  "Landroid/opengl/GLES20;",  // Calls android.opengl.GLES20._nativeClassInit.
  "Landroid/opengl/GLUtils;",  // Calls android.opengl.GLUtils.nativeClassInit.
  "Landroid/os/Build;",  // Calls -..-> android.os.SystemProperties.native_get.
  "Landroid/os/Build$VERSION;",  // Requires Build.
  "Landroid/os/Bundle;",  // Calls android.os.Parcel.obtain -..> Parcel.nativeCreate.
  "Landroid/os/Debug;",  // Requires android.os.Environment.
  "Landroid/os/Environment;",  // Calls System.getenv.
  "Landroid/os/FileUtils;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/os/StrictMode;",  // Calls android.util.Log.isLoggable.
  "Landroid/os/StrictMode$VmPolicy;",  // Requires StrictMode.
  "Landroid/os/Trace;",  // Calls android.os.Trace.nativeGetEnabledTags.
  "Landroid/os/UEventObserver;",  // Calls Class.getSimpleName -> Class.isAnonymousClass -> Class.getDex.
  "Landroid/provider/ContactsContract;",  // Calls OsConstants.initConstants.
  "Landroid/provider/Settings$Global;",  // Calls OsConstants.initConstants.
  "Landroid/provider/Settings$Secure;",  // Requires android.net.Uri.
  "Landroid/provider/Settings$System;",  // Requires android.net.Uri.
  "Landroid/renderscript/RenderScript;",  // Calls System.loadLibrary.
  "Landroid/server/BluetoothService;",  // Calls android.server.BluetoothService.classInitNative.
  "Landroid/server/BluetoothEventLoop;",  // Calls android.server.BluetoothEventLoop.classInitNative.
  "Landroid/telephony/PhoneNumberUtils;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/telephony/TelephonyManager;",  // Calls OsConstants.initConstants.
  "Landroid/text/AutoText;",  // Requires android.util.DisplayMetrics -..-> android.os.SystemProperties.native_get_int.
  "Landroid/text/Layout;",  // Calls com.android.internal.util.ArrayUtils.emptyArray -> System.identityHashCode.
  "Landroid/text/BoringLayout;",  // Requires Layout.
  "Landroid/text/DynamicLayout;",  // Requires Layout.
  "Landroid/text/Html$HtmlParser;",  // Calls -..-> String.toLowerCase -> java.util.Locale.
  "Landroid/text/StaticLayout;",  // Requires Layout.
  "Landroid/text/TextUtils;",  // Requires android.util.DisplayMetrics.
  "Landroid/util/DisplayMetrics;",  // Calls SystemProperties.native_get_int.
  "Landroid/util/Patterns;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/view/Choreographer;",  // Calls SystemProperties.native_get_boolean.
  "Landroid/util/Patterns;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/view/GLES20Canvas;",  // Calls GLES20Canvas.nIsAvailable().
  "Landroid/view/GLES20RecordingCanvas;",  // Requires android.view.GLES20Canvas.
  "Landroid/view/GestureDetector;",  // Calls android.view.GLES20Canvas.nIsAvailable.
  "Landroid/view/HardwareRenderer$Gl20Renderer;",  // Requires SystemProperties.native_get.
  "Landroid/view/HardwareRenderer$GlRenderer;",  // Requires SystemProperties.native_get.
  "Landroid/view/InputEventConsistencyVerifier;",  // Requires android.os.Build.
  "Landroid/view/Surface;",  // Requires SystemProperties.native_get.
  "Landroid/view/SurfaceControl;",  // Calls OsConstants.initConstants.
  "Landroid/view/animation/AlphaAnimation;",  // Requires Animation.
  "Landroid/view/animation/Animation;",  // Calls SystemProperties.native_get_boolean.
  "Landroid/view/animation/AnimationSet;",  // Calls OsConstants.initConstants.
  "Landroid/view/textservice/SpellCheckerSubtype;",  // Calls Class.getDex().
  "Landroid/webkit/JniUtil;",  // Calls System.loadLibrary.
  "Landroid/webkit/PluginManager;",  // // Calls OsConstants.initConstants.
  "Landroid/webkit/WebViewCore;",  // Calls System.loadLibrary.
  "Landroid/webkit/WebViewFactory;",  // Calls -..-> android.os.SystemProperties.native_get.
  "Landroid/webkit/WebViewFactory$Preloader;",  // Calls to Class.forName.
  "Landroid/webkit/WebViewInputDispatcher;",  // Calls Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/webkit/URLUtil;",  // Calls Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Landroid/widget/AutoCompleteTextView;",  // Requires TextView.
  "Landroid/widget/Button;",  // Requires TextView.
  "Landroid/widget/CheckBox;",  // Requires TextView.
  "Landroid/widget/CheckedTextView;",  // Requires TextView.
  "Landroid/widget/CompoundButton;",  // Requires TextView.
  "Landroid/widget/EditText;",  // Requires TextView.
  "Landroid/widget/NumberPicker;",  // Requires java.util.Locale.
  "Landroid/widget/ScrollBarDrawable;",  // Sub-class of Drawable.
  "Landroid/widget/SearchView$SearchAutoComplete;",  // Requires TextView.
  "Landroid/widget/Switch;",  // Requires TextView.
  "Landroid/widget/TextView;",  // Calls Paint.<init> -> Paint.native_init.
  "Lcom/android/i18n/phonenumbers/AsYouTypeFormatter;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Lcom/android/i18n/phonenumbers/MetadataManager;",  // Calls OsConstants.initConstants.
  "Lcom/android/i18n/phonenumbers/PhoneNumberMatcher;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Lcom/android/i18n/phonenumbers/PhoneNumberUtil;",  // Requires java.util.logging.LogManager.
  "Lcom/android/i18n/phonenumbers/geocoding/AreaCodeMap;",  // Calls OsConstants.initConstants.
  "Lcom/android/i18n/phonenumbers/geocoding/PhoneNumberOfflineGeocoder;",  // Calls OsConstants.initConstants.
  "Lcom/android/internal/os/SamplingProfilerIntegration;",  // Calls SystemProperties.native_get_int.
  "Lcom/android/internal/policy/impl/PhoneWindow;",  // Calls android.os.Binder.init.
  "Lcom/android/internal/view/menu/ActionMenuItemView;",  // Requires TextView.
  "Lcom/android/internal/widget/DialogTitle;",  // Requires TextView.
  "Lcom/android/org/bouncycastle/asn1/StreamUtil;",  // Calls Runtime.getRuntime().maxMemory().
  "Lcom/android/org/bouncycastle/asn1/pkcs/MacData;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/asn1/pkcs/RSASSAPSSparams;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/asn1/cms/SignedData;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/asn1/x509/GeneralSubtree;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/asn1/x9/X9ECParameters;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/crypto/digests/OpenSSLDigest$MD5;",  // Requires com.android.org.conscrypt.NativeCrypto.
  "Lcom/android/org/bouncycastle/crypto/digests/OpenSSLDigest$SHA1;",  // Requires com.android.org.conscrypt.NativeCrypto.
  "Lcom/android/org/bouncycastle/crypto/digests/OpenSSLDigest$SHA256;",  // Requires com.android.org.conscrypt.NativeCrypto.
  "Lcom/android/org/bouncycastle/crypto/digests/OpenSSLDigest$SHA384;",  // Requires com.android.org.conscrypt.NativeCrypto.
  "Lcom/android/org/bouncycastle/crypto/digests/OpenSSLDigest$SHA512;",  // Requires com.android.org.conscrypt.NativeCrypto.
  "Lcom/android/org/bouncycastle/crypto/engines/RSABlindedEngine;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/crypto/generators/DHKeyGeneratorHelper;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/crypto/generators/DHParametersGenerator;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/crypto/generators/DHParametersHelper;",  // Calls System.getenv -> OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/crypto/generators/DSAKeyPairGenerator;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/crypto/generators/DSAParametersGenerator;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/crypto/generators/RSAKeyPairGenerator;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/jcajce/provider/asymmetric/dh/KeyPairGeneratorSpi;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/jcajce/provider/asymmetric/dsa/KeyPairGeneratorSpi;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/jcajce/provider/asymmetric/ec/KeyPairGeneratorSpi$EC;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/jcajce/provider/asymmetric/ec/KeyPairGeneratorSpi$ECDH;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/jcajce/provider/asymmetric/ec/KeyPairGeneratorSpi$ECDHC;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/jcajce/provider/asymmetric/ec/KeyPairGeneratorSpi$ECDSA;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/jcajce/provider/asymmetric/ec/KeyPairGeneratorSpi$ECMQV;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/jcajce/provider/asymmetric/ec/KeyPairGeneratorSpi;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/jcajce/provider/asymmetric/rsa/BCRSAPrivateCrtKey;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/jcajce/provider/asymmetric/rsa/BCRSAPrivateKey;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/jcajce/provider/asymmetric/rsa/KeyPairGeneratorSpi;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/jcajce/provider/keystore/pkcs12/PKCS12KeyStoreSpi$BCPKCS12KeyStore;",  // Calls Thread.currentThread.
  "Lcom/android/org/bouncycastle/jcajce/provider/keystore/pkcs12/PKCS12KeyStoreSpi;",  // Calls Thread.currentThread.
  "Lcom/android/org/bouncycastle/jce/PKCS10CertificationRequest;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/jce/provider/CertBlacklist;",  // Calls System.getenv -> OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/jce/provider/JCERSAPrivateCrtKey;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/jce/provider/JCERSAPrivateKey;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/jce/provider/PKIXCertPathValidatorSpi;",  // Calls System.getenv -> OsConstants.initConstants.
  "Lcom/android/org/bouncycastle/math/ec/ECConstants;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/math/ec/Tnaf;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/util/BigIntegers;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/bouncycastle/x509/X509Util;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lcom/android/org/conscrypt/CipherSuite;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/conscrypt/FileClientSessionCache$CacheFile;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/conscrypt/HandshakeIODataStream;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/conscrypt/Logger;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/conscrypt/NativeCrypto;",  // Calls native NativeCrypto.clinit().
  "Lcom/android/org/conscrypt/OpenSSLECKeyPairGenerator;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/conscrypt/OpenSSLEngine;",  // Requires com.android.org.conscrypt.NativeCrypto.
  "Lcom/android/org/conscrypt/OpenSSLMac$HmacMD5;",  // Calls native NativeCrypto.clinit().
  "Lcom/android/org/conscrypt/OpenSSLMac$HmacSHA1;",  // Calls native NativeCrypto.clinit().
  "Lcom/android/org/conscrypt/OpenSSLMac$HmacSHA256;",  // Calls native NativeCrypto.clinit().
  "Lcom/android/org/conscrypt/OpenSSLMac$HmacSHA384;",  // Calls native NativeCrypto.clinit().
  "Lcom/android/org/conscrypt/OpenSSLMac$HmacSHA512;",  // Calls native NativeCrypto.clinit().
  "Lcom/android/org/conscrypt/OpenSSLMessageDigestJDK$MD5;",  // Requires com.android.org.conscrypt.NativeCrypto.
  "Lcom/android/org/conscrypt/OpenSSLMessageDigestJDK$SHA1;",  // Requires com.android.org.conscrypt.NativeCrypto.
  "Lcom/android/org/conscrypt/OpenSSLMessageDigestJDK$SHA256;",  // Requires com.android.org.conscrypt.NativeCrypto.
  "Lcom/android/org/conscrypt/OpenSSLMessageDigestJDK$SHA384;",  // Requires com.android.org.conscrypt.NativeCrypto.
  "Lcom/android/org/conscrypt/OpenSSLMessageDigestJDK$SHA512;",  // Requires com.android.org.conscrypt.NativeCrypto.
  "Lcom/android/org/conscrypt/OpenSSLX509CertPath;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/conscrypt/OpenSSLX509CertificateFactory;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/conscrypt/PRF;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/conscrypt/SSLSessionImpl;",  // Calls OsConstants.initConstants.
  "Lcom/android/org/conscrypt/TrustedCertificateStore;",  // Calls System.getenv -> OsConstants.initConstants.
  "Lcom/android/okhttp/ConnectionPool;",  // Calls OsConstants.initConstants.
  "Lcom/android/okhttp/OkHttpClient;",  // Calls OsConstants.initConstants.
  "Lcom/android/okhttp/internal/DiskLruCache;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Lcom/android/okhttp/internal/Util;",  // Calls OsConstants.initConstants.
  "Lcom/android/okhttp/internal/http/HttpsURLConnectionImpl;",  // Calls VMClassLoader.getBootClassPathSize.
  "Lcom/android/okhttp/internal/spdy/SpdyConnection;",  // Calls OsConstants.initConstants.
  "Lcom/android/okhttp/internal/spdy/SpdyReader;",  // Calls OsConstants.initConstants.
  "Lcom/android/okhttp/internal/tls/OkHostnameVerifier;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Lcom/google/android/gles_jni/EGLContextImpl;",  // Calls com.google.android.gles_jni.EGLImpl._nativeClassInit.
  "Lcom/google/android/gles_jni/EGLImpl;",  // Calls com.google.android.gles_jni.EGLImpl._nativeClassInit.
  "Lcom/google/android/gles_jni/GLImpl;",  // Calls com.google.android.gles_jni.GLImpl._nativeClassInit.
  "Lgov/nist/core/GenericObject;",  // Calls OsConstants.initConstants.
  "Lgov/nist/core/Host;",  // Calls OsConstants.initConstants.
  "Lgov/nist/core/HostPort;",  // Calls OsConstants.initConstants.
  "Lgov/nist/core/NameValue;",  // Calls OsConstants.initConstants.
  "Lgov/nist/core/net/DefaultNetworkLayer;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/Utils;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/address/AddressImpl;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/address/Authority;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/address/GenericURI;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/address/NetObject;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/address/SipUri;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/address/TelephoneNumber;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/address/UserInfo;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/Accept;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/AcceptEncoding;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/AcceptLanguage;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/AddressParametersHeader;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/AlertInfoList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/AllowEvents;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/AllowEventsList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/AuthenticationInfo;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/Authorization;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/CSeq;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/CallIdentifier;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/Challenge;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ContactList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ContentEncoding;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ContentEncodingList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ContentLanguageList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ContentType;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/Credentials;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ErrorInfoList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/Expires;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/From;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/MimeVersion;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/NameMap;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/Priority;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/Protocol;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ProxyAuthenticate;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ProxyAuthenticateList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ProxyAuthorizationList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ProxyRequire;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ProxyRequireList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/RSeq;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/RecordRoute;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ReferTo;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/RequestLine;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/Require;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/RetryAfter;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/SIPETag;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/SIPHeader;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/SIPHeaderNamesCache;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/StatusLine;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/SubscriptionState;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/TimeStamp;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/UserAgent;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/Unsupported;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/Warning;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ViaList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/extensions/Join;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/extensions/References;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/extensions/Replaces;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/PAccessNetworkInfo;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/PAssertedIdentity;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/PAssertedIdentityList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/PAssociatedURI;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/PCalledPartyID;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/PChargingVector;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/PPreferredIdentity;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/PVisitedNetworkIDList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/PathList;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/SecurityAgree;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/SecurityClient;",  // Calls OsConstants.initConstants.
  "Lgov/nist/javax/sip/header/ims/ServiceRoute;",  // Calls OsConstants.initConstants.
  "Ljava/io/Console;",  // Has FileDescriptor(s).
  "Ljava/io/File;",  // Calls to Random.<init> -> System.currentTimeMillis -> OsConstants.initConstants.
  "Ljava/io/FileDescriptor;",  // Requires libcore.io.OsConstants.
  "Ljava/io/ObjectInputStream;",  // Requires java.lang.ClassLoader$SystemClassLoader.
  "Ljava/io/ObjectStreamClass;",  // Calls to Class.forName -> java.io.FileDescriptor.
  "Ljava/io/ObjectStreamConstants;",  // Instance of non-image class SerializablePermission.
  "Ljava/lang/ClassLoader$SystemClassLoader;",  // Calls System.getProperty -> OsConstants.initConstants.
  "Ljava/lang/HexStringParser;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Ljava/lang/ProcessManager;",  // Calls Thread.currentThread.
  "Ljava/lang/Runtime;",  // Calls System.getProperty -> OsConstants.initConstants.
  "Ljava/lang/System;",  // Calls OsConstants.initConstants.
  "Ljava/math/BigDecimal;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Ljava/math/BigInteger;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Ljava/math/Primality;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Ljava/math/Multiplication;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Ljava/net/InetAddress;",  // Requires libcore.io.OsConstants.
  "Ljava/net/Inet4Address;",  // Sub-class of InetAddress.
  "Ljava/net/Inet6Address;",  // Sub-class of InetAddress.
  "Ljava/net/InetUnixAddress;",  // Sub-class of InetAddress.
  "Ljava/net/NetworkInterface;",  // Calls to Random.<init> -> System.currentTimeMillis -> OsConstants.initConstants.
  "Ljava/nio/charset/Charset;",  // Calls Charset.getDefaultCharset -> System.getProperty -> OsConstants.initConstants.
  "Ljava/nio/charset/CharsetICU;",  // Sub-class of Charset.
  "Ljava/nio/charset/Charsets;",  // Calls Charset.forName.
  "Ljava/nio/charset/StandardCharsets;",  // Calls OsConstants.initConstants.
  "Ljava/security/AlgorithmParameterGenerator;",  // Calls OsConstants.initConstants.
  "Ljava/security/KeyPairGenerator$KeyPairGeneratorImpl;",  // Calls OsConstants.initConstants.
  "Ljava/security/KeyPairGenerator;",  // Calls OsConstants.initConstants.
  "Ljava/security/Security;",  // Tries to do disk IO for "security.properties".
  "Ljava/security/spec/RSAKeyGenParameterSpec;",  // java.math.NativeBN.BN_new()
  "Ljava/sql/Date;",  // Calls OsConstants.initConstants.
  "Ljava/sql/DriverManager;",  // Calls OsConstants.initConstants.
  "Ljava/sql/Time;",  // Calls OsConstants.initConstants.
  "Ljava/sql/Timestamp;",  // Calls OsConstants.initConstants.
  "Ljava/util/Date;",  // Calls Date.<init> -> System.currentTimeMillis -> OsConstants.initConstants.
  "Ljava/util/ListResourceBundle;",  // Calls OsConstants.initConstants.
  "Ljava/util/Locale;",  // Calls System.getProperty -> OsConstants.initConstants.
  "Ljava/util/PropertyResourceBundle;",  // Calls OsConstants.initConstants.
  "Ljava/util/ResourceBundle;",  // Calls OsConstants.initConstants.
  "Ljava/util/ResourceBundle$MissingBundle;",  // Calls OsConstants.initConstants.
  "Ljava/util/Scanner;",  // regex.Pattern.compileImpl.
  "Ljava/util/SimpleTimeZone;",  // Sub-class of TimeZone.
  "Ljava/util/TimeZone;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
  "Ljava/util/concurrent/ConcurrentHashMap;",  // Calls Runtime.getRuntime().availableProcessors().
  "Ljava/util/concurrent/ConcurrentHashMap$Segment;",  // Calls Runtime.getRuntime().availableProcessors().
  "Ljava/util/concurrent/ConcurrentSkipListMap;",  // Calls Random() -> OsConstants.initConstants.
  "Ljava/util/concurrent/Exchanger;",  // Calls Runtime.getRuntime().availableProcessors().
  "Ljava/util/concurrent/ForkJoinPool;",  // Makes a thread pool ..-> calls OsConstants.initConstants.
  "Ljava/util/concurrent/LinkedTransferQueue;",  // Calls Runtime.getRuntime().availableProcessors().
  "Ljava/util/concurrent/Phaser;",  // Calls Runtime.getRuntime().availableProcessors().
  "Ljava/util/concurrent/ScheduledThreadPoolExecutor;",  // Calls AtomicLong.VMSupportsCS8()
  "Ljava/util/concurrent/SynchronousQueue;",  // Calls Runtime.getRuntime().availableProcessors().
  "Ljava/util/concurrent/atomic/AtomicLong;",  // Calls AtomicLong.VMSupportsCS8()
  "Ljava/util/logging/LogManager;",  // Calls System.getProperty -> OsConstants.initConstants.
  "Ljava/util/prefs/AbstractPreferences;",  // Calls OsConstants.initConstants.
  "Ljava/util/prefs/FilePreferencesImpl;",  // Calls OsConstants.initConstants.
  "Ljava/util/prefs/FilePreferencesFactoryImpl;",  // Calls OsConstants.initConstants.
  "Ljava/util/prefs/Preferences;",  // Calls OsConstants.initConstants.
  "Ljavax/crypto/KeyAgreement;",  // Calls OsConstants.initConstants.
  "Ljavax/crypto/KeyGenerator;",  // Calls OsConstants.initConstants.
  "Ljavax/security/cert/X509Certificate;",  // Calls VMClassLoader.getBootClassPathSize.
  "Ljavax/security/cert/X509Certificate$1;",  // Calls VMClassLoader.getBootClassPathSize.
  "Ljavax/microedition/khronos/egl/EGL10;",  // Requires EGLContext.
  "Ljavax/microedition/khronos/egl/EGLContext;",  // Requires com.google.android.gles_jni.EGLImpl.
  "Ljavax/xml/datatype/DatatypeConstants;",  // Calls OsConstants.initConstants.
  "Ljavax/xml/datatype/FactoryFinder;",  // Calls OsConstants.initConstants.
  "Ljavax/xml/namespace/QName;",  // Calls OsConstants.initConstants.
  "Ljavax/xml/validation/SchemaFactoryFinder;",  // Calls OsConstants.initConstants.
  "Ljavax/xml/xpath/XPathConstants;",  // Calls OsConstants.initConstants.
  "Ljavax/xml/xpath/XPathFactoryFinder;",  // Calls OsConstants.initConstants.
  "Llibcore/icu/LocaleData;",  // Requires java.util.Locale.
  "Llibcore/icu/TimeZoneNames;",  // Requires java.util.TimeZone.
  "Llibcore/io/IoUtils;",  // Calls Random.<init> -> System.currentTimeMillis -> FileDescriptor -> OsConstants.initConstants.
  "Llibcore/io/OsConstants;",  // Platform specific.
  "Llibcore/net/MimeUtils;",  // Calls libcore.net.MimeUtils.getContentTypesPropertiesStream -> System.getProperty.
  "Llibcore/reflect/Types;",  // Calls OsConstants.initConstants.
  "Llibcore/util/ZoneInfo;",  // Sub-class of TimeZone.
  "Llibcore/util/ZoneInfoDB;",  // Calls System.getenv -> OsConstants.initConstants.
  "Lorg/apache/commons/logging/LogFactory;",  // Calls System.getProperty.
  "Lorg/apache/commons/logging/impl/LogFactoryImpl;",  // Calls OsConstants.initConstants.
  "Lorg/apache/harmony/security/fortress/Services;",  // Calls ClassLoader.getSystemClassLoader -> System.getProperty.
  "Lorg/apache/harmony/security/provider/cert/X509CertFactoryImpl;",  // Requires java.nio.charsets.Charsets.
  "Lorg/apache/harmony/security/provider/crypto/RandomBitsSupplier;",  // Requires java.io.File.
  "Lorg/apache/harmony/security/utils/AlgNameMapper;",  // Requires java.util.Locale.
  "Lorg/apache/harmony/security/pkcs10/CertificationRequest;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/pkcs10/CertificationRequestInfo;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/pkcs7/AuthenticatedAttributes;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/pkcs7/SignedData;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/pkcs7/SignerInfo;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/pkcs8/PrivateKeyInfo;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/provider/crypto/SHA1PRNG_SecureRandomImpl;",  // Calls OsConstants.initConstants.
  "Lorg/apache/harmony/security/x501/AttributeTypeAndValue;",  // Calls IntegralToString.convertInt -> Thread.currentThread.
  "Lorg/apache/harmony/security/x501/DirectoryString;",  // Requires BigInteger.
  "Lorg/apache/harmony/security/x501/Name;",  // Requires org.apache.harmony.security.x501.AttributeTypeAndValue.
  "Lorg/apache/harmony/security/x509/AccessDescription;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/AuthorityKeyIdentifier;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/CRLDistributionPoints;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/Certificate;",  // Requires org.apache.harmony.security.x509.TBSCertificate.
  "Lorg/apache/harmony/security/x509/CertificateIssuer;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/CertificateList;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/DistributionPoint;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/DistributionPointName;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/EDIPartyName;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lorg/apache/harmony/security/x509/GeneralName;",  // Requires org.apache.harmony.security.x501.Name.
  "Lorg/apache/harmony/security/x509/GeneralNames;",  // Requires GeneralName.
  "Lorg/apache/harmony/security/x509/GeneralSubtree;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/GeneralSubtrees;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/InfoAccessSyntax;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/IssuingDistributionPoint;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/NameConstraints;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/TBSCertList$RevokedCertificate;",  // Calls NativeBN.BN_new().
  "Lorg/apache/harmony/security/x509/TBSCertList;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/security/x509/TBSCertificate;",  // Requires org.apache.harmony.security.x501.Name.
  "Lorg/apache/harmony/security/x509/Time;",  // Calls native ... -> java.math.NativeBN.BN_new().
  "Lorg/apache/harmony/security/x509/Validity;",  // Requires x509.Time.
  "Lorg/apache/harmony/security/x509/tsp/TSTInfo;",  // Calls Thread.currentThread.
  "Lorg/apache/harmony/xml/ExpatParser;",  // Calls native ExpatParser.staticInitialize.
  "Lorg/apache/harmony/xml/ExpatParser$EntityParser;",  // Calls ExpatParser.staticInitialize.
  "Lorg/apache/http/conn/params/ConnRouteParams;",  // Requires java.util.Locale.
  "Lorg/apache/http/conn/ssl/SSLSocketFactory;",  // Calls java.security.Security.getProperty.
  "Lorg/apache/http/conn/util/InetAddressUtils;",  // Calls regex.Pattern.compile -..-> regex.Pattern.compileImpl.
};

static void InitializeClass(const ParallelCompilationManager* manager, size_t class_def_index)
    LOCKS_EXCLUDED(Locks::mutator_lock_) {
  ATRACE_CALL();
  jobject jclass_loader = manager->GetClassLoader();
  const DexFile& dex_file = *manager->GetDexFile();
  const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
  const char* descriptor = dex_file.GetClassDescriptor(class_def);
  ClassLinker* class_linker = manager->GetClassLinker();
  ScopedObjectAccess soa(Thread::Current());
  mirror::ClassLoader* class_loader = soa.Decode<mirror::ClassLoader*>(jclass_loader);
  mirror::Class* klass = class_linker->FindClass(descriptor, class_loader);

  if (klass != NULL && !SkipClass(jclass_loader, dex_file, klass)) {
    // Only try to initialize classes that were successfully verified.
    if (klass->IsVerified()) {
      // Attempt to initialize the class but bail if we either need to initialize the super-class
      // or static fields.
      manager->GetClassLinker()->EnsureInitialized(klass, false, false);
      if (!klass->IsInitialized()) {
        // We don't want non-trivial class initialization occurring on multiple threads due to
        // deadlock problems. For example, a parent class is initialized (holding its lock) that
        // refers to a sub-class in its static/class initializer causing it to try to acquire the
        // sub-class' lock. While on a second thread the sub-class is initialized (holding its lock)
        // after first initializing its parents, whose locks are acquired. This leads to a
        // parent-to-child and a child-to-parent lock ordering and consequent potential deadlock.
        // We need to use an ObjectLock due to potential suspension in the interpreting code. Rather
        // than use a special Object for the purpose we use the Class of java.lang.Class.
        ObjectLock lock(soa.Self(), klass->GetClass());
        // Attempt to initialize allowing initialization of parent classes but still not static
        // fields.
        manager->GetClassLinker()->EnsureInitialized(klass, false, true);
        if (!klass->IsInitialized()) {
          // We need to initialize static fields, we only do this for image classes that aren't
          // black listed or marked with the $NoPreloadHolder.
          bool can_init_static_fields = manager->GetCompiler()->IsImage() &&
              manager->GetCompiler()->IsImageClass(descriptor);
          if (can_init_static_fields) {
            // NoPreloadHolder inner class implies this should not be initialized early.
            bool is_black_listed = StringPiece(descriptor).ends_with("$NoPreloadHolder;");
            if (!is_black_listed) {
              for (size_t i = 0; i < arraysize(class_initializer_black_list); ++i) {
                if (StringPiece(descriptor) == class_initializer_black_list[i]) {
                  is_black_listed = true;
                  break;
                }
              }
            }
            if (!is_black_listed) {
              VLOG(compiler) << "Initializing: " << descriptor;
              if (StringPiece(descriptor) == "Ljava/lang/Void;") {
                // Hand initialize j.l.Void to avoid Dex file operations in un-started runtime.
                ObjectLock lock(soa.Self(), klass);
                mirror::ObjectArray<mirror::ArtField>* fields = klass->GetSFields();
                CHECK_EQ(fields->GetLength(), 1);
                fields->Get(0)->SetObj(klass, manager->GetClassLinker()->FindPrimitiveClass('V'));
                klass->SetStatus(mirror::Class::kStatusInitialized, soa.Self());
              } else {
                manager->GetClassLinker()->EnsureInitialized(klass, true, true);
              }
            }
          }
        }
        soa.Self()->AssertNoPendingException();
      }
      // If successfully initialized place in SSB array.
      if (klass->IsInitialized()) {
        int32_t ssb_index = klass->GetDexTypeIndex();
        klass->GetDexCache()->GetInitializedStaticStorage()->Set(ssb_index, klass);
      }
    }
    // Record the final class status if necessary.
    ClassReference ref(manager->GetDexFile(), class_def_index);
    manager->GetCompiler()->RecordClassStatus(ref, klass->GetStatus());
  }
  // Clear any class not found or verification exceptions.
  soa.Self()->ClearException();
}

void CompilerDriver::InitializeClasses(jobject jni_class_loader, const DexFile& dex_file,
                                       ThreadPool& thread_pool, base::TimingLogger& timings) {
  // TODO: strdup memory leak.
  timings.NewSplit(strdup(("InitializeNoClinit " + dex_file.GetLocation()).c_str()));
#ifndef NDEBUG
  // Sanity check blacklist descriptors.
  if (IsImage()) {
    for (size_t i = 0; i < arraysize(class_initializer_black_list); ++i) {
      const char* descriptor = class_initializer_black_list[i];
      CHECK(IsValidDescriptor(descriptor)) << descriptor;
    }
  }
#endif
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ParallelCompilationManager context(class_linker, jni_class_loader, this, &dex_file, thread_pool);
  context.ForAll(0, dex_file.NumClassDefs(), InitializeClass, thread_count_);
}

void CompilerDriver::InitializeClasses(jobject class_loader,
                                       const std::vector<const DexFile*>& dex_files,
                                       ThreadPool& thread_pool, base::TimingLogger& timings) {
  for (size_t i = 0; i != dex_files.size(); ++i) {
    const DexFile* dex_file = dex_files[i];
    CHECK(dex_file != NULL);
    InitializeClasses(class_loader, *dex_file, thread_pool, timings);
  }
}

void CompilerDriver::Compile(jobject class_loader, const std::vector<const DexFile*>& dex_files,
                       ThreadPool& thread_pool, base::TimingLogger& timings) {
  for (size_t i = 0; i != dex_files.size(); ++i) {
    const DexFile* dex_file = dex_files[i];
    CHECK(dex_file != NULL);
    CompileDexFile(class_loader, *dex_file, thread_pool, timings);
  }
}

void CompilerDriver::CompileClass(const ParallelCompilationManager* manager, size_t class_def_index) {
  ATRACE_CALL();
  jobject jclass_loader = manager->GetClassLoader();
  const DexFile& dex_file = *manager->GetDexFile();
  const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
  ClassLinker* class_linker = manager->GetClassLinker();
  if (SkipClass(class_linker, jclass_loader, dex_file, class_def)) {
    return;
  }
  ClassReference ref(&dex_file, class_def_index);
  // Skip compiling classes with generic verifier failures since they will still fail at runtime
  if (verifier::MethodVerifier::IsClassRejected(ref)) {
    return;
  }
  const byte* class_data = dex_file.GetClassData(class_def);
  if (class_data == NULL) {
    // empty class, probably a marker interface
    return;
  }

  // Can we run DEX-to-DEX compiler on this class ?
  DexToDexCompilationLevel dex_to_dex_compilation_level = kDontDexToDexCompile;
  {
    ScopedObjectAccess soa(Thread::Current());
    mirror::ClassLoader* class_loader = soa.Decode<mirror::ClassLoader*>(jclass_loader);
    dex_to_dex_compilation_level = GetDexToDexCompilationlevel(class_loader, dex_file, class_def);
  }
  ClassDataItemIterator it(dex_file, class_data);
  // Skip fields
  while (it.HasNextStaticField()) {
    it.Next();
  }
  while (it.HasNextInstanceField()) {
    it.Next();
  }
  CompilerDriver* driver = manager->GetCompiler();
  // Compile direct methods
  int64_t previous_direct_method_idx = -1;
  while (it.HasNextDirectMethod()) {
    uint32_t method_idx = it.GetMemberIndex();
    if (method_idx == previous_direct_method_idx) {
      // smali can create dex files with two encoded_methods sharing the same method_idx
      // http://code.google.com/p/smali/issues/detail?id=119
      it.Next();
      continue;
    }
    previous_direct_method_idx = method_idx;
    driver->CompileMethod(it.GetMethodCodeItem(), it.GetMemberAccessFlags(),
                          it.GetMethodInvokeType(class_def), class_def_index,
                          method_idx, jclass_loader, dex_file, dex_to_dex_compilation_level);
    it.Next();
  }
  // Compile virtual methods
  int64_t previous_virtual_method_idx = -1;
  while (it.HasNextVirtualMethod()) {
    uint32_t method_idx = it.GetMemberIndex();
    if (method_idx == previous_virtual_method_idx) {
      // smali can create dex files with two encoded_methods sharing the same method_idx
      // http://code.google.com/p/smali/issues/detail?id=119
      it.Next();
      continue;
    }
    previous_virtual_method_idx = method_idx;
    driver->CompileMethod(it.GetMethodCodeItem(), it.GetMemberAccessFlags(),
                          it.GetMethodInvokeType(class_def), class_def_index,
                          method_idx, jclass_loader, dex_file, dex_to_dex_compilation_level);
    it.Next();
  }
  DCHECK(!it.HasNext());
}

void CompilerDriver::CompileDexFile(jobject class_loader, const DexFile& dex_file,
                                    ThreadPool& thread_pool, base::TimingLogger& timings) {
  // TODO: strdup memory leak.
  timings.NewSplit(strdup(("Compile " + dex_file.GetLocation()).c_str()));
  ParallelCompilationManager context(Runtime::Current()->GetClassLinker(), class_loader, this,
                                     &dex_file, thread_pool);
  context.ForAll(0, dex_file.NumClassDefs(), CompilerDriver::CompileClass, thread_count_);
}

void CompilerDriver::CompileMethod(const DexFile::CodeItem* code_item, uint32_t access_flags,
                                   InvokeType invoke_type, uint16_t class_def_idx,
                                   uint32_t method_idx, jobject class_loader,
                                   const DexFile& dex_file,
                                   DexToDexCompilationLevel dex_to_dex_compilation_level) {
  CompiledMethod* compiled_method = NULL;
  uint64_t start_ns = NanoTime();

  if ((access_flags & kAccNative) != 0) {
    compiled_method = (*jni_compiler_)(*this, access_flags, method_idx, dex_file);
    CHECK(compiled_method != NULL);
  } else if ((access_flags & kAccAbstract) != 0) {
  } else {
    MethodReference method_ref(&dex_file, method_idx);
    bool compile = verifier::MethodVerifier::IsCandidateForCompilation(method_ref, access_flags);

    if (compile) {
      CompilerFn compiler = compiler_;
#ifdef ART_SEA_IR_MODE
      bool use_sea = Runtime::Current()->IsSeaIRMode();
      use_sea = use_sea &&
          (std::string::npos != PrettyMethod(method_idx, dex_file).find("fibonacci"));
      if (use_sea) {
        compiler = sea_ir_compiler_;
        LOG(INFO) << "Using SEA IR to compile..." << std::endl;
      }
#endif
      // NOTE: if compiler declines to compile this method, it will return NULL.
      compiled_method = (*compiler)(*this, code_item, access_flags, invoke_type, class_def_idx,
                                    method_idx, class_loader, dex_file);
    } else if (dex_to_dex_compilation_level != kDontDexToDexCompile) {
      // TODO: add a mode to disable DEX-to-DEX compilation ?
      (*dex_to_dex_compiler_)(*this, code_item, access_flags,
                              invoke_type, class_def_idx,
                              method_idx, class_loader, dex_file,
                              dex_to_dex_compilation_level);
    }
  }
  uint64_t duration_ns = NanoTime() - start_ns;
#ifdef ART_USE_PORTABLE_COMPILER
  const uint64_t kWarnMilliSeconds = 1000;
#else
  const uint64_t kWarnMilliSeconds = 100;
#endif
  if (duration_ns > MsToNs(kWarnMilliSeconds)) {
    LOG(WARNING) << "Compilation of " << PrettyMethod(method_idx, dex_file)
                 << " took " << PrettyDuration(duration_ns);
  }

  Thread* self = Thread::Current();
  if (compiled_method != NULL) {
    MethodReference ref(&dex_file, method_idx);
    DCHECK(GetCompiledMethod(ref) == NULL) << PrettyMethod(method_idx, dex_file);
    {
      MutexLock mu(self, compiled_methods_lock_);
      compiled_methods_.Put(ref, compiled_method);
    }
    DCHECK(GetCompiledMethod(ref) != NULL) << PrettyMethod(method_idx, dex_file);
  }

  if (self->IsExceptionPending()) {
    ScopedObjectAccess soa(self);
    LOG(FATAL) << "Unexpected exception compiling: " << PrettyMethod(method_idx, dex_file) << "\n"
        << self->GetException(NULL)->Dump();
  }
}

CompiledClass* CompilerDriver::GetCompiledClass(ClassReference ref) const {
  MutexLock mu(Thread::Current(), compiled_classes_lock_);
  ClassTable::const_iterator it = compiled_classes_.find(ref);
  if (it == compiled_classes_.end()) {
    return NULL;
  }
  CHECK(it->second != NULL);
  return it->second;
}

void CompilerDriver::RecordClassStatus(ClassReference ref, mirror::Class::Status status) {
  MutexLock mu(Thread::Current(), compiled_classes_lock_);
  auto it = compiled_classes_.find(ref);
  if (it == compiled_classes_.end() || it->second->GetStatus() != status) {
    // An entry doesn't exist or the status is lower than the new status.
    if (it != compiled_classes_.end()) {
      CHECK_GT(status, it->second->GetStatus());
      delete it->second;
    }
    switch (status) {
      case mirror::Class::kStatusNotReady:
      case mirror::Class::kStatusError:
      case mirror::Class::kStatusRetryVerificationAtRuntime:
      case mirror::Class::kStatusVerified:
      case mirror::Class::kStatusInitialized:
        break;  // Expected states.
      default:
        LOG(FATAL) << "Unexpected class status for class "
            << PrettyDescriptor(ref.first->GetClassDescriptor(ref.first->GetClassDef(ref.second)))
            << " of " << status;
    }
    CompiledClass* compiled_class = new CompiledClass(status);
    compiled_classes_.Overwrite(ref, compiled_class);
  }
}

CompiledMethod* CompilerDriver::GetCompiledMethod(MethodReference ref) const {
  MutexLock mu(Thread::Current(), compiled_methods_lock_);
  MethodTable::const_iterator it = compiled_methods_.find(ref);
  if (it == compiled_methods_.end()) {
    return NULL;
  }
  CHECK(it->second != NULL);
  return it->second;
}

void CompilerDriver::SetBitcodeFileName(std::string const& filename) {
  typedef void (*SetBitcodeFileNameFn)(CompilerDriver&, std::string const&);

  SetBitcodeFileNameFn set_bitcode_file_name =
    reinterpret_cast<SetBitcodeFileNameFn>(compilerLLVMSetBitcodeFileName);

  set_bitcode_file_name(*this, filename);
}


void CompilerDriver::AddRequiresConstructorBarrier(Thread* self, const DexFile* dex_file,
                                                   uint16_t class_def_index) {
  WriterMutexLock mu(self, freezing_constructor_lock_);
  freezing_constructor_classes_.insert(ClassReference(dex_file, class_def_index));
}

bool CompilerDriver::RequiresConstructorBarrier(Thread* self, const DexFile* dex_file,
                                                uint16_t class_def_index) {
  ReaderMutexLock mu(self, freezing_constructor_lock_);
  return freezing_constructor_classes_.count(ClassReference(dex_file, class_def_index)) != 0;
}

bool CompilerDriver::WriteElf(const std::string& android_root,
                              bool is_host,
                              const std::vector<const art::DexFile*>& dex_files,
                              OatWriter& oat_writer,
                              art::File* file)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
#if defined(ART_USE_PORTABLE_COMPILER)
  return art::ElfWriterMclinker::Create(file, oat_writer, dex_files, android_root, is_host, *this);
#else
  return art::ElfWriterQuick::Create(file, oat_writer, dex_files, android_root, is_host, *this);
#endif
}
void CompilerDriver::InstructionSetToLLVMTarget(InstructionSet instruction_set,
                                                std::string& target_triple,
                                                std::string& target_cpu,
                                                std::string& target_attr) {
  switch (instruction_set) {
    case kThumb2:
      target_triple = "thumb-none-linux-gnueabi";
      target_cpu = "cortex-a9";
      target_attr = "+thumb2,+neon,+neonfp,+vfp3,+db";
      break;

    case kArm:
      target_triple = "armv7-none-linux-gnueabi";
      // TODO: Fix for Nexus S.
      target_cpu = "cortex-a9";
      // TODO: Fix for Xoom.
      target_attr = "+v7,+neon,+neonfp,+vfp3,+db";
      break;

    case kX86:
      target_triple = "i386-pc-linux-gnu";
      target_attr = "";
      break;

    case kMips:
      target_triple = "mipsel-unknown-linux";
      target_attr = "mips32r2";
      break;

    default:
      LOG(FATAL) << "Unknown instruction set: " << instruction_set;
    }
  }
}  // namespace art
