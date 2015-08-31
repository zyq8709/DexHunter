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
#include "sea_ir/types/type_inference_visitor.h"
#include "sea_ir/ir/sea.h"

namespace sea_ir {

class TestInstructionNode:public InstructionNode {
 public:
  explicit TestInstructionNode(std::vector<InstructionNode*> prods): InstructionNode(NULL),
      producers_(prods) { }
  std::vector<InstructionNode*> GetSSAProducers() {
    return producers_;
  }
 protected:
  std::vector<InstructionNode*> producers_;
};

class TypeInferenceVisitorTest : public art::CommonTest {
};

TEST_F(TypeInferenceVisitorTest, MergeIntWithByte) {
  TypeData td;
  art::verifier::RegTypeCache type_cache(false);
  TypeInferenceVisitor tiv(NULL, &td, &type_cache);
  const Type* int_type = &type_cache.Integer();
  const Type* byte_type = &type_cache.Byte();
  const Type* ib_type = tiv.MergeTypes(int_type, byte_type);
  const Type* bi_type = tiv.MergeTypes(byte_type, int_type);
  EXPECT_TRUE(ib_type == int_type);
  EXPECT_TRUE(bi_type == int_type);
}

TEST_F(TypeInferenceVisitorTest, MergeIntWithShort) {
  TypeData td;
  art::verifier::RegTypeCache type_cache(false);
  TypeInferenceVisitor tiv(NULL, &td, &type_cache);
  const Type* int_type = &type_cache.Integer();
  const Type* short_type = &type_cache.Short();
  const Type* is_type = tiv.MergeTypes(int_type, short_type);
  const Type* si_type = tiv.MergeTypes(short_type, int_type);
  EXPECT_TRUE(is_type == int_type);
  EXPECT_TRUE(si_type == int_type);
}

TEST_F(TypeInferenceVisitorTest, MergeMultipleInts) {
  int N = 10;  // Number of types to merge.
  TypeData td;
  art::verifier::RegTypeCache type_cache(false);
  TypeInferenceVisitor tiv(NULL, &td, &type_cache);
  std::vector<const Type*> types;
  for (int i = 0; i < N; i++) {
    const Type* new_type = &type_cache.Integer();
    types.push_back(new_type);
  }
  const Type* merged_type = tiv.MergeTypes(types);
  EXPECT_TRUE(merged_type == &type_cache.Integer());
}

TEST_F(TypeInferenceVisitorTest, MergeMultipleShorts) {
  int N = 10;  // Number of types to merge.
  TypeData td;
  art::verifier::RegTypeCache type_cache(false);
  TypeInferenceVisitor tiv(NULL, &td, &type_cache);
  std::vector<const Type*> types;
  for (int i = 0; i < N; i++) {
    const Type* new_type = &type_cache.Short();
    types.push_back(new_type);
  }
  const Type* merged_type = tiv.MergeTypes(types);
  EXPECT_TRUE(merged_type == &type_cache.Short());
}

TEST_F(TypeInferenceVisitorTest, MergeMultipleIntsWithShorts) {
  int N = 10;  // Number of types to merge.
  TypeData td;
  art::verifier::RegTypeCache type_cache(false);
  TypeInferenceVisitor tiv(NULL, &td, &type_cache);
  std::vector<const Type*> types;
  for (int i = 0; i < N; i++) {
    const Type* short_type = &type_cache.Short();
    const Type* int_type = &type_cache.Integer();
    types.push_back(short_type);
    types.push_back(int_type);
  }
  const Type* merged_type = tiv.MergeTypes(types);
  EXPECT_TRUE(merged_type == &type_cache.Integer());
}

TEST_F(TypeInferenceVisitorTest, GetOperandTypes) {
  int N = 10;  // Number of types to merge.
  TypeData td;
  art::verifier::RegTypeCache type_cache(false);
  TypeInferenceVisitor tiv(NULL, &td, &type_cache);
  std::vector<const Type*> types;
  std::vector<InstructionNode*> preds;
  for (int i = 0; i < N; i++) {
    const Type* short_type = &type_cache.Short();
    const Type* int_type = &type_cache.Integer();
    TestInstructionNode* short_inst =
        new TestInstructionNode(std::vector<InstructionNode*>());
    TestInstructionNode* int_inst =
        new TestInstructionNode(std::vector<InstructionNode*>());
    preds.push_back(short_inst);
    preds.push_back(int_inst);
    td.SetTypeOf(short_inst->Id(), short_type);
    td.SetTypeOf(int_inst->Id(), int_type);
    types.push_back(short_type);
    types.push_back(int_type);
  }
  TestInstructionNode* inst_to_test = new TestInstructionNode(preds);
  std::vector<const Type*> result = tiv.GetOperandTypes(inst_to_test);
  EXPECT_TRUE(result.size() == types.size());
  EXPECT_TRUE(true == std::equal(types.begin(), types.begin() + 2, result.begin()));
}


}  // namespace sea_ir
