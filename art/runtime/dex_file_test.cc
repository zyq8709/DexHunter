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

#include "dex_file.h"

#include "UniquePtr.h"
#include "common_test.h"

namespace art {

class DexFileTest : public CommonTest {};

TEST_F(DexFileTest, Open) {
  ScopedObjectAccess soa(Thread::Current());
  const DexFile* dex(OpenTestDexFile("Nested"));
  ASSERT_TRUE(dex != NULL);
}

// Although this is the same content logically as the Nested test dex,
// the DexFileHeader test is sensitive to subtle changes in the
// contents due to the checksum etc, so we embed the exact input here.
//
// class Nested {
//     class Inner {
//     }
// }
static const char kRawDex[] =
  "ZGV4CjAzNQAQedgAe7gM1B/WHsWJ6L7lGAISGC7yjD2IAwAAcAAAAHhWNBIAAAAAAAAAAMQCAAAP"
  "AAAAcAAAAAcAAACsAAAAAgAAAMgAAAABAAAA4AAAAAMAAADoAAAAAgAAAAABAABIAgAAQAEAAK4B"
  "AAC2AQAAvQEAAM0BAADXAQAA+wEAABsCAAA+AgAAUgIAAF8CAABiAgAAZgIAAHMCAAB5AgAAgQIA"
  "AAIAAAADAAAABAAAAAUAAAAGAAAABwAAAAkAAAAJAAAABgAAAAAAAAAKAAAABgAAAKgBAAAAAAEA"
  "DQAAAAAAAQAAAAAAAQAAAAAAAAAFAAAAAAAAAAAAAAAAAAAABQAAAAAAAAAIAAAAiAEAAKsCAAAA"
  "AAAAAQAAAAAAAAAFAAAAAAAAAAgAAACYAQAAuAIAAAAAAAACAAAAlAIAAJoCAAABAAAAowIAAAIA"
  "AgABAAAAiAIAAAYAAABbAQAAcBACAAAADgABAAEAAQAAAI4CAAAEAAAAcBACAAAADgBAAQAAAAAA"
  "AAAAAAAAAAAATAEAAAAAAAAAAAAAAAAAAAEAAAABAAY8aW5pdD4ABUlubmVyAA5MTmVzdGVkJElu"
  "bmVyOwAITE5lc3RlZDsAIkxkYWx2aWsvYW5ub3RhdGlvbi9FbmNsb3NpbmdDbGFzczsAHkxkYWx2"
  "aWsvYW5ub3RhdGlvbi9Jbm5lckNsYXNzOwAhTGRhbHZpay9hbm5vdGF0aW9uL01lbWJlckNsYXNz"
  "ZXM7ABJMamF2YS9sYW5nL09iamVjdDsAC05lc3RlZC5qYXZhAAFWAAJWTAALYWNjZXNzRmxhZ3MA"
  "BG5hbWUABnRoaXMkMAAFdmFsdWUAAgEABw4AAQAHDjwAAgIBDhgBAgMCCwQADBcBAgQBDhwBGAAA"
  "AQEAAJAgAICABNQCAAABAAGAgATwAgAAEAAAAAAAAAABAAAAAAAAAAEAAAAPAAAAcAAAAAIAAAAH"
  "AAAArAAAAAMAAAACAAAAyAAAAAQAAAABAAAA4AAAAAUAAAADAAAA6AAAAAYAAAACAAAAAAEAAAMQ"
  "AAACAAAAQAEAAAEgAAACAAAAVAEAAAYgAAACAAAAiAEAAAEQAAABAAAAqAEAAAIgAAAPAAAArgEA"
  "AAMgAAACAAAAiAIAAAQgAAADAAAAlAIAAAAgAAACAAAAqwIAAAAQAAABAAAAxAIAAA==";

static const DexFile* OpenDexFileBase64(const char* base64,
                                        const std::string& location) {
  // decode base64
  CHECK(base64 != NULL);
  size_t length;
  UniquePtr<byte[]> dex_bytes(DecodeBase64(base64, &length));
  CHECK(dex_bytes.get() != NULL);

  // write to provided file
  UniquePtr<File> file(OS::CreateEmptyFile(location.c_str()));
  CHECK(file.get() != NULL);
  if (!file->WriteFully(dex_bytes.get(), length)) {
    PLOG(FATAL) << "Failed to write base64 as dex file";
  }
  file.reset();

  // read dex file
  ScopedObjectAccess soa(Thread::Current());
  const DexFile* dex_file = DexFile::Open(location, location);
  CHECK(dex_file != NULL);
  EXPECT_EQ(PROT_READ, dex_file->GetPermissions());
  EXPECT_TRUE(dex_file->IsReadOnly());
  return dex_file;
}

TEST_F(DexFileTest, Header) {
  ScratchFile tmp;
  UniquePtr<const DexFile> raw(OpenDexFileBase64(kRawDex, tmp.GetFilename()));
  ASSERT_TRUE(raw.get() != NULL);

  const DexFile::Header& header = raw->GetHeader();
  // TODO: header.magic_
  EXPECT_EQ(0x00d87910U, header.checksum_);
  // TODO: header.signature_
  EXPECT_EQ(904U, header.file_size_);
  EXPECT_EQ(112U, header.header_size_);
  EXPECT_EQ(0U, header.link_size_);
  EXPECT_EQ(0U, header.link_off_);
  EXPECT_EQ(15U, header.string_ids_size_);
  EXPECT_EQ(112U, header.string_ids_off_);
  EXPECT_EQ(7U, header.type_ids_size_);
  EXPECT_EQ(172U, header.type_ids_off_);
  EXPECT_EQ(2U, header.proto_ids_size_);
  EXPECT_EQ(200U, header.proto_ids_off_);
  EXPECT_EQ(1U, header.field_ids_size_);
  EXPECT_EQ(224U, header.field_ids_off_);
  EXPECT_EQ(3U, header.method_ids_size_);
  EXPECT_EQ(232U, header.method_ids_off_);
  EXPECT_EQ(2U, header.class_defs_size_);
  EXPECT_EQ(256U, header.class_defs_off_);
  EXPECT_EQ(584U, header.data_size_);
  EXPECT_EQ(320U, header.data_off_);

  EXPECT_EQ(header.checksum_, raw->GetLocationChecksum());
}

TEST_F(DexFileTest, GetLocationChecksum) {
  ScopedObjectAccess soa(Thread::Current());
  const DexFile* raw(OpenTestDexFile("Main"));
  EXPECT_NE(raw->GetHeader().checksum_, raw->GetLocationChecksum());
}

TEST_F(DexFileTest, GetChecksum) {
  uint32_t checksum;
  ScopedObjectAccess soa(Thread::Current());
  EXPECT_TRUE(DexFile::GetChecksum(GetLibCoreDexFileName(), &checksum));
  EXPECT_EQ(java_lang_dex_file_->GetLocationChecksum(), checksum);
}

TEST_F(DexFileTest, ClassDefs) {
  ScopedObjectAccess soa(Thread::Current());
  const DexFile* raw(OpenTestDexFile("Nested"));
  ASSERT_TRUE(raw != NULL);
  EXPECT_EQ(2U, raw->NumClassDefs());

  const DexFile::ClassDef& c0 = raw->GetClassDef(0);
  EXPECT_STREQ("LNested$Inner;", raw->GetClassDescriptor(c0));

  const DexFile::ClassDef& c1 = raw->GetClassDef(1);
  EXPECT_STREQ("LNested;", raw->GetClassDescriptor(c1));
}

TEST_F(DexFileTest, CreateMethodSignature) {
  ScopedObjectAccess soa(Thread::Current());
  const DexFile* raw(OpenTestDexFile("CreateMethodSignature"));
  ASSERT_TRUE(raw != NULL);
  EXPECT_EQ(1U, raw->NumClassDefs());

  const DexFile::ClassDef& class_def = raw->GetClassDef(0);
  ASSERT_STREQ("LCreateMethodSignature;", raw->GetClassDescriptor(class_def));

  const byte* class_data = raw->GetClassData(class_def);
  ASSERT_TRUE(class_data != NULL);
  ClassDataItemIterator it(*raw, class_data);

  EXPECT_EQ(1u, it.NumDirectMethods());

  // Check the signature for the static initializer.
  {
    ASSERT_EQ(1U, it.NumDirectMethods());
    const DexFile::MethodId& method_id = raw->GetMethodId(it.GetMemberIndex());
    uint32_t proto_idx = method_id.proto_idx_;
    const char* name = raw->StringDataByIdx(method_id.name_idx_);
    ASSERT_STREQ("<init>", name);
    int32_t length;
    std::string signature(raw->CreateMethodSignature(proto_idx, &length));
    ASSERT_EQ("()V", signature);
  }

  // Check both virtual methods.
  ASSERT_EQ(2U, it.NumVirtualMethods());
  {
    it.Next();
    const DexFile::MethodId& method_id = raw->GetMethodId(it.GetMemberIndex());

    const char* name = raw->StringDataByIdx(method_id.name_idx_);
    ASSERT_STREQ("m1", name);

    uint32_t proto_idx = method_id.proto_idx_;
    int32_t length;
    std::string signature(raw->CreateMethodSignature(proto_idx, &length));
    ASSERT_EQ("(IDJLjava/lang/Object;)Ljava/lang/Float;", signature);
  }

  {
    it.Next();
    const DexFile::MethodId& method_id = raw->GetMethodId(it.GetMemberIndex());

    const char* name = raw->StringDataByIdx(method_id.name_idx_);
    ASSERT_STREQ("m2", name);

    uint32_t proto_idx = method_id.proto_idx_;
    int32_t length;
    std::string signature(raw->CreateMethodSignature(proto_idx, &length));
    ASSERT_EQ("(ZSC)LCreateMethodSignature;", signature);
  }
}

TEST_F(DexFileTest, FindStringId) {
  ScopedObjectAccess soa(Thread::Current());
  const DexFile* raw(OpenTestDexFile("CreateMethodSignature"));
  ASSERT_TRUE(raw != NULL);
  EXPECT_EQ(1U, raw->NumClassDefs());

  const char* strings[] = { "LCreateMethodSignature;", "Ljava/lang/Float;", "Ljava/lang/Object;",
      "D", "I", "J", NULL };
  for (size_t i = 0; strings[i] != NULL; i++) {
    const char* str = strings[i];
    const DexFile::StringId* str_id = raw->FindStringId(str);
    const char* dex_str = raw->GetStringData(*str_id);
    EXPECT_STREQ(dex_str, str);
  }
}

TEST_F(DexFileTest, FindTypeId) {
  for (size_t i = 0; i < java_lang_dex_file_->NumTypeIds(); i++) {
    const char* type_str = java_lang_dex_file_->StringByTypeIdx(i);
    const DexFile::StringId* type_str_id = java_lang_dex_file_->FindStringId(type_str);
    ASSERT_TRUE(type_str_id != NULL);
    uint32_t type_str_idx = java_lang_dex_file_->GetIndexForStringId(*type_str_id);
    const DexFile::TypeId* type_id = java_lang_dex_file_->FindTypeId(type_str_idx);
    ASSERT_TRUE(type_id != NULL);
    EXPECT_EQ(java_lang_dex_file_->GetIndexForTypeId(*type_id), i);
  }
}

TEST_F(DexFileTest, FindProtoId) {
  for (size_t i = 0; i < java_lang_dex_file_->NumProtoIds(); i++) {
    const DexFile::ProtoId& to_find = java_lang_dex_file_->GetProtoId(i);
    const DexFile::TypeList* to_find_tl = java_lang_dex_file_->GetProtoParameters(to_find);
    std::vector<uint16_t> to_find_types;
    if (to_find_tl != NULL) {
      for (size_t j = 0; j < to_find_tl->Size(); j++) {
        to_find_types.push_back(to_find_tl->GetTypeItem(j).type_idx_);
      }
    }
    const DexFile::ProtoId* found =
        java_lang_dex_file_->FindProtoId(to_find.return_type_idx_, to_find_types);
    ASSERT_TRUE(found != NULL);
    EXPECT_EQ(java_lang_dex_file_->GetIndexForProtoId(*found), i);
  }
}

TEST_F(DexFileTest, FindMethodId) {
  for (size_t i = 0; i < java_lang_dex_file_->NumMethodIds(); i++) {
    const DexFile::MethodId& to_find = java_lang_dex_file_->GetMethodId(i);
    const DexFile::TypeId& klass = java_lang_dex_file_->GetTypeId(to_find.class_idx_);
    const DexFile::StringId& name = java_lang_dex_file_->GetStringId(to_find.name_idx_);
    const DexFile::ProtoId& signature = java_lang_dex_file_->GetProtoId(to_find.proto_idx_);
    const DexFile::MethodId* found = java_lang_dex_file_->FindMethodId(klass, name, signature);
    int32_t length;
    ASSERT_TRUE(found != NULL) << "Didn't find method " << i << ": "
        << java_lang_dex_file_->StringByTypeIdx(to_find.class_idx_) << "."
        << java_lang_dex_file_->GetStringData(name)
        << java_lang_dex_file_->CreateMethodSignature(to_find.proto_idx_, &length);
    EXPECT_EQ(java_lang_dex_file_->GetIndexForMethodId(*found), i);
  }
}

TEST_F(DexFileTest, FindFieldId) {
  for (size_t i = 0; i < java_lang_dex_file_->NumFieldIds(); i++) {
    const DexFile::FieldId& to_find = java_lang_dex_file_->GetFieldId(i);
    const DexFile::TypeId& klass = java_lang_dex_file_->GetTypeId(to_find.class_idx_);
    const DexFile::StringId& name = java_lang_dex_file_->GetStringId(to_find.name_idx_);
    const DexFile::TypeId& type = java_lang_dex_file_->GetTypeId(to_find.type_idx_);
    const DexFile::FieldId* found = java_lang_dex_file_->FindFieldId(klass, name, type);
    ASSERT_TRUE(found != NULL) << "Didn't find field " << i << ": "
        << java_lang_dex_file_->StringByTypeIdx(to_find.type_idx_) << " "
        << java_lang_dex_file_->StringByTypeIdx(to_find.class_idx_) << "."
        << java_lang_dex_file_->GetStringData(name);
    EXPECT_EQ(java_lang_dex_file_->GetIndexForFieldId(*found), i);
  }
}

}  // namespace art
