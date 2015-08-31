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

#include "image.h"

#include "mirror/object_array.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "utils.h"

namespace art {

const byte ImageHeader::kImageMagic[] = { 'a', 'r', 't', '\n' };
const byte ImageHeader::kImageVersion[] = { '0', '0', '5', '\0' };

ImageHeader::ImageHeader(uint32_t image_begin,
                         uint32_t image_size,
                         uint32_t image_bitmap_offset,
                         uint32_t image_bitmap_size,
                         uint32_t image_roots,
                         uint32_t oat_checksum,
                         uint32_t oat_file_begin,
                         uint32_t oat_data_begin,
                         uint32_t oat_data_end,
                         uint32_t oat_file_end)
  : image_begin_(image_begin),
    image_size_(image_size),
    image_bitmap_offset_(image_bitmap_offset),
    image_bitmap_size_(image_bitmap_size),
    oat_checksum_(oat_checksum),
    oat_file_begin_(oat_file_begin),
    oat_data_begin_(oat_data_begin),
    oat_data_end_(oat_data_end),
    oat_file_end_(oat_file_end),
    image_roots_(image_roots) {
  CHECK_EQ(image_begin, RoundUp(image_begin, kPageSize));
  CHECK_EQ(oat_file_begin, RoundUp(oat_file_begin, kPageSize));
  CHECK_EQ(oat_data_begin, RoundUp(oat_data_begin, kPageSize));
  CHECK_LT(image_begin, image_roots);
  CHECK_LT(image_roots, oat_file_begin);
  CHECK_LE(oat_file_begin, oat_data_begin);
  CHECK_LT(oat_data_begin, oat_data_end);
  CHECK_LE(oat_data_end, oat_file_end);
  memcpy(magic_, kImageMagic, sizeof(kImageMagic));
  memcpy(version_, kImageVersion, sizeof(kImageVersion));
}

bool ImageHeader::IsValid() const {
  if (memcmp(magic_, kImageMagic, sizeof(kImageMagic)) != 0) {
    return false;
  }
  if (memcmp(version_, kImageVersion, sizeof(kImageVersion)) != 0) {
    return false;
  }
  return true;
}

const char* ImageHeader::GetMagic() const {
  CHECK(IsValid());
  return reinterpret_cast<const char*>(magic_);
}

mirror::Object* ImageHeader::GetImageRoot(ImageRoot image_root) const {
  return GetImageRoots()->Get(image_root);
}

mirror::ObjectArray<mirror::Object>* ImageHeader::GetImageRoots() const {
  return reinterpret_cast<mirror::ObjectArray<mirror::Object>*>(image_roots_);
}

}  // namespace art
