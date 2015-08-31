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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <valgrind.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "base/stl_util.h"
#include "base/stringpiece.h"
#include "base/timing_logger.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "dex_file-inl.h"
#include "driver/compiler_driver.h"
#include "elf_fixup.h"
#include "elf_stripper.h"
#include "gc/space/image_space.h"
#include "gc/space/space-inl.h"
#include "image_writer.h"
#include "leb128.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "oat_writer.h"
#include "object_utils.h"
#include "os.h"
#include "runtime.h"
#include "ScopedLocalRef.h"
#include "scoped_thread_state_change.h"
#include "sirt_ref.h"
#include "vector_output_stream.h"
#include "well_known_classes.h"
#include "zip_archive.h"

namespace art {

static void UsageErrorV(const char* fmt, va_list ap) {
  std::string error;
  StringAppendV(&error, fmt, ap);
  LOG(ERROR) << error;
}

static void UsageError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);
}

static void Usage(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);

  UsageError("Usage: dex2oat [options]...");
  UsageError("");
  UsageError("  --dex-file=<dex-file>: specifies a .dex file to compile.");
  UsageError("      Example: --dex-file=/system/framework/core.jar");
  UsageError("");
  UsageError("  --zip-fd=<file-descriptor>: specifies a file descriptor of a zip file");
  UsageError("      containing a classes.dex file to compile.");
  UsageError("      Example: --zip-fd=5");
  UsageError("");
  UsageError("  --zip-location=<zip-location>: specifies a symbolic name for the file");
  UsageError("      corresponding to the file descriptor specified by --zip-fd.");
  UsageError("      Example: --zip-location=/system/app/Calculator.apk");
  UsageError("");
  UsageError("  --oat-file=<file.oat>: specifies the oat output destination via a filename.");
  UsageError("      Example: --oat-file=/system/framework/boot.oat");
  UsageError("");
  UsageError("  --oat-fd=<number>: specifies the oat output destination via a file descriptor.");
  UsageError("      Example: --oat-file=/system/framework/boot.oat");
  UsageError("");
  UsageError("  --oat-location=<oat-name>: specifies a symbolic name for the file corresponding");
  UsageError("      to the file descriptor specified by --oat-fd.");
  UsageError("      Example: --oat-location=/data/dalvik-cache/system@app@Calculator.apk.oat");
  UsageError("");
  UsageError("  --oat-symbols=<file.oat>: specifies the oat output destination with full symbols.");
  UsageError("      Example: --oat-symbols=/symbols/system/framework/boot.oat");
  UsageError("");
  UsageError("  --bitcode=<file.bc>: specifies the optional bitcode filename.");
  UsageError("      Example: --bitcode=/system/framework/boot.bc");
  UsageError("");
  UsageError("  --image=<file.art>: specifies the output image filename.");
  UsageError("      Example: --image=/system/framework/boot.art");
  UsageError("");
  UsageError("  --image-classes=<classname-file>: specifies classes to include in an image.");
  UsageError("      Example: --image=frameworks/base/preloaded-classes");
  UsageError("");
  UsageError("  --base=<hex-address>: specifies the base address when creating a boot image.");
  UsageError("      Example: --base=0x50000000");
  UsageError("");
  UsageError("  --boot-image=<file.art>: provide the image file for the boot class path.");
  UsageError("      Example: --boot-image=/system/framework/boot.art");
  UsageError("      Default: <host-prefix>/system/framework/boot.art");
  UsageError("");
  UsageError("  --host-prefix=<path>: used to translate host paths to target paths during");
  UsageError("      cross compilation.");
  UsageError("      Example: --host-prefix=out/target/product/crespo");
  UsageError("      Default: $ANDROID_PRODUCT_OUT");
  UsageError("");
  UsageError("  --android-root=<path>: used to locate libraries for portable linking.");
  UsageError("      Example: --android-root=out/host/linux-x86");
  UsageError("      Default: $ANDROID_ROOT");
  UsageError("");
  UsageError("  --instruction-set=(arm|mips|x86): compile for a particular instruction");
  UsageError("      set.");
  UsageError("      Example: --instruction-set=x86");
  UsageError("      Default: arm");
  UsageError("");
  UsageError("  --compiler-backend=(Quick|QuickGBC|Portable): select compiler backend");
  UsageError("      set.");
  UsageError("      Example: --instruction-set=Portable");
  UsageError("      Default: Quick");
  UsageError("");
  UsageError("  --host: used with Portable backend to link against host runtime libraries");
  UsageError("");
  UsageError("  --dump-timing: display a breakdown of where time was spent");
  UsageError("");
  UsageError("  --runtime-arg <argument>: used to specify various arguments for the runtime,");
  UsageError("      such as initial heap size, maximum heap size, and verbose output.");
  UsageError("      Use a separate --runtime-arg switch for each argument.");
  UsageError("      Example: --runtime-arg -Xms256m");
  UsageError("");
  std::cerr << "See log for usage error information\n";
  exit(EXIT_FAILURE);
}

class Dex2Oat {
 public:
  static bool Create(Dex2Oat** p_dex2oat,
                     Runtime::Options& options,
                     CompilerBackend compiler_backend,
                     InstructionSet instruction_set,
                     size_t thread_count)
      SHARED_TRYLOCK_FUNCTION(true, Locks::mutator_lock_) {
    if (!CreateRuntime(options, instruction_set)) {
      *p_dex2oat = NULL;
      return false;
    }
    *p_dex2oat = new Dex2Oat(Runtime::Current(), compiler_backend, instruction_set, thread_count);
    return true;
  }

  ~Dex2Oat() {
    delete runtime_;
    VLOG(compiler) << "dex2oat took " << PrettyDuration(NanoTime() - start_ns_)
              << " (threads: " << thread_count_ << ")";
  }


  // Reads the class names (java.lang.Object) and returns a set of descriptors (Ljava/lang/Object;)
  CompilerDriver::DescriptorSet* ReadImageClassesFromFile(const char* image_classes_filename) {
    UniquePtr<std::ifstream> image_classes_file(new std::ifstream(image_classes_filename,
                                                                  std::ifstream::in));
    if (image_classes_file.get() == NULL) {
      LOG(ERROR) << "Failed to open image classes file " << image_classes_filename;
      return NULL;
    }
    UniquePtr<CompilerDriver::DescriptorSet> result(ReadImageClasses(*image_classes_file.get()));
    image_classes_file->close();
    return result.release();
  }

  CompilerDriver::DescriptorSet* ReadImageClasses(std::istream& image_classes_stream) {
    UniquePtr<CompilerDriver::DescriptorSet> image_classes(new CompilerDriver::DescriptorSet);
    while (image_classes_stream.good()) {
      std::string dot;
      std::getline(image_classes_stream, dot);
      if (StartsWith(dot, "#") || dot.empty()) {
        continue;
      }
      std::string descriptor(DotToDescriptor(dot.c_str()));
      image_classes->insert(descriptor);
    }
    return image_classes.release();
  }

  // Reads the class names (java.lang.Object) and returns a set of descriptors (Ljava/lang/Object;)
  CompilerDriver::DescriptorSet* ReadImageClassesFromZip(const std::string& zip_filename,
                                                         const char* image_classes_filename) {
    UniquePtr<ZipArchive> zip_archive(ZipArchive::Open(zip_filename));
    if (zip_archive.get() == NULL) {
      LOG(ERROR) << "Failed to open zip file " << zip_filename;
      return NULL;
    }
    UniquePtr<ZipEntry> zip_entry(zip_archive->Find(image_classes_filename));
    if (zip_entry.get() == NULL) {
      LOG(ERROR) << "Failed to find " << image_classes_filename << " within " << zip_filename;
      return NULL;
    }
    UniquePtr<MemMap> image_classes_file(zip_entry->ExtractToMemMap(image_classes_filename));
    if (image_classes_file.get() == NULL) {
      LOG(ERROR) << "Failed to extract " << image_classes_filename << " from " << zip_filename;
      return NULL;
    }
    const std::string image_classes_string(reinterpret_cast<char*>(image_classes_file->Begin()),
                                           image_classes_file->Size());
    std::istringstream image_classes_stream(image_classes_string);
    return ReadImageClasses(image_classes_stream);
  }

  const CompilerDriver* CreateOatFile(const std::string& boot_image_option,
                                      const std::string* host_prefix,
                                      const std::string& android_root,
                                      bool is_host,
                                      const std::vector<const DexFile*>& dex_files,
                                      File* oat_file,
                                      const std::string& bitcode_filename,
                                      bool image,
                                      UniquePtr<CompilerDriver::DescriptorSet>& image_classes,
                                      bool dump_stats,
                                      base::TimingLogger& timings) {
    // SirtRef and ClassLoader creation needs to come after Runtime::Create
    jobject class_loader = NULL;
    Thread* self = Thread::Current();
    if (!boot_image_option.empty()) {
      ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
      std::vector<const DexFile*> class_path_files(dex_files);
      OpenClassPathFiles(runtime_->GetClassPathString(), class_path_files);
      ScopedObjectAccess soa(self);
      for (size_t i = 0; i < class_path_files.size(); i++) {
        class_linker->RegisterDexFile(*class_path_files[i]);
      }
      soa.Env()->AllocObject(WellKnownClasses::dalvik_system_PathClassLoader);
      ScopedLocalRef<jobject> class_loader_local(soa.Env(),
          soa.Env()->AllocObject(WellKnownClasses::dalvik_system_PathClassLoader));
      class_loader = soa.Env()->NewGlobalRef(class_loader_local.get());
      Runtime::Current()->SetCompileTimeClassPath(class_loader, class_path_files);
    }

    UniquePtr<CompilerDriver> driver(new CompilerDriver(compiler_backend_,
                                                        instruction_set_,
                                                        image,
                                                        image_classes.release(),
                                                        thread_count_,
                                                        dump_stats));

    if (compiler_backend_ == kPortable) {
      driver->SetBitcodeFileName(bitcode_filename);
    }

    driver->CompileAll(class_loader, dex_files, timings);

    timings.NewSplit("dex2oat OatWriter");
    std::string image_file_location;
    uint32_t image_file_location_oat_checksum = 0;
    uint32_t image_file_location_oat_data_begin = 0;
    if (!driver->IsImage()) {
      gc::space::ImageSpace* image_space = Runtime::Current()->GetHeap()->GetImageSpace();
      image_file_location_oat_checksum = image_space->GetImageHeader().GetOatChecksum();
      image_file_location_oat_data_begin =
          reinterpret_cast<uint32_t>(image_space->GetImageHeader().GetOatDataBegin());
      image_file_location = image_space->GetImageFilename();
      if (host_prefix != NULL && StartsWith(image_file_location, host_prefix->c_str())) {
        image_file_location = image_file_location.substr(host_prefix->size());
      }
    }

    OatWriter oat_writer(dex_files,
                         image_file_location_oat_checksum,
                         image_file_location_oat_data_begin,
                         image_file_location,
                         driver.get());

    if (!driver->WriteElf(android_root, is_host, dex_files, oat_writer, oat_file)) {
      LOG(ERROR) << "Failed to write ELF file " << oat_file->GetPath();
      return NULL;
    }

    return driver.release();
  }

  bool CreateImageFile(const std::string& image_filename,
                       uintptr_t image_base,
                       const std::string& oat_filename,
                       const std::string& oat_location,
                       const CompilerDriver& compiler)
      LOCKS_EXCLUDED(Locks::mutator_lock_) {
    uintptr_t oat_data_begin;
    {
      // ImageWriter is scoped so it can free memory before doing FixupElf
      ImageWriter image_writer(compiler);
      if (!image_writer.Write(image_filename, image_base, oat_filename, oat_location)) {
        LOG(ERROR) << "Failed to create image file " << image_filename;
        return false;
      }
      oat_data_begin = image_writer.GetOatDataBegin();
    }

    UniquePtr<File> oat_file(OS::OpenFileReadWrite(oat_filename.c_str()));
    if (oat_file.get() == NULL) {
      PLOG(ERROR) << "Failed to open ELF file: " << oat_filename;
      return false;
    }
    if (!ElfFixup::Fixup(oat_file.get(), oat_data_begin)) {
      LOG(ERROR) << "Failed to fixup ELF file " << oat_file->GetPath();
      return false;
    }
    return true;
  }

 private:
  explicit Dex2Oat(Runtime* runtime,
                   CompilerBackend compiler_backend,
                   InstructionSet instruction_set,
                   size_t thread_count)
      : compiler_backend_(compiler_backend),
        instruction_set_(instruction_set),
        runtime_(runtime),
        thread_count_(thread_count),
        start_ns_(NanoTime()) {
  }

  static bool CreateRuntime(Runtime::Options& options, InstructionSet instruction_set)
      SHARED_TRYLOCK_FUNCTION(true, Locks::mutator_lock_) {
    if (!Runtime::Create(options, false)) {
      LOG(ERROR) << "Failed to create runtime";
      return false;
    }
    Runtime* runtime = Runtime::Current();
    // if we loaded an existing image, we will reuse values from the image roots.
    if (!runtime->HasResolutionMethod()) {
      runtime->SetResolutionMethod(runtime->CreateResolutionMethod());
    }
    for (int i = 0; i < Runtime::kLastCalleeSaveType; i++) {
      Runtime::CalleeSaveType type = Runtime::CalleeSaveType(i);
      if (!runtime->HasCalleeSaveMethod(type)) {
        runtime->SetCalleeSaveMethod(runtime->CreateCalleeSaveMethod(instruction_set, type), type);
      }
    }
    runtime->GetClassLinker()->FixupDexCaches(runtime->GetResolutionMethod());
    return true;
  }

  // Appends to dex_files any elements of class_path that it doesn't already
  // contain. This will open those dex files as necessary.
  static void OpenClassPathFiles(const std::string& class_path,
                                 std::vector<const DexFile*>& dex_files) {
    std::vector<std::string> parsed;
    Split(class_path, ':', parsed);
    // Take Locks::mutator_lock_ so that lock ordering on the ClassLinker::dex_lock_ is maintained.
    ScopedObjectAccess soa(Thread::Current());
    for (size_t i = 0; i < parsed.size(); ++i) {
      if (DexFilesContains(dex_files, parsed[i])) {
        continue;
      }
      const DexFile* dex_file = DexFile::Open(parsed[i], parsed[i]);
      if (dex_file == NULL) {
        LOG(WARNING) << "Failed to open dex file " << parsed[i];
      } else {
        dex_files.push_back(dex_file);
      }
    }
  }

  // Returns true if dex_files has a dex with the named location.
  static bool DexFilesContains(const std::vector<const DexFile*>& dex_files,
                               const std::string& location) {
    for (size_t i = 0; i < dex_files.size(); ++i) {
      if (dex_files[i]->GetLocation() == location) {
        return true;
      }
    }
    return false;
  }

  const CompilerBackend compiler_backend_;

  const InstructionSet instruction_set_;

  Runtime* runtime_;
  size_t thread_count_;
  uint64_t start_ns_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Dex2Oat);
};

static bool ParseInt(const char* in, int* out) {
  char* end;
  int result = strtol(in, &end, 10);
  if (in == end || *end != '\0') {
    return false;
  }
  *out = result;
  return true;
}

static size_t OpenDexFiles(const std::vector<const char*>& dex_filenames,
                           const std::vector<const char*>& dex_locations,
                           std::vector<const DexFile*>& dex_files) {
  size_t failure_count = 0;
  for (size_t i = 0; i < dex_filenames.size(); i++) {
    const char* dex_filename = dex_filenames[i];
    const char* dex_location = dex_locations[i];
    std::string error_msg;
    if (!OS::FileExists(dex_filename)) {
      LOG(WARNING) << "Skipping non-existent dex file '" << dex_filename << "'";
      continue;
    }
    const DexFile* dex_file = DexFile::Open(dex_filename, dex_location);
    if (dex_file == NULL) {
      LOG(WARNING) << "Failed to open .dex from file '" << dex_filename << "'\n";
      ++failure_count;
    } else {
      dex_files.push_back(dex_file);
    }
  }
  return failure_count;
}

// The primary goal of the watchdog is to prevent stuck build servers
// during development when fatal aborts lead to a cascade of failures
// that result in a deadlock.
class WatchDog {
// WatchDog defines its own CHECK_PTHREAD_CALL to avoid using Log which uses locks
#undef CHECK_PTHREAD_CALL
#define CHECK_WATCH_DOG_PTHREAD_CALL(call, args, what) \
  do { \
    int rc = call args; \
    if (rc != 0) { \
      errno = rc; \
      std::string message(# call); \
      message += " failed for "; \
      message += reason; \
      Fatal(message); \
    } \
  } while (false)

 public:
  explicit WatchDog(bool is_watch_dog_enabled) {
    is_watch_dog_enabled_ = is_watch_dog_enabled;
    if (!is_watch_dog_enabled_) {
      return;
    }
    shutting_down_ = false;
    const char* reason = "dex2oat watch dog thread startup";
    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_mutex_init, (&mutex_, NULL), reason);
    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_cond_init, (&cond_, NULL), reason);
    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_attr_init, (&attr_), reason);
    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_create, (&pthread_, &attr_, &CallBack, this), reason);
    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_attr_destroy, (&attr_), reason);
  }
  ~WatchDog() {
    if (!is_watch_dog_enabled_) {
      return;
    }
    const char* reason = "dex2oat watch dog thread shutdown";
    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_mutex_lock, (&mutex_), reason);
    shutting_down_ = true;
    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_cond_signal, (&cond_), reason);
    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_mutex_unlock, (&mutex_), reason);

    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_join, (pthread_, NULL), reason);

    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_cond_destroy, (&cond_), reason);
    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_mutex_destroy, (&mutex_), reason);
  }

 private:
  static void* CallBack(void* arg) {
    WatchDog* self = reinterpret_cast<WatchDog*>(arg);
    ::art::SetThreadName("dex2oat watch dog");
    self->Wait();
    return NULL;
  }

  static void Message(char severity, const std::string& message) {
    // TODO: Remove when we switch to LOG when we can guarantee it won't prevent shutdown in error
    //       cases.
    fprintf(stderr, "dex2oat%s %c %d %d %s\n",
            kIsDebugBuild ? "d" : "",
            severity,
            getpid(),
            GetTid(),
            message.c_str());
  }

  static void Warn(const std::string& message) {
    Message('W', message);
  }

  static void Fatal(const std::string& message) {
    Message('F', message);
    exit(1);
  }

  void Wait() {
    bool warning = true;
    CHECK_GT(kWatchDogTimeoutSeconds, kWatchDogWarningSeconds);
    // TODO: tune the multiplier for GC verification, the following is just to make the timeout
    //       large.
    int64_t multiplier = gc::kDesiredHeapVerification > gc::kVerifyAllFast ? 100 : 1;
    timespec warning_ts;
    InitTimeSpec(true, CLOCK_REALTIME, multiplier * kWatchDogWarningSeconds * 1000, 0, &warning_ts);
    timespec timeout_ts;
    InitTimeSpec(true, CLOCK_REALTIME, multiplier * kWatchDogTimeoutSeconds * 1000, 0, &timeout_ts);
    const char* reason = "dex2oat watch dog thread waiting";
    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_mutex_lock, (&mutex_), reason);
    while (!shutting_down_) {
      int rc = TEMP_FAILURE_RETRY(pthread_cond_timedwait(&cond_, &mutex_,
                                                         warning ? &warning_ts
                                                                 : &timeout_ts));
      if (rc == ETIMEDOUT) {
        std::string message(StringPrintf("dex2oat did not finish after %d seconds",
                                         warning ? kWatchDogWarningSeconds
                                                 : kWatchDogTimeoutSeconds));
        if (warning) {
          Warn(message.c_str());
          warning = false;
        } else {
          Fatal(message.c_str());
        }
      } else if (rc != 0) {
        std::string message(StringPrintf("pthread_cond_timedwait failed: %s",
                                         strerror(errno)));
        Fatal(message.c_str());
      }
    }
    CHECK_WATCH_DOG_PTHREAD_CALL(pthread_mutex_unlock, (&mutex_), reason);
  }

  // When setting timeouts, keep in mind that the build server may not be as fast as your desktop.
#if ART_USE_PORTABLE_COMPILER
  static const unsigned int kWatchDogWarningSeconds =  2 * 60;  // 2 minutes.
  static const unsigned int kWatchDogTimeoutSeconds = 30 * 60;  // 25 minutes + buffer.
#else
  static const unsigned int kWatchDogWarningSeconds =  1 * 60;  // 1 minute.
  static const unsigned int kWatchDogTimeoutSeconds =  6 * 60;  // 5 minutes + buffer.
#endif

  bool is_watch_dog_enabled_;
  bool shutting_down_;
  // TODO: Switch to Mutex when we can guarantee it won't prevent shutdown in error cases.
  pthread_mutex_t mutex_;
  pthread_cond_t cond_;
  pthread_attr_t attr_;
  pthread_t pthread_;
};
const unsigned int WatchDog::kWatchDogWarningSeconds;
const unsigned int WatchDog::kWatchDogTimeoutSeconds;

static int dex2oat(int argc, char** argv) {
  base::TimingLogger timings("compiler", false, false);

  InitLogging(argv);

  // Skip over argv[0].
  argv++;
  argc--;

  if (argc == 0) {
    Usage("No arguments specified");
  }

  std::vector<const char*> dex_filenames;
  std::vector<const char*> dex_locations;
  int zip_fd = -1;
  std::string zip_location;
  std::string oat_filename;
  std::string oat_symbols;
  std::string oat_location;
  int oat_fd = -1;
  std::string bitcode_filename;
  const char* image_classes_zip_filename = NULL;
  const char* image_classes_filename = NULL;
  std::string image_filename;
  std::string boot_image_filename;
  uintptr_t image_base = 0;
  UniquePtr<std::string> host_prefix;
  std::string android_root;
  std::vector<const char*> runtime_args;
  int thread_count = sysconf(_SC_NPROCESSORS_CONF);
#if defined(ART_USE_PORTABLE_COMPILER)
  CompilerBackend compiler_backend = kPortable;
#else
  CompilerBackend compiler_backend = kQuick;
#endif
#if defined(__arm__)
  InstructionSet instruction_set = kThumb2;
#elif defined(__i386__)
  InstructionSet instruction_set = kX86;
#elif defined(__mips__)
  InstructionSet instruction_set = kMips;
#else
#error "Unsupported architecture"
#endif
  bool is_host = false;
  bool dump_stats = kIsDebugBuild;
  bool dump_timing = false;
  bool dump_slow_timing = kIsDebugBuild;
  bool watch_dog_enabled = !kIsTargetBuild;


  for (int i = 0; i < argc; i++) {
    const StringPiece option(argv[i]);
    bool log_options = false;
    if (log_options) {
      LOG(INFO) << "dex2oat: option[" << i << "]=" << argv[i];
    }
    if (option.starts_with("--dex-file=")) {
      dex_filenames.push_back(option.substr(strlen("--dex-file=")).data());
    } else if (option.starts_with("--dex-location=")) {
      dex_locations.push_back(option.substr(strlen("--dex-location=")).data());
    } else if (option.starts_with("--zip-fd=")) {
      const char* zip_fd_str = option.substr(strlen("--zip-fd=")).data();
      if (!ParseInt(zip_fd_str, &zip_fd)) {
        Usage("Failed to parse --zip-fd argument '%s' as an integer", zip_fd_str);
      }
    } else if (option.starts_with("--zip-location=")) {
      zip_location = option.substr(strlen("--zip-location=")).data();
    } else if (option.starts_with("--oat-file=")) {
      oat_filename = option.substr(strlen("--oat-file=")).data();
    } else if (option.starts_with("--oat-symbols=")) {
      oat_symbols = option.substr(strlen("--oat-symbols=")).data();
    } else if (option.starts_with("--oat-fd=")) {
      const char* oat_fd_str = option.substr(strlen("--oat-fd=")).data();
      if (!ParseInt(oat_fd_str, &oat_fd)) {
        Usage("Failed to parse --oat-fd argument '%s' as an integer", oat_fd_str);
      }
    } else if (option == "--watch-dog") {
      watch_dog_enabled = true;
    } else if (option == "--no-watch-dog") {
      watch_dog_enabled = false;
    } else if (option.starts_with("-j")) {
      const char* thread_count_str = option.substr(strlen("-j")).data();
      if (!ParseInt(thread_count_str, &thread_count)) {
        Usage("Failed to parse -j argument '%s' as an integer", thread_count_str);
      }
    } else if (option.starts_with("--oat-location=")) {
      oat_location = option.substr(strlen("--oat-location=")).data();
    } else if (option.starts_with("--bitcode=")) {
      bitcode_filename = option.substr(strlen("--bitcode=")).data();
    } else if (option.starts_with("--image=")) {
      image_filename = option.substr(strlen("--image=")).data();
    } else if (option.starts_with("--image-classes=")) {
      image_classes_filename = option.substr(strlen("--image-classes=")).data();
    } else if (option.starts_with("--image-classes-zip=")) {
      image_classes_zip_filename = option.substr(strlen("--image-classes-zip=")).data();
    } else if (option.starts_with("--base=")) {
      const char* image_base_str = option.substr(strlen("--base=")).data();
      char* end;
      image_base = strtoul(image_base_str, &end, 16);
      if (end == image_base_str || *end != '\0') {
        Usage("Failed to parse hexadecimal value for option %s", option.data());
      }
    } else if (option.starts_with("--boot-image=")) {
      boot_image_filename = option.substr(strlen("--boot-image=")).data();
    } else if (option.starts_with("--host-prefix=")) {
      host_prefix.reset(new std::string(option.substr(strlen("--host-prefix=")).data()));
    } else if (option.starts_with("--android-root=")) {
      android_root = option.substr(strlen("--android-root=")).data();
    } else if (option.starts_with("--instruction-set=")) {
      StringPiece instruction_set_str = option.substr(strlen("--instruction-set=")).data();
      if (instruction_set_str == "arm") {
        instruction_set = kThumb2;
      } else if (instruction_set_str == "mips") {
        instruction_set = kMips;
      } else if (instruction_set_str == "x86") {
        instruction_set = kX86;
      }
    } else if (option.starts_with("--compiler-backend=")) {
      StringPiece backend_str = option.substr(strlen("--compiler-backend=")).data();
      if (backend_str == "Quick") {
        compiler_backend = kQuick;
      } else if (backend_str == "Portable") {
        compiler_backend = kPortable;
      }
    } else if (option == "--host") {
      is_host = true;
    } else if (option == "--runtime-arg") {
      if (++i >= argc) {
        Usage("Missing required argument for --runtime-arg");
      }
      if (log_options) {
        LOG(INFO) << "dex2oat: option[" << i << "]=" << argv[i];
      }
      runtime_args.push_back(argv[i]);
    } else if (option == "--dump-timing") {
      dump_timing = true;
    } else {
      Usage("Unknown argument %s", option.data());
    }
  }

  if (oat_filename.empty() && oat_fd == -1) {
    Usage("Output must be supplied with either --oat-file or --oat-fd");
  }

  if (!oat_filename.empty() && oat_fd != -1) {
    Usage("--oat-file should not be used with --oat-fd");
  }

  if (!oat_symbols.empty() && oat_fd != -1) {
    Usage("--oat-symbols should not be used with --oat-fd");
  }

  if (!oat_symbols.empty() && is_host) {
    Usage("--oat-symbols should not be used with --host");
  }

  if (oat_fd != -1 && !image_filename.empty()) {
    Usage("--oat-fd should not be used with --image");
  }

  if (host_prefix.get() == NULL) {
    const char* android_product_out = getenv("ANDROID_PRODUCT_OUT");
    if (android_product_out != NULL) {
        host_prefix.reset(new std::string(android_product_out));
    }
  }

  if (android_root.empty()) {
    const char* android_root_env_var = getenv("ANDROID_ROOT");
    if (android_root_env_var == NULL) {
      Usage("--android-root unspecified and ANDROID_ROOT not set");
    }
    android_root += android_root_env_var;
  }

  bool image = (!image_filename.empty());
  if (!image && boot_image_filename.empty()) {
    if (host_prefix.get() == NULL) {
      boot_image_filename += GetAndroidRoot();
    } else {
      boot_image_filename += *host_prefix.get();
      boot_image_filename += "/system";
    }
    boot_image_filename += "/framework/boot.art";
  }
  std::string boot_image_option;
  if (!boot_image_filename.empty()) {
    boot_image_option += "-Ximage:";
    boot_image_option += boot_image_filename;
  }

  if (image_classes_filename != NULL && !image) {
    Usage("--image-classes should only be used with --image");
  }

  if (image_classes_filename != NULL && !boot_image_option.empty()) {
    Usage("--image-classes should not be used with --boot-image");
  }

  if (image_classes_zip_filename != NULL && image_classes_filename == NULL) {
    Usage("--image-classes-zip should be used with --image-classes");
  }

  if (dex_filenames.empty() && zip_fd == -1) {
    Usage("Input must be supplied with either --dex-file or --zip-fd");
  }

  if (!dex_filenames.empty() && zip_fd != -1) {
    Usage("--dex-file should not be used with --zip-fd");
  }

  if (!dex_filenames.empty() && !zip_location.empty()) {
    Usage("--dex-file should not be used with --zip-location");
  }

  if (dex_locations.empty()) {
    for (size_t i = 0; i < dex_filenames.size(); i++) {
      dex_locations.push_back(dex_filenames[i]);
    }
  } else if (dex_locations.size() != dex_filenames.size()) {
    Usage("--dex-location arguments do not match --dex-file arguments");
  }

  if (zip_fd != -1 && zip_location.empty()) {
    Usage("--zip-location should be supplied with --zip-fd");
  }

  if (boot_image_option.empty()) {
    if (image_base == 0) {
      Usage("Non-zero --base not specified");
    }
  }

  std::string oat_stripped(oat_filename);
  std::string oat_unstripped;
  if (!oat_symbols.empty()) {
    oat_unstripped += oat_symbols;
  } else {
    oat_unstripped += oat_filename;
  }

  // Done with usage checks, enable watchdog if requested
  WatchDog watch_dog(watch_dog_enabled);

  // Check early that the result of compilation can be written
  UniquePtr<File> oat_file;
  bool create_file = !oat_unstripped.empty();  // as opposed to using open file descriptor
  if (create_file) {
    oat_file.reset(OS::CreateEmptyFile(oat_unstripped.c_str()));
    if (oat_location.empty()) {
      oat_location = oat_filename;
    }
  } else {
    oat_file.reset(new File(oat_fd, oat_location));
    oat_file->DisableAutoClose();
  }
  if (oat_file.get() == NULL) {
    PLOG(ERROR) << "Failed to create oat file: " << oat_location;
    return EXIT_FAILURE;
  }
  if (create_file && fchmod(oat_file->Fd(), 0644) != 0) {
    PLOG(ERROR) << "Failed to make oat file world readable: " << oat_location;
    return EXIT_FAILURE;
  }

  timings.StartSplit("dex2oat Setup");
  LOG(INFO) << "dex2oat: " << oat_location;

  if (image) {
    bool has_compiler_filter = false;
    for (const char* r : runtime_args) {
      if (strncmp(r, "-compiler-filter:", 17) == 0) {
        has_compiler_filter = true;
        break;
      }
    }
    if (!has_compiler_filter) {
      runtime_args.push_back("-compiler-filter:everything");
    }
  }

  Runtime::Options options;
  options.push_back(std::make_pair("compiler", reinterpret_cast<void*>(NULL)));
  std::vector<const DexFile*> boot_class_path;
  if (boot_image_option.empty()) {
    size_t failure_count = OpenDexFiles(dex_filenames, dex_locations, boot_class_path);
    if (failure_count > 0) {
      LOG(ERROR) << "Failed to open some dex files: " << failure_count;
      return EXIT_FAILURE;
    }
    options.push_back(std::make_pair("bootclasspath", &boot_class_path));
  } else {
    options.push_back(std::make_pair(boot_image_option.c_str(), reinterpret_cast<void*>(NULL)));
  }
  if (host_prefix.get() != NULL) {
    options.push_back(std::make_pair("host-prefix", host_prefix->c_str()));
  }
  for (size_t i = 0; i < runtime_args.size(); i++) {
    options.push_back(std::make_pair(runtime_args[i], reinterpret_cast<void*>(NULL)));
  }

#ifdef ART_SEA_IR_MODE
  options.push_back(std::make_pair("-sea_ir", reinterpret_cast<void*>(NULL)));
#endif

  Dex2Oat* p_dex2oat;
  if (!Dex2Oat::Create(&p_dex2oat, options, compiler_backend, instruction_set, thread_count)) {
    LOG(ERROR) << "Failed to create dex2oat";
    return EXIT_FAILURE;
  }
  UniquePtr<Dex2Oat> dex2oat(p_dex2oat);
  // Runtime::Create acquired the mutator_lock_ that is normally given away when we Runtime::Start,
  // give it away now so that we don't starve GC.
  Thread* self = Thread::Current();
  self->TransitionFromRunnableToSuspended(kNative);
  // If we're doing the image, override the compiler filter to force full compilation. Must be
  // done ahead of WellKnownClasses::Init that causes verification.  Note: doesn't force
  // compilation of class initializers.
  // Whilst we're in native take the opportunity to initialize well known classes.
  WellKnownClasses::Init(self->GetJniEnv());

  // If --image-classes was specified, calculate the full list of classes to include in the image
  UniquePtr<CompilerDriver::DescriptorSet> image_classes(NULL);
  if (image_classes_filename != NULL) {
    if (image_classes_zip_filename != NULL) {
      image_classes.reset(dex2oat->ReadImageClassesFromZip(image_classes_zip_filename,
                                                           image_classes_filename));
    } else {
      image_classes.reset(dex2oat->ReadImageClassesFromFile(image_classes_filename));
    }
    if (image_classes.get() == NULL) {
      LOG(ERROR) << "Failed to create list of image classes from " << image_classes_filename;
      return EXIT_FAILURE;
    }
  }

  std::vector<const DexFile*> dex_files;
  if (boot_image_option.empty()) {
    dex_files = Runtime::Current()->GetClassLinker()->GetBootClassPath();
  } else {
    if (dex_filenames.empty()) {
      UniquePtr<ZipArchive> zip_archive(ZipArchive::OpenFromFd(zip_fd));
      if (zip_archive.get() == NULL) {
        LOG(ERROR) << "Failed to open zip from file descriptor for " << zip_location;
        return EXIT_FAILURE;
      }
      const DexFile* dex_file = DexFile::Open(*zip_archive.get(), zip_location);
      if (dex_file == NULL) {
        LOG(ERROR) << "Failed to open dex from file descriptor for zip file: " << zip_location;
        return EXIT_FAILURE;
      }
      dex_files.push_back(dex_file);
    } else {
      size_t failure_count = OpenDexFiles(dex_filenames, dex_locations, dex_files);
      if (failure_count > 0) {
        LOG(ERROR) << "Failed to open some dex files: " << failure_count;
        return EXIT_FAILURE;
      }
    }

    // Ensure opened dex files are writable for dex-to-dex transformations.
    for (const auto& dex_file : dex_files) {
      if (!dex_file->EnableWrite()) {
        PLOG(ERROR) << "Failed to make .dex file writeable '" << dex_file->GetLocation() << "'\n";
      }
    }
  }

  /*
   * If we're not in interpret-only mode, go ahead and compile small applications. Don't
   * bother to check if we're doing the image.
   */
  if (!image && (Runtime::Current()->GetCompilerFilter() != Runtime::kInterpretOnly)) {
    size_t num_methods = 0;
    for (size_t i = 0; i != dex_files.size(); ++i) {
      const DexFile* dex_file = dex_files[i];
      CHECK(dex_file != NULL);
      num_methods += dex_file->NumMethodIds();
    }
    if (num_methods <= Runtime::Current()->GetNumDexMethodsThreshold()) {
      Runtime::Current()->SetCompilerFilter(Runtime::kSpeed);
      VLOG(compiler) << "Below method threshold, compiling anyways";
    }
  }

  UniquePtr<const CompilerDriver> compiler(dex2oat->CreateOatFile(boot_image_option,
                                                                  host_prefix.get(),
                                                                  android_root,
                                                                  is_host,
                                                                  dex_files,
                                                                  oat_file.get(),
                                                                  bitcode_filename,
                                                                  image,
                                                                  image_classes,
                                                                  dump_stats,
                                                                  timings));

  if (compiler.get() == NULL) {
    LOG(ERROR) << "Failed to create oat file: " << oat_location;
    return EXIT_FAILURE;
  }

  VLOG(compiler) << "Oat file written successfully (unstripped): " << oat_location;

  // Notes on the interleaving of creating the image and oat file to
  // ensure the references between the two are correct.
  //
  // Currently we have a memory layout that looks something like this:
  //
  // +--------------+
  // | image        |
  // +--------------+
  // | boot oat     |
  // +--------------+
  // | alloc spaces |
  // +--------------+
  //
  // There are several constraints on the loading of the image and boot.oat.
  //
  // 1. The image is expected to be loaded at an absolute address and
  // contains Objects with absolute pointers within the image.
  //
  // 2. There are absolute pointers from Methods in the image to their
  // code in the oat.
  //
  // 3. There are absolute pointers from the code in the oat to Methods
  // in the image.
  //
  // 4. There are absolute pointers from code in the oat to other code
  // in the oat.
  //
  // To get this all correct, we go through several steps.
  //
  // 1. We have already created that oat file above with
  // CreateOatFile. Originally this was just our own proprietary file
  // but now it is contained within an ELF dynamic object (aka an .so
  // file). The Compiler returned by CreateOatFile provides
  // PatchInformation for references to oat code and Methods that need
  // to be update once we know where the oat file will be located
  // after the image.
  //
  // 2. We create the image file. It needs to know where the oat file
  // will be loaded after itself. Originally when oat file was simply
  // memory mapped so we could predict where its contents were based
  // on the file size. Now that it is an ELF file, we need to inspect
  // the ELF file to understand the in memory segment layout including
  // where the oat header is located within. ImageWriter's
  // PatchOatCodeAndMethods uses the PatchInformation from the
  // Compiler to touch up absolute references in the oat file.
  //
  // 3. We fixup the ELF program headers so that dlopen will try to
  // load the .so at the desired location at runtime by offsetting the
  // Elf32_Phdr.p_vaddr values by the desired base address.
  //
  if (image) {
    timings.NewSplit("dex2oat ImageWriter");
    bool image_creation_success = dex2oat->CreateImageFile(image_filename,
                                                           image_base,
                                                           oat_unstripped,
                                                           oat_location,
                                                           *compiler.get());
    if (!image_creation_success) {
      return EXIT_FAILURE;
    }
    VLOG(compiler) << "Image written successfully: " << image_filename;
  }

  if (is_host) {
    if (dump_timing || (dump_slow_timing && timings.GetTotalNs() > MsToNs(1000))) {
      LOG(INFO) << Dumpable<base::TimingLogger>(timings);
    }
    return EXIT_SUCCESS;
  }

  // If we don't want to strip in place, copy from unstripped location to stripped location.
  // We need to strip after image creation because FixupElf needs to use .strtab.
  if (oat_unstripped != oat_stripped) {
    timings.NewSplit("dex2oat OatFile copy");
    oat_file.reset();
     UniquePtr<File> in(OS::OpenFileForReading(oat_unstripped.c_str()));
    UniquePtr<File> out(OS::CreateEmptyFile(oat_stripped.c_str()));
    size_t buffer_size = 8192;
    UniquePtr<uint8_t> buffer(new uint8_t[buffer_size]);
    while (true) {
      int bytes_read = TEMP_FAILURE_RETRY(read(in->Fd(), buffer.get(), buffer_size));
      if (bytes_read <= 0) {
        break;
      }
      bool write_ok = out->WriteFully(buffer.get(), bytes_read);
      CHECK(write_ok);
    }
    oat_file.reset(out.release());
    VLOG(compiler) << "Oat file copied successfully (stripped): " << oat_stripped;
  }

#if ART_USE_PORTABLE_COMPILER  // We currently only generate symbols on Portable
  timings.NewSplit("dex2oat ElfStripper");
  // Strip unneeded sections for target
  off_t seek_actual = lseek(oat_file->Fd(), 0, SEEK_SET);
  CHECK_EQ(0, seek_actual);
  ElfStripper::Strip(oat_file.get());


  // We wrote the oat file successfully, and want to keep it.
  VLOG(compiler) << "Oat file written successfully (stripped): " << oat_location;
#endif  // ART_USE_PORTABLE_COMPILER

  timings.EndSplit();

  if (dump_timing || (dump_slow_timing && timings.GetTotalNs() > MsToNs(1000))) {
    LOG(INFO) << Dumpable<base::TimingLogger>(timings);
  }

  // Everything was successfully written, do an explicit exit here to avoid running Runtime
  // destructors that take time (bug 10645725) unless we're a debug build or running on valgrind.
  if (!kIsDebugBuild || (RUNNING_ON_VALGRIND == 0)) {
    exit(EXIT_SUCCESS);
  }

  return EXIT_SUCCESS;
}


}  // namespace art

int main(int argc, char** argv) {
  return art::dex2oat(argc, argv);
}
