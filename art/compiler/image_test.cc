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

#include <string>
#include <vector>

#include "common_test.h"
#include "compiler/elf_fixup.h"
#include "compiler/image_writer.h"
#include "compiler/oat_writer.h"
#include "gc/space/image_space.h"
#include "image.h"
#include "signal_catcher.h"
#include "UniquePtr.h"
#include "utils.h"
#include "vector_output_stream.h"

namespace art {

class ImageTest : public CommonTest {
 protected:
  virtual void SetUp() {
    ReserveImageSpace();
    CommonTest::SetUp();
  }
};

TEST_F(ImageTest, WriteRead) {
  ScratchFile tmp_elf;
  {
    {
      jobject class_loader = NULL;
      ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
      base::TimingLogger timings("ImageTest::WriteRead", false, false);
      timings.StartSplit("CompileAll");
#if defined(ART_USE_PORTABLE_COMPILER)
      // TODO: we disable this for portable so the test executes in a reasonable amount of time.
      //       We shouldn't need to do this.
      runtime_->SetCompilerFilter(Runtime::kInterpretOnly);
#endif
      for (const DexFile* dex_file : class_linker->GetBootClassPath()) {
        dex_file->EnableWrite();
      }
      compiler_driver_->CompileAll(class_loader, class_linker->GetBootClassPath(), timings);

      ScopedObjectAccess soa(Thread::Current());
      OatWriter oat_writer(class_linker->GetBootClassPath(),
                           0, 0, "", compiler_driver_.get());
      bool success = compiler_driver_->WriteElf(GetTestAndroidRoot(),
                                                !kIsTargetBuild,
                                                class_linker->GetBootClassPath(),
                                                oat_writer,
                                                tmp_elf.GetFile());
      ASSERT_TRUE(success);
    }
  }
  // Workound bug that mcld::Linker::emit closes tmp_elf by reopening as tmp_oat.
  UniquePtr<File> tmp_oat(OS::OpenFileReadWrite(tmp_elf.GetFilename().c_str()));
  ASSERT_TRUE(tmp_oat.get() != NULL);

  ScratchFile tmp_image;
  const uintptr_t requested_image_base = ART_BASE_ADDRESS;
  {
    ImageWriter writer(*compiler_driver_.get());
    bool success_image = writer.Write(tmp_image.GetFilename(), requested_image_base,
                                      tmp_oat->GetPath(), tmp_oat->GetPath());
    ASSERT_TRUE(success_image);
    bool success_fixup = ElfFixup::Fixup(tmp_oat.get(), writer.GetOatDataBegin());
    ASSERT_TRUE(success_fixup);
  }

  {
    UniquePtr<File> file(OS::OpenFileForReading(tmp_image.GetFilename().c_str()));
    ASSERT_TRUE(file.get() != NULL);
    ImageHeader image_header;
    file->ReadFully(&image_header, sizeof(image_header));
    ASSERT_TRUE(image_header.IsValid());
    ASSERT_GE(image_header.GetImageBitmapOffset(), sizeof(image_header));
    ASSERT_NE(0U, image_header.GetImageBitmapSize());

    gc::Heap* heap = Runtime::Current()->GetHeap();
    ASSERT_EQ(1U, heap->GetContinuousSpaces().size());
    gc::space::ContinuousSpace* space = heap->GetContinuousSpaces().front();
    ASSERT_FALSE(space->IsImageSpace());
    ASSERT_TRUE(space != NULL);
    ASSERT_TRUE(space->IsDlMallocSpace());
    ASSERT_GE(sizeof(image_header) + space->Size(), static_cast<size_t>(file->GetLength()));
  }

  ASSERT_TRUE(compiler_driver_->GetImageClasses() != NULL);
  CompilerDriver::DescriptorSet image_classes(*compiler_driver_->GetImageClasses());

  // Need to delete the compiler since it has worker threads which are attached to runtime.
  compiler_driver_.reset();

  // Tear down old runtime before making a new one, clearing out misc state.
  runtime_.reset();
  java_lang_dex_file_ = NULL;

  UniquePtr<const DexFile> dex(DexFile::Open(GetLibCoreDexFileName(), GetLibCoreDexFileName()));
  ASSERT_TRUE(dex.get() != NULL);

  // Remove the reservation of the memory for use to load the image.
  UnreserveImageSpace();

  Runtime::Options options;
  std::string image("-Ximage:");
  image.append(tmp_image.GetFilename());
  options.push_back(std::make_pair(image.c_str(), reinterpret_cast<void*>(NULL)));

  if (!Runtime::Create(options, false)) {
    LOG(FATAL) << "Failed to create runtime";
    return;
  }
  runtime_.reset(Runtime::Current());
  // Runtime::Create acquired the mutator_lock_ that is normally given away when we Runtime::Start,
  // give it away now and then switch to a more managable ScopedObjectAccess.
  Thread::Current()->TransitionFromRunnableToSuspended(kNative);
  ScopedObjectAccess soa(Thread::Current());
  ASSERT_TRUE(runtime_.get() != NULL);
  class_linker_ = runtime_->GetClassLinker();

  gc::Heap* heap = Runtime::Current()->GetHeap();
  ASSERT_EQ(2U, heap->GetContinuousSpaces().size());
  ASSERT_TRUE(heap->GetContinuousSpaces()[0]->IsImageSpace());
  ASSERT_FALSE(heap->GetContinuousSpaces()[0]->IsDlMallocSpace());
  ASSERT_FALSE(heap->GetContinuousSpaces()[1]->IsImageSpace());
  ASSERT_TRUE(heap->GetContinuousSpaces()[1]->IsDlMallocSpace());

  gc::space::ImageSpace* image_space = heap->GetImageSpace();
  image_space->VerifyImageAllocations();
  byte* image_begin = image_space->Begin();
  byte* image_end = image_space->End();
  CHECK_EQ(requested_image_base, reinterpret_cast<uintptr_t>(image_begin));
  for (size_t i = 0; i < dex->NumClassDefs(); ++i) {
    const DexFile::ClassDef& class_def = dex->GetClassDef(i);
    const char* descriptor = dex->GetClassDescriptor(class_def);
    mirror::Class* klass = class_linker_->FindSystemClass(descriptor);
    EXPECT_TRUE(klass != NULL) << descriptor;
    EXPECT_LT(image_begin, reinterpret_cast<byte*>(klass)) << descriptor;
    if (image_classes.find(descriptor) != image_classes.end()) {
      // image classes should be located before the end of the image.
      EXPECT_LT(reinterpret_cast<byte*>(klass), image_end) << descriptor;
    } else {
      // non image classes should be in a space after the image.
      EXPECT_GT(reinterpret_cast<byte*>(klass), image_end) << descriptor;
    }
    EXPECT_TRUE(Monitor::IsValidLockWord(*klass->GetRawLockWordAddress()));
  }
}

TEST_F(ImageTest, ImageHeaderIsValid) {
    uint32_t image_begin = ART_BASE_ADDRESS;
    uint32_t image_size_ = 16 * KB;
    uint32_t image_bitmap_offset = 0;
    uint32_t image_bitmap_size = 0;
    uint32_t image_roots = ART_BASE_ADDRESS + (1 * KB);
    uint32_t oat_checksum = 0;
    uint32_t oat_file_begin = ART_BASE_ADDRESS + (4 * KB);  // page aligned
    uint32_t oat_data_begin = ART_BASE_ADDRESS + (8 * KB);  // page aligned
    uint32_t oat_data_end = ART_BASE_ADDRESS + (9 * KB);
    uint32_t oat_file_end = ART_BASE_ADDRESS + (10 * KB);
    ImageHeader image_header(image_begin,
                             image_size_,
                             image_bitmap_offset,
                             image_bitmap_size,
                             image_roots,
                             oat_checksum,
                             oat_file_begin,
                             oat_data_begin,
                             oat_data_end,
                             oat_file_end);
    ASSERT_TRUE(image_header.IsValid());

    char* magic = const_cast<char*>(image_header.GetMagic());
    strcpy(magic, "");  // bad magic
    ASSERT_FALSE(image_header.IsValid());
    strcpy(magic, "art\n000");  // bad version
    ASSERT_FALSE(image_header.IsValid());
}

}  // namespace art
