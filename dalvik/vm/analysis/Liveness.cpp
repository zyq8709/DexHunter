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

/*
 * Liveness analysis for Dalvik bytecode.
 */
#include "Dalvik.h"
#include "analysis/Liveness.h"
#include "analysis/CodeVerify.h"

static bool processInstruction(VerifierData* vdata, u4 curIdx,
    BitVector* workBits);
static bool markDebugLocals(VerifierData* vdata);
static void dumpLiveState(const VerifierData* vdata, u4 curIdx,
    const BitVector* workBits);


/*
 * Create a table of instruction widths that indicate the width of the
 * *previous* instruction.  The values are copied from the width table
 * in "vdata", not derived from the instruction stream.
 *
 * Caller must free the return value.
 */
static InstructionWidth* createBackwardWidthTable(VerifierData* vdata)
{
    InstructionWidth* widths;

    widths = (InstructionWidth*)
            calloc(vdata->insnsSize, sizeof(InstructionWidth));
    if (widths == NULL)
        return NULL;

    u4 insnWidth = 0;
    for (u4 idx = 0; idx < vdata->insnsSize; ) {
        widths[idx] = insnWidth;
        insnWidth = dvmInsnGetWidth(vdata->insnFlags, idx);
        idx += insnWidth;
    }

    return widths;
}

/*
 * Compute the "liveness" of every register at all GC points.
 */
bool dvmComputeLiveness(VerifierData* vdata)
{
    const InsnFlags* insnFlags = vdata->insnFlags;
    InstructionWidth* backwardWidth;
    VfyBasicBlock* startGuess = NULL;
    BitVector* workBits = NULL;
    bool result = false;

    bool verbose = false; //= dvmWantVerboseVerification(vdata->method);
    if (verbose) {
        const Method* meth = vdata->method;
        ALOGI("Computing liveness for %s.%s:%s",
            meth->clazz->descriptor, meth->name, meth->shorty);
    }

    assert(vdata->registerLines != NULL);

    backwardWidth = createBackwardWidthTable(vdata);
    if (backwardWidth == NULL)
        goto bail;

    /*
     * Allocate space for intra-block work set.  Does not include space
     * for method result "registers", which aren't visible to the GC.
     * (They would be made live by move-result and then die on the
     * instruction immediately before it.)
     */
    workBits = dvmAllocBitVector(vdata->insnRegCount, false);
    if (workBits == NULL)
        goto bail;

    /*
     * We continue until all blocks have been visited, and no block
     * requires further attention ("visited" is set and "changed" is
     * clear).
     *
     * TODO: consider creating a "dense" array of basic blocks to make
     * the walking faster.
     */
    for (int iter = 0;;) {
        VfyBasicBlock* workBlock = NULL;

        if (iter++ > 100000) {
            LOG_VFY_METH(vdata->method, "oh dear");
            dvmAbort();
        }

        /*
         * If a block is marked "changed", we stop and handle it.  If it
         * just hasn't been visited yet, we remember it but keep searching
         * for one that has been changed.
         *
         * The thought here is that this is more likely to let us work
         * from end to start, which reduces the amount of re-evaluation
         * required (both by using "changed" as a work list, and by picking
         * un-visited blocks from the tail end of the method).
         */
        if (startGuess != NULL) {
            assert(startGuess->changed);
            workBlock = startGuess;
        } else {
            for (u4 idx = 0; idx < vdata->insnsSize; idx++) {
                VfyBasicBlock* block = vdata->basicBlocks[idx];
                if (block == NULL)
                    continue;

                if (block->changed) {
                    workBlock = block;
                    break;
                } else if (!block->visited) {
                    workBlock = block;
                }
            }
        }

        if (workBlock == NULL) {
            /* all done */
            break;
        }

        assert(workBlock->changed || !workBlock->visited);
        startGuess = NULL;

        /*
         * Load work bits.  These represent the liveness of registers
         * after the last instruction in the block has finished executing.
         */
        assert(workBlock->liveRegs != NULL);
        dvmCopyBitVector(workBits, workBlock->liveRegs);
        if (verbose) {
            ALOGI("Loaded work bits from last=0x%04x", workBlock->lastAddr);
            dumpLiveState(vdata, 0xfffd, workBlock->liveRegs);
            dumpLiveState(vdata, 0xffff, workBits);
        }

        /*
         * Process a single basic block.
         *
         * If this instruction is a GC point, we want to save the result
         * in the RegisterLine.
         *
         * We don't break basic blocks on every GC point -- in particular,
         * instructions that might throw but have no "try" block don't
         * end a basic block -- so there could be more than one GC point
         * in a given basic block.
         *
         * We could change this, but it turns out to be not all that useful.
         * At first glance it appears that we could share the liveness bit
         * vector between the basic block struct and the register line,
         * but the basic block needs to reflect the state *after* the
         * instruction has finished, while the GC points need to describe
         * the state before the instruction starts.
         */
        u4 curIdx = workBlock->lastAddr;
        while (true) {
            if (!processInstruction(vdata, curIdx, workBits))
                goto bail;

            if (verbose) {
                dumpLiveState(vdata, curIdx + 0x8000, workBits);
            }

            if (dvmInsnIsGcPoint(insnFlags, curIdx)) {
                BitVector* lineBits = vdata->registerLines[curIdx].liveRegs;
                if (lineBits == NULL) {
                    lineBits = vdata->registerLines[curIdx].liveRegs =
                        dvmAllocBitVector(vdata->insnRegCount, false);
                }
                dvmCopyBitVector(lineBits, workBits);
            }

            if (curIdx == workBlock->firstAddr)
                break;
            assert(curIdx >= backwardWidth[curIdx]);
            curIdx -= backwardWidth[curIdx];
        }

        workBlock->visited = true;
        workBlock->changed = false;

        if (verbose) {
            dumpLiveState(vdata, curIdx, workBits);
        }

        /*
         * Merge changes to all predecessors.  If the new bits don't match
         * the old bits, set the "changed" flag.
         */
        PointerSet* preds = workBlock->predecessors;
        size_t numPreds = dvmPointerSetGetCount(preds);
        unsigned int predIdx;

        for (predIdx = 0; predIdx < numPreds; predIdx++) {
            VfyBasicBlock* pred =
                    (VfyBasicBlock*) dvmPointerSetGetEntry(preds, predIdx);

            pred->changed = dvmCheckMergeBitVectors(pred->liveRegs, workBits);
            if (verbose) {
                ALOGI("merging cur=%04x into pred last=%04x (ch=%d)",
                    curIdx, pred->lastAddr, pred->changed);
                dumpLiveState(vdata, 0xfffa, pred->liveRegs);
                dumpLiveState(vdata, 0xfffb, workBits);
            }

            /*
             * We want to set the "changed" flag on unvisited predecessors
             * as a way of guiding the verifier through basic blocks in
             * a reasonable order.  We can't count on variable liveness
             * changing, so we force "changed" to true even if it hasn't.
             */
            if (!pred->visited)
                pred->changed = true;

            /*
             * Keep track of one of the changed blocks so we can start
             * there instead of having to scan through the list.
             */
            if (pred->changed)
                startGuess = pred;
        }
    }

#ifndef NDEBUG
    /*
     * Sanity check: verify that all GC point register lines have a
     * liveness bit vector allocated.  Also, we're not expecting non-GC
     * points to have them.
     */
    u4 checkIdx;
    for (checkIdx = 0; checkIdx < vdata->insnsSize; ) {
        if (dvmInsnIsGcPoint(insnFlags, checkIdx)) {
            if (vdata->registerLines[checkIdx].liveRegs == NULL) {
                LOG_VFY_METH(vdata->method,
                    "GLITCH: no liveRegs for GC point 0x%04x", checkIdx);
                dvmAbort();
            }
        } else if (vdata->registerLines[checkIdx].liveRegs != NULL) {
            LOG_VFY_METH(vdata->method,
                "GLITCH: liveRegs for non-GC point 0x%04x", checkIdx);
            dvmAbort();
        }
        u4 insnWidth = dvmInsnGetWidth(insnFlags, checkIdx);
        checkIdx += insnWidth;
    }
#endif

    /*
     * Factor in the debug info, if any.
     */
    if (!markDebugLocals(vdata))
        goto bail;

    result = true;

bail:
    free(backwardWidth);
    dvmFreeBitVector(workBits);
    return result;
}


/*
 * Add a register to the LIVE set.
 */
static inline void GEN(BitVector* workBits, u4 regIndex)
{
    dvmSetBit(workBits, regIndex);
}

/*
 * Add a register pair to the LIVE set.
 */
static inline void GENW(BitVector* workBits, u4 regIndex)
{
    dvmSetBit(workBits, regIndex);
    dvmSetBit(workBits, regIndex+1);
}

/*
 * Remove a register from the LIVE set.
 */
static inline void KILL(BitVector* workBits, u4 regIndex)
{
    dvmClearBit(workBits, regIndex);
}

/*
 * Remove a register pair from the LIVE set.
 */
static inline void KILLW(BitVector* workBits, u4 regIndex)
{
    dvmClearBit(workBits, regIndex);
    dvmClearBit(workBits, regIndex+1);
}

/*
 * Process a single instruction.
 *
 * Returns "false" if something goes fatally wrong.
 */
static bool processInstruction(VerifierData* vdata, u4 insnIdx,
    BitVector* workBits)
{
    const Method* meth = vdata->method;
    const u2* insns = meth->insns + insnIdx;
    DecodedInstruction decInsn;

    dexDecodeInstruction(insns, &decInsn);

    /*
     * Add registers to the "GEN" or "KILL" sets.  We want to do KILL
     * before GEN to handle cases where the source and destination
     * register is the same.
     */
    switch (decInsn.opcode) {
    case OP_NOP:
    case OP_RETURN_VOID:
    case OP_GOTO:
    case OP_GOTO_16:
    case OP_GOTO_32:
        /* no registers are used */
        break;

    case OP_RETURN:
    case OP_RETURN_OBJECT:
    case OP_MONITOR_ENTER:
    case OP_MONITOR_EXIT:
    case OP_CHECK_CAST:
    case OP_THROW:
    case OP_PACKED_SWITCH:
    case OP_SPARSE_SWITCH:
    case OP_FILL_ARRAY_DATA:
    case OP_IF_EQZ:
    case OP_IF_NEZ:
    case OP_IF_LTZ:
    case OP_IF_GEZ:
    case OP_IF_GTZ:
    case OP_IF_LEZ:
    case OP_SPUT:
    case OP_SPUT_BOOLEAN:
    case OP_SPUT_BYTE:
    case OP_SPUT_CHAR:
    case OP_SPUT_SHORT:
    case OP_SPUT_OBJECT:
        /* action <- vA */
        GEN(workBits, decInsn.vA);
        break;

    case OP_RETURN_WIDE:
    case OP_SPUT_WIDE:
        /* action <- vA(wide) */
        GENW(workBits, decInsn.vA);
        break;

    case OP_IF_EQ:
    case OP_IF_NE:
    case OP_IF_LT:
    case OP_IF_GE:
    case OP_IF_GT:
    case OP_IF_LE:
    case OP_IPUT:
    case OP_IPUT_BOOLEAN:
    case OP_IPUT_BYTE:
    case OP_IPUT_CHAR:
    case OP_IPUT_SHORT:
    case OP_IPUT_OBJECT:
        /* action <- vA, vB */
        GEN(workBits, decInsn.vA);
        GEN(workBits, decInsn.vB);
        break;

    case OP_IPUT_WIDE:
        /* action <- vA(wide), vB */
        GENW(workBits, decInsn.vA);
        GEN(workBits, decInsn.vB);
        break;

    case OP_APUT:
    case OP_APUT_BOOLEAN:
    case OP_APUT_BYTE:
    case OP_APUT_CHAR:
    case OP_APUT_SHORT:
    case OP_APUT_OBJECT:
        /* action <- vA, vB, vC */
        GEN(workBits, decInsn.vA);
        GEN(workBits, decInsn.vB);
        GEN(workBits, decInsn.vC);
        break;

    case OP_APUT_WIDE:
        /* action <- vA(wide), vB, vC */
        GENW(workBits, decInsn.vA);
        GEN(workBits, decInsn.vB);
        GEN(workBits, decInsn.vC);
        break;

    case OP_FILLED_NEW_ARRAY:
    case OP_INVOKE_VIRTUAL:
    case OP_INVOKE_SUPER:
    case OP_INVOKE_DIRECT:
    case OP_INVOKE_STATIC:
    case OP_INVOKE_INTERFACE:
        /* action <- vararg */
        {
            unsigned int idx;
            for (idx = 0; idx < decInsn.vA; idx++) {
                GEN(workBits, decInsn.arg[idx]);
            }
        }
        break;

    case OP_FILLED_NEW_ARRAY_RANGE:
    case OP_INVOKE_VIRTUAL_RANGE:
    case OP_INVOKE_SUPER_RANGE:
    case OP_INVOKE_DIRECT_RANGE:
    case OP_INVOKE_STATIC_RANGE:
    case OP_INVOKE_INTERFACE_RANGE:
        /* action <- vararg/range */
        {
            unsigned int idx;
            for (idx = 0; idx < decInsn.vA; idx++) {
                GEN(workBits, decInsn.vC + idx);
            }
        }
        break;

    case OP_MOVE_RESULT:
    case OP_MOVE_RESULT_WIDE:
    case OP_MOVE_RESULT_OBJECT:
    case OP_MOVE_EXCEPTION:
    case OP_CONST_4:
    case OP_CONST_16:
    case OP_CONST:
    case OP_CONST_HIGH16:
    case OP_CONST_STRING:
    case OP_CONST_STRING_JUMBO:
    case OP_CONST_CLASS:
    case OP_NEW_INSTANCE:
    case OP_SGET:
    case OP_SGET_BOOLEAN:
    case OP_SGET_BYTE:
    case OP_SGET_CHAR:
    case OP_SGET_SHORT:
    case OP_SGET_OBJECT:
        /* vA <- value */
        KILL(workBits, decInsn.vA);
        break;

    case OP_CONST_WIDE_16:
    case OP_CONST_WIDE_32:
    case OP_CONST_WIDE:
    case OP_CONST_WIDE_HIGH16:
    case OP_SGET_WIDE:
        /* vA(wide) <- value */
        KILLW(workBits, decInsn.vA);
        break;

    case OP_MOVE:
    case OP_MOVE_FROM16:
    case OP_MOVE_16:
    case OP_MOVE_OBJECT:
    case OP_MOVE_OBJECT_FROM16:
    case OP_MOVE_OBJECT_16:
    case OP_INSTANCE_OF:
    case OP_ARRAY_LENGTH:
    case OP_NEW_ARRAY:
    case OP_IGET:
    case OP_IGET_BOOLEAN:
    case OP_IGET_BYTE:
    case OP_IGET_CHAR:
    case OP_IGET_SHORT:
    case OP_IGET_OBJECT:
    case OP_NEG_INT:
    case OP_NOT_INT:
    case OP_NEG_FLOAT:
    case OP_INT_TO_FLOAT:
    case OP_FLOAT_TO_INT:
    case OP_INT_TO_BYTE:
    case OP_INT_TO_CHAR:
    case OP_INT_TO_SHORT:
    case OP_ADD_INT_LIT16:
    case OP_RSUB_INT:
    case OP_MUL_INT_LIT16:
    case OP_DIV_INT_LIT16:
    case OP_REM_INT_LIT16:
    case OP_AND_INT_LIT16:
    case OP_OR_INT_LIT16:
    case OP_XOR_INT_LIT16:
    case OP_ADD_INT_LIT8:
    case OP_RSUB_INT_LIT8:
    case OP_MUL_INT_LIT8:
    case OP_DIV_INT_LIT8:
    case OP_REM_INT_LIT8:
    case OP_SHL_INT_LIT8:
    case OP_SHR_INT_LIT8:
    case OP_USHR_INT_LIT8:
    case OP_AND_INT_LIT8:
    case OP_OR_INT_LIT8:
    case OP_XOR_INT_LIT8:
        /* vA <- vB */
        KILL(workBits, decInsn.vA);
        GEN(workBits, decInsn.vB);
        break;

    case OP_IGET_WIDE:
    case OP_INT_TO_LONG:
    case OP_INT_TO_DOUBLE:
    case OP_FLOAT_TO_LONG:
    case OP_FLOAT_TO_DOUBLE:
        /* vA(wide) <- vB */
        KILLW(workBits, decInsn.vA);
        GEN(workBits, decInsn.vB);
        break;

    case OP_LONG_TO_INT:
    case OP_LONG_TO_FLOAT:
    case OP_DOUBLE_TO_INT:
    case OP_DOUBLE_TO_FLOAT:
        /* vA <- vB(wide) */
        KILL(workBits, decInsn.vA);
        GENW(workBits, decInsn.vB);
        break;

    case OP_MOVE_WIDE:
    case OP_MOVE_WIDE_FROM16:
    case OP_MOVE_WIDE_16:
    case OP_NEG_LONG:
    case OP_NOT_LONG:
    case OP_NEG_DOUBLE:
    case OP_LONG_TO_DOUBLE:
    case OP_DOUBLE_TO_LONG:
        /* vA(wide) <- vB(wide) */
        KILLW(workBits, decInsn.vA);
        GENW(workBits, decInsn.vB);
        break;

    case OP_CMPL_FLOAT:
    case OP_CMPG_FLOAT:
    case OP_AGET:
    case OP_AGET_BOOLEAN:
    case OP_AGET_BYTE:
    case OP_AGET_CHAR:
    case OP_AGET_SHORT:
    case OP_AGET_OBJECT:
    case OP_ADD_INT:
    case OP_SUB_INT:
    case OP_MUL_INT:
    case OP_REM_INT:
    case OP_DIV_INT:
    case OP_AND_INT:
    case OP_OR_INT:
    case OP_XOR_INT:
    case OP_SHL_INT:
    case OP_SHR_INT:
    case OP_USHR_INT:
    case OP_ADD_FLOAT:
    case OP_SUB_FLOAT:
    case OP_MUL_FLOAT:
    case OP_DIV_FLOAT:
    case OP_REM_FLOAT:
        /* vA <- vB, vC */
        KILL(workBits, decInsn.vA);
        GEN(workBits, decInsn.vB);
        GEN(workBits, decInsn.vC);
        break;

    case OP_AGET_WIDE:
        /* vA(wide) <- vB, vC */
        KILLW(workBits, decInsn.vA);
        GEN(workBits, decInsn.vB);
        GEN(workBits, decInsn.vC);
        break;

    case OP_CMPL_DOUBLE:
    case OP_CMPG_DOUBLE:
    case OP_CMP_LONG:
        /* vA <- vB(wide), vC(wide) */
        KILL(workBits, decInsn.vA);
        GENW(workBits, decInsn.vB);
        GENW(workBits, decInsn.vC);
        break;

    case OP_SHL_LONG:
    case OP_SHR_LONG:
    case OP_USHR_LONG:
        /* vA(wide) <- vB(wide), vC */
        KILLW(workBits, decInsn.vA);
        GENW(workBits, decInsn.vB);
        GEN(workBits, decInsn.vC);
        break;

    case OP_ADD_LONG:
    case OP_SUB_LONG:
    case OP_MUL_LONG:
    case OP_DIV_LONG:
    case OP_REM_LONG:
    case OP_AND_LONG:
    case OP_OR_LONG:
    case OP_XOR_LONG:
    case OP_ADD_DOUBLE:
    case OP_SUB_DOUBLE:
    case OP_MUL_DOUBLE:
    case OP_DIV_DOUBLE:
    case OP_REM_DOUBLE:
        /* vA(wide) <- vB(wide), vC(wide) */
        KILLW(workBits, decInsn.vA);
        GENW(workBits, decInsn.vB);
        GENW(workBits, decInsn.vC);
        break;

    case OP_ADD_INT_2ADDR:
    case OP_SUB_INT_2ADDR:
    case OP_MUL_INT_2ADDR:
    case OP_REM_INT_2ADDR:
    case OP_SHL_INT_2ADDR:
    case OP_SHR_INT_2ADDR:
    case OP_USHR_INT_2ADDR:
    case OP_AND_INT_2ADDR:
    case OP_OR_INT_2ADDR:
    case OP_XOR_INT_2ADDR:
    case OP_DIV_INT_2ADDR:
        /* vA <- vA, vB */
        /* KILL(workBits, decInsn.vA); */
        GEN(workBits, decInsn.vA);
        GEN(workBits, decInsn.vB);
        break;

    case OP_SHL_LONG_2ADDR:
    case OP_SHR_LONG_2ADDR:
    case OP_USHR_LONG_2ADDR:
        /* vA(wide) <- vA(wide), vB */
        /* KILLW(workBits, decInsn.vA); */
        GENW(workBits, decInsn.vA);
        GEN(workBits, decInsn.vB);
        break;

    case OP_ADD_LONG_2ADDR:
    case OP_SUB_LONG_2ADDR:
    case OP_MUL_LONG_2ADDR:
    case OP_DIV_LONG_2ADDR:
    case OP_REM_LONG_2ADDR:
    case OP_AND_LONG_2ADDR:
    case OP_OR_LONG_2ADDR:
    case OP_XOR_LONG_2ADDR:
    case OP_ADD_FLOAT_2ADDR:
    case OP_SUB_FLOAT_2ADDR:
    case OP_MUL_FLOAT_2ADDR:
    case OP_DIV_FLOAT_2ADDR:
    case OP_REM_FLOAT_2ADDR:
    case OP_ADD_DOUBLE_2ADDR:
    case OP_SUB_DOUBLE_2ADDR:
    case OP_MUL_DOUBLE_2ADDR:
    case OP_DIV_DOUBLE_2ADDR:
    case OP_REM_DOUBLE_2ADDR:
        /* vA(wide) <- vA(wide), vB(wide) */
        /* KILLW(workBits, decInsn.vA); */
        GENW(workBits, decInsn.vA);
        GENW(workBits, decInsn.vB);
        break;

    /* we will only see this if liveness analysis is done after general vfy */
    case OP_THROW_VERIFICATION_ERROR:
        /* no registers used */
        break;

    /* quickened instructions, not expected to appear */
    case OP_EXECUTE_INLINE:
    case OP_EXECUTE_INLINE_RANGE:
    case OP_IGET_QUICK:
    case OP_IGET_WIDE_QUICK:
    case OP_IGET_OBJECT_QUICK:
    case OP_IPUT_QUICK:
    case OP_IPUT_WIDE_QUICK:
    case OP_IPUT_OBJECT_QUICK:
    case OP_INVOKE_VIRTUAL_QUICK:
    case OP_INVOKE_VIRTUAL_QUICK_RANGE:
    case OP_INVOKE_SUPER_QUICK:
    case OP_INVOKE_SUPER_QUICK_RANGE:
        /* fall through to failure */

    /* correctness fixes, not expected to appear */
    case OP_INVOKE_OBJECT_INIT_RANGE:
    case OP_RETURN_VOID_BARRIER:
    case OP_SPUT_VOLATILE:
    case OP_SPUT_OBJECT_VOLATILE:
    case OP_SPUT_WIDE_VOLATILE:
    case OP_IPUT_VOLATILE:
    case OP_IPUT_OBJECT_VOLATILE:
    case OP_IPUT_WIDE_VOLATILE:
    case OP_SGET_VOLATILE:
    case OP_SGET_OBJECT_VOLATILE:
    case OP_SGET_WIDE_VOLATILE:
    case OP_IGET_VOLATILE:
    case OP_IGET_OBJECT_VOLATILE:
    case OP_IGET_WIDE_VOLATILE:
        /* fall through to failure */

    /* these should never appear during verification */
    case OP_UNUSED_3E:
    case OP_UNUSED_3F:
    case OP_UNUSED_40:
    case OP_UNUSED_41:
    case OP_UNUSED_42:
    case OP_UNUSED_43:
    case OP_UNUSED_73:
    case OP_UNUSED_79:
    case OP_UNUSED_7A:
    case OP_BREAKPOINT:
    case OP_UNUSED_FF:
        return false;
    }

    return true;
}

/*
 * This is a dexDecodeDebugInfo callback, used by markDebugLocals().
 */
static void markLocalsCb(void* ctxt, u2 reg, u4 startAddress, u4 endAddress,
    const char* name, const char* descriptor, const char* signature)
{
    VerifierData* vdata = (VerifierData*) ctxt;
    bool verbose = dvmWantVerboseVerification(vdata->method);

    if (verbose) {
        ALOGI("%04x-%04x %2d (%s %s)",
            startAddress, endAddress, reg, name, descriptor);
    }

    bool wide = (descriptor[0] == 'D' || descriptor[0] == 'J');
    assert(reg <= vdata->insnRegCount + (wide ? 1 : 0));

    /*
     * Set the bit in all GC point instructions in the range
     * [startAddress, endAddress).
     */
    unsigned int idx;
    for (idx = startAddress; idx < endAddress; idx++) {
        BitVector* liveRegs = vdata->registerLines[idx].liveRegs;
        if (liveRegs != NULL) {
            if (wide) {
                GENW(liveRegs, reg);
            } else {
                GEN(liveRegs, reg);
            }
        }
    }
}

/*
 * Mark all debugger-visible locals as live.
 *
 * The "locals" table describes the positions of the various locals in the
 * stack frame based on the current execution address.  If the debugger
 * wants to display one, it issues a request by "slot number".  We need
 * to ensure that references in stack slots that might be queried by the
 * debugger aren't GCed.
 *
 * (If the GC had some way to mark the slot as invalid we wouldn't have
 * to do this.  We could also have the debugger interface check the
 * register map and simply refuse to return a "dead" value, but that's
 * potentially confusing since the referred-to object might actually be
 * alive, and being able to see it without having to hunt around for a
 * "live" stack frame is useful.)
 */
static bool markDebugLocals(VerifierData* vdata)
{
    const Method* meth = vdata->method;

    dexDecodeDebugInfo(meth->clazz->pDvmDex->pDexFile, dvmGetMethodCode(meth),
        meth->clazz->descriptor, meth->prototype.protoIdx, meth->accessFlags,
        NULL, markLocalsCb, vdata);

    return true;
}


/*
 * Dump the liveness bits to the log.
 *
 * "curIdx" is for display only.
 */
static void dumpLiveState(const VerifierData* vdata, u4 curIdx,
    const BitVector* workBits)
{
    u4 insnRegCount = vdata->insnRegCount;
    size_t regCharSize = insnRegCount + (insnRegCount-1)/4 + 2 +1;
    char regChars[regCharSize +1];
    unsigned int idx;

    memset(regChars, ' ', regCharSize);
    regChars[0] = '[';
    if (insnRegCount == 0)
        regChars[1] = ']';
    else
        regChars[1 + (insnRegCount-1) + (insnRegCount-1)/4 +1] = ']';
    regChars[regCharSize] = '\0';

    for (idx = 0; idx < insnRegCount; idx++) {
        char ch = dvmIsBitSet(workBits, idx) ? '+' : '-';
        regChars[1 + idx + (idx/4)] = ch;
    }

    ALOGI("0x%04x %s", curIdx, regChars);
}
