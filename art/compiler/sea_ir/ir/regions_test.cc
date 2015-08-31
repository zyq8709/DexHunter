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
#include "sea_ir/ir/sea.h"

using utils::ScopedHashtable;

namespace sea_ir {

class RegionsTest : public art::CommonTest {
};

TEST_F(RegionsTest, Basics) {
  sea_ir::SeaGraph sg(*java_lang_dex_file_);
  sea_ir::Region* root = sg.GetNewRegion();
  sea_ir::Region* then_region = sg.GetNewRegion();
  sea_ir::Region* else_region = sg.GetNewRegion();
  std::vector<sea_ir::Region*>* regions = sg.GetRegions();
  // Test that regions have been registered correctly as children of the graph.
  EXPECT_TRUE(std::find(regions->begin(), regions->end(), root) != regions->end());
  EXPECT_TRUE(std::find(regions->begin(), regions->end(), then_region) != regions->end());
  EXPECT_TRUE(std::find(regions->begin(), regions->end(), else_region) != regions->end());
  // Check that an edge recorded correctly in both the head and the tail.
  sg.AddEdge(root, then_region);
  std::vector<sea_ir::Region*>* succs = root->GetSuccessors();
  EXPECT_EQ(1U, succs->size());
  EXPECT_EQ(then_region, succs->at(0));
  std::vector<sea_ir::Region*>* preds = then_region->GetPredecessors();
  EXPECT_EQ(1U, preds->size());
  EXPECT_EQ(root, preds->at(0));
  // Check that two edges are recorded properly for both head and tail.
  sg.AddEdge(root, else_region);
  succs = root->GetSuccessors();
  EXPECT_EQ(2U, succs->size());
  EXPECT_TRUE(std::find(succs->begin(), succs->end(), then_region) != succs->end());
  EXPECT_TRUE(std::find(succs->begin(), succs->end(), else_region) != succs->end());
  preds = then_region->GetPredecessors();
  EXPECT_EQ(1U, preds->size());
  EXPECT_EQ(root, preds->at(0));
  preds = else_region->GetPredecessors();
  EXPECT_EQ(1U, preds->size());
  EXPECT_EQ(root, preds->at(0));
}

}  // namespace sea_ir
