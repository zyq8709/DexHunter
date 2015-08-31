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

#ifndef ART_RUNTIME_GC_SPACE_IMAGE_SPACE_H_
#define ART_RUNTIME_GC_SPACE_IMAGE_SPACE_H_

#include "space.h"

namespace art {

class OatFile;

namespace gc {
namespace space {

// An image space is a space backed with a memory mapped image.
class ImageSpace : public MemMapSpace {
 public:
  bool CanAllocateInto() const {
    return false;
  }

  SpaceType GetType() const {
    return kSpaceTypeImageSpace;
  }

  // Create a Space from an image file. Cannot be used for future
  // allocation or collected.
  //
  // Create also opens the OatFile associated with the image file so
  // that it be contiguously allocated with the image before the
  // creation of the alloc space. The ReleaseOatFile will later be
  // used to transfer ownership of the OatFile to the ClassLinker when
  // it is initialized.
  static ImageSpace* Create(const std::string& image)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Releases the OatFile from the ImageSpace so it can be transfer to
  // the caller, presumably the ClassLinker.
  OatFile& ReleaseOatFile()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void VerifyImageAllocations()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const ImageHeader& GetImageHeader() const {
    return *reinterpret_cast<ImageHeader*>(Begin());
  }

  const std::string GetImageFilename() const {
    return GetName();
  }

  accounting::SpaceBitmap* GetLiveBitmap() const {
    return live_bitmap_.get();
  }

  accounting::SpaceBitmap* GetMarkBitmap() const {
    // ImageSpaces have the same bitmap for both live and marked. This helps reduce the number of
    // special cases to test against.
    return live_bitmap_.get();
  }

  void Dump(std::ostream& os) const;

 private:
  // Tries to initialize an ImageSpace from the given image path,
  // returning NULL on error.
  //
  // If validate_oat_file is false (for /system), do not verify that
  // image's OatFile is up-to-date relative to its DexFile
  // inputs. Otherwise (for /data), validate the inputs and generate
  // the OatFile in /data/dalvik-cache if necessary.
  static ImageSpace* Init(const std::string& image, bool validate_oat_file)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  OatFile* OpenOatFile() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool ValidateOatFile() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  friend class Space;

  static AtomicInteger bitmap_index_;

  UniquePtr<accounting::SpaceBitmap> live_bitmap_;

  ImageSpace(const std::string& name, MemMap* mem_map, accounting::SpaceBitmap* live_bitmap);

  // The OatFile associated with the image during early startup to
  // reserve space contiguous to the image. It is later released to
  // the ClassLinker during it's initialization.
  UniquePtr<OatFile> oat_file_;

  DISALLOW_COPY_AND_ASSIGN(ImageSpace);
};

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_IMAGE_SPACE_H_
