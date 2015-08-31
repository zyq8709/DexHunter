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

#include "runtime.h"

#include "UniquePtr.h"
#include "common_test.h"

namespace art {

class RuntimeTest : public CommonTest {};

TEST_F(RuntimeTest, ParsedOptions) {
  void* test_vfprintf = reinterpret_cast<void*>(0xa);
  void* test_abort = reinterpret_cast<void*>(0xb);
  void* test_exit = reinterpret_cast<void*>(0xc);
  void* null = reinterpret_cast<void*>(NULL);

  std::string lib_core(GetLibCoreDexFileName());

  std::string boot_class_path;
  boot_class_path += "-Xbootclasspath:";
  boot_class_path += lib_core;

  Runtime::Options options;
  options.push_back(std::make_pair(boot_class_path.c_str(), null));
  options.push_back(std::make_pair("-classpath", null));
  options.push_back(std::make_pair(lib_core.c_str(), null));
  options.push_back(std::make_pair("-cp", null));
  options.push_back(std::make_pair(lib_core.c_str(), null));
  options.push_back(std::make_pair("-Ximage:boot_image", null));
  options.push_back(std::make_pair("-Xcheck:jni", null));
  options.push_back(std::make_pair("-Xms2048", null));
  options.push_back(std::make_pair("-Xmx4k", null));
  options.push_back(std::make_pair("-Xss1m", null));
  options.push_back(std::make_pair("-XX:HeapTargetUtilization=0.75", null));
  options.push_back(std::make_pair("-Dfoo=bar", null));
  options.push_back(std::make_pair("-Dbaz=qux", null));
  options.push_back(std::make_pair("-verbose:gc,class,jni", null));
  options.push_back(std::make_pair("host-prefix", "host_prefix"));
  options.push_back(std::make_pair("vfprintf", test_vfprintf));
  options.push_back(std::make_pair("abort", test_abort));
  options.push_back(std::make_pair("exit", test_exit));
  UniquePtr<Runtime::ParsedOptions> parsed(Runtime::ParsedOptions::Create(options, false));
  ASSERT_TRUE(parsed.get() != NULL);

  EXPECT_EQ(lib_core, parsed->boot_class_path_string_);
  EXPECT_EQ(lib_core, parsed->class_path_string_);
  EXPECT_EQ(std::string("boot_image"), parsed->image_);
  EXPECT_EQ(true, parsed->check_jni_);
  EXPECT_EQ(2048U, parsed->heap_initial_size_);
  EXPECT_EQ(4 * KB, parsed->heap_maximum_size_);
  EXPECT_EQ(1 * MB, parsed->stack_size_);
  EXPECT_EQ(0.75, parsed->heap_target_utilization_);
  EXPECT_EQ("host_prefix", parsed->host_prefix_);
  EXPECT_TRUE(test_vfprintf == parsed->hook_vfprintf_);
  EXPECT_TRUE(test_exit == parsed->hook_exit_);
  EXPECT_TRUE(test_abort == parsed->hook_abort_);
  EXPECT_TRUE(VLOG_IS_ON(class_linker));
  EXPECT_FALSE(VLOG_IS_ON(compiler));
  EXPECT_FALSE(VLOG_IS_ON(heap));
  EXPECT_TRUE(VLOG_IS_ON(gc));
  EXPECT_FALSE(VLOG_IS_ON(jdwp));
  EXPECT_TRUE(VLOG_IS_ON(jni));
  EXPECT_FALSE(VLOG_IS_ON(monitor));
  EXPECT_FALSE(VLOG_IS_ON(startup));
  EXPECT_FALSE(VLOG_IS_ON(third_party_jni));
  EXPECT_FALSE(VLOG_IS_ON(threads));
  ASSERT_EQ(2U, parsed->properties_.size());
  EXPECT_EQ("foo=bar", parsed->properties_[0]);
  EXPECT_EQ("baz=qux", parsed->properties_[1]);
}

}  // namespace art
