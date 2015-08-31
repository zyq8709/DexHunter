/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include "Dalvik.h"
#include "Dataflow.h"
#include "Loop.h"
#include "libdex/DexOpcodes.h"

/* Enter the node to the dfsOrder list then visit its successors */
static void recordDFSPreOrder(CompilationUnit *cUnit, BasicBlock *block)
{

    if (block->visited || block->hidden) return;
    block->visited = true;

    /* Enqueue the block id */
    dvmInsertGrowableList(&cUnit->dfsOrder, block->id);

    if (block->fallThrough) recordDFSPreOrder(cUnit, block->fallThrough);
    if (block->taken) recordDFSPreOrder(cUnit, block->taken);
    if (block->successorBlockList.blockListType != kNotUsed) {
        GrowableListIterator iterator;
        dvmGrowableListIteratorInit(&block->successorBlockList.blocks,
                                    &iterator);
        while (true) {
            SuccessorBlockInfo *successorBlockInfo =
                (SuccessorBlockInfo *) dvmGrowableListIteratorNext(&iterator);
            if (successorBlockInfo == NULL) break;
            BasicBlock *succBB = successorBlockInfo->block;
            recordDFSPreOrder(cUnit, succBB);
        }
    }
    return;
}

/* Sort the blocks by the Depth-First-Search pre-order */
static void computeDFSOrder(CompilationUnit *cUnit)
{
    /* Initialize or reset the DFS order list */
    if (cUnit->dfsOrder.elemList == NULL) {
        dvmInitGrowableList(&cUnit->dfsOrder, cUnit->numBlocks);
    } else {
        /* Just reset the used length on the counter */
        cUnit->dfsOrder.numUsed = 0;
    }

    dvmCompilerDataFlowAnalysisDispatcher(cUnit, dvmCompilerClearVisitedFlag,
                                          kAllNodes,
                                          false /* isIterative */);

    recordDFSPreOrder(cUnit, cUnit->entryBlock);
    cUnit->numReachableBlocks = cUnit->dfsOrder.numUsed;
}

/*
 * Mark block bit on the per-Dalvik register vector to denote that Dalvik
 * register idx is defined in BasicBlock bb.
 */
static bool fillDefBlockMatrix(CompilationUnit *cUnit, BasicBlock *bb)
{
    if (bb->dataFlowInfo == NULL) return false;

    BitVectorIterator iterator;

    dvmBitVectorIteratorInit(bb->dataFlowInfo->defV, &iterator);
    while (true) {
        int idx = dvmBitVectorIteratorNext(&iterator);
        if (idx == -1) break;
        /* Block bb defines register idx */
        dvmCompilerSetBit(cUnit->defBlockMatrix[idx], bb->id);
    }
    return true;
}

static void computeDefBlockMatrix(CompilationUnit *cUnit)
{
    int numRegisters = cUnit->numDalvikRegisters;
    /* Allocate numDalvikRegisters bit vector pointers */
    cUnit->defBlockMatrix = (BitVector **)
        dvmCompilerNew(sizeof(BitVector *) * numRegisters, true);
    int i;

    /* Initialize numRegister vectors with numBlocks bits each */
    for (i = 0; i < numRegisters; i++) {
        cUnit->defBlockMatrix[i] = dvmCompilerAllocBitVector(cUnit->numBlocks,
                                                             false);
    }
    dvmCompilerDataFlowAnalysisDispatcher(cUnit, dvmCompilerFindLocalLiveIn,
                                          kAllNodes,
                                          false /* isIterative */);
    dvmCompilerDataFlowAnalysisDispatcher(cUnit, fillDefBlockMatrix,
                                          kAllNodes,
                                          false /* isIterative */);

    if (cUnit->jitMode == kJitMethod) {
        /*
         * Also set the incoming parameters as defs in the entry block.
         * Only need to handle the parameters for the outer method.
         */
        int inReg = cUnit->method->registersSize - cUnit->method->insSize;
        for (; inReg < cUnit->method->registersSize; inReg++) {
            dvmCompilerSetBit(cUnit->defBlockMatrix[inReg],
                              cUnit->entryBlock->id);
        }
    }
}

/* Compute the post-order traversal of the CFG */
static void computeDomPostOrderTraversal(CompilationUnit *cUnit, BasicBlock *bb)
{
    BitVectorIterator bvIterator;
    dvmBitVectorIteratorInit(bb->iDominated, &bvIterator);
    GrowableList *blockList = &cUnit->blockList;

    /* Iterate through the dominated blocks first */
    while (true) {
        int bbIdx = dvmBitVectorIteratorNext(&bvIterator);
        if (bbIdx == -1) break;
        BasicBlock *dominatedBB =
            (BasicBlock *) dvmGrowableListGetElement(blockList, bbIdx);
        computeDomPostOrderTraversal(cUnit, dominatedBB);
    }

    /* Enter the current block id */
    dvmInsertGrowableList(&cUnit->domPostOrderTraversal, bb->id);

    /* hacky loop detection */
    if (bb->taken && dvmIsBitSet(bb->dominators, bb->taken->id)) {
        cUnit->hasLoop = true;
    }
}

static void checkForDominanceFrontier(BasicBlock *domBB,
                                      const BasicBlock *succBB)
{
    /*
     * TODO - evaluate whether phi will ever need to be inserted into exit
     * blocks.
     */
    if (succBB->iDom != domBB &&
        succBB->blockType == kDalvikByteCode &&
        succBB->hidden == false) {
        dvmSetBit(domBB->domFrontier, succBB->id);
    }
}

/* Worker function to compute the dominance frontier */
static bool computeDominanceFrontier(CompilationUnit *cUnit, BasicBlock *bb)
{
    GrowableList *blockList = &cUnit->blockList;

    /* Calculate DF_local */
    if (bb->taken) {
        checkForDominanceFrontier(bb, bb->taken);
    }
    if (bb->fallThrough) {
        checkForDominanceFrontier(bb, bb->fallThrough);
    }
    if (bb->successorBlockList.blockListType != kNotUsed) {
        GrowableListIterator iterator;
        dvmGrowableListIteratorInit(&bb->successorBlockList.blocks,
                                    &iterator);
        while (true) {
            SuccessorBlockInfo *successorBlockInfo =
                (SuccessorBlockInfo *) dvmGrowableListIteratorNext(&iterator);
            if (successorBlockInfo == NULL) break;
            BasicBlock *succBB = successorBlockInfo->block;
            checkForDominanceFrontier(bb, succBB);
        }
    }

    /* Calculate DF_up */
    BitVectorIterator bvIterator;
    dvmBitVectorIteratorInit(bb->iDominated, &bvIterator);
    while (true) {
        int dominatedIdx = dvmBitVectorIteratorNext(&bvIterator);
        if (dominatedIdx == -1) break;
        BasicBlock *dominatedBB = (BasicBlock *)
            dvmGrowableListGetElement(blockList, dominatedIdx);
        BitVectorIterator dfIterator;
        dvmBitVectorIteratorInit(dominatedBB->domFrontier, &dfIterator);
        while (true) {
            int dfUpIdx = dvmBitVectorIteratorNext(&dfIterator);
            if (dfUpIdx == -1) break;
            BasicBlock *dfUpBlock = (BasicBlock *)
                dvmGrowableListGetElement(blockList, dfUpIdx);
            checkForDominanceFrontier(bb, dfUpBlock);
        }
    }

    return true;
}

/* Worker function for initializing domination-related data structures */
static bool initializeDominationInfo(CompilationUnit *cUnit, BasicBlock *bb)
{
    int numTotalBlocks = cUnit->blockList.numUsed;

    if (bb->dominators == NULL ) {
        bb->dominators = dvmCompilerAllocBitVector(numTotalBlocks,
                                                   false /* expandable */);
        bb->iDominated = dvmCompilerAllocBitVector(numTotalBlocks,
                                                   false /* expandable */);
        bb->domFrontier = dvmCompilerAllocBitVector(numTotalBlocks,
                                                   false /* expandable */);
    } else {
        dvmClearAllBits(bb->dominators);
        dvmClearAllBits(bb->iDominated);
        dvmClearAllBits(bb->domFrontier);
    }
    /* Set all bits in the dominator vector */
    dvmSetInitialBits(bb->dominators, numTotalBlocks);

    return true;
}

/* Worker function to compute each block's dominators */
static bool computeBlockDominators(CompilationUnit *cUnit, BasicBlock *bb)
{
    GrowableList *blockList = &cUnit->blockList;
    int numTotalBlocks = blockList->numUsed;
    BitVector *tempBlockV = cUnit->tempBlockV;
    BitVectorIterator bvIterator;

    /*
     * The dominator of the entry block has been preset to itself and we need
     * to skip the calculation here.
     */
    if (bb == cUnit->entryBlock) return false;

    dvmSetInitialBits(tempBlockV, numTotalBlocks);

    /* Iterate through the predecessors */
    dvmBitVectorIteratorInit(bb->predecessors, &bvIterator);
    while (true) {
        int predIdx = dvmBitVectorIteratorNext(&bvIterator);
        if (predIdx == -1) break;
        BasicBlock *predBB = (BasicBlock *) dvmGrowableListGetElement(
                                 blockList, predIdx);
        /* tempBlockV = tempBlockV ^ dominators */
        dvmIntersectBitVectors(tempBlockV, tempBlockV, predBB->dominators);
    }
    dvmSetBit(tempBlockV, bb->id);
    if (dvmCompareBitVectors(tempBlockV, bb->dominators)) {
        dvmCopyBitVector(bb->dominators, tempBlockV);
        return true;
    }
    return false;
}

/* Worker function to compute the idom */
static bool computeImmediateDominator(CompilationUnit *cUnit, BasicBlock *bb)
{
    GrowableList *blockList = &cUnit->blockList;
    BitVector *tempBlockV = cUnit->tempBlockV;
    BitVectorIterator bvIterator;
    BasicBlock *iDom;

    if (bb == cUnit->entryBlock) return false;

    dvmCopyBitVector(tempBlockV, bb->dominators);
    dvmClearBit(tempBlockV, bb->id);
    dvmBitVectorIteratorInit(tempBlockV, &bvIterator);

    /* Should not see any dead block */
    assert(dvmCountSetBits(tempBlockV) != 0);
    if (dvmCountSetBits(tempBlockV) == 1) {
        iDom = (BasicBlock *) dvmGrowableListGetElement(
                       blockList, dvmBitVectorIteratorNext(&bvIterator));
        bb->iDom = iDom;
    } else {
        int iDomIdx = dvmBitVectorIteratorNext(&bvIterator);
        assert(iDomIdx != -1);
        while (true) {
            int nextDom = dvmBitVectorIteratorNext(&bvIterator);
            if (nextDom == -1) break;
            BasicBlock *nextDomBB = (BasicBlock *)
                dvmGrowableListGetElement(blockList, nextDom);
            /* iDom dominates nextDom - set new iDom */
            if (dvmIsBitSet(nextDomBB->dominators, iDomIdx)) {
                iDomIdx = nextDom;
            }

        }
        iDom = (BasicBlock *) dvmGrowableListGetElement(blockList, iDomIdx);
        /* Set the immediate dominator block for bb */
        bb->iDom = iDom;
    }
    /* Add bb to the iDominated set of the immediate dominator block */
    dvmCompilerSetBit(iDom->iDominated, bb->id);
    return true;
}

/* Compute dominators, immediate dominator, and dominance fronter */
static void computeDominators(CompilationUnit *cUnit)
{
    int numReachableBlocks = cUnit->numReachableBlocks;
    int numTotalBlocks = cUnit->blockList.numUsed;

    /* Initialize domination-related data structures */
    dvmCompilerDataFlowAnalysisDispatcher(cUnit, initializeDominationInfo,
                                          kReachableNodes,
                                          false /* isIterative */);

    /* Set the dominator for the root node */
    dvmClearAllBits(cUnit->entryBlock->dominators);
    dvmSetBit(cUnit->entryBlock->dominators, cUnit->entryBlock->id);

    if (cUnit->tempBlockV == NULL) {
        cUnit->tempBlockV = dvmCompilerAllocBitVector(numTotalBlocks,
                                                  false /* expandable */);
    } else {
        dvmClearAllBits(cUnit->tempBlockV);
    }
    dvmCompilerDataFlowAnalysisDispatcher(cUnit, computeBlockDominators,
                                          kPreOrderDFSTraversal,
                                          true /* isIterative */);

    cUnit->entryBlock->iDom = NULL;
    dvmCompilerDataFlowAnalysisDispatcher(cUnit, computeImmediateDominator,
                                          kReachableNodes,
                                          false /* isIterative */);

    /*
     * Now go ahead and compute the post order traversal based on the
     * iDominated sets.
     */
    if (cUnit->domPostOrderTraversal.elemList == NULL) {
        dvmInitGrowableList(&cUnit->domPostOrderTraversal, numReachableBlocks);
    } else {
        cUnit->domPostOrderTraversal.numUsed = 0;
    }

    computeDomPostOrderTraversal(cUnit, cUnit->entryBlock);
    assert(cUnit->domPostOrderTraversal.numUsed ==
           (unsigned) cUnit->numReachableBlocks);

    /* Now compute the dominance frontier for each block */
    dvmCompilerDataFlowAnalysisDispatcher(cUnit, computeDominanceFrontier,
                                          kPostOrderDOMTraversal,
                                          false /* isIterative */);
}

/*
 * Perform dest U= src1 ^ ~src2
 * This is probably not general enough to be placed in BitVector.[ch].
 */
static void computeSuccLiveIn(BitVector *dest,
                              const BitVector *src1,
                              const BitVector *src2)
{
    if (dest->storageSize != src1->storageSize ||
        dest->storageSize != src2->storageSize ||
        dest->expandable != src1->expandable ||
        dest->expandable != src2->expandable) {
        ALOGE("Incompatible set properties");
        dvmAbort();
    }

    unsigned int idx;
    for (idx = 0; idx < dest->storageSize; idx++) {
        dest->storage[idx] |= src1->storage[idx] & ~src2->storage[idx];
    }
}

/*
 * Iterate through all successor blocks and propagate up the live-in sets.
 * The calculated result is used for phi-node pruning - where we only need to
 * insert a phi node if the variable is live-in to the block.
 */
static bool computeBlockLiveIns(CompilationUnit *cUnit, BasicBlock *bb)
{
    BitVector *tempDalvikRegisterV = cUnit->tempDalvikRegisterV;

    if (bb->dataFlowInfo == NULL) return false;
    dvmCopyBitVector(tempDalvikRegisterV, bb->dataFlowInfo->liveInV);
    if (bb->taken && bb->taken->dataFlowInfo)
        computeSuccLiveIn(tempDalvikRegisterV, bb->taken->dataFlowInfo->liveInV,
                          bb->dataFlowInfo->defV);
    if (bb->fallThrough && bb->fallThrough->dataFlowInfo)
        computeSuccLiveIn(tempDalvikRegisterV,
                          bb->fallThrough->dataFlowInfo->liveInV,
                          bb->dataFlowInfo->defV);
    if (bb->successorBlockList.blockListType != kNotUsed) {
        GrowableListIterator iterator;
        dvmGrowableListIteratorInit(&bb->successorBlockList.blocks,
                                    &iterator);
        while (true) {
            SuccessorBlockInfo *successorBlockInfo =
                (SuccessorBlockInfo *) dvmGrowableListIteratorNext(&iterator);
            if (successorBlockInfo == NULL) break;
            BasicBlock *succBB = successorBlockInfo->block;
            if (succBB->dataFlowInfo) {
                computeSuccLiveIn(tempDalvikRegisterV,
                                  succBB->dataFlowInfo->liveInV,
                                  bb->dataFlowInfo->defV);
            }
        }
    }
    if (dvmCompareBitVectors(tempDalvikRegisterV, bb->dataFlowInfo->liveInV)) {
        dvmCopyBitVector(bb->dataFlowInfo->liveInV, tempDalvikRegisterV);
        return true;
    }
    return false;
}

/* Insert phi nodes to for each variable to the dominance frontiers */
static void insertPhiNodes(CompilationUnit *cUnit)
{
    int dalvikReg;
    const GrowableList *blockList = &cUnit->blockList;
    BitVector *phiBlocks =
        dvmCompilerAllocBitVector(cUnit->numBlocks, false);
    BitVector *tmpBlocks =
        dvmCompilerAllocBitVector(cUnit->numBlocks, false);
    BitVector *inputBlocks =
        dvmCompilerAllocBitVector(cUnit->numBlocks, false);

    cUnit->tempDalvikRegisterV =
        dvmCompilerAllocBitVector(cUnit->numDalvikRegisters, false);

    dvmCompilerDataFlowAnalysisDispatcher(cUnit, computeBlockLiveIns,
                                          kPostOrderDFSTraversal,
                                          true /* isIterative */);

    /* Iterate through each Dalvik register */
    for (dalvikReg = 0; dalvikReg < cUnit->numDalvikRegisters; dalvikReg++) {
        bool change;
        BitVectorIterator iterator;

        dvmCopyBitVector(inputBlocks, cUnit->defBlockMatrix[dalvikReg]);
        dvmClearAllBits(phiBlocks);

        /* Calculate the phi blocks for each Dalvik register */
        do {
            change = false;
            dvmClearAllBits(tmpBlocks);
            dvmBitVectorIteratorInit(inputBlocks, &iterator);

            while (true) {
                int idx = dvmBitVectorIteratorNext(&iterator);
                if (idx == -1) break;
                BasicBlock *defBB =
                    (BasicBlock *) dvmGrowableListGetElement(blockList, idx);

                /* Merge the dominance frontier to tmpBlocks */
                dvmUnifyBitVectors(tmpBlocks, tmpBlocks, defBB->domFrontier);
            }
            if (dvmCompareBitVectors(phiBlocks, tmpBlocks)) {
                change = true;
                dvmCopyBitVector(phiBlocks, tmpBlocks);

                /*
                 * Iterate through the original blocks plus the new ones in
                 * the dominance frontier.
                 */
                dvmCopyBitVector(inputBlocks, phiBlocks);
                dvmUnifyBitVectors(inputBlocks, inputBlocks,
                                   cUnit->defBlockMatrix[dalvikReg]);
            }
        } while (change);

        /*
         * Insert a phi node for dalvikReg in the phiBlocks if the Dalvik
         * register is in the live-in set.
         */
        dvmBitVectorIteratorInit(phiBlocks, &iterator);
        while (true) {
            int idx = dvmBitVectorIteratorNext(&iterator);
            if (idx == -1) break;
            BasicBlock *phiBB =
                (BasicBlock *) dvmGrowableListGetElement(blockList, idx);
            /* Variable will be clobbered before being used - no need for phi */
            if (!dvmIsBitSet(phiBB->dataFlowInfo->liveInV, dalvikReg)) continue;
            MIR *phi = (MIR *) dvmCompilerNew(sizeof(MIR), true);
            phi->dalvikInsn.opcode = (Opcode)kMirOpPhi;
            phi->dalvikInsn.vA = dalvikReg;
            phi->offset = phiBB->startOffset;
            dvmCompilerPrependMIR(phiBB, phi);
        }
    }
}

/*
 * Worker function to insert phi-operands with latest SSA names from
 * predecessor blocks
 */
static bool insertPhiNodeOperands(CompilationUnit *cUnit, BasicBlock *bb)
{
    BitVector *ssaRegV = cUnit->tempSSARegisterV;
    BitVectorIterator bvIterator;
    GrowableList *blockList = &cUnit->blockList;
    MIR *mir;

    /* Phi nodes are at the beginning of each block */
    for (mir = bb->firstMIRInsn; mir; mir = mir->next) {
        if (mir->dalvikInsn.opcode != (Opcode)kMirOpPhi)
            return true;
        int ssaReg = mir->ssaRep->defs[0];
        int encodedDalvikValue =
            (int) dvmGrowableListGetElement(cUnit->ssaToDalvikMap, ssaReg);
        int dalvikReg = DECODE_REG(encodedDalvikValue);

        dvmClearAllBits(ssaRegV);

        /* Iterate through the predecessors */
        dvmBitVectorIteratorInit(bb->predecessors, &bvIterator);
        while (true) {
            int predIdx = dvmBitVectorIteratorNext(&bvIterator);
            if (predIdx == -1) break;
            BasicBlock *predBB = (BasicBlock *) dvmGrowableListGetElement(
                                     blockList, predIdx);
            int encodedSSAValue =
                predBB->dataFlowInfo->dalvikToSSAMap[dalvikReg];
            int ssaReg = DECODE_REG(encodedSSAValue);
            dvmSetBit(ssaRegV, ssaReg);
        }

        /* Count the number of SSA registers for a Dalvik register */
        int numUses = dvmCountSetBits(ssaRegV);
        mir->ssaRep->numUses = numUses;
        mir->ssaRep->uses =
            (int *) dvmCompilerNew(sizeof(int) * numUses, false);
        mir->ssaRep->fpUse =
            (bool *) dvmCompilerNew(sizeof(bool) * numUses, true);

        BitVectorIterator phiIterator;

        dvmBitVectorIteratorInit(ssaRegV, &phiIterator);
        int *usePtr = mir->ssaRep->uses;

        /* Set the uses array for the phi node */
        while (true) {
            int ssaRegIdx = dvmBitVectorIteratorNext(&phiIterator);
            if (ssaRegIdx == -1) break;
            *usePtr++ = ssaRegIdx;
        }
    }

    return true;
}

/* Perform SSA transformation for the whole method */
void dvmCompilerMethodSSATransformation(CompilationUnit *cUnit)
{
    /* Compute the DFS order */
    computeDFSOrder(cUnit);

    /* Compute the dominator info */
    computeDominators(cUnit);

    /* Allocate data structures in preparation for SSA conversion */
    dvmInitializeSSAConversion(cUnit);

    /* Find out the "Dalvik reg def x block" relation */
    computeDefBlockMatrix(cUnit);

    /* Insert phi nodes to dominance frontiers for all variables */
    insertPhiNodes(cUnit);

    /* Rename register names by local defs and phi nodes */
    dvmCompilerDataFlowAnalysisDispatcher(cUnit, dvmCompilerDoSSAConversion,
                                          kPreOrderDFSTraversal,
                                          false /* isIterative */);

    /*
     * Shared temp bit vector used by each block to count the number of defs
     * from all the predecessor blocks.
     */
    cUnit->tempSSARegisterV = dvmCompilerAllocBitVector(cUnit->numSSARegs,
                                                        false);

    /* Insert phi-operands with latest SSA names from predecessor blocks */
    dvmCompilerDataFlowAnalysisDispatcher(cUnit, insertPhiNodeOperands,
                                          kReachableNodes,
                                          false /* isIterative */);
}

/* Build a loop. Return true if a loop structure is successfully identified. */
bool dvmCompilerBuildLoop(CompilationUnit *cUnit)
{
    /* Compute the DFS order */
    computeDFSOrder(cUnit);

    /* Compute the dominator info */
    computeDominators(cUnit);

    /* Loop structure not recognized/supported - return false */
    if (dvmCompilerFilterLoopBlocks(cUnit) == false)
        return false;

    /* Re-compute the DFS order just for the loop */
    computeDFSOrder(cUnit);

    /* Re-compute the dominator info just for the loop */
    computeDominators(cUnit);

    /* Allocate data structures in preparation for SSA conversion */
    dvmInitializeSSAConversion(cUnit);

    /* Find out the "Dalvik reg def x block" relation */
    computeDefBlockMatrix(cUnit);

    /* Insert phi nodes to dominance frontiers for all variables */
    insertPhiNodes(cUnit);

    /* Rename register names by local defs and phi nodes */
    dvmCompilerDataFlowAnalysisDispatcher(cUnit, dvmCompilerDoSSAConversion,
                                          kPreOrderDFSTraversal,
                                          false /* isIterative */);

    /*
     * Shared temp bit vector used by each block to count the number of defs
     * from all the predecessor blocks.
     */
    cUnit->tempSSARegisterV = dvmCompilerAllocBitVector(cUnit->numSSARegs,
                                                        false);

    /* Insert phi-operands with latest SSA names from predecessor blocks */
    dvmCompilerDataFlowAnalysisDispatcher(cUnit, insertPhiNodeOperands,
                                          kReachableNodes,
                                          false /* isIterative */);

    if (gDvmJit.receivedSIGUSR2 || gDvmJit.printMe) {
        dvmDumpCFG(cUnit, "/sdcard/cfg/");
    }

    return true;
}
