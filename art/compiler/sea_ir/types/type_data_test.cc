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
#include "sea_ir/types/types.h"

namespace sea_ir {

class TypeDataTest : public art::CommonTest {
};

TEST_F(TypeDataTest, Basics) {
  TypeData td;
  art::verifier::RegTypeCache type_cache(false);
  int first_instruction_id = 1;
  int second_instruction_id = 3;
  EXPECT_TRUE(NULL == td.FindTypeOf(first_instruction_id));
  const Type* int_type = &type_cache.Integer();
  const Type* byte_type = &type_cache.Byte();
  td.SetTypeOf(first_instruction_id, int_type);
  EXPECT_TRUE(int_type == td.FindTypeOf(first_instruction_id));
  EXPECT_TRUE(NULL == td.FindTypeOf(second_instruction_id));
  td.SetTypeOf(second_instruction_id, byte_type);
  EXPECT_TRUE(int_type == td.FindTypeOf(first_instruction_id));
  EXPECT_TRUE(byte_type == td.FindTypeOf(second_instruction_id));
}

}  // namespace sea_ir
