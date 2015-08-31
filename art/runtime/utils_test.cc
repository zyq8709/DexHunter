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
#include "mirror/array.h"
#include "mirror/array-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/string.h"
#include "scoped_thread_state_change.h"
#include "sirt_ref.h"
#include "utils.h"

namespace art {

std::string PrettyArguments(const char* signature);
std::string PrettyReturnType(const char* signature);

class UtilsTest : public CommonTest {
};

TEST_F(UtilsTest, PrettyDescriptor_ArrayReferences) {
  EXPECT_EQ("java.lang.Class[]", PrettyDescriptor("[Ljava/lang/Class;"));
  EXPECT_EQ("java.lang.Class[][]", PrettyDescriptor("[[Ljava/lang/Class;"));
}

TEST_F(UtilsTest, PrettyDescriptor_ScalarReferences) {
  EXPECT_EQ("java.lang.String", PrettyDescriptor("Ljava.lang.String;"));
  EXPECT_EQ("java.lang.String", PrettyDescriptor("Ljava/lang/String;"));
}

TEST_F(UtilsTest, PrettyDescriptor_PrimitiveArrays) {
  EXPECT_EQ("boolean[]", PrettyDescriptor("[Z"));
  EXPECT_EQ("boolean[][]", PrettyDescriptor("[[Z"));
  EXPECT_EQ("byte[]", PrettyDescriptor("[B"));
  EXPECT_EQ("byte[][]", PrettyDescriptor("[[B"));
  EXPECT_EQ("char[]", PrettyDescriptor("[C"));
  EXPECT_EQ("char[][]", PrettyDescriptor("[[C"));
  EXPECT_EQ("double[]", PrettyDescriptor("[D"));
  EXPECT_EQ("double[][]", PrettyDescriptor("[[D"));
  EXPECT_EQ("float[]", PrettyDescriptor("[F"));
  EXPECT_EQ("float[][]", PrettyDescriptor("[[F"));
  EXPECT_EQ("int[]", PrettyDescriptor("[I"));
  EXPECT_EQ("int[][]", PrettyDescriptor("[[I"));
  EXPECT_EQ("long[]", PrettyDescriptor("[J"));
  EXPECT_EQ("long[][]", PrettyDescriptor("[[J"));
  EXPECT_EQ("short[]", PrettyDescriptor("[S"));
  EXPECT_EQ("short[][]", PrettyDescriptor("[[S"));
}

TEST_F(UtilsTest, PrettyDescriptor_PrimitiveScalars) {
  EXPECT_EQ("boolean", PrettyDescriptor("Z"));
  EXPECT_EQ("byte", PrettyDescriptor("B"));
  EXPECT_EQ("char", PrettyDescriptor("C"));
  EXPECT_EQ("double", PrettyDescriptor("D"));
  EXPECT_EQ("float", PrettyDescriptor("F"));
  EXPECT_EQ("int", PrettyDescriptor("I"));
  EXPECT_EQ("long", PrettyDescriptor("J"));
  EXPECT_EQ("short", PrettyDescriptor("S"));
}

TEST_F(UtilsTest, PrettyArguments) {
  EXPECT_EQ("()", PrettyArguments("()V"));
  EXPECT_EQ("(int)", PrettyArguments("(I)V"));
  EXPECT_EQ("(int, int)", PrettyArguments("(II)V"));
  EXPECT_EQ("(int, int, int[][])", PrettyArguments("(II[[I)V"));
  EXPECT_EQ("(int, int, int[][], java.lang.Poop)", PrettyArguments("(II[[ILjava/lang/Poop;)V"));
  EXPECT_EQ("(int, int, int[][], java.lang.Poop, java.lang.Poop[][])", PrettyArguments("(II[[ILjava/lang/Poop;[[Ljava/lang/Poop;)V"));
}

TEST_F(UtilsTest, PrettyReturnType) {
  EXPECT_EQ("void", PrettyReturnType("()V"));
  EXPECT_EQ("int", PrettyReturnType("()I"));
  EXPECT_EQ("int[][]", PrettyReturnType("()[[I"));
  EXPECT_EQ("java.lang.Poop", PrettyReturnType("()Ljava/lang/Poop;"));
  EXPECT_EQ("java.lang.Poop[][]", PrettyReturnType("()[[Ljava/lang/Poop;"));
}

TEST_F(UtilsTest, PrettyTypeOf) {
  ScopedObjectAccess soa(Thread::Current());
  EXPECT_EQ("null", PrettyTypeOf(NULL));

  SirtRef<mirror::String> s(soa.Self(), mirror::String::AllocFromModifiedUtf8(soa.Self(), ""));
  EXPECT_EQ("java.lang.String", PrettyTypeOf(s.get()));

  SirtRef<mirror::ShortArray> a(soa.Self(), mirror::ShortArray::Alloc(soa.Self(), 2));
  EXPECT_EQ("short[]", PrettyTypeOf(a.get()));

  mirror::Class* c = class_linker_->FindSystemClass("[Ljava/lang/String;");
  ASSERT_TRUE(c != NULL);
  mirror::Object* o = mirror::ObjectArray<mirror::String>::Alloc(soa.Self(), c, 0);
  EXPECT_EQ("java.lang.String[]", PrettyTypeOf(o));
  EXPECT_EQ("java.lang.Class<java.lang.String[]>", PrettyTypeOf(o->GetClass()));
}

TEST_F(UtilsTest, PrettyClass) {
  ScopedObjectAccess soa(Thread::Current());
  EXPECT_EQ("null", PrettyClass(NULL));
  mirror::Class* c = class_linker_->FindSystemClass("[Ljava/lang/String;");
  ASSERT_TRUE(c != NULL);
  mirror::Object* o = mirror::ObjectArray<mirror::String>::Alloc(soa.Self(), c, 0);
  EXPECT_EQ("java.lang.Class<java.lang.String[]>", PrettyClass(o->GetClass()));
}

TEST_F(UtilsTest, PrettyClassAndClassLoader) {
  ScopedObjectAccess soa(Thread::Current());
  EXPECT_EQ("null", PrettyClassAndClassLoader(NULL));
  mirror::Class* c = class_linker_->FindSystemClass("[Ljava/lang/String;");
  ASSERT_TRUE(c != NULL);
  mirror::Object* o = mirror::ObjectArray<mirror::String>::Alloc(soa.Self(), c, 0);
  EXPECT_EQ("java.lang.Class<java.lang.String[],null>", PrettyClassAndClassLoader(o->GetClass()));
}

TEST_F(UtilsTest, PrettyField) {
  ScopedObjectAccess soa(Thread::Current());
  EXPECT_EQ("null", PrettyField(NULL));

  mirror::Class* java_lang_String = class_linker_->FindSystemClass("Ljava/lang/String;");

  mirror::ArtField* f;
  f = java_lang_String->FindDeclaredInstanceField("count", "I");
  EXPECT_EQ("int java.lang.String.count", PrettyField(f));
  EXPECT_EQ("java.lang.String.count", PrettyField(f, false));
  f = java_lang_String->FindDeclaredInstanceField("value", "[C");
  EXPECT_EQ("char[] java.lang.String.value", PrettyField(f));
  EXPECT_EQ("java.lang.String.value", PrettyField(f, false));
}

TEST_F(UtilsTest, PrettySize) {
  EXPECT_EQ("1GB", PrettySize(1 * GB));
  EXPECT_EQ("2GB", PrettySize(2 * GB));
  if (sizeof(size_t) > sizeof(uint32_t)) {
    EXPECT_EQ("100GB", PrettySize(100 * GB));
  }
  EXPECT_EQ("1024KB", PrettySize(1 * MB));
  EXPECT_EQ("10MB", PrettySize(10 * MB));
  EXPECT_EQ("100MB", PrettySize(100 * MB));
  EXPECT_EQ("1024B", PrettySize(1 * KB));
  EXPECT_EQ("10KB", PrettySize(10 * KB));
  EXPECT_EQ("100KB", PrettySize(100 * KB));
  EXPECT_EQ("0B", PrettySize(0));
  EXPECT_EQ("1B", PrettySize(1));
  EXPECT_EQ("10B", PrettySize(10));
  EXPECT_EQ("100B", PrettySize(100));
  EXPECT_EQ("512B", PrettySize(512));
}

TEST_F(UtilsTest, PrettyDuration) {
  const uint64_t one_sec = 1000000000;
  const uint64_t one_ms  = 1000000;
  const uint64_t one_us  = 1000;

  EXPECT_EQ("1s", PrettyDuration(1 * one_sec));
  EXPECT_EQ("10s", PrettyDuration(10 * one_sec));
  EXPECT_EQ("100s", PrettyDuration(100 * one_sec));
  EXPECT_EQ("1.001s", PrettyDuration(1 * one_sec + one_ms));
  EXPECT_EQ("1.000001s", PrettyDuration(1 * one_sec + one_us));
  EXPECT_EQ("1.000000001s", PrettyDuration(1 * one_sec + 1));

  EXPECT_EQ("1ms", PrettyDuration(1 * one_ms));
  EXPECT_EQ("10ms", PrettyDuration(10 * one_ms));
  EXPECT_EQ("100ms", PrettyDuration(100 * one_ms));
  EXPECT_EQ("1.001ms", PrettyDuration(1 * one_ms + one_us));
  EXPECT_EQ("1.000001ms", PrettyDuration(1 * one_ms + 1));

  EXPECT_EQ("1us", PrettyDuration(1 * one_us));
  EXPECT_EQ("10us", PrettyDuration(10 * one_us));
  EXPECT_EQ("100us", PrettyDuration(100 * one_us));
  EXPECT_EQ("1.001us", PrettyDuration(1 * one_us + 1));

  EXPECT_EQ("1ns", PrettyDuration(1));
  EXPECT_EQ("10ns", PrettyDuration(10));
  EXPECT_EQ("100ns", PrettyDuration(100));
}

TEST_F(UtilsTest, MangleForJni) {
  ScopedObjectAccess soa(Thread::Current());
  EXPECT_EQ("hello_00024world", MangleForJni("hello$world"));
  EXPECT_EQ("hello_000a9world", MangleForJni("hello\xc2\xa9world"));
  EXPECT_EQ("hello_1world", MangleForJni("hello_world"));
  EXPECT_EQ("Ljava_lang_String_2", MangleForJni("Ljava/lang/String;"));
  EXPECT_EQ("_3C", MangleForJni("[C"));
}

TEST_F(UtilsTest, JniShortName_JniLongName) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::Class* c = class_linker_->FindSystemClass("Ljava/lang/String;");
  ASSERT_TRUE(c != NULL);
  mirror::ArtMethod* m;

  m = c->FindVirtualMethod("charAt", "(I)C");
  ASSERT_TRUE(m != NULL);
  EXPECT_EQ("Java_java_lang_String_charAt", JniShortName(m));
  EXPECT_EQ("Java_java_lang_String_charAt__I", JniLongName(m));

  m = c->FindVirtualMethod("indexOf", "(Ljava/lang/String;I)I");
  ASSERT_TRUE(m != NULL);
  EXPECT_EQ("Java_java_lang_String_indexOf", JniShortName(m));
  EXPECT_EQ("Java_java_lang_String_indexOf__Ljava_lang_String_2I", JniLongName(m));

  m = c->FindDirectMethod("copyValueOf", "([CII)Ljava/lang/String;");
  ASSERT_TRUE(m != NULL);
  EXPECT_EQ("Java_java_lang_String_copyValueOf", JniShortName(m));
  EXPECT_EQ("Java_java_lang_String_copyValueOf___3CII", JniLongName(m));
}

TEST_F(UtilsTest, Split) {
  std::vector<std::string> actual;
  std::vector<std::string> expected;

  expected.clear();

  actual.clear();
  Split("", ':', actual);
  EXPECT_EQ(expected, actual);

  actual.clear();
  Split(":", ':', actual);
  EXPECT_EQ(expected, actual);

  expected.clear();
  expected.push_back("foo");

  actual.clear();
  Split(":foo", ':', actual);
  EXPECT_EQ(expected, actual);

  actual.clear();
  Split("foo:", ':', actual);
  EXPECT_EQ(expected, actual);

  actual.clear();
  Split(":foo:", ':', actual);
  EXPECT_EQ(expected, actual);

  expected.push_back("bar");

  actual.clear();
  Split("foo:bar", ':', actual);
  EXPECT_EQ(expected, actual);

  actual.clear();
  Split(":foo:bar", ':', actual);
  EXPECT_EQ(expected, actual);

  actual.clear();
  Split("foo:bar:", ':', actual);
  EXPECT_EQ(expected, actual);

  actual.clear();
  Split(":foo:bar:", ':', actual);
  EXPECT_EQ(expected, actual);

  expected.push_back("baz");

  actual.clear();
  Split("foo:bar:baz", ':', actual);
  EXPECT_EQ(expected, actual);

  actual.clear();
  Split(":foo:bar:baz", ':', actual);
  EXPECT_EQ(expected, actual);

  actual.clear();
  Split("foo:bar:baz:", ':', actual);
  EXPECT_EQ(expected, actual);

  actual.clear();
  Split(":foo:bar:baz:", ':', actual);
  EXPECT_EQ(expected, actual);
}

TEST_F(UtilsTest, Join) {
  std::vector<std::string> strings;

  strings.clear();
  EXPECT_EQ("", Join(strings, ':'));

  strings.clear();
  strings.push_back("foo");
  EXPECT_EQ("foo", Join(strings, ':'));

  strings.clear();
  strings.push_back("");
  strings.push_back("foo");
  EXPECT_EQ(":foo", Join(strings, ':'));

  strings.clear();
  strings.push_back("foo");
  strings.push_back("");
  EXPECT_EQ("foo:", Join(strings, ':'));

  strings.clear();
  strings.push_back("");
  strings.push_back("foo");
  strings.push_back("");
  EXPECT_EQ(":foo:", Join(strings, ':'));

  strings.clear();
  strings.push_back("foo");
  strings.push_back("bar");
  EXPECT_EQ("foo:bar", Join(strings, ':'));

  strings.clear();
  strings.push_back("foo");
  strings.push_back("bar");
  strings.push_back("baz");
  EXPECT_EQ("foo:bar:baz", Join(strings, ':'));
}

TEST_F(UtilsTest, StartsWith) {
  EXPECT_FALSE(StartsWith("foo", "bar"));
  EXPECT_TRUE(StartsWith("foo", "foo"));
  EXPECT_TRUE(StartsWith("food", "foo"));
  EXPECT_FALSE(StartsWith("fo", "foo"));
}

TEST_F(UtilsTest, EndsWith) {
  EXPECT_FALSE(EndsWith("foo", "bar"));
  EXPECT_TRUE(EndsWith("foo", "foo"));
  EXPECT_TRUE(EndsWith("foofoo", "foo"));
  EXPECT_FALSE(EndsWith("oo", "foo"));
}

void CheckGetDalvikCacheFilenameOrDie(const char* in, const char* out) {
  std::string expected(getenv("ANDROID_DATA"));
  expected += "/dalvik-cache/";
  expected += out;
  EXPECT_STREQ(expected.c_str(), GetDalvikCacheFilenameOrDie(in).c_str());
}

TEST_F(UtilsTest, GetDalvikCacheFilenameOrDie) {
  CheckGetDalvikCacheFilenameOrDie("/system/app/Foo.apk", "system@app@Foo.apk@classes.dex");
  CheckGetDalvikCacheFilenameOrDie("/data/app/foo-1.apk", "data@app@foo-1.apk@classes.dex");
  CheckGetDalvikCacheFilenameOrDie("/system/framework/core.jar", "system@framework@core.jar@classes.dex");
  CheckGetDalvikCacheFilenameOrDie("/system/framework/boot.art", "system@framework@boot.art");
}

}  // namespace art
