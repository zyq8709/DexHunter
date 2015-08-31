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

#include "reference_table.h"

namespace art {

class ReferenceTableTest : public CommonTest {
};

TEST_F(ReferenceTableTest, Basics) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::Object* o1 = mirror::String::AllocFromModifiedUtf8(soa.Self(), "hello");

  ReferenceTable rt("test", 0, 11);

  // Check dumping the empty table.
  {
    std::ostringstream oss;
    rt.Dump(oss);
    EXPECT_NE(oss.str().find("(empty)"), std::string::npos) << oss.str();
    EXPECT_EQ(0U, rt.Size());
  }

  // Check removal of all NULLs in a empty table is a no-op.
  rt.Remove(NULL);
  EXPECT_EQ(0U, rt.Size());

  // Check removal of all o1 in a empty table is a no-op.
  rt.Remove(o1);
  EXPECT_EQ(0U, rt.Size());

  // Add o1 and check we have 1 element and can dump.
  {
    rt.Add(o1);
    EXPECT_EQ(1U, rt.Size());
    std::ostringstream oss;
    rt.Dump(oss);
    EXPECT_NE(oss.str().find("1 of java.lang.String"), std::string::npos) << oss.str();
    EXPECT_EQ(oss.str().find("short[]"), std::string::npos) << oss.str();
  }

  // Add a second object 10 times and check dumping is sane.
  mirror::Object* o2 = mirror::ShortArray::Alloc(soa.Self(), 0);
  for (size_t i = 0; i < 10; ++i) {
    rt.Add(o2);
    EXPECT_EQ(i + 2, rt.Size());
    std::ostringstream oss;
    rt.Dump(oss);
    EXPECT_NE(oss.str().find(StringPrintf("Last %zd entries (of %zd):",
                                          i + 2 > 10 ? 10 : i + 2,
                                          i + 2)),
              std::string::npos) << oss.str();
    EXPECT_NE(oss.str().find("1 of java.lang.String"), std::string::npos) << oss.str();
    if (i == 0) {
      EXPECT_NE(oss.str().find("1 of short[]"), std::string::npos) << oss.str();
    } else {
      EXPECT_NE(oss.str().find(StringPrintf("%zd of short[] (1 unique instances)", i + 1)),
                std::string::npos) << oss.str();
    }
  }

  // Remove o1 (first element).
  {
    rt.Remove(o1);
    EXPECT_EQ(10U, rt.Size());
    std::ostringstream oss;
    rt.Dump(oss);
    EXPECT_EQ(oss.str().find("java.lang.String"), std::string::npos) << oss.str();
  }

  // Remove o2 ten times.
  for (size_t i = 0; i < 10; ++i) {
    rt.Remove(o2);
    EXPECT_EQ(9 - i, rt.Size());
    std::ostringstream oss;
    rt.Dump(oss);
    if (i == 9) {
      EXPECT_EQ(oss.str().find("short[]"), std::string::npos) << oss.str();
    } else if (i == 8) {
      EXPECT_NE(oss.str().find("1 of short[]"), std::string::npos) << oss.str();
    } else {
      EXPECT_NE(oss.str().find(StringPrintf("%zd of short[] (1 unique instances)", 10 - i - 1)),
                std::string::npos) << oss.str();
    }
  }
}

}  // namespace art
