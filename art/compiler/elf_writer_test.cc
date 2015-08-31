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

#include "common_test.h"

#include "oat.h"
#include "elf_file.h"

namespace art {

class ElfWriterTest : public CommonTest {
 protected:
  virtual void SetUp() {
    ReserveImageSpace();
    CommonTest::SetUp();
  }
};

#define EXPECT_ELF_FILE_ADDRESS(ef, value, name, build_map) \
  EXPECT_EQ(value, reinterpret_cast<void*>(ef->FindSymbolAddress(::llvm::ELF::SHT_DYNSYM, name, build_map))); \
  EXPECT_EQ(value, ef->FindDynamicSymbolAddress(name)); \

TEST_F(ElfWriterTest, dlsym) {
  std::string elf_filename;
  if (IsHost()) {
    const char* host_dir = getenv("ANDROID_HOST_OUT");
    CHECK(host_dir != NULL);
    elf_filename = StringPrintf("%s/framework/core.oat", host_dir);
  } else {
    elf_filename = "/data/art-test/core.oat";
  }
  LOG(INFO) << "elf_filename=" << elf_filename;

  UnreserveImageSpace();
  void* dl_oat_so = dlopen(elf_filename.c_str(), RTLD_NOW);
  ASSERT_TRUE(dl_oat_so != NULL) << dlerror();
  void* dl_oatdata = dlsym(dl_oat_so, "oatdata");
  ASSERT_TRUE(dl_oatdata != NULL);

  OatHeader* dl_oat_header = reinterpret_cast<OatHeader*>(dl_oatdata);
  ASSERT_TRUE(dl_oat_header->IsValid());
  void* dl_oatexec = dlsym(dl_oat_so, "oatexec");
  ASSERT_TRUE(dl_oatexec != NULL);
  ASSERT_LT(dl_oatdata, dl_oatexec);

  void* dl_oatlastword = dlsym(dl_oat_so, "oatlastword");
  ASSERT_TRUE(dl_oatlastword != NULL);
  ASSERT_LT(dl_oatexec, dl_oatlastword);

  ASSERT_EQ(0, dlclose(dl_oat_so));

  UniquePtr<File> file(OS::OpenFileForReading(elf_filename.c_str()));
  ASSERT_TRUE(file.get() != NULL);
  {
    UniquePtr<ElfFile> ef(ElfFile::Open(file.get(), false, false));
    CHECK(ef.get() != NULL);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatdata, "oatdata", false);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatexec, "oatexec", false);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatlastword, "oatlastword", false);
  }
  {
    UniquePtr<ElfFile> ef(ElfFile::Open(file.get(), false, false));
    CHECK(ef.get() != NULL);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatdata, "oatdata", true);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatexec, "oatexec", true);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatlastword, "oatlastword", true);
  }
  {
    UniquePtr<ElfFile> ef(ElfFile::Open(file.get(), false, true));
    CHECK(ef.get() != NULL);
    ef->Load(false);
    EXPECT_EQ(dl_oatdata, ef->FindDynamicSymbolAddress("oatdata"));
    EXPECT_EQ(dl_oatexec, ef->FindDynamicSymbolAddress("oatexec"));
    EXPECT_EQ(dl_oatlastword, ef->FindDynamicSymbolAddress("oatlastword"));
  }
}

}  // namespace art
