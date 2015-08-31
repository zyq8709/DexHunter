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

#include "gtest/gtest.h"
#include "indenter.h"

TEST(IndenterTest, MultiLineTest) {
  std::ostringstream output;
  Indenter indent_filter(output.rdbuf(), '\t', 2);
  std::ostream input(&indent_filter);

  EXPECT_EQ(output.str(), "");

  input << "hello";
  EXPECT_EQ(output.str(), "\t\thello");

  input << "\nhello again";
  EXPECT_EQ(output.str(), "\t\thello\n\t\thello again");

  input << "\n";
  EXPECT_EQ(output.str(), "\t\thello\n\t\thello again\n");
}
