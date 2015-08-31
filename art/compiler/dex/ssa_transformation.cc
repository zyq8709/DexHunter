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

#include "compiler_internals.h"
#include "dataflow_iterator-inl.h"

#define NOTVISITED (-1)

namespace art {

void MIRGraph::ClearAllVisitedFlags() {
  AllNodesIterator iter(this, false /* not iterative */);
  for (BasicBlock* bb = iter.Next(); bb != NULL; bb = iter.Next()) {
    bb->visited = false;
  }
}

BasicBlock* MIRGraph::NeedsVisit(BasicBlock* bb) {
  if (bb != NULL) {
    if (bb->visited || bb->hidden) {
      bb = NULL;
    }
  }
  return bb;
}

BasicBlock* MIRGraph::NextUnvisitedSuccessor(BasicBlock* bb) {
  BasicBlock* res = NeedsVisit(bb->fall_through);
  if (res == NULL) {
    res = NeedsVisit(bb->taken);
    if (res == NULL) {
      if (bb->successor_block_list.block_list_type != kNotUsed) {
        GrowableArray<SuccessorBlockInfo*>::Iterator iterator(bb->successor_block_list.blocks);
        while (true) {
          SuccessorBlockInfo *sbi = iterator.Next();
          if (sbi == NULL) {
            break;
          }
          res = NeedsVisit(sbi->block);
          if (res != NULL) {
            break;
          }
        }
      }
    }
  }
  return res;
}

void MIRGraph::MarkPreOrder(BasicBlock* block) {
  block->visited = true;
  /* Enqueue the pre_order block id */
  dfs_order_->Insert(block->id);
}

void MIRGraph::RecordDFSOrders(BasicBlock* block) {
  std::vector<BasicBlock*> succ;
  MarkPreOrder(block);
  succ.push_back(block);
  while (!succ.empty()) {
    BasicBlock* curr = succ.back();
    BasicBlock* next_successor = NextUnvisitedSuccessor(curr);
    if (next_successor != NULL) {
      MarkPreOrder(next_successor);
      succ.push_back(next_successor);
      continue;
    }
    curr->dfs_id = dfs_post_order_->Size();
    dfs_post_order_->Insert(curr->id);
    succ.pop_back();
  }
}

/* Sort the blocks by the Depth-First-Search */
void MIRGraph::ComputeDFSOrders() {
  /* Initialize or reset the DFS pre_order list */
  if (dfs_order_ == NULL) {
    dfs_order_ = new (arena_) GrowableArray<int>(arena_, GetNumBlocks(), kGrowableArrayDfsOrder);
  } else {
    /* Just reset the used length on the counter */
    dfs_order_->Reset();
  }

  /* Initialize or reset the DFS post_order list */
  if (dfs_post_order_ == NULL) {
    dfs_post_order_ = new (arena_) GrowableArray<int>(arena_, GetNumBlocks(), kGrowableArrayDfsPostOrder);
  } else {
    /* Just reset the used length on the counter */
    dfs_post_order_->Reset();
  }

  // Reset visited flags from all nodes
  ClearAllVisitedFlags();

  // Record dfs orders
  RecordDFSOrders(GetEntryBlock());

  num_reachable_blocks_ = dfs_order_->Size();
}

/*
 * Mark block bit on the per-Dalvik register vector to denote that Dalvik
 * register idx is defined in BasicBlock bb.
 */
bool MIRGraph::FillDefBlockMatrix(BasicBlock* bb) {
  if (bb->data_flow_info == NULL) {
    return false;
  }

  ArenaBitVector::Iterator iterator(bb->data_flow_info->def_v);
  while (true) {
    int idx = iterator.Next();
    if (idx == -1) {
      break;
    }
    /* Block bb defines register idx */
    def_block_matrix_[idx]->SetBit(bb->id);
  }
  return true;
}

void MIRGraph::ComputeDefBlockMatrix() {
  int num_registers = cu_->num_dalvik_registers;
  /* Allocate num_dalvik_registers bit vector pointers */
  def_block_matrix_ = static_cast<ArenaBitVector**>
      (arena_->Alloc(sizeof(ArenaBitVector *) * num_registers,
                     ArenaAllocator::kAllocDFInfo));
  int i;

  /* Initialize num_register vectors with num_blocks bits each */
  for (i = 0; i < num_registers; i++) {
    def_block_matrix_[i] =
        new (arena_) ArenaBitVector(arena_, GetNumBlocks(), false, kBitMapBMatrix);
  }
  AllNodesIterator iter(this, false /* not iterative */);
  for (BasicBlock* bb = iter.Next(); bb != NULL; bb = iter.Next()) {
    FindLocalLiveIn(bb);
  }
  AllNodesIterator iter2(this, false /* not iterative */);
  for (BasicBlock* bb = iter2.Next(); bb != NULL; bb = iter2.Next()) {
    FillDefBlockMatrix(bb);
  }

  /*
   * Also set the incoming parameters as defs in the entry block.
   * Only need to handle the parameters for the outer method.
   */
  int num_regs = cu_->num_dalvik_registers;
  int in_reg = num_regs - cu_->num_ins;
  for (; in_reg < num_regs; in_reg++) {
    def_block_matrix_[in_reg]->SetBit(GetEntryBlock()->id);
  }
}

void MIRGraph::ComputeDomPostOrderTraversal(BasicBlock* bb) {
  if (dom_post_order_traversal_ == NULL) {
    // First time - create the array.
    dom_post_order_traversal_ =
        new (arena_) GrowableArray<int>(arena_, num_reachable_blocks_,
                                        kGrowableArrayDomPostOrderTraversal);
  } else {
    dom_post_order_traversal_->Reset();
  }
  ClearAllVisitedFlags();
  std::vector<std::pair<BasicBlock*, ArenaBitVector::Iterator*> > work_stack;
  bb->visited = true;
  work_stack.push_back(std::make_pair(bb, new (arena_) ArenaBitVector::Iterator(bb->i_dominated)));
  while (!work_stack.empty()) {
    std::pair<BasicBlock*, ArenaBitVector::Iterator*> curr = work_stack.back();
    BasicBlock* curr_bb = curr.first;
    ArenaBitVector::Iterator* curr_idom_iter = curr.second;
    int bb_idx = curr_idom_iter->Next();
    while ((bb_idx != -1) && (NeedsVisit(GetBasicBlock(bb_idx)) == NULL)) {
      bb_idx = curr_idom_iter->Next();
    }
    if (bb_idx != -1) {
      BasicBlock* new_bb = GetBasicBlock(bb_idx);
      new_bb->visited = true;
      work_stack.push_back(
          std::make_pair(new_bb, new (arena_) ArenaBitVector::Iterator(new_bb->i_dominated)));
    } else {
      // no successor/next
      dom_post_order_traversal_->Insert(curr_bb->id);
      work_stack.pop_back();

      /* hacky loop detection */
      if (curr_bb->taken && curr_bb->dominators->IsBitSet(curr_bb->taken->id)) {
        attributes_ |= METHOD_HAS_LOOP;
      }
    }
  }
}

void MIRGraph::CheckForDominanceFrontier(BasicBlock* dom_bb,
                                         const BasicBlock* succ_bb) {
  /*
   * TODO - evaluate whether phi will ever need to be inserted into exit
   * blocks.
   */
  if (succ_bb->i_dom != dom_bb &&
    succ_bb->block_type == kDalvikByteCode &&
    succ_bb->hidden == false) {
    dom_bb->dom_frontier->SetBit(succ_bb->id);
  }
}

/* Worker function to compute the dominance frontier */
bool MIRGraph::ComputeDominanceFrontier(BasicBlock* bb) {
  /* Calculate DF_local */
  if (bb->taken) {
    CheckForDominanceFrontier(bb, bb->taken);
  }
  if (bb->fall_through) {
    CheckForDominanceFrontier(bb, bb->fall_through);
  }
  if (bb->successor_block_list.block_list_type != kNotUsed) {
    GrowableArray<SuccessorBlockInfo*>::Iterator iterator(bb->successor_block_list.blocks);
      while (true) {
        SuccessorBlockInfo *successor_block_info = iterator.Next();
        if (successor_block_info == NULL) {
          break;
        }
        BasicBlock* succ_bb = successor_block_info->block;
        CheckForDominanceFrontier(bb, succ_bb);
      }
  }

  /* Calculate DF_up */
  ArenaBitVector::Iterator bv_iterator(bb->i_dominated);
  while (true) {
    // TUNING: hot call to BitVectorIteratorNext
    int dominated_idx = bv_iterator.Next();
    if (dominated_idx == -1) {
      break;
    }
    BasicBlock* dominated_bb = GetBasicBlock(dominated_idx);
    ArenaBitVector::Iterator df_iterator(dominated_bb->dom_frontier);
    while (true) {
      // TUNING: hot call to BitVectorIteratorNext
      int df_up_idx = df_iterator.Next();
      if (df_up_idx == -1) {
        break;
      }
      BasicBlock* df_up_block = GetBasicBlock(df_up_idx);
      CheckForDominanceFrontier(bb, df_up_block);
    }
  }

  return true;
}

/* Worker function for initializing domination-related data structures */
void MIRGraph::InitializeDominationInfo(BasicBlock* bb) {
  int num_total_blocks = GetBasicBlockListCount();

  if (bb->dominators == NULL) {
    bb->dominators = new (arena_) ArenaBitVector(arena_, num_total_blocks,
                                                 false /* expandable */, kBitMapDominators);
    bb->i_dominated = new (arena_) ArenaBitVector(arena_, num_total_blocks,
                                                  false /* expandable */, kBitMapIDominated);
    bb->dom_frontier = new (arena_) ArenaBitVector(arena_, num_total_blocks,
                                                   false /* expandable */, kBitMapDomFrontier);
  } else {
    bb->dominators->ClearAllBits();
    bb->i_dominated->ClearAllBits();
    bb->dom_frontier->ClearAllBits();
  }
  /* Set all bits in the dominator vector */
  bb->dominators->SetInitialBits(num_total_blocks);

  return;
}

/*
 * Walk through the ordered i_dom list until we reach common parent.
 * Given the ordering of i_dom_list, this common parent represents the
 * last element of the intersection of block1 and block2 dominators.
  */
int MIRGraph::FindCommonParent(int block1, int block2) {
  while (block1 != block2) {
    while (block1 < block2) {
      block1 = i_dom_list_[block1];
      DCHECK_NE(block1, NOTVISITED);
    }
    while (block2 < block1) {
      block2 = i_dom_list_[block2];
      DCHECK_NE(block2, NOTVISITED);
    }
  }
  return block1;
}

/* Worker function to compute each block's immediate dominator */
bool MIRGraph::ComputeblockIDom(BasicBlock* bb) {
  /* Special-case entry block */
  if (bb == GetEntryBlock()) {
    return false;
  }

  /* Iterate through the predecessors */
  GrowableArray<BasicBlock*>::Iterator iter(bb->predecessors);

  /* Find the first processed predecessor */
  int idom = -1;
  while (true) {
    BasicBlock* pred_bb = iter.Next();
    CHECK(pred_bb != NULL);
    if (i_dom_list_[pred_bb->dfs_id] != NOTVISITED) {
      idom = pred_bb->dfs_id;
      break;
    }
  }

  /* Scan the rest of the predecessors */
  while (true) {
      BasicBlock* pred_bb = iter.Next();
      if (!pred_bb) {
        break;
      }
      if (i_dom_list_[pred_bb->dfs_id] == NOTVISITED) {
        continue;
      } else {
        idom = FindCommonParent(pred_bb->dfs_id, idom);
      }
  }

  DCHECK_NE(idom, NOTVISITED);

  /* Did something change? */
  if (i_dom_list_[bb->dfs_id] != idom) {
    i_dom_list_[bb->dfs_id] = idom;
    return true;
  }
  return false;
}

/* Worker function to compute each block's domintors */
bool MIRGraph::ComputeBlockDominators(BasicBlock* bb) {
  if (bb == GetEntryBlock()) {
    bb->dominators->ClearAllBits();
  } else {
    bb->dominators->Copy(bb->i_dom->dominators);
  }
  bb->dominators->SetBit(bb->id);
  return false;
}

bool MIRGraph::SetDominators(BasicBlock* bb) {
  if (bb != GetEntryBlock()) {
    int idom_dfs_idx = i_dom_list_[bb->dfs_id];
    DCHECK_NE(idom_dfs_idx, NOTVISITED);
    int i_dom_idx = dfs_post_order_->Get(idom_dfs_idx);
    BasicBlock* i_dom = GetBasicBlock(i_dom_idx);
    bb->i_dom = i_dom;
    /* Add bb to the i_dominated set of the immediate dominator block */
    i_dom->i_dominated->SetBit(bb->id);
  }
  return false;
}

/* Compute dominators, immediate dominator, and dominance fronter */
void MIRGraph::ComputeDominators() {
  int num_reachable_blocks = num_reachable_blocks_;
  int num_total_blocks = GetBasicBlockListCount();

  /* Initialize domination-related data structures */
  ReachableNodesIterator iter(this, false /* not iterative */);
  for (BasicBlock* bb = iter.Next(); bb != NULL; bb = iter.Next()) {
    InitializeDominationInfo(bb);
  }

  /* Initalize & Clear i_dom_list */
  if (i_dom_list_ == NULL) {
    i_dom_list_ = static_cast<int*>(arena_->Alloc(sizeof(int) * num_reachable_blocks,
                                                  ArenaAllocator::kAllocDFInfo));
  }
  for (int i = 0; i < num_reachable_blocks; i++) {
    i_dom_list_[i] = NOTVISITED;
  }

  /* For post-order, last block is entry block.  Set its i_dom to istelf */
  DCHECK_EQ(GetEntryBlock()->dfs_id, num_reachable_blocks-1);
  i_dom_list_[GetEntryBlock()->dfs_id] = GetEntryBlock()->dfs_id;

  /* Compute the immediate dominators */
  ReversePostOrderDfsIterator iter2(this, true /* iterative */);
  bool change = false;
  for (BasicBlock* bb = iter2.Next(false); bb != NULL; bb = iter2.Next(change)) {
    change = ComputeblockIDom(bb);
  }

  /* Set the dominator for the root node */
  GetEntryBlock()->dominators->ClearAllBits();
  GetEntryBlock()->dominators->SetBit(GetEntryBlock()->id);

  if (temp_block_v_ == NULL) {
    temp_block_v_ = new (arena_) ArenaBitVector(arena_, num_total_blocks,
                                                false /* expandable */, kBitMapTmpBlockV);
  } else {
    temp_block_v_->ClearAllBits();
  }
  GetEntryBlock()->i_dom = NULL;

  ReachableNodesIterator iter3(this, false /* not iterative */);
  for (BasicBlock* bb = iter3.Next(); bb != NULL; bb = iter3.Next()) {
    SetDominators(bb);
  }

  ReversePostOrderDfsIterator iter4(this, false /* not iterative */);
  for (BasicBlock* bb = iter4.Next(); bb != NULL; bb = iter4.Next()) {
    ComputeBlockDominators(bb);
  }

  // Compute the dominance frontier for each block.
  ComputeDomPostOrderTraversal(GetEntryBlock());
  PostOrderDOMIterator iter5(this, false /* not iterative */);
  for (BasicBlock* bb = iter5.Next(); bb != NULL; bb = iter5.Next()) {
    ComputeDominanceFrontier(bb);
  }
}

/*
 * Perform dest U= src1 ^ ~src2
 * This is probably not general enough to be placed in BitVector.[ch].
 */
void MIRGraph::ComputeSuccLineIn(ArenaBitVector* dest, const ArenaBitVector* src1,
                                 const ArenaBitVector* src2) {
  if (dest->GetStorageSize() != src1->GetStorageSize() ||
      dest->GetStorageSize() != src2->GetStorageSize() ||
      dest->IsExpandable() != src1->IsExpandable() ||
      dest->IsExpandable() != src2->IsExpandable()) {
    LOG(FATAL) << "Incompatible set properties";
  }

  unsigned int idx;
  for (idx = 0; idx < dest->GetStorageSize(); idx++) {
    dest->GetRawStorage()[idx] |= src1->GetRawStorageWord(idx) & ~(src2->GetRawStorageWord(idx));
  }
}

/*
 * Iterate through all successor blocks and propagate up the live-in sets.
 * The calculated result is used for phi-node pruning - where we only need to
 * insert a phi node if the variable is live-in to the block.
 */
bool MIRGraph::ComputeBlockLiveIns(BasicBlock* bb) {
  ArenaBitVector* temp_dalvik_register_v = temp_dalvik_register_v_;

  if (bb->data_flow_info == NULL) {
    return false;
  }
  temp_dalvik_register_v->Copy(bb->data_flow_info->live_in_v);
  if (bb->taken && bb->taken->data_flow_info)
    ComputeSuccLineIn(temp_dalvik_register_v, bb->taken->data_flow_info->live_in_v,
                      bb->data_flow_info->def_v);
  if (bb->fall_through && bb->fall_through->data_flow_info)
    ComputeSuccLineIn(temp_dalvik_register_v, bb->fall_through->data_flow_info->live_in_v,
                      bb->data_flow_info->def_v);
  if (bb->successor_block_list.block_list_type != kNotUsed) {
    GrowableArray<SuccessorBlockInfo*>::Iterator iterator(bb->successor_block_list.blocks);
    while (true) {
      SuccessorBlockInfo *successor_block_info = iterator.Next();
      if (successor_block_info == NULL) {
        break;
      }
      BasicBlock* succ_bb = successor_block_info->block;
      if (succ_bb->data_flow_info) {
        ComputeSuccLineIn(temp_dalvik_register_v, succ_bb->data_flow_info->live_in_v,
                          bb->data_flow_info->def_v);
      }
    }
  }
  if (!temp_dalvik_register_v->Equal(bb->data_flow_info->live_in_v)) {
    bb->data_flow_info->live_in_v->Copy(temp_dalvik_register_v);
    return true;
  }
  return false;
}

/* Insert phi nodes to for each variable to the dominance frontiers */
void MIRGraph::InsertPhiNodes() {
  int dalvik_reg;
  ArenaBitVector* phi_blocks =
      new (arena_) ArenaBitVector(arena_, GetNumBlocks(), false, kBitMapPhi);
  ArenaBitVector* tmp_blocks =
      new (arena_) ArenaBitVector(arena_, GetNumBlocks(), false, kBitMapTmpBlocks);
  ArenaBitVector* input_blocks =
      new (arena_) ArenaBitVector(arena_, GetNumBlocks(), false, kBitMapInputBlocks);

  temp_dalvik_register_v_ =
      new (arena_) ArenaBitVector(arena_, cu_->num_dalvik_registers, false, kBitMapRegisterV);

  PostOrderDfsIterator iter(this, true /* iterative */);
  bool change = false;
  for (BasicBlock* bb = iter.Next(false); bb != NULL; bb = iter.Next(change)) {
    change = ComputeBlockLiveIns(bb);
  }

  /* Iterate through each Dalvik register */
  for (dalvik_reg = cu_->num_dalvik_registers - 1; dalvik_reg >= 0; dalvik_reg--) {
    bool change;

    input_blocks->Copy(def_block_matrix_[dalvik_reg]);
    phi_blocks->ClearAllBits();

    /* Calculate the phi blocks for each Dalvik register */
    do {
      change = false;
      tmp_blocks->ClearAllBits();
      ArenaBitVector::Iterator iterator(input_blocks);

      while (true) {
        int idx = iterator.Next();
        if (idx == -1) {
          break;
        }
        BasicBlock* def_bb = GetBasicBlock(idx);

        /* Merge the dominance frontier to tmp_blocks */
        // TUNING: hot call to Union().
        if (def_bb->dom_frontier != NULL) {
          tmp_blocks->Union(def_bb->dom_frontier);
        }
      }
      if (!phi_blocks->Equal(tmp_blocks)) {
        change = true;
        phi_blocks->Copy(tmp_blocks);

        /*
         * Iterate through the original blocks plus the new ones in
         * the dominance frontier.
         */
        input_blocks->Copy(phi_blocks);
        input_blocks->Union(def_block_matrix_[dalvik_reg]);
      }
    } while (change);

    /*
     * Insert a phi node for dalvik_reg in the phi_blocks if the Dalvik
     * register is in the live-in set.
     */
    ArenaBitVector::Iterator iterator(phi_blocks);
    while (true) {
      int idx = iterator.Next();
      if (idx == -1) {
        break;
      }
      BasicBlock* phi_bb = GetBasicBlock(idx);
      /* Variable will be clobbered before being used - no need for phi */
      if (!phi_bb->data_flow_info->live_in_v->IsBitSet(dalvik_reg)) {
        continue;
      }
      MIR *phi =
          static_cast<MIR*>(arena_->Alloc(sizeof(MIR), ArenaAllocator::kAllocDFInfo));
      phi->dalvikInsn.opcode = static_cast<Instruction::Code>(kMirOpPhi);
      phi->dalvikInsn.vA = dalvik_reg;
      phi->offset = phi_bb->start_offset;
      phi->m_unit_index = 0;  // Arbitrarily assign all Phi nodes to outermost method.
      PrependMIR(phi_bb, phi);
    }
  }
}

/*
 * Worker function to insert phi-operands with latest SSA names from
 * predecessor blocks
 */
bool MIRGraph::InsertPhiNodeOperands(BasicBlock* bb) {
  MIR *mir;
  std::vector<int> uses;
  std::vector<int> incoming_arc;

  /* Phi nodes are at the beginning of each block */
  for (mir = bb->first_mir_insn; mir != NULL; mir = mir->next) {
    if (mir->dalvikInsn.opcode != static_cast<Instruction::Code>(kMirOpPhi))
      return true;
    int ssa_reg = mir->ssa_rep->defs[0];
    DCHECK_GE(ssa_reg, 0);   // Shouldn't see compiler temps here
    int v_reg = SRegToVReg(ssa_reg);

    uses.clear();
    incoming_arc.clear();

    /* Iterate through the predecessors */
    GrowableArray<BasicBlock*>::Iterator iter(bb->predecessors);
    while (true) {
      BasicBlock* pred_bb = iter.Next();
      if (!pred_bb) {
        break;
      }
      int ssa_reg = pred_bb->data_flow_info->vreg_to_ssa_map[v_reg];
      uses.push_back(ssa_reg);
      incoming_arc.push_back(pred_bb->id);
    }

    /* Count the number of SSA registers for a Dalvik register */
    int num_uses = uses.size();
    mir->ssa_rep->num_uses = num_uses;
    mir->ssa_rep->uses =
        static_cast<int*>(arena_->Alloc(sizeof(int) * num_uses, ArenaAllocator::kAllocDFInfo));
    mir->ssa_rep->fp_use =
        static_cast<bool*>(arena_->Alloc(sizeof(bool) * num_uses, ArenaAllocator::kAllocDFInfo));
    int* incoming =
        static_cast<int*>(arena_->Alloc(sizeof(int) * num_uses, ArenaAllocator::kAllocDFInfo));
    // TODO: Ugly, rework (but don't burden each MIR/LIR for Phi-only needs)
    mir->dalvikInsn.vB = reinterpret_cast<uintptr_t>(incoming);

    /* Set the uses array for the phi node */
    int *use_ptr = mir->ssa_rep->uses;
    for (int i = 0; i < num_uses; i++) {
      *use_ptr++ = uses[i];
      *incoming++ = incoming_arc[i];
    }
  }

  return true;
}

void MIRGraph::DoDFSPreOrderSSARename(BasicBlock* block) {
  if (block->visited || block->hidden) {
    return;
  }
  block->visited = true;

  /* Process this block */
  DoSSAConversion(block);
  int map_size = sizeof(int) * cu_->num_dalvik_registers;

  /* Save SSA map snapshot */
  int* saved_ssa_map =
      static_cast<int*>(arena_->Alloc(map_size, ArenaAllocator::kAllocDalvikToSSAMap));
  memcpy(saved_ssa_map, vreg_to_ssa_map_, map_size);

  if (block->fall_through) {
    DoDFSPreOrderSSARename(block->fall_through);
    /* Restore SSA map snapshot */
    memcpy(vreg_to_ssa_map_, saved_ssa_map, map_size);
  }
  if (block->taken) {
    DoDFSPreOrderSSARename(block->taken);
    /* Restore SSA map snapshot */
    memcpy(vreg_to_ssa_map_, saved_ssa_map, map_size);
  }
  if (block->successor_block_list.block_list_type != kNotUsed) {
    GrowableArray<SuccessorBlockInfo*>::Iterator iterator(block->successor_block_list.blocks);
    while (true) {
      SuccessorBlockInfo *successor_block_info = iterator.Next();
      if (successor_block_info == NULL) {
        break;
      }
      BasicBlock* succ_bb = successor_block_info->block;
      DoDFSPreOrderSSARename(succ_bb);
      /* Restore SSA map snapshot */
      memcpy(vreg_to_ssa_map_, saved_ssa_map, map_size);
    }
  }
  vreg_to_ssa_map_ = saved_ssa_map;
  return;
}

/* Perform SSA transformation for the whole method */
void MIRGraph::SSATransformation() {
  /* Compute the DFS order */
  ComputeDFSOrders();

  /* Compute the dominator info */
  ComputeDominators();

  /* Allocate data structures in preparation for SSA conversion */
  CompilerInitializeSSAConversion();

  /* Find out the "Dalvik reg def x block" relation */
  ComputeDefBlockMatrix();

  /* Insert phi nodes to dominance frontiers for all variables */
  InsertPhiNodes();

  /* Rename register names by local defs and phi nodes */
  ClearAllVisitedFlags();
  DoDFSPreOrderSSARename(GetEntryBlock());

  /*
   * Shared temp bit vector used by each block to count the number of defs
   * from all the predecessor blocks.
   */
  temp_ssa_register_v_ =
      new (arena_) ArenaBitVector(arena_, GetNumSSARegs(), false, kBitMapTempSSARegisterV);

  /* Insert phi-operands with latest SSA names from predecessor blocks */
  ReachableNodesIterator iter2(this, false /* not iterative */);
  for (BasicBlock* bb = iter2.Next(); bb != NULL; bb = iter2.Next()) {
    InsertPhiNodeOperands(bb);
  }

  if (cu_->enable_debug & (1 << kDebugDumpCFG)) {
    DumpCFG("/sdcard/3_post_ssa_cfg/", false);
  }
  if (cu_->enable_debug & (1 << kDebugVerifyDataflow)) {
    VerifyDataflow();
  }
}

}  // namespace art
