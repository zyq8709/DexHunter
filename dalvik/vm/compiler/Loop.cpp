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

#include "Dalvik.h"
#include "CompilerInternals.h"
#include "Dataflow.h"
#include "Loop.h"

#define DEBUG_LOOP(X)

#if 0
/* Debugging routines */
static void dumpConstants(CompilationUnit *cUnit)
{
    int i;
    ALOGE("LOOP starting offset: %x", cUnit->entryBlock->startOffset);
    for (i = 0; i < cUnit->numSSARegs; i++) {
        if (dvmIsBitSet(cUnit->isConstantV, i)) {
            int subNReg = dvmConvertSSARegToDalvik(cUnit, i);
            ALOGE("CONST: s%d(v%d_%d) has %d", i,
                 DECODE_REG(subNReg), DECODE_SUB(subNReg),
                 cUnit->constantValues[i]);
        }
    }
}

static void dumpIVList(CompilationUnit *cUnit)
{
    unsigned int i;
    GrowableList *ivList = cUnit->loopAnalysis->ivList;

    for (i = 0; i < ivList->numUsed; i++) {
        InductionVariableInfo *ivInfo =
            (InductionVariableInfo *) ivList->elemList[i];
        int iv = dvmConvertSSARegToDalvik(cUnit, ivInfo->ssaReg);
        /* Basic IV */
        if (ivInfo->ssaReg == ivInfo->basicSSAReg) {
            ALOGE("BIV %d: s%d(v%d_%d) + %d", i,
                 ivInfo->ssaReg,
                 DECODE_REG(iv), DECODE_SUB(iv),
                 ivInfo->inc);
        /* Dependent IV */
        } else {
            int biv = dvmConvertSSARegToDalvik(cUnit, ivInfo->basicSSAReg);

            ALOGE("DIV %d: s%d(v%d_%d) = %d * s%d(v%d_%d) + %d", i,
                 ivInfo->ssaReg,
                 DECODE_REG(iv), DECODE_SUB(iv),
                 ivInfo->m,
                 ivInfo->basicSSAReg,
                 DECODE_REG(biv), DECODE_SUB(biv),
                 ivInfo->c);
        }
    }
}

static void dumpHoistedChecks(CompilationUnit *cUnit)
{
    LoopAnalysis *loopAnalysis = cUnit->loopAnalysis;
    unsigned int i;

    for (i = 0; i < loopAnalysis->arrayAccessInfo->numUsed; i++) {
        ArrayAccessInfo *arrayAccessInfo =
            GET_ELEM_N(loopAnalysis->arrayAccessInfo,
                       ArrayAccessInfo*, i);
        int arrayReg = DECODE_REG(
            dvmConvertSSARegToDalvik(cUnit, arrayAccessInfo->arrayReg));
        int idxReg = DECODE_REG(
            dvmConvertSSARegToDalvik(cUnit, arrayAccessInfo->ivReg));
        ALOGE("Array access %d", i);
        ALOGE("  arrayReg %d", arrayReg);
        ALOGE("  idxReg %d", idxReg);
        ALOGE("  endReg %d", loopAnalysis->endConditionReg);
        ALOGE("  maxC %d", arrayAccessInfo->maxC);
        ALOGE("  minC %d", arrayAccessInfo->minC);
        ALOGE("  opcode %d", loopAnalysis->loopBranchOpcode);
    }
}

#endif

static BasicBlock *findPredecessorBlock(const CompilationUnit *cUnit,
                                        const BasicBlock *bb)
{
    int numPred = dvmCountSetBits(bb->predecessors);
    BitVectorIterator bvIterator;
    dvmBitVectorIteratorInit(bb->predecessors, &bvIterator);

    if (numPred == 1) {
        int predIdx = dvmBitVectorIteratorNext(&bvIterator);
        return (BasicBlock *) dvmGrowableListGetElement(&cUnit->blockList,
                                                        predIdx);
    /* First loop block */
    } else if ((numPred == 2) &&
               dvmIsBitSet(bb->predecessors, cUnit->entryBlock->id)) {
        while (true) {
            int predIdx = dvmBitVectorIteratorNext(&bvIterator);
            if (predIdx == cUnit->entryBlock->id) continue;
            return (BasicBlock *) dvmGrowableListGetElement(&cUnit->blockList,
                                                            predIdx);
        }
    /* Doesn't support other shape of control flow yet */
    } else {
        return NULL;
    }
}

/* Used for normalized loop exit condition checks */
static Opcode negateOpcode(Opcode opcode)
{
    switch (opcode) {
        /* reg/reg cmp */
        case OP_IF_EQ:
            return OP_IF_NE;
        case OP_IF_NE:
            return OP_IF_EQ;
        case OP_IF_LT:
            return OP_IF_GE;
        case OP_IF_GE:
            return OP_IF_LT;
        case OP_IF_GT:
            return OP_IF_LE;
        case OP_IF_LE:
            return OP_IF_GT;
        /* reg/zero cmp */
        case OP_IF_EQZ:
            return OP_IF_NEZ;
        case OP_IF_NEZ:
            return OP_IF_EQZ;
        case OP_IF_LTZ:
            return OP_IF_GEZ;
        case OP_IF_GEZ:
            return OP_IF_LTZ;
        case OP_IF_GTZ:
            return OP_IF_LEZ;
        case OP_IF_LEZ:
            return OP_IF_GTZ;
        default:
            ALOGE("opcode %d cannot be negated", opcode);
            dvmAbort();
            break;
    }
    return (Opcode)-1;  // unreached
}

/*
 * A loop is considered optimizable if:
 * 1) It has one basic induction variable.
 * 2) The loop back branch compares the BIV with a constant.
 * 3) We need to normalize the loop exit condition so that the loop is exited
 *    via the taken path.
 * 4) If it is a count-up loop, the condition is GE/GT. Otherwise it is
 *    LE/LT/LEZ/LTZ for a count-down loop.
 *
 * Return false for loops that fail the above tests.
 */
static bool isSimpleCountedLoop(CompilationUnit *cUnit)
{
    unsigned int i;
    BasicBlock *loopBackBlock = cUnit->entryBlock->fallThrough;
    LoopAnalysis *loopAnalysis = cUnit->loopAnalysis;

    if (loopAnalysis->numBasicIV != 1) return false;
    for (i = 0; i < loopAnalysis->ivList->numUsed; i++) {
        InductionVariableInfo *ivInfo;

        ivInfo = GET_ELEM_N(loopAnalysis->ivList, InductionVariableInfo*, i);
        /* Count up or down loop? */
        if (ivInfo->ssaReg == ivInfo->basicSSAReg) {
            /* Infinite loop */
            if (ivInfo->inc == 0) {
                return false;
            }
            loopAnalysis->isCountUpLoop = ivInfo->inc > 0;
            break;
        }
    }

    /* Find the block that ends with a branch to exit the loop */
    while (true) {
        loopBackBlock = findPredecessorBlock(cUnit, loopBackBlock);
        /* Loop structure not recognized as counted blocks */
        if (loopBackBlock == NULL) {
            return false;
        }
        /* Unconditional goto - continue to trace up the predecessor chain */
        if (loopBackBlock->taken == NULL) {
            continue;
        }
        break;
    }

    MIR *branch = loopBackBlock->lastMIRInsn;
    Opcode opcode = branch->dalvikInsn.opcode;

    /* Last instruction is not a conditional branch - bail */
    if (dexGetFlagsFromOpcode(opcode) != (kInstrCanContinue|kInstrCanBranch)) {
        return false;
    }

    int endSSAReg;
    int endDalvikReg;

    /* reg/reg comparison */
    if (branch->ssaRep->numUses == 2) {
        if (branch->ssaRep->uses[0] == loopAnalysis->ssaBIV) {
            endSSAReg = branch->ssaRep->uses[1];
        } else if (branch->ssaRep->uses[1] == loopAnalysis->ssaBIV) {
            endSSAReg = branch->ssaRep->uses[0];
            opcode = negateOpcode(opcode);
        } else {
            return false;
        }
        endDalvikReg = dvmConvertSSARegToDalvik(cUnit, endSSAReg);
        /*
         * If the comparison is not between the BIV and a loop invariant,
         * return false. endDalvikReg is loop invariant if one of the
         * following is true:
         * - It is not defined in the loop (ie DECODE_SUB returns 0)
         * - It is reloaded with a constant
         */
        if ((DECODE_SUB(endDalvikReg) != 0) &&
            !dvmIsBitSet(cUnit->isConstantV, endSSAReg)) {
            return false;
        }
    /* Compare against zero */
    } else if (branch->ssaRep->numUses == 1) {
        if (branch->ssaRep->uses[0] == loopAnalysis->ssaBIV) {
            /* Keep the compiler happy */
            endDalvikReg = -1;
        } else {
            return false;
        }
    } else {
        return false;
    }

    /* Normalize the loop exit check as "if (iv op end) exit;" */
    if (loopBackBlock->taken->blockType == kDalvikByteCode) {
        opcode = negateOpcode(opcode);
    }

    if (loopAnalysis->isCountUpLoop) {
        /*
         * If the normalized condition op is not > or >=, this is not an
         * optimization candidate.
         */
        switch (opcode) {
            case OP_IF_GT:
            case OP_IF_GE:
                break;
            default:
                return false;
        }
        loopAnalysis->endConditionReg = DECODE_REG(endDalvikReg);
    } else  {
        /*
         * If the normalized condition op is not < or <=, this is not an
         * optimization candidate.
         */
        switch (opcode) {
            case OP_IF_LT:
            case OP_IF_LE:
                loopAnalysis->endConditionReg = DECODE_REG(endDalvikReg);
                break;
            case OP_IF_LTZ:
            case OP_IF_LEZ:
                break;
            default:
                return false;
        }
    }
    /*
     * Remember the normalized opcode, which will be used to determine the end
     * value used for the yanked range checks.
     */
    loopAnalysis->loopBranchOpcode = opcode;
    return true;
}

/*
 * Record the upper and lower bound information for range checks for each
 * induction variable. If array A is accessed by index "i+5", the upper and
 * lower bound will be len(A)-5 and -5, respectively.
 */
static void updateRangeCheckInfo(CompilationUnit *cUnit, int arrayReg,
                                 int idxReg)
{
    InductionVariableInfo *ivInfo;
    LoopAnalysis *loopAnalysis = cUnit->loopAnalysis;
    unsigned int i, j;

    for (i = 0; i < loopAnalysis->ivList->numUsed; i++) {
        ivInfo = GET_ELEM_N(loopAnalysis->ivList, InductionVariableInfo*, i);
        if (ivInfo->ssaReg == idxReg) {
            ArrayAccessInfo *arrayAccessInfo = NULL;
            for (j = 0; j < loopAnalysis->arrayAccessInfo->numUsed; j++) {
                ArrayAccessInfo *existingArrayAccessInfo =
                    GET_ELEM_N(loopAnalysis->arrayAccessInfo,
                               ArrayAccessInfo*,
                               j);
                if (existingArrayAccessInfo->arrayReg == arrayReg) {
                    if (ivInfo->c > existingArrayAccessInfo->maxC) {
                        existingArrayAccessInfo->maxC = ivInfo->c;
                    }
                    if (ivInfo->c < existingArrayAccessInfo->minC) {
                        existingArrayAccessInfo->minC = ivInfo->c;
                    }
                    arrayAccessInfo = existingArrayAccessInfo;
                    break;
                }
            }
            if (arrayAccessInfo == NULL) {
                arrayAccessInfo =
                    (ArrayAccessInfo *)dvmCompilerNew(sizeof(ArrayAccessInfo),
                                                      false);
                arrayAccessInfo->ivReg = ivInfo->basicSSAReg;
                arrayAccessInfo->arrayReg = arrayReg;
                arrayAccessInfo->maxC = (ivInfo->c > 0) ? ivInfo->c : 0;
                arrayAccessInfo->minC = (ivInfo->c < 0) ? ivInfo->c : 0;
                dvmInsertGrowableList(loopAnalysis->arrayAccessInfo,
                                      (intptr_t) arrayAccessInfo);
            }
            break;
        }
    }
}

/* Returns true if the loop body cannot throw any exceptions */
static bool doLoopBodyCodeMotion(CompilationUnit *cUnit)
{
    BasicBlock *loopBody = cUnit->entryBlock->fallThrough;
    MIR *mir;
    bool loopBodyCanThrow = false;

    for (mir = loopBody->firstMIRInsn; mir; mir = mir->next) {
        DecodedInstruction *dInsn = &mir->dalvikInsn;
        int dfAttributes =
            dvmCompilerDataFlowAttributes[mir->dalvikInsn.opcode];

        /* Skip extended MIR instructions */
        if ((u2) dInsn->opcode >= kNumPackedOpcodes) continue;

        int instrFlags = dexGetFlagsFromOpcode(dInsn->opcode);

        /* Instruction is clean */
        if ((instrFlags & kInstrCanThrow) == 0) continue;

        /*
         * Currently we can only optimize away null and range checks. Punt on
         * instructions that can throw due to other exceptions.
         */
        if (!(dfAttributes & DF_HAS_NR_CHECKS)) {
            loopBodyCanThrow = true;
            continue;
        }

        /*
         * This comparison is redundant now, but we will have more than one
         * group of flags to check soon.
         */
        if (dfAttributes & DF_HAS_NR_CHECKS) {
            /*
             * Check if the null check is applied on a loop invariant register?
             * If the register's SSA id is less than the number of Dalvik
             * registers, then it is loop invariant.
             */
            int refIdx;
            switch (dfAttributes & DF_HAS_NR_CHECKS) {
                case DF_NULL_N_RANGE_CHECK_0:
                    refIdx = 0;
                    break;
                case DF_NULL_N_RANGE_CHECK_1:
                    refIdx = 1;
                    break;
                case DF_NULL_N_RANGE_CHECK_2:
                    refIdx = 2;
                    break;
                default:
                    refIdx = 0;
                    ALOGE("Jit: bad case in doLoopBodyCodeMotion");
                    dvmCompilerAbort(cUnit);
            }

            int useIdx = refIdx + 1;
            int subNRegArray =
                dvmConvertSSARegToDalvik(cUnit, mir->ssaRep->uses[refIdx]);
            int arraySub = DECODE_SUB(subNRegArray);

            /*
             * If the register is never updated in the loop (ie subscript == 0),
             * it is an optimization candidate.
             */
            if (arraySub != 0) {
                loopBodyCanThrow = true;
                continue;
            }

            /*
             * Then check if the range check can be hoisted out of the loop if
             * it is basic or dependent induction variable.
             */
            if (dvmIsBitSet(cUnit->loopAnalysis->isIndVarV,
                            mir->ssaRep->uses[useIdx])) {
                mir->OptimizationFlags |=
                    MIR_IGNORE_RANGE_CHECK | MIR_IGNORE_NULL_CHECK;
                updateRangeCheckInfo(cUnit, mir->ssaRep->uses[refIdx],
                                     mir->ssaRep->uses[useIdx]);
            }
        }
    }

    return !loopBodyCanThrow;
}

static void genHoistedChecks(CompilationUnit *cUnit)
{
    unsigned int i;
    BasicBlock *entry = cUnit->entryBlock;
    LoopAnalysis *loopAnalysis = cUnit->loopAnalysis;
    int globalMaxC = 0;
    int globalMinC = 0;
    /* Should be loop invariant */
    int idxReg = 0;

    for (i = 0; i < loopAnalysis->arrayAccessInfo->numUsed; i++) {
        ArrayAccessInfo *arrayAccessInfo =
            GET_ELEM_N(loopAnalysis->arrayAccessInfo,
                       ArrayAccessInfo*, i);
        int arrayReg = DECODE_REG(
            dvmConvertSSARegToDalvik(cUnit, arrayAccessInfo->arrayReg));
        idxReg = DECODE_REG(
            dvmConvertSSARegToDalvik(cUnit, arrayAccessInfo->ivReg));

        MIR *rangeCheckMIR = (MIR *)dvmCompilerNew(sizeof(MIR), true);
        rangeCheckMIR->dalvikInsn.opcode = (loopAnalysis->isCountUpLoop) ?
            (Opcode)kMirOpNullNRangeUpCheck : (Opcode)kMirOpNullNRangeDownCheck;
        rangeCheckMIR->dalvikInsn.vA = arrayReg;
        rangeCheckMIR->dalvikInsn.vB = idxReg;
        rangeCheckMIR->dalvikInsn.vC = loopAnalysis->endConditionReg;
        rangeCheckMIR->dalvikInsn.arg[0] = arrayAccessInfo->maxC;
        rangeCheckMIR->dalvikInsn.arg[1] = arrayAccessInfo->minC;
        rangeCheckMIR->dalvikInsn.arg[2] = loopAnalysis->loopBranchOpcode;
        dvmCompilerAppendMIR(entry, rangeCheckMIR);
        if (arrayAccessInfo->maxC > globalMaxC) {
            globalMaxC = arrayAccessInfo->maxC;
        }
        if (arrayAccessInfo->minC < globalMinC) {
            globalMinC = arrayAccessInfo->minC;
        }
    }

    if (loopAnalysis->arrayAccessInfo->numUsed != 0) {
        if (loopAnalysis->isCountUpLoop) {
            MIR *boundCheckMIR = (MIR *)dvmCompilerNew(sizeof(MIR), true);
            boundCheckMIR->dalvikInsn.opcode = (Opcode)kMirOpLowerBound;
            boundCheckMIR->dalvikInsn.vA = idxReg;
            boundCheckMIR->dalvikInsn.vB = globalMinC;
            dvmCompilerAppendMIR(entry, boundCheckMIR);
        } else {
            if (loopAnalysis->loopBranchOpcode == OP_IF_LT ||
                loopAnalysis->loopBranchOpcode == OP_IF_LE) {
                MIR *boundCheckMIR = (MIR *)dvmCompilerNew(sizeof(MIR), true);
                boundCheckMIR->dalvikInsn.opcode = (Opcode)kMirOpLowerBound;
                boundCheckMIR->dalvikInsn.vA = loopAnalysis->endConditionReg;
                boundCheckMIR->dalvikInsn.vB = globalMinC;
                /*
                 * If the end condition is ">" in the source, the check in the
                 * Dalvik bytecode is OP_IF_LE. In this case add 1 back to the
                 * constant field to reflect the fact that the smallest index
                 * value is "endValue + constant + 1".
                 */
                if (loopAnalysis->loopBranchOpcode == OP_IF_LE) {
                    boundCheckMIR->dalvikInsn.vB++;
                }
                dvmCompilerAppendMIR(entry, boundCheckMIR);
            } else if (loopAnalysis->loopBranchOpcode == OP_IF_LTZ) {
                /* Array index will fall below 0 */
                if (globalMinC < 0) {
                    MIR *boundCheckMIR = (MIR *)dvmCompilerNew(sizeof(MIR),
                                                               true);
                    boundCheckMIR->dalvikInsn.opcode = (Opcode)kMirOpPunt;
                    dvmCompilerAppendMIR(entry, boundCheckMIR);
                }
            } else if (loopAnalysis->loopBranchOpcode == OP_IF_LEZ) {
                /* Array index will fall below 0 */
                if (globalMinC < -1) {
                    MIR *boundCheckMIR = (MIR *)dvmCompilerNew(sizeof(MIR),
                                                               true);
                    boundCheckMIR->dalvikInsn.opcode = (Opcode)kMirOpPunt;
                    dvmCompilerAppendMIR(entry, boundCheckMIR);
                }
            } else {
                ALOGE("Jit: bad case in genHoistedChecks");
                dvmCompilerAbort(cUnit);
            }
        }
    }
}

void resetBlockEdges(BasicBlock *bb)
{
    bb->taken = NULL;
    bb->fallThrough = NULL;
    bb->successorBlockList.blockListType = kNotUsed;
}

static bool clearPredecessorVector(struct CompilationUnit *cUnit,
                                   struct BasicBlock *bb)
{
    dvmClearAllBits(bb->predecessors);
    return false;
}

bool dvmCompilerFilterLoopBlocks(CompilationUnit *cUnit)
{
    BasicBlock *firstBB = cUnit->entryBlock->fallThrough;

    int numPred = dvmCountSetBits(firstBB->predecessors);
    /*
     * A loop body should have at least two incoming edges.
     */
    if (numPred < 2) return false;

    GrowableList *blockList = &cUnit->blockList;

    /* Record blocks included in the loop */
    dvmClearAllBits(cUnit->tempBlockV);

    dvmCompilerSetBit(cUnit->tempBlockV, cUnit->entryBlock->id);
    dvmCompilerSetBit(cUnit->tempBlockV, firstBB->id);

    BasicBlock *bodyBB = firstBB;

    /*
     * First try to include the fall-through block in the loop, then the taken
     * block. Stop loop formation on the first backward branch that enters the
     * first block (ie only include the inner-most loop).
     */
    while (true) {
        /* Loop formed */
        if (bodyBB->taken == firstBB) {
            /* Check if the fallThrough edge will cause a nested loop */
            if (bodyBB->fallThrough &&
                dvmIsBitSet(cUnit->tempBlockV, bodyBB->fallThrough->id)) {
                return false;
            }
            /* Single loop formed */
            break;
        } else if (bodyBB->fallThrough == firstBB) {
            /* Check if the taken edge will cause a nested loop */
            if (bodyBB->taken &&
                dvmIsBitSet(cUnit->tempBlockV, bodyBB->taken->id)) {
                return false;
            }
            /* Single loop formed */
            break;
        }

        /* Inner loops formed first - quit */
        if (bodyBB->fallThrough &&
            dvmIsBitSet(cUnit->tempBlockV, bodyBB->fallThrough->id)) {
            return false;
        }
        if (bodyBB->taken &&
            dvmIsBitSet(cUnit->tempBlockV, bodyBB->taken->id)) {
            return false;
        }

        if (bodyBB->fallThrough) {
            if (bodyBB->fallThrough->iDom == bodyBB) {
                bodyBB = bodyBB->fallThrough;
                dvmCompilerSetBit(cUnit->tempBlockV, bodyBB->id);
                /*
                 * Loop formation to be detected at the beginning of next
                 * iteration.
                 */
                continue;
            }
        }
        if (bodyBB->taken) {
            if (bodyBB->taken->iDom == bodyBB) {
                bodyBB = bodyBB->taken;
                dvmCompilerSetBit(cUnit->tempBlockV, bodyBB->id);
                /*
                 * Loop formation to be detected at the beginning of next
                 * iteration.
                 */
                continue;
            }
        }
        /*
         * Current block is not the immediate dominator of either fallthrough
         * nor taken block - bail out of loop formation.
         */
        return false;
    }


    /* Now mark blocks not included in the loop as hidden */
    GrowableListIterator iterator;
    dvmGrowableListIteratorInit(blockList, &iterator);
    while (true) {
        BasicBlock *bb = (BasicBlock *) dvmGrowableListIteratorNext(&iterator);
        if (bb == NULL) break;
        if (!dvmIsBitSet(cUnit->tempBlockV, bb->id)) {
            bb->hidden = true;
            /* Clear the insn list */
            bb->firstMIRInsn = bb->lastMIRInsn = NULL;
            resetBlockEdges(bb);
        }
    }

    dvmCompilerDataFlowAnalysisDispatcher(cUnit, clearPredecessorVector,
                                          kAllNodes, false /* isIterative */);

    dvmGrowableListIteratorInit(blockList, &iterator);
    while (true) {
        BasicBlock *bb = (BasicBlock *) dvmGrowableListIteratorNext(&iterator);
        if (bb == NULL) break;
        if (dvmIsBitSet(cUnit->tempBlockV, bb->id)) {
            if (bb->taken) {
                /*
                 * exit block means we run into control-flow that we don't want
                 * to handle.
                 */
                if (bb->taken == cUnit->exitBlock) {
                    return false;
                }
                if (bb->taken->hidden) {
                    bb->taken->blockType = kChainingCellNormal;
                    bb->taken->hidden = false;
                }
                dvmCompilerSetBit(bb->taken->predecessors, bb->id);
            }
            if (bb->fallThrough) {
                /*
                 * exit block means we run into control-flow that we don't want
                 * to handle.
                 */
                if (bb->fallThrough == cUnit->exitBlock) {
                    return false;
                }
                if (bb->fallThrough->hidden) {
                    bb->fallThrough->blockType = kChainingCellNormal;
                    bb->fallThrough->hidden = false;
                }
                dvmCompilerSetBit(bb->fallThrough->predecessors, bb->id);
            }
            /* Loop blocks shouldn't contain any successor blocks (yet) */
            assert(bb->successorBlockList.blockListType == kNotUsed);
        }
    }
    return true;
}

/*
 * Main entry point to do loop optimization.
 * Return false if sanity checks for loop formation/optimization failed.
 */
bool dvmCompilerLoopOpt(CompilationUnit *cUnit)
{
    LoopAnalysis *loopAnalysis =
        (LoopAnalysis *)dvmCompilerNew(sizeof(LoopAnalysis), true);
    cUnit->loopAnalysis = loopAnalysis;

    /* Constant propagation */
    cUnit->isConstantV = dvmCompilerAllocBitVector(cUnit->numSSARegs, false);
    cUnit->constantValues =
        (int *)dvmCompilerNew(sizeof(int) * cUnit->numSSARegs,
                              true);
    dvmCompilerDataFlowAnalysisDispatcher(cUnit,
                                          dvmCompilerDoConstantPropagation,
                                          kAllNodes,
                                          false /* isIterative */);
    DEBUG_LOOP(dumpConstants(cUnit);)

    /* Find induction variables - basic and dependent */
    loopAnalysis->ivList =
        (GrowableList *)dvmCompilerNew(sizeof(GrowableList), true);
    dvmInitGrowableList(loopAnalysis->ivList, 4);
    loopAnalysis->isIndVarV = dvmCompilerAllocBitVector(cUnit->numSSARegs, false);
    dvmCompilerDataFlowAnalysisDispatcher(cUnit,
                                          dvmCompilerFindInductionVariables,
                                          kAllNodes,
                                          false /* isIterative */);
    DEBUG_LOOP(dumpIVList(cUnit);)

    /* Only optimize array accesses for simple counted loop for now */
    if (!isSimpleCountedLoop(cUnit))
        return false;

    loopAnalysis->arrayAccessInfo =
        (GrowableList *)dvmCompilerNew(sizeof(GrowableList), true);
    dvmInitGrowableList(loopAnalysis->arrayAccessInfo, 4);
    loopAnalysis->bodyIsClean = doLoopBodyCodeMotion(cUnit);
    DEBUG_LOOP(dumpHoistedChecks(cUnit);)

    /*
     * Convert the array access information into extended MIR code in the loop
     * header.
     */
    genHoistedChecks(cUnit);
    return true;
}

/*
 * Select the target block of the backward branch.
 */
void dvmCompilerInsertBackwardChaining(CompilationUnit *cUnit)
{
    /*
     * If we are not in self-verification or profiling mode, the backward
     * branch can go to the entryBlock->fallThrough directly. Suspend polling
     * code will be generated along the backward branch to honor the suspend
     * requests.
     */
#ifndef ARCH_IA32
#if !defined(WITH_SELF_VERIFICATION)
    if (gDvmJit.profileMode != kTraceProfilingContinuous &&
        gDvmJit.profileMode != kTraceProfilingPeriodicOn) {
        return;
    }
#endif
#endif

    /*
     * In self-verification or profiling mode, the backward branch is altered
     * to go to the backward chaining cell. Without using the backward chaining
     * cell we won't be able to do check-pointing on the target PC, or count the
     * number of iterations accurately.
     */
    BasicBlock *firstBB = cUnit->entryBlock->fallThrough;
    BasicBlock *backBranchBB = findPredecessorBlock(cUnit, firstBB);
    if (backBranchBB->taken == firstBB) {
        backBranchBB->taken = cUnit->backChainBlock;
    } else {
        assert(backBranchBB->fallThrough == firstBB);
        backBranchBB->fallThrough = cUnit->backChainBlock;
    }
    cUnit->backChainBlock->startOffset = firstBB->startOffset;
}
