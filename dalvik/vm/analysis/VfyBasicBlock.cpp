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
 * Verifier basic block functions.
 */
#include "Dalvik.h"
#include "analysis/VfyBasicBlock.h"
#include "analysis/CodeVerify.h"
#include "analysis/VerifySubs.h"
#include "libdex/DexCatch.h"
#include "libdex/InstrUtils.h"


/*
 * Extract the list of catch handlers from "pTry" into "addrBuf".
 *
 * Returns the size of the catch handler list.  If the return value
 * exceeds "addrBufSize", the items at the end of the list will not be
 * represented in the output array, and this function should be called
 * again with a larger buffer.
 */
static u4 extractCatchHandlers(const DexCode* pCode, const DexTry* pTry,
    u4* addrBuf, size_t addrBufSize)
{
    DexCatchIterator iterator;
    unsigned int idx = 0;

    dexCatchIteratorInit(&iterator, pCode, pTry->handlerOff);
    while (true) {
        DexCatchHandler* handler = dexCatchIteratorNext(&iterator);
        if (handler == NULL)
            break;

        if (idx < addrBufSize) {
            addrBuf[idx] = handler->address;
        }
        idx++;
    }

    return idx;
}

/*
 * Returns "true" if the instruction represents a data chunk, such as a
 * switch statement block.
 */
static bool isDataChunk(u2 insn)
{
    return (insn == kPackedSwitchSignature ||
            insn == kSparseSwitchSignature ||
            insn == kArrayDataSignature);
}

/*
 * Alloc a basic block in the specified slot.  The storage will be
 * initialized.
 */
static VfyBasicBlock* allocVfyBasicBlock(VerifierData* vdata, u4 idx)
{
    VfyBasicBlock* newBlock = (VfyBasicBlock*) calloc(1, sizeof(VfyBasicBlock));
    if (newBlock == NULL)
        return NULL;

    /*
     * TODO: there is no good default size here -- the problem is that most
     * addresses will only have one predecessor, but a fair number will
     * have 10+, and a few will have 100+ (e.g. the synthetic "finally"
     * in a large synchronized method).  We probably want to use a small
     * base allocation (perhaps two) and then have the first overflow
     * allocation jump dramatically (to 32 or thereabouts).
     */
    newBlock->predecessors = dvmPointerSetAlloc(32);
    if (newBlock->predecessors == NULL) {
        free(newBlock);
        return NULL;
    }

    newBlock->firstAddr = (u4) -1;      // DEBUG

    newBlock->liveRegs = dvmAllocBitVector(vdata->insnRegCount, false);
    if (newBlock->liveRegs == NULL) {
        dvmPointerSetFree(newBlock->predecessors);
        free(newBlock);
        return NULL;
    }

    return newBlock;
}

/*
 * Add "curBlock" to the predecessor list in "targetIdx".
 */
static bool addToPredecessor(VerifierData* vdata, VfyBasicBlock* curBlock,
    u4 targetIdx)
{
    assert(targetIdx < vdata->insnsSize);

    /*
     * Allocate the target basic block if necessary.  This will happen
     * on e.g. forward branches.
     *
     * We can't fill in all the fields, but that will happen automatically
     * when we get to that part of the code.
     */
    VfyBasicBlock* targetBlock = vdata->basicBlocks[targetIdx];
    if (targetBlock == NULL) {
        targetBlock = allocVfyBasicBlock(vdata, targetIdx);
        if (targetBlock == NULL)
            return false;
        vdata->basicBlocks[targetIdx] = targetBlock;
    }

    PointerSet* preds = targetBlock->predecessors;
    bool added = dvmPointerSetAddEntry(preds, curBlock);
    if (!added) {
        /*
         * This happens sometimes for packed-switch instructions, where
         * the same target address appears more than once.  Also, a
         * (pointless) conditional branch to the next instruction will
         * trip over this.
         */
        ALOGV("ODD: point set for targ=0x%04x (%p) already had block "
             "fir=0x%04x (%p)",
            targetIdx, targetBlock, curBlock->firstAddr, curBlock);
    }

    return true;
}

/*
 * Add ourselves to the predecessor list in all blocks we might transfer
 * control to.
 *
 * There are four ways to proceed to a new instruction:
 *  (1) continue to the following instruction
 *  (2) [un]conditionally branch to a specific location
 *  (3) conditionally branch through a "switch" statement
 *  (4) throw an exception
 *
 * Returning from the method (via a return statement or an uncaught
 * exception) are not interesting for liveness analysis.
 */
static bool setPredecessors(VerifierData* vdata, VfyBasicBlock* curBlock,
    u4 curIdx, OpcodeFlags opFlags, u4 nextIdx, u4* handlerList,
    size_t numHandlers)
{
    const InsnFlags* insnFlags = vdata->insnFlags;
    const Method* meth = vdata->method;

    unsigned int handlerIdx;
    for (handlerIdx = 0; handlerIdx < numHandlers; handlerIdx++) {
        if (!addToPredecessor(vdata, curBlock, handlerList[handlerIdx]))
            return false;
    }

    if ((opFlags & kInstrCanContinue) != 0) {
        if (!addToPredecessor(vdata, curBlock, nextIdx))
            return false;
    }
    if ((opFlags & kInstrCanBranch) != 0) {
        bool unused, gotBranch;
        s4 branchOffset, absOffset;

        gotBranch = dvmGetBranchOffset(meth, insnFlags, curIdx,
                &branchOffset, &unused);
        assert(gotBranch);
        absOffset = curIdx + branchOffset;
        assert(absOffset >= 0 && (u4) absOffset < vdata->insnsSize);

        if (!addToPredecessor(vdata, curBlock, absOffset))
            return false;
    }

    if ((opFlags & kInstrCanSwitch) != 0) {
        const u2* curInsn = &meth->insns[curIdx];
        const u2* dataPtr;

        /* these values have already been verified, so we can trust them */
        s4 offsetToData = curInsn[1] | ((s4) curInsn[2]) << 16;
        dataPtr = curInsn + offsetToData;

        /*
         * dataPtr points to the start of the switch data.  The first
         * item is the NOP+magic, the second is the number of entries in
         * the switch table.
         */
        u2 switchCount = dataPtr[1];

        /*
         * Skip past the ident field, size field, and the first_key field
         * (for packed) or the key list (for sparse).
         */
        if (dexOpcodeFromCodeUnit(meth->insns[curIdx]) == OP_PACKED_SWITCH) {
            dataPtr += 4;
        } else {
            assert(dexOpcodeFromCodeUnit(meth->insns[curIdx]) ==
                    OP_SPARSE_SWITCH);
            dataPtr += 2 + 2 * switchCount;
        }

        u4 switchIdx;
        for (switchIdx = 0; switchIdx < switchCount; switchIdx++) {
            s4 offset, absOffset;

            offset = (s4) dataPtr[switchIdx*2] |
                     (s4) (dataPtr[switchIdx*2 +1] << 16);
            absOffset = curIdx + offset;
            assert(absOffset >= 0 && (u4) absOffset < vdata->insnsSize);

            if (!addToPredecessor(vdata, curBlock, absOffset))
                return false;
        }
    }

    if (false) {
        if (dvmPointerSetGetCount(curBlock->predecessors) > 256) {
            ALOGI("Lots of preds at 0x%04x in %s.%s:%s", curIdx,
                meth->clazz->descriptor, meth->name, meth->shorty);
        }
    }

    return true;
}

/*
 * Dump the contents of the basic blocks.
 */
static void dumpBasicBlocks(const VerifierData* vdata)
{
    char printBuf[256];
    unsigned int idx;
    int count;

    ALOGI("Basic blocks for %s.%s:%s", vdata->method->clazz->descriptor,
        vdata->method->name, vdata->method->shorty);
    for (idx = 0; idx < vdata->insnsSize; idx++) {
        VfyBasicBlock* block = vdata->basicBlocks[idx];
        if (block == NULL)
            continue;

        assert(block->firstAddr == idx);
        count = snprintf(printBuf, sizeof(printBuf), " %04x-%04x ",
            block->firstAddr, block->lastAddr);

        PointerSet* preds = block->predecessors;
        size_t numPreds = dvmPointerSetGetCount(preds);

        if (numPreds > 0) {
            count += snprintf(printBuf + count, sizeof(printBuf) - count,
                    "preds:");

            unsigned int predIdx;
            for (predIdx = 0; predIdx < numPreds; predIdx++) {
                if (count >= (int) sizeof(printBuf))
                    break;
                const VfyBasicBlock* pred =
                    (const VfyBasicBlock*) dvmPointerSetGetEntry(preds, predIdx);
                count += snprintf(printBuf + count, sizeof(printBuf) - count,
                        "%04x(%p),", pred->firstAddr, pred);
            }
        } else {
            count += snprintf(printBuf + count, sizeof(printBuf) - count,
                    "(no preds)");
        }

        printBuf[sizeof(printBuf)-2] = '!';
        printBuf[sizeof(printBuf)-1] = '\0';

        ALOGI("%s", printBuf);
    }

    usleep(100 * 1000);      /* ugh...let logcat catch up */
}


/*
 * Generate a list of basic blocks and related information.
 *
 * On success, returns "true" with vdata->basicBlocks initialized.
 */
bool dvmComputeVfyBasicBlocks(VerifierData* vdata)
{
    const InsnFlags* insnFlags = vdata->insnFlags;
    const Method* meth = vdata->method;
    const u4 insnsSize = vdata->insnsSize;
    const DexCode* pCode = dvmGetMethodCode(meth);
    const DexTry* pTries = NULL;
    const size_t kHandlerStackAllocSize = 16;   /* max seen so far is 7 */
    u4 handlerAddrs[kHandlerStackAllocSize];
    u4* handlerListAlloc = NULL;
    u4* handlerList = NULL;
    size_t numHandlers = 0;
    u4 idx, blockStartAddr;
    bool result = false;

    bool verbose = false; //dvmWantVerboseVerification(meth);
    if (verbose) {
        ALOGI("Basic blocks for %s.%s:%s",
            meth->clazz->descriptor, meth->name, meth->shorty);
    }

    /*
     * Allocate a data structure that allows us to map from an address to
     * the corresponding basic block.  Initially all pointers are NULL.
     * They are populated on demand as we proceed (either when we reach a
     * new BB, or when we need to add an item to the predecessor list in
     * a not-yet-reached BB).
     *
     * Only the first instruction in the block points to the BB structure;
     * the rest remain NULL.
     */
    vdata->basicBlocks =
        (VfyBasicBlock**) calloc(insnsSize, sizeof(VfyBasicBlock*));
    if (vdata->basicBlocks == NULL)
      return false;

    /*
     * The "tries" list is a series of non-overlapping regions with a list
     * of "catch" handlers.  Rather than do the "find a matching try block"
     * computation at each step, we just walk the "try" list in parallel.
     *
     * Not all methods have "try" blocks.  If this one does, we init tryEnd
     * to zero, so that the (exclusive bound) range check trips immediately.
     */
    u4 tryIndex = 0, tryStart = 0, tryEnd = 0;
    if (pCode->triesSize != 0) {
        pTries = dexGetTries(pCode);
    }

    u4 debugBBIndex = 0;

    /*
     * The address associated with a basic block is the start address.
     */
    blockStartAddr = 0;

    for (idx = 0; idx < insnsSize; ) {
        /*
         * Make sure we're pointing at the right "try" block.  It should
         * not be possible to "jump over" a block, so if we're no longer
         * in the correct one we can just advance to the next.
         */
        if (pTries != NULL && idx >= tryEnd) {
            if (tryIndex == pCode->triesSize) {
                /* no more try blocks in this method */
                pTries = NULL;
                numHandlers = 0;
            } else {
                /*
                 * Extract the set of handlers.  We want to avoid doing
                 * this for each block, so we copy them to local storage.
                 * If it doesn't fit in the small stack area, we'll use
                 * the heap instead.
                 *
                 * It's rare to encounter a method with more than half a
                 * dozen possible handlers.
                 */
                tryStart = pTries[tryIndex].startAddr;
                tryEnd = tryStart + pTries[tryIndex].insnCount;

                if (handlerListAlloc != NULL) {
                    free(handlerListAlloc);
                    handlerListAlloc = NULL;
                }
                numHandlers = extractCatchHandlers(pCode, &pTries[tryIndex],
                    handlerAddrs, kHandlerStackAllocSize);
                assert(numHandlers > 0);    // TODO make sure this is verified
                if (numHandlers <= kHandlerStackAllocSize) {
                    handlerList = handlerAddrs;
                } else {
                    ALOGD("overflow, numHandlers=%d", numHandlers);
                    handlerListAlloc = (u4*) malloc(sizeof(u4) * numHandlers);
                    if (handlerListAlloc == NULL)
                        return false;
                    extractCatchHandlers(pCode, &pTries[tryIndex],
                        handlerListAlloc, numHandlers);
                    handlerList = handlerListAlloc;
                }

                ALOGV("+++ start=%x end=%x numHan=%d",
                    tryStart, tryEnd, numHandlers);

                tryIndex++;
            }
        }

        /*
         * Check the current instruction, and possibly aspects of the
         * next instruction, to see if this instruction ends the current
         * basic block.
         *
         * Instructions that can throw only end the block if there is the
         * possibility of a local handler catching the exception.
         */
        Opcode opcode = dexOpcodeFromCodeUnit(meth->insns[idx]);
        OpcodeFlags opFlags = dexGetFlagsFromOpcode(opcode);
        size_t nextIdx = idx + dexGetWidthFromInstruction(&meth->insns[idx]);
        bool endBB = false;
        bool ignoreInstr = false;

        if ((opFlags & kInstrCanContinue) == 0) {
            /* does not continue */
            endBB = true;
        } else if ((opFlags & (kInstrCanBranch | kInstrCanSwitch)) != 0) {
            /* conditionally branches elsewhere */
            endBB = true;
        } else if ((opFlags & kInstrCanThrow) != 0 &&
                dvmInsnIsInTry(insnFlags, idx))
        {
            /* throws an exception that might be caught locally */
            endBB = true;
        } else if (isDataChunk(meth->insns[idx])) {
            /*
             * If this is a data chunk (e.g. switch data) we want to skip
             * over it entirely.  Set endBB so we don't carry this along as
             * the start of a block, and ignoreInstr so we don't try to
             * open a basic block for this instruction.
             */
            endBB = ignoreInstr = true;
        } else if (dvmInsnIsBranchTarget(insnFlags, nextIdx)) {
            /*
             * We also need to end it if the next instruction is a branch
             * target.  Note we've tagged exception catch blocks as such.
             *
             * If we're this far along in the "else" chain, we know that
             * this isn't a data-chunk NOP, and control can continue to
             * the next instruction, so we're okay examining "nextIdx".
             */
            assert(nextIdx < insnsSize);
            endBB = true;
        } else if (opcode == OP_NOP && isDataChunk(meth->insns[nextIdx])) {
            /*
             * Handle an odd special case: if this is NOP padding before a
             * data chunk, also treat it as "ignore".  Otherwise it'll look
             * like a block that starts and doesn't end.
             */
            endBB = ignoreInstr = true;
        } else {
            /* check: return ops should be caught by absence of can-continue */
            assert((opFlags & kInstrCanReturn) == 0);
        }

        if (verbose) {
            char btc = dvmInsnIsBranchTarget(insnFlags, idx) ? '>' : ' ';
            char tryc =
                (pTries != NULL && idx >= tryStart && idx < tryEnd) ? 't' : ' ';
            bool startBB = (idx == blockStartAddr);
            const char* startEnd;


            if (ignoreInstr)
                startEnd = "IGNORE";
            else if (startBB && endBB)
                startEnd = "START/END";
            else if (startBB)
                startEnd = "START";
            else if (endBB)
                startEnd = "END";
            else
                startEnd = "-";

            ALOGI("%04x: %c%c%s #%d", idx, tryc, btc, startEnd, debugBBIndex);

            if (pTries != NULL && idx == tryStart) {
                assert(numHandlers > 0);
                ALOGI("  EXC block: [%04x, %04x) %d:(%04x...)",
                    tryStart, tryEnd, numHandlers, handlerList[0]);
            }
        }

        if (idx != blockStartAddr) {
            /* should not be a basic block struct associated with this addr */
            assert(vdata->basicBlocks[idx] == NULL);
        }
        if (endBB) {
            if (!ignoreInstr) {
                /*
                 * Create a new BB if one doesn't already exist.
                 */
                VfyBasicBlock* curBlock = vdata->basicBlocks[blockStartAddr];
                if (curBlock == NULL) {
                    curBlock = allocVfyBasicBlock(vdata, blockStartAddr);
                    if (curBlock == NULL)
                        return false;
                    vdata->basicBlocks[blockStartAddr] = curBlock;
                }

                curBlock->firstAddr = blockStartAddr;
                curBlock->lastAddr = idx;

                if (!setPredecessors(vdata, curBlock, idx, opFlags, nextIdx,
                        handlerList, numHandlers))
                {
                    goto bail;
                }
            }

            blockStartAddr = nextIdx;
            debugBBIndex++;
        }

        idx = nextIdx;
    }

    assert(idx == insnsSize);

    result = true;

    if (verbose)
        dumpBasicBlocks(vdata);

bail:
    free(handlerListAlloc);
    return result;
}

/*
 * Free the storage used by basic blocks.
 */
void dvmFreeVfyBasicBlocks(VerifierData* vdata)
{
    unsigned int idx;

    if (vdata->basicBlocks == NULL)
        return;

    for (idx = 0; idx < vdata->insnsSize; idx++) {
        VfyBasicBlock* block = vdata->basicBlocks[idx];
        if (block == NULL)
            continue;

        dvmPointerSetFree(block->predecessors);
        dvmFreeBitVector(block->liveRegs);
        free(block);
    }

    free(vdata->basicBlocks);
}
