/*
 * Copyright (C) 2013 The Android Open Source Project
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
#include "utils/scoped_hashtable.h"

using utils::ScopedHashtable;

namespace art {

class Value {
 public:
  explicit Value(int v):value_(v) {}
  int value_;
};

class ScopedHashtableTest : public CommonTest {
};

TEST_F(ScopedHashtableTest, Basics) {
  ScopedHashtable<int, Value*> sht;
  // Check table is empty when no scope is open.
  EXPECT_TRUE(NULL == sht.Lookup(1));

  // Check table is empty when scope open.
  sht.OpenScope();
  EXPECT_TRUE(NULL == sht.Lookup(1));
  // Check table is empty after closing scope.
  EXPECT_EQ(sht.CloseScope(), true);
  // Check closing scope on empty table is no-op.
  EXPECT_EQ(sht.CloseScope(), false);
  // Check that find in current scope works.
  sht.OpenScope();
  sht.Add(1, new Value(1));
  EXPECT_EQ(sht.Lookup(1)->value_, 1);
  // Check that updating values in current scope works.
  sht.Add(1, new Value(2));
  EXPECT_EQ(sht.Lookup(1)->value_, 2);
  // Check that find works in previous scope.
  sht.OpenScope();
  EXPECT_EQ(sht.Lookup(1)->value_, 2);
  // Check that shadowing scopes works.
  sht.Add(1, new Value(3));
  EXPECT_EQ(sht.Lookup(1)->value_, 3);
  // Check that having multiple keys work correctly.
  sht.Add(2, new Value(4));
  EXPECT_EQ(sht.Lookup(1)->value_, 3);
  EXPECT_EQ(sht.Lookup(2)->value_, 4);
  // Check that scope removal works corectly.
  sht.CloseScope();
  EXPECT_EQ(sht.Lookup(1)->value_, 2);
  EXPECT_TRUE(NULL == sht.Lookup(2));
}

}  // namespace art
