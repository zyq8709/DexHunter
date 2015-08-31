/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include "indirect_reference_table.h"

namespace art {

class IndirectReferenceTableTest : public CommonTest {
};

static void CheckDump(IndirectReferenceTable* irt, size_t num_objects, size_t num_unique)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::ostringstream oss;
  irt->Dump(oss);
  if (num_objects == 0) {
    EXPECT_EQ(oss.str().find("java.lang.Object"), std::string::npos) << oss.str();
  } else if (num_objects == 1) {
    EXPECT_NE(oss.str().find("1 of java.lang.Object"), std::string::npos) << oss.str();
  } else {
    EXPECT_NE(oss.str().find(StringPrintf("%zd of java.lang.Object (%zd unique instances)",
                                          num_objects, num_unique)),
              std::string::npos)
                  << "\n Expected number of objects: " << num_objects
                  << "\n Expected unique objects: " << num_unique << "\n"
                  << oss.str();
  }
}

TEST_F(IndirectReferenceTableTest, BasicTest) {
  ScopedObjectAccess soa(Thread::Current());
  static const size_t kTableInitial = 10;
  static const size_t kTableMax = 20;
  IndirectReferenceTable irt(kTableInitial, kTableMax, kGlobal);

  mirror::Class* c = class_linker_->FindSystemClass("Ljava/lang/Object;");
  ASSERT_TRUE(c != NULL);
  mirror::Object* obj0 = c->AllocObject(soa.Self());
  ASSERT_TRUE(obj0 != NULL);
  mirror::Object* obj1 = c->AllocObject(soa.Self());
  ASSERT_TRUE(obj1 != NULL);
  mirror::Object* obj2 = c->AllocObject(soa.Self());
  ASSERT_TRUE(obj2 != NULL);
  mirror::Object* obj3 = c->AllocObject(soa.Self());
  ASSERT_TRUE(obj3 != NULL);

  const uint32_t cookie = IRT_FIRST_SEGMENT;

  CheckDump(&irt, 0, 0);

  IndirectRef iref0 = (IndirectRef) 0x11110;
  EXPECT_FALSE(irt.Remove(cookie, iref0)) << "unexpectedly successful removal";

  // Add three, check, remove in the order in which they were added.
  iref0 = irt.Add(cookie, obj0);
  EXPECT_TRUE(iref0 != NULL);
  CheckDump(&irt, 1, 1);
  IndirectRef iref1 = irt.Add(cookie, obj1);
  EXPECT_TRUE(iref1 != NULL);
  CheckDump(&irt, 2, 2);
  IndirectRef iref2 = irt.Add(cookie, obj2);
  EXPECT_TRUE(iref2 != NULL);
  CheckDump(&irt, 3, 3);

  EXPECT_EQ(obj0, irt.Get(iref0));
  EXPECT_EQ(obj1, irt.Get(iref1));
  EXPECT_EQ(obj2, irt.Get(iref2));

  EXPECT_TRUE(irt.Remove(cookie, iref0));
  CheckDump(&irt, 2, 2);
  EXPECT_TRUE(irt.Remove(cookie, iref1));
  CheckDump(&irt, 1, 1);
  EXPECT_TRUE(irt.Remove(cookie, iref2));
  CheckDump(&irt, 0, 0);

  // Table should be empty now.
  EXPECT_EQ(0U, irt.Capacity());

  // Get invalid entry (off the end of the list).
  EXPECT_EQ(kInvalidIndirectRefObject, irt.Get(iref0));

  // Add three, remove in the opposite order.
  iref0 = irt.Add(cookie, obj0);
  EXPECT_TRUE(iref0 != NULL);
  iref1 = irt.Add(cookie, obj1);
  EXPECT_TRUE(iref1 != NULL);
  iref2 = irt.Add(cookie, obj2);
  EXPECT_TRUE(iref2 != NULL);
  CheckDump(&irt, 3, 3);

  ASSERT_TRUE(irt.Remove(cookie, iref2));
  CheckDump(&irt, 2, 2);
  ASSERT_TRUE(irt.Remove(cookie, iref1));
  CheckDump(&irt, 1, 1);
  ASSERT_TRUE(irt.Remove(cookie, iref0));
  CheckDump(&irt, 0, 0);

  // Table should be empty now.
  ASSERT_EQ(0U, irt.Capacity());

  // Add three, remove middle / middle / bottom / top.  (Second attempt
  // to remove middle should fail.)
  iref0 = irt.Add(cookie, obj0);
  EXPECT_TRUE(iref0 != NULL);
  iref1 = irt.Add(cookie, obj1);
  EXPECT_TRUE(iref1 != NULL);
  iref2 = irt.Add(cookie, obj2);
  EXPECT_TRUE(iref2 != NULL);
  CheckDump(&irt, 3, 3);

  ASSERT_EQ(3U, irt.Capacity());

  ASSERT_TRUE(irt.Remove(cookie, iref1));
  CheckDump(&irt, 2, 2);
  ASSERT_FALSE(irt.Remove(cookie, iref1));
  CheckDump(&irt, 2, 2);

  // Get invalid entry (from hole).
  EXPECT_EQ(kInvalidIndirectRefObject, irt.Get(iref1));

  ASSERT_TRUE(irt.Remove(cookie, iref2));
  CheckDump(&irt, 1, 1);
  ASSERT_TRUE(irt.Remove(cookie, iref0));
  CheckDump(&irt, 0, 0);

  // Table should be empty now.
  ASSERT_EQ(0U, irt.Capacity());

  // Add four entries.  Remove #1, add new entry, verify that table size
  // is still 4 (i.e. holes are getting filled).  Remove #1 and #3, verify
  // that we delete one and don't hole-compact the other.
  iref0 = irt.Add(cookie, obj0);
  EXPECT_TRUE(iref0 != NULL);
  iref1 = irt.Add(cookie, obj1);
  EXPECT_TRUE(iref1 != NULL);
  iref2 = irt.Add(cookie, obj2);
  EXPECT_TRUE(iref2 != NULL);
  IndirectRef iref3 = irt.Add(cookie, obj3);
  EXPECT_TRUE(iref3 != NULL);
  CheckDump(&irt, 4, 4);

  ASSERT_TRUE(irt.Remove(cookie, iref1));
  CheckDump(&irt, 3, 3);

  iref1 = irt.Add(cookie, obj1);
  EXPECT_TRUE(iref1 != NULL);

  ASSERT_EQ(4U, irt.Capacity()) << "hole not filled";
  CheckDump(&irt, 4, 4);

  ASSERT_TRUE(irt.Remove(cookie, iref1));
  CheckDump(&irt, 3, 3);
  ASSERT_TRUE(irt.Remove(cookie, iref3));
  CheckDump(&irt, 2, 2);

  ASSERT_EQ(3U, irt.Capacity()) << "should be 3 after two deletions";

  ASSERT_TRUE(irt.Remove(cookie, iref2));
  CheckDump(&irt, 1, 1);
  ASSERT_TRUE(irt.Remove(cookie, iref0));
  CheckDump(&irt, 0, 0);

  ASSERT_EQ(0U, irt.Capacity()) << "not empty after split remove";

  // Add an entry, remove it, add a new entry, and try to use the original
  // iref.  They have the same slot number but are for different objects.
  // With the extended checks in place, this should fail.
  iref0 = irt.Add(cookie, obj0);
  EXPECT_TRUE(iref0 != NULL);
  CheckDump(&irt, 1, 1);
  ASSERT_TRUE(irt.Remove(cookie, iref0));
  CheckDump(&irt, 0, 0);
  iref1 = irt.Add(cookie, obj1);
  EXPECT_TRUE(iref1 != NULL);
  CheckDump(&irt, 1, 1);
  ASSERT_FALSE(irt.Remove(cookie, iref0)) << "mismatched del succeeded";
  CheckDump(&irt, 1, 1);
  ASSERT_TRUE(irt.Remove(cookie, iref1)) << "switched del failed";
  ASSERT_EQ(0U, irt.Capacity()) << "switching del not empty";
  CheckDump(&irt, 0, 0);

  // Same as above, but with the same object.  A more rigorous checker
  // (e.g. with slot serialization) will catch this.
  iref0 = irt.Add(cookie, obj0);
  EXPECT_TRUE(iref0 != NULL);
  CheckDump(&irt, 1, 1);
  ASSERT_TRUE(irt.Remove(cookie, iref0));
  CheckDump(&irt, 0, 0);
  iref1 = irt.Add(cookie, obj0);
  EXPECT_TRUE(iref1 != NULL);
  CheckDump(&irt, 1, 1);
  if (iref0 != iref1) {
    // Try 0, should not work.
    ASSERT_FALSE(irt.Remove(cookie, iref0)) << "temporal del succeeded";
  }
  ASSERT_TRUE(irt.Remove(cookie, iref1)) << "temporal cleanup failed";
  ASSERT_EQ(0U, irt.Capacity()) << "temporal del not empty";
  CheckDump(&irt, 0, 0);

  // NULL isn't a valid iref.
  ASSERT_EQ(kInvalidIndirectRefObject, irt.Get(NULL));

  // Stale lookup.
  iref0 = irt.Add(cookie, obj0);
  EXPECT_TRUE(iref0 != NULL);
  CheckDump(&irt, 1, 1);
  ASSERT_TRUE(irt.Remove(cookie, iref0));
  EXPECT_EQ(kInvalidIndirectRefObject, irt.Get(iref0)) << "stale lookup succeeded";
  CheckDump(&irt, 0, 0);

  // Test table resizing.
  // These ones fit...
  IndirectRef manyRefs[kTableInitial];
  for (size_t i = 0; i < kTableInitial; i++) {
    manyRefs[i] = irt.Add(cookie, obj0);
    ASSERT_TRUE(manyRefs[i] != NULL) << "Failed adding " << i;
    CheckDump(&irt, i + 1, 1);
  }
  // ...this one causes overflow.
  iref0 = irt.Add(cookie, obj0);
  ASSERT_TRUE(iref0 != NULL);
  ASSERT_EQ(kTableInitial + 1, irt.Capacity());
  CheckDump(&irt, kTableInitial + 1, 1);

  for (size_t i = 0; i < kTableInitial; i++) {
    ASSERT_TRUE(irt.Remove(cookie, manyRefs[i])) << "failed removing " << i;
    CheckDump(&irt, kTableInitial - i, 1);
  }
  // Because of removal order, should have 11 entries, 10 of them holes.
  ASSERT_EQ(kTableInitial + 1, irt.Capacity());

  ASSERT_TRUE(irt.Remove(cookie, iref0)) << "multi-remove final failed";

  ASSERT_EQ(0U, irt.Capacity()) << "multi-del not empty";
  CheckDump(&irt, 0, 0);
}

}  // namespace art
