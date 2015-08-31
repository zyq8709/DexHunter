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

#include "compiler/oat_writer.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "oat_file.h"
#include "vector_output_stream.h"

#include "common_test.h"

namespace art {

class OatTest : public CommonTest {
 protected:
  void CheckMethod(mirror::ArtMethod* method,
                   const OatFile::OatMethod& oat_method,
                   const DexFile* dex_file)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    const CompiledMethod* compiled_method =
        compiler_driver_->GetCompiledMethod(MethodReference(dex_file,
                                                            method->GetDexMethodIndex()));

    if (compiled_method == NULL) {
      EXPECT_TRUE(oat_method.GetCode() == NULL) << PrettyMethod(method) << " "
                                                << oat_method.GetCode();
#if !defined(ART_USE_PORTABLE_COMPILER)
      EXPECT_EQ(oat_method.GetFrameSizeInBytes(), static_cast<uint32_t>(kStackAlignment));
      EXPECT_EQ(oat_method.GetCoreSpillMask(), 0U);
      EXPECT_EQ(oat_method.GetFpSpillMask(), 0U);
#endif
    } else {
      const void* oat_code = oat_method.GetCode();
      EXPECT_TRUE(oat_code != NULL) << PrettyMethod(method);
      uintptr_t oat_code_aligned = RoundDown(reinterpret_cast<uintptr_t>(oat_code), 2);
      oat_code = reinterpret_cast<const void*>(oat_code_aligned);

      const std::vector<uint8_t>& code = compiled_method->GetCode();
      size_t code_size = code.size() * sizeof(code[0]);
      EXPECT_EQ(0, memcmp(oat_code, &code[0], code_size))
          << PrettyMethod(method) << " " << code_size;
      CHECK_EQ(0, memcmp(oat_code, &code[0], code_size));
#if !defined(ART_USE_PORTABLE_COMPILER)
      EXPECT_EQ(oat_method.GetFrameSizeInBytes(), compiled_method->GetFrameSizeInBytes());
      EXPECT_EQ(oat_method.GetCoreSpillMask(), compiled_method->GetCoreSpillMask());
      EXPECT_EQ(oat_method.GetFpSpillMask(), compiled_method->GetFpSpillMask());
#endif
    }
  }
};

TEST_F(OatTest, WriteRead) {
  const bool compile = false;  // DISABLED_ due to the time to compile libcore
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();

  // TODO: make selectable
#if defined(ART_USE_PORTABLE_COMPILER)
  CompilerBackend compiler_backend = kPortable;
#else
  CompilerBackend compiler_backend = kQuick;
#endif
  InstructionSet insn_set = kIsTargetBuild ? kThumb2 : kX86;
  compiler_driver_.reset(new CompilerDriver(compiler_backend, insn_set, false, NULL, 2, true));
  jobject class_loader = NULL;
  if (compile) {
    base::TimingLogger timings("OatTest::WriteRead", false, false);
    compiler_driver_->CompileAll(class_loader, class_linker->GetBootClassPath(), timings);
  }

  ScopedObjectAccess soa(Thread::Current());
  ScratchFile tmp;
  OatWriter oat_writer(class_linker->GetBootClassPath(),
                       42U,
                       4096U,
                       "lue.art",
                       compiler_driver_.get());
  bool success = compiler_driver_->WriteElf(GetTestAndroidRoot(),
                                            !kIsTargetBuild,
                                            class_linker->GetBootClassPath(),
                                            oat_writer,
                                            tmp.GetFile());
  ASSERT_TRUE(success);

  if (compile) {  // OatWriter strips the code, regenerate to compare
    base::TimingLogger timings("CommonTest::WriteRead", false, false);
    compiler_driver_->CompileAll(class_loader, class_linker->GetBootClassPath(), timings);
  }
  UniquePtr<OatFile> oat_file(OatFile::Open(tmp.GetFilename(), tmp.GetFilename(), NULL, false));
  ASSERT_TRUE(oat_file.get() != NULL);
  const OatHeader& oat_header = oat_file->GetOatHeader();
  ASSERT_TRUE(oat_header.IsValid());
  ASSERT_EQ(2U, oat_header.GetDexFileCount());  // core and conscrypt
  ASSERT_EQ(42U, oat_header.GetImageFileLocationOatChecksum());
  ASSERT_EQ(4096U, oat_header.GetImageFileLocationOatDataBegin());
  ASSERT_EQ("lue.art", oat_header.GetImageFileLocation());

  const DexFile* dex_file = java_lang_dex_file_;
  uint32_t dex_file_checksum = dex_file->GetLocationChecksum();
  const OatFile::OatDexFile* oat_dex_file = oat_file->GetOatDexFile(dex_file->GetLocation(),
                                                                    &dex_file_checksum);
  CHECK_EQ(dex_file->GetLocationChecksum(), oat_dex_file->GetDexFileLocationChecksum());
  for (size_t i = 0; i < dex_file->NumClassDefs(); i++) {
    const DexFile::ClassDef& class_def = dex_file->GetClassDef(i);
    const byte* class_data = dex_file->GetClassData(class_def);
    size_t num_virtual_methods =0;
    if (class_data != NULL) {
      ClassDataItemIterator it(*dex_file, class_data);
      num_virtual_methods = it.NumVirtualMethods();
    }
    const char* descriptor = dex_file->GetClassDescriptor(class_def);

    UniquePtr<const OatFile::OatClass> oat_class(oat_dex_file->GetOatClass(i));

    mirror::Class* klass = class_linker->FindClass(descriptor, NULL);

    size_t method_index = 0;
    for (size_t i = 0; i < klass->NumDirectMethods(); i++, method_index++) {
      CheckMethod(klass->GetDirectMethod(i),
                  oat_class->GetOatMethod(method_index), dex_file);
    }
    for (size_t i = 0; i < num_virtual_methods; i++, method_index++) {
      CheckMethod(klass->GetVirtualMethod(i),
                  oat_class->GetOatMethod(method_index), dex_file);
    }
  }
}

TEST_F(OatTest, OatHeaderSizeCheck) {
  // If this test is failing and you have to update these constants,
  // it is time to update OatHeader::kOatVersion
  EXPECT_EQ(64U, sizeof(OatHeader));
  EXPECT_EQ(28U, sizeof(OatMethodOffsets));
}

TEST_F(OatTest, OatHeaderIsValid) {
    InstructionSet instruction_set = kX86;
    std::vector<const DexFile*> dex_files;
    uint32_t image_file_location_oat_checksum = 0;
    uint32_t image_file_location_oat_begin = 0;
    const std::string image_file_location;
    OatHeader oat_header(instruction_set,
                         &dex_files,
                         image_file_location_oat_checksum,
                         image_file_location_oat_begin,
                         image_file_location);
    ASSERT_TRUE(oat_header.IsValid());

    char* magic = const_cast<char*>(oat_header.GetMagic());
    strcpy(magic, "");  // bad magic
    ASSERT_FALSE(oat_header.IsValid());
    strcpy(magic, "oat\n000");  // bad version
    ASSERT_FALSE(oat_header.IsValid());
}

}  // namespace art
