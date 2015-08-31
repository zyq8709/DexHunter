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

#include "image_space.h"

#include <sys/types.h>
#include <sys/wait.h>

#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "mirror/art_method.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "oat_file.h"
#include "os.h"
#include "runtime.h"
#include "space-inl.h"
#include "utils.h"

namespace art {
namespace gc {
namespace space {

AtomicInteger ImageSpace::bitmap_index_(0);

ImageSpace::ImageSpace(const std::string& name, MemMap* mem_map,
                       accounting::SpaceBitmap* live_bitmap)
    : MemMapSpace(name, mem_map, mem_map->Size(), kGcRetentionPolicyNeverCollect) {
  DCHECK(live_bitmap != NULL);
  live_bitmap_.reset(live_bitmap);
}

static bool GenerateImage(const std::string& image_file_name) {
  const std::string boot_class_path_string(Runtime::Current()->GetBootClassPathString());
  std::vector<std::string> boot_class_path;
  Split(boot_class_path_string, ':', boot_class_path);
  if (boot_class_path.empty()) {
    LOG(FATAL) << "Failed to generate image because no boot class path specified";
  }

  std::vector<std::string> arg_vector;

  std::string dex2oat(GetAndroidRoot());
  dex2oat += (kIsDebugBuild ? "/bin/dex2oatd" : "/bin/dex2oat");
  arg_vector.push_back(dex2oat);

  std::string image_option_string("--image=");
  image_option_string += image_file_name;
  arg_vector.push_back(image_option_string);

  arg_vector.push_back("--runtime-arg");
  arg_vector.push_back("-Xms64m");

  arg_vector.push_back("--runtime-arg");
  arg_vector.push_back("-Xmx64m");

  for (size_t i = 0; i < boot_class_path.size(); i++) {
    arg_vector.push_back(std::string("--dex-file=") + boot_class_path[i]);
  }

  std::string oat_file_option_string("--oat-file=");
  oat_file_option_string += image_file_name;
  oat_file_option_string.erase(oat_file_option_string.size() - 3);
  oat_file_option_string += "oat";
  arg_vector.push_back(oat_file_option_string);

  arg_vector.push_back(StringPrintf("--base=0x%x", ART_BASE_ADDRESS));

  if (kIsTargetBuild) {
    arg_vector.push_back("--image-classes-zip=/system/framework/framework.jar");
    arg_vector.push_back("--image-classes=preloaded-classes");
  } else {
    arg_vector.push_back("--host");
  }

  std::string command_line(Join(arg_vector, ' '));
  LOG(INFO) << "GenerateImage: " << command_line;

  // Convert the args to char pointers.
  std::vector<char*> char_args;
  for (std::vector<std::string>::iterator it = arg_vector.begin(); it != arg_vector.end();
      ++it) {
    char_args.push_back(const_cast<char*>(it->c_str()));
  }
  char_args.push_back(NULL);

  // fork and exec dex2oat
  pid_t pid = fork();
  if (pid == 0) {
    // no allocation allowed between fork and exec

    // change process groups, so we don't get reaped by ProcessManager
    setpgid(0, 0);

    execv(dex2oat.c_str(), &char_args[0]);

    PLOG(FATAL) << "execv(" << dex2oat << ") failed";
    return false;
  } else {
    if (pid == -1) {
      PLOG(ERROR) << "fork failed";
    }

    // wait for dex2oat to finish
    int status;
    pid_t got_pid = TEMP_FAILURE_RETRY(waitpid(pid, &status, 0));
    if (got_pid != pid) {
      PLOG(ERROR) << "waitpid failed: wanted " << pid << ", got " << got_pid;
      return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      LOG(ERROR) << dex2oat << " failed: " << command_line;
      return false;
    }
  }
  return true;
}

ImageSpace* ImageSpace::Create(const std::string& original_image_file_name) {
  if (OS::FileExists(original_image_file_name.c_str())) {
    // If the /system file exists, it should be up-to-date, don't try to generate
    return space::ImageSpace::Init(original_image_file_name, false);
  }
  // If the /system file didn't exist, we need to use one from the dalvik-cache.
  // If the cache file exists, try to open, but if it fails, regenerate.
  // If it does not exist, generate.
  std::string image_file_name(GetDalvikCacheFilenameOrDie(original_image_file_name));
  if (OS::FileExists(image_file_name.c_str())) {
    space::ImageSpace* image_space = space::ImageSpace::Init(image_file_name, true);
    if (image_space != NULL) {
      return image_space;
    }
  }
  CHECK(GenerateImage(image_file_name)) << "Failed to generate image: " << image_file_name;
  return space::ImageSpace::Init(image_file_name, true);
}

void ImageSpace::VerifyImageAllocations() {
  byte* current = Begin() + RoundUp(sizeof(ImageHeader), kObjectAlignment);
  while (current < End()) {
    DCHECK_ALIGNED(current, kObjectAlignment);
    const mirror::Object* obj = reinterpret_cast<const mirror::Object*>(current);
    CHECK(live_bitmap_->Test(obj));
    CHECK(obj->GetClass() != nullptr) << "Image object at address " << obj << " has null class";
    current += RoundUp(obj->SizeOf(), kObjectAlignment);
  }
}

ImageSpace* ImageSpace::Init(const std::string& image_file_name, bool validate_oat_file) {
  CHECK(!image_file_name.empty());

  uint64_t start_time = 0;
  if (VLOG_IS_ON(heap) || VLOG_IS_ON(startup)) {
    start_time = NanoTime();
    LOG(INFO) << "ImageSpace::Init entering image_file_name=" << image_file_name;
  }

  UniquePtr<File> file(OS::OpenFileForReading(image_file_name.c_str()));
  if (file.get() == NULL) {
    LOG(ERROR) << "Failed to open " << image_file_name;
    return NULL;
  }
  ImageHeader image_header;
  bool success = file->ReadFully(&image_header, sizeof(image_header));
  if (!success || !image_header.IsValid()) {
    LOG(ERROR) << "Invalid image header " << image_file_name;
    return NULL;
  }

  // Note: The image header is part of the image due to mmap page alignment required of offset.
  UniquePtr<MemMap> map(MemMap::MapFileAtAddress(image_header.GetImageBegin(),
                                                 image_header.GetImageSize(),
                                                 PROT_READ | PROT_WRITE,
                                                 MAP_PRIVATE | MAP_FIXED,
                                                 file->Fd(),
                                                 0,
                                                 false));
  if (map.get() == NULL) {
    LOG(ERROR) << "Failed to map " << image_file_name;
    return NULL;
  }
  CHECK_EQ(image_header.GetImageBegin(), map->Begin());
  DCHECK_EQ(0, memcmp(&image_header, map->Begin(), sizeof(ImageHeader)));

  UniquePtr<MemMap> image_map(MemMap::MapFileAtAddress(nullptr, image_header.GetImageBitmapSize(),
                                                       PROT_READ, MAP_PRIVATE,
                                                       file->Fd(), image_header.GetBitmapOffset(),
                                                       false));
  CHECK(image_map.get() != nullptr) << "failed to map image bitmap";
  size_t bitmap_index = bitmap_index_.fetch_add(1);
  std::string bitmap_name(StringPrintf("imagespace %s live-bitmap %u", image_file_name.c_str(),
                                       bitmap_index));
  UniquePtr<accounting::SpaceBitmap> bitmap(
      accounting::SpaceBitmap::CreateFromMemMap(bitmap_name, image_map.release(),
                                                reinterpret_cast<byte*>(map->Begin()),
                                                map->Size()));
  CHECK(bitmap.get() != nullptr) << "could not create " << bitmap_name;

  Runtime* runtime = Runtime::Current();
  mirror::Object* resolution_method = image_header.GetImageRoot(ImageHeader::kResolutionMethod);
  runtime->SetResolutionMethod(down_cast<mirror::ArtMethod*>(resolution_method));

  mirror::Object* callee_save_method = image_header.GetImageRoot(ImageHeader::kCalleeSaveMethod);
  runtime->SetCalleeSaveMethod(down_cast<mirror::ArtMethod*>(callee_save_method), Runtime::kSaveAll);
  callee_save_method = image_header.GetImageRoot(ImageHeader::kRefsOnlySaveMethod);
  runtime->SetCalleeSaveMethod(down_cast<mirror::ArtMethod*>(callee_save_method), Runtime::kRefsOnly);
  callee_save_method = image_header.GetImageRoot(ImageHeader::kRefsAndArgsSaveMethod);
  runtime->SetCalleeSaveMethod(down_cast<mirror::ArtMethod*>(callee_save_method), Runtime::kRefsAndArgs);

  UniquePtr<ImageSpace> space(new ImageSpace(image_file_name, map.release(), bitmap.release()));
  if (kIsDebugBuild) {
    space->VerifyImageAllocations();
  }

  space->oat_file_.reset(space->OpenOatFile());
  if (space->oat_file_.get() == NULL) {
    LOG(ERROR) << "Failed to open oat file for image: " << image_file_name;
    return NULL;
  }

  if (validate_oat_file && !space->ValidateOatFile()) {
    LOG(WARNING) << "Failed to validate oat file for image: " << image_file_name;
    return NULL;
  }

  if (VLOG_IS_ON(heap) || VLOG_IS_ON(startup)) {
    LOG(INFO) << "ImageSpace::Init exiting (" << PrettyDuration(NanoTime() - start_time)
             << ") " << *space.get();
  }
  return space.release();
}

OatFile* ImageSpace::OpenOatFile() const {
  const Runtime* runtime = Runtime::Current();
  const ImageHeader& image_header = GetImageHeader();
  // Grab location but don't use Object::AsString as we haven't yet initialized the roots to
  // check the down cast
  mirror::String* oat_location =
      down_cast<mirror::String*>(image_header.GetImageRoot(ImageHeader::kOatLocation));
  std::string oat_filename;
  oat_filename += runtime->GetHostPrefix();
  oat_filename += oat_location->ToModifiedUtf8();
  OatFile* oat_file = OatFile::Open(oat_filename, oat_filename, image_header.GetOatDataBegin(),
                                    !Runtime::Current()->IsCompiler());
  if (oat_file == NULL) {
    LOG(ERROR) << "Failed to open oat file " << oat_filename << " referenced from image.";
    return NULL;
  }
  uint32_t oat_checksum = oat_file->GetOatHeader().GetChecksum();
  uint32_t image_oat_checksum = image_header.GetOatChecksum();
  if (oat_checksum != image_oat_checksum) {
    LOG(ERROR) << "Failed to match oat file checksum " << std::hex << oat_checksum
               << " to expected oat checksum " << std::hex << image_oat_checksum
               << " in image";
    return NULL;
  }
  return oat_file;
}

bool ImageSpace::ValidateOatFile() const {
  CHECK(oat_file_.get() != NULL);
  for (const OatFile::OatDexFile* oat_dex_file : oat_file_->GetOatDexFiles()) {
    const std::string& dex_file_location = oat_dex_file->GetDexFileLocation();
    uint32_t dex_file_location_checksum;
    if (!DexFile::GetChecksum(dex_file_location.c_str(), &dex_file_location_checksum)) {
      LOG(WARNING) << "ValidateOatFile could not find checksum for " << dex_file_location;
      return false;
    }
    if (dex_file_location_checksum != oat_dex_file->GetDexFileLocationChecksum()) {
      LOG(WARNING) << "ValidateOatFile found checksum mismatch between oat file "
                   << oat_file_->GetLocation() << " and dex file " << dex_file_location
                   << " (" << oat_dex_file->GetDexFileLocationChecksum() << " != "
                   << dex_file_location_checksum << ")";
      return false;
    }
  }
  return true;
}

OatFile& ImageSpace::ReleaseOatFile() {
  CHECK(oat_file_.get() != NULL);
  return *oat_file_.release();
}

void ImageSpace::Dump(std::ostream& os) const {
  os << GetType()
      << "begin=" << reinterpret_cast<void*>(Begin())
      << ",end=" << reinterpret_cast<void*>(End())
      << ",size=" << PrettySize(Size())
      << ",name=\"" << GetName() << "\"]";
}

}  // namespace space
}  // namespace gc
}  // namespace art
