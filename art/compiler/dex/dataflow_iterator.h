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

#ifndef ART_COMPILER_DEX_DATAFLOW_ITERATOR_H_
#define ART_COMPILER_DEX_DATAFLOW_ITERATOR_H_

#include "compiler_ir.h"
#include "mir_graph.h"

namespace art {

  /*
   * This class supports iterating over lists of basic blocks in various
   * interesting orders.  Note that for efficiency, the visit orders have been pre-computed.
   * The order itself will not change during the iteration.  However, for some uses,
   * auxiliary data associated with the basic blocks may be changed during the iteration,
   * necessitating another pass over the list.
   *
   * To support this usage, we have is_iterative_.  If false, the iteration is a one-shot
   * pass through the pre-computed list using Next().  If true, the caller must tell the
   * iterator whether a change has been made that necessitates another pass.  Use
   * Next(had_change) for this.  The general idea is that the iterative_ use case means
   * that the iterator will keep repeating the full basic block list until a complete pass
   * is made through it with no changes.  Note that calling Next(true) does not affect
   * the iteration order or short-curcuit the current pass - it simply tells the iterator
   * that once it has finished walking through the block list it should reset and do another
   * full pass through the list.
   */
  class DataflowIterator {
    public:
      virtual ~DataflowIterator() {}

      // Return the next BasicBlock* to visit.
      BasicBlock* Next() {
        DCHECK(!is_iterative_);
        return NextBody(false);
      }

      /*
       * Return the next BasicBlock* to visit, and tell the iterator whether any change
       * has occurred that requires another full pass over the block list.
       */
      BasicBlock* Next(bool had_change) {
        DCHECK(is_iterative_);
        return NextBody(had_change);
      }

    protected:
      DataflowIterator(MIRGraph* mir_graph, bool is_iterative, int start_idx, int end_idx,
                       bool reverse)
          : mir_graph_(mir_graph),
            is_iterative_(is_iterative),
            start_idx_(start_idx),
            end_idx_(end_idx),
            reverse_(reverse),
            block_id_list_(NULL),
            idx_(0),
            changed_(false) {}

      virtual BasicBlock* NextBody(bool had_change) ALWAYS_INLINE;

      MIRGraph* const mir_graph_;
      const bool is_iterative_;
      const int start_idx_;
      const int end_idx_;
      const bool reverse_;
      GrowableArray<int>* block_id_list_;
      int idx_;
      bool changed_;
  };  // DataflowIterator

  class ReachableNodesIterator : public DataflowIterator {
    public:
      ReachableNodesIterator(MIRGraph* mir_graph, bool is_iterative)
          : DataflowIterator(mir_graph, is_iterative, 0,
                             mir_graph->GetNumReachableBlocks(), false) {
        idx_ = start_idx_;
        block_id_list_ = mir_graph->GetDfsOrder();
      }
  };

  class PreOrderDfsIterator : public DataflowIterator {
    public:
      PreOrderDfsIterator(MIRGraph* mir_graph, bool is_iterative)
          : DataflowIterator(mir_graph, is_iterative, 0,
                             mir_graph->GetNumReachableBlocks(), false) {
        idx_ = start_idx_;
        block_id_list_ = mir_graph->GetDfsOrder();
      }
  };

  class PostOrderDfsIterator : public DataflowIterator {
    public:
      PostOrderDfsIterator(MIRGraph* mir_graph, bool is_iterative)
          : DataflowIterator(mir_graph, is_iterative, 0,
                             mir_graph->GetNumReachableBlocks(), false) {
        idx_ = start_idx_;
        block_id_list_ = mir_graph->GetDfsPostOrder();
      }
  };

  class ReversePostOrderDfsIterator : public DataflowIterator {
    public:
      ReversePostOrderDfsIterator(MIRGraph* mir_graph, bool is_iterative)
          : DataflowIterator(mir_graph, is_iterative,
                             mir_graph->GetNumReachableBlocks() -1, 0, true) {
        idx_ = start_idx_;
        block_id_list_ = mir_graph->GetDfsPostOrder();
      }
  };

  class PostOrderDOMIterator : public DataflowIterator {
    public:
      PostOrderDOMIterator(MIRGraph* mir_graph, bool is_iterative)
          : DataflowIterator(mir_graph, is_iterative, 0,
                             mir_graph->GetNumReachableBlocks(), false) {
        idx_ = start_idx_;
        block_id_list_ = mir_graph->GetDomPostOrder();
      }
  };

  class AllNodesIterator : public DataflowIterator {
    public:
      AllNodesIterator(MIRGraph* mir_graph, bool is_iterative)
          : DataflowIterator(mir_graph, is_iterative, 0, 0, false) {
        all_nodes_iterator_ =
            new (mir_graph->GetArena()) GrowableArray<BasicBlock*>::Iterator(mir_graph->GetBlockList());
      }

      void Reset() {
        all_nodes_iterator_->Reset();
      }

      BasicBlock* NextBody(bool had_change) ALWAYS_INLINE;

    private:
      GrowableArray<BasicBlock*>::Iterator* all_nodes_iterator_;
  };

}  // namespace art

#endif  // ART_COMPILER_DEX_DATAFLOW_ITERATOR_H_
