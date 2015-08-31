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
#include "vm/compiler/CompilerInternals.h"
#include "ArmLIR.h"
#include "Codegen.h"

#define DEBUG_OPT(X)

/* Check RAW, WAR, and WAR dependency on the register operands */
#define CHECK_REG_DEP(use, def, check) ((def & check->useMask) || \
                                        ((use | def) & check->defMask))

/* Scheduler heuristics */
#define MAX_HOIST_DISTANCE 20
#define LDLD_DISTANCE 4
#define LD_LATENCY 2

static inline bool isDalvikRegisterClobbered(ArmLIR *lir1, ArmLIR *lir2)
{
    int reg1Lo = DECODE_ALIAS_INFO_REG(lir1->aliasInfo);
    int reg1Hi = reg1Lo + DECODE_ALIAS_INFO_WIDE(lir1->aliasInfo);
    int reg2Lo = DECODE_ALIAS_INFO_REG(lir2->aliasInfo);
    int reg2Hi = reg2Lo + DECODE_ALIAS_INFO_WIDE(lir2->aliasInfo);

    return (reg1Lo == reg2Lo) || (reg1Lo == reg2Hi) || (reg1Hi == reg2Lo);
}

#if 0
/* Debugging utility routine */
static void dumpDependentInsnPair(ArmLIR *thisLIR, ArmLIR *checkLIR,
                                  const char *optimization)
{
    ALOGD("************ %s ************", optimization);
    dvmDumpLIRInsn((LIR *) thisLIR, 0);
    dvmDumpLIRInsn((LIR *) checkLIR, 0);
}
#endif

/* Convert a more expensive instruction (ie load) into a move */
static void convertMemOpIntoMove(CompilationUnit *cUnit, ArmLIR *origLIR,
                                 int dest, int src)
{
    /* Insert a move to replace the load */
    ArmLIR *moveLIR;
    moveLIR = dvmCompilerRegCopyNoInsert( cUnit, dest, src);
    /*
     * Insert the converted instruction after the original since the
     * optimization is scannng in the top-down order and the new instruction
     * will need to be re-checked (eg the new dest clobbers the src used in
     * thisLIR).
     */
    dvmCompilerInsertLIRAfter((LIR *) origLIR, (LIR *) moveLIR);
}

/*
 * Perform a pass of top-down walk, from the second-last instruction in the
 * superblock, to eliminate redundant loads and stores.
 *
 * An earlier load can eliminate a later load iff
 *   1) They are must-aliases
 *   2) The native register is not clobbered in between
 *   3) The memory location is not written to in between
 *
 * An earlier store can eliminate a later load iff
 *   1) They are must-aliases
 *   2) The native register is not clobbered in between
 *   3) The memory location is not written to in between
 *
 * A later store can be eliminated by an earlier store iff
 *   1) They are must-aliases
 *   2) The memory location is not written to in between
 */
static void applyLoadStoreElimination(CompilationUnit *cUnit,
                                      ArmLIR *headLIR,
                                      ArmLIR *tailLIR)
{
    ArmLIR *thisLIR;

    if (headLIR == tailLIR) return;

    for (thisLIR = PREV_LIR(tailLIR);
         thisLIR != headLIR;
         thisLIR = PREV_LIR(thisLIR)) {
        int sinkDistance = 0;

        /* Skip non-interesting instructions */
        if ((thisLIR->flags.isNop == true) ||
            isPseudoOpcode(thisLIR->opcode) ||
            !(EncodingMap[thisLIR->opcode].flags & (IS_LOAD | IS_STORE))) {
            continue;
        }

        int nativeRegId = thisLIR->operands[0];
        bool isThisLIRLoad = EncodingMap[thisLIR->opcode].flags & IS_LOAD;
        ArmLIR *checkLIR;
        /* Use the mem mask to determine the rough memory location */
        u8 thisMemMask = (thisLIR->useMask | thisLIR->defMask) & ENCODE_MEM;

        /*
         * Currently only eliminate redundant ld/st for constant and Dalvik
         * register accesses.
         */
        if (!(thisMemMask & (ENCODE_LITERAL | ENCODE_DALVIK_REG))) continue;

        /*
         * Add r15 (pc) to the resource mask to prevent this instruction
         * from sinking past branch instructions. Also take out the memory
         * region bits since stopMask is used to check data/control
         * dependencies.
         */
        u8 stopUseRegMask = (ENCODE_REG_PC | thisLIR->useMask) &
                            ~ENCODE_MEM;
        u8 stopDefRegMask = thisLIR->defMask & ~ENCODE_MEM;

        for (checkLIR = NEXT_LIR(thisLIR);
             checkLIR != tailLIR;
             checkLIR = NEXT_LIR(checkLIR)) {

            /*
             * Skip already dead instructions (whose dataflow information is
             * outdated and misleading).
             */
            if (checkLIR->flags.isNop) continue;

            u8 checkMemMask = (checkLIR->useMask | checkLIR->defMask) &
                              ENCODE_MEM;
            u8 aliasCondition = thisMemMask & checkMemMask;
            bool stopHere = false;

            /*
             * Potential aliases seen - check the alias relations
             */
            if (checkMemMask != ENCODE_MEM && aliasCondition != 0) {
                bool isCheckLIRLoad = EncodingMap[checkLIR->opcode].flags &
                                      IS_LOAD;
                if  (aliasCondition == ENCODE_LITERAL) {
                    /*
                     * Should only see literal loads in the instruction
                     * stream.
                     */
                    assert(!(EncodingMap[checkLIR->opcode].flags &
                             IS_STORE));
                    /* Same value && same register type */
                    if (checkLIR->aliasInfo == thisLIR->aliasInfo &&
                        REGTYPE(checkLIR->operands[0]) == REGTYPE(nativeRegId)){
                        /*
                         * Different destination register - insert
                         * a move
                         */
                        if (checkLIR->operands[0] != nativeRegId) {
                            convertMemOpIntoMove(cUnit, checkLIR,
                                                 checkLIR->operands[0],
                                                 nativeRegId);
                        }
                        checkLIR->flags.isNop = true;
                    }
                } else if (aliasCondition == ENCODE_DALVIK_REG) {
                    /* Must alias */
                    if (checkLIR->aliasInfo == thisLIR->aliasInfo) {
                        /* Only optimize compatible registers */
                        bool regCompatible =
                            REGTYPE(checkLIR->operands[0]) ==
                            REGTYPE(nativeRegId);
                        if ((isThisLIRLoad && isCheckLIRLoad) ||
                            (!isThisLIRLoad && isCheckLIRLoad)) {
                            /* RAR or RAW */
                            if (regCompatible) {
                                /*
                                 * Different destination register -
                                 * insert a move
                                 */
                                if (checkLIR->operands[0] !=
                                    nativeRegId) {
                                    convertMemOpIntoMove(cUnit,
                                                 checkLIR,
                                                 checkLIR->operands[0],
                                                 nativeRegId);
                                }
                                checkLIR->flags.isNop = true;
                            } else {
                                /*
                                 * Destinaions are of different types -
                                 * something complicated going on so
                                 * stop looking now.
                                 */
                                stopHere = true;
                            }
                        } else if (isThisLIRLoad && !isCheckLIRLoad) {
                            /* WAR - register value is killed */
                            stopHere = true;
                        } else if (!isThisLIRLoad && !isCheckLIRLoad) {
                            /* WAW - nuke the earlier store */
                            thisLIR->flags.isNop = true;
                            stopHere = true;
                        }
                    /* Partial overlap */
                    } else if (isDalvikRegisterClobbered(thisLIR, checkLIR)) {
                        /*
                         * It is actually ok to continue if checkLIR
                         * is a read. But it is hard to make a test
                         * case for this so we just stop here to be
                         * conservative.
                         */
                        stopHere = true;
                    }
                }
                /* Memory content may be updated. Stop looking now. */
                if (stopHere) {
                    break;
                /* The checkLIR has been transformed - check the next one */
                } else if (checkLIR->flags.isNop) {
                    continue;
                }
            }


            /*
             * this and check LIRs have no memory dependency. Now check if
             * their register operands have any RAW, WAR, and WAW
             * dependencies. If so, stop looking.
             */
            if (stopHere == false) {
                stopHere = CHECK_REG_DEP(stopUseRegMask, stopDefRegMask,
                                         checkLIR);
            }

            if (stopHere == true) {
                DEBUG_OPT(dumpDependentInsnPair(thisLIR, checkLIR,
                                                "REG CLOBBERED"));
                /* Only sink store instructions */
                if (sinkDistance && !isThisLIRLoad) {
                    ArmLIR *newStoreLIR =
                        (ArmLIR *) dvmCompilerNew(sizeof(ArmLIR), true);
                    *newStoreLIR = *thisLIR;
                    /*
                     * Stop point found - insert *before* the checkLIR
                     * since the instruction list is scanned in the
                     * top-down order.
                     */
                    dvmCompilerInsertLIRBefore((LIR *) checkLIR,
                                               (LIR *) newStoreLIR);
                    thisLIR->flags.isNop = true;
                }
                break;
            } else if (!checkLIR->flags.isNop) {
                sinkDistance++;
            }
        }
    }
}

/*
 * Perform a pass of bottom-up walk, from the second instruction in the
 * superblock, to try to hoist loads to earlier slots.
 */
static void applyLoadHoisting(CompilationUnit *cUnit,
                              ArmLIR *headLIR,
                              ArmLIR *tailLIR)
{
    ArmLIR *thisLIR, *checkLIR;
    /*
     * Store the list of independent instructions that can be hoisted past.
     * Will decide the best place to insert later.
     */
    ArmLIR *prevInstList[MAX_HOIST_DISTANCE];

    /* Empty block */
    if (headLIR == tailLIR) return;

    /* Start from the second instruction */
    for (thisLIR = NEXT_LIR(headLIR);
         thisLIR != tailLIR;
         thisLIR = NEXT_LIR(thisLIR)) {

        /* Skip non-interesting instructions */
        if ((thisLIR->flags.isNop == true) ||
            isPseudoOpcode(thisLIR->opcode) ||
            !(EncodingMap[thisLIR->opcode].flags & IS_LOAD)) {
            continue;
        }

        u8 stopUseAllMask = thisLIR->useMask;

        /*
         * Branches for null/range checks are marked with the true resource
         * bits, and loads to Dalvik registers, constant pools, and non-alias
         * locations are safe to be hoisted. So only mark the heap references
         * conservatively here.
         */
        if (stopUseAllMask & ENCODE_HEAP_REF) {
            stopUseAllMask |= ENCODE_REG_PC;
        }

        /* Similar as above, but just check for pure register dependency */
        u8 stopUseRegMask = stopUseAllMask & ~ENCODE_MEM;
        u8 stopDefRegMask = thisLIR->defMask & ~ENCODE_MEM;

        int nextSlot = 0;
        bool stopHere = false;

        /* Try to hoist the load to a good spot */
        for (checkLIR = PREV_LIR(thisLIR);
             checkLIR != headLIR;
             checkLIR = PREV_LIR(checkLIR)) {

            /*
             * Skip already dead instructions (whose dataflow information is
             * outdated and misleading).
             */
            if (checkLIR->flags.isNop) continue;

            u8 checkMemMask = checkLIR->defMask & ENCODE_MEM;
            u8 aliasCondition = stopUseAllMask & checkMemMask;
            stopHere = false;

            /* Potential WAR alias seen - check the exact relation */
            if (checkMemMask != ENCODE_MEM && aliasCondition != 0) {
                /* We can fully disambiguate Dalvik references */
                if (aliasCondition == ENCODE_DALVIK_REG) {
                    /* Must alias or partually overlap */
                    if ((checkLIR->aliasInfo == thisLIR->aliasInfo) ||
                        isDalvikRegisterClobbered(thisLIR, checkLIR)) {
                        stopHere = true;
                    }
                /* Conservatively treat all heap refs as may-alias */
                } else {
                    assert(aliasCondition == ENCODE_HEAP_REF);
                    stopHere = true;
                }
                /* Memory content may be updated. Stop looking now. */
                if (stopHere) {
                    prevInstList[nextSlot++] = checkLIR;
                    break;
                }
            }

            if (stopHere == false) {
                stopHere = CHECK_REG_DEP(stopUseRegMask, stopDefRegMask,
                                         checkLIR);
            }

            /*
             * Store the dependent or non-pseudo/indepedent instruction to the
             * list.
             */
            if (stopHere || !isPseudoOpcode(checkLIR->opcode)) {
                prevInstList[nextSlot++] = checkLIR;
                if (nextSlot == MAX_HOIST_DISTANCE) break;
            }

            /* Found a new place to put the load - move it here */
            if (stopHere == true) {
                DEBUG_OPT(dumpDependentInsnPair(checkLIR, thisLIR
                                                "HOIST STOP"));
                break;
            }
        }

        /*
         * Reached the top - use headLIR as the dependent marker as all labels
         * are barriers.
         */
        if (stopHere == false && nextSlot < MAX_HOIST_DISTANCE) {
            prevInstList[nextSlot++] = headLIR;
        }

        /*
         * At least one independent instruction is found. Scan in the reversed
         * direction to find a beneficial slot.
         */
        if (nextSlot >= 2) {
            int firstSlot = nextSlot - 2;
            int slot;
            ArmLIR *depLIR = prevInstList[nextSlot-1];
            /* If there is ld-ld dependency, wait LDLD_DISTANCE cycles */
            if (!isPseudoOpcode(depLIR->opcode) &&
                (EncodingMap[depLIR->opcode].flags & IS_LOAD)) {
                firstSlot -= LDLD_DISTANCE;
            }
            /*
             * Make sure we check slot >= 0 since firstSlot may be negative
             * when the loop is first entered.
             */
            for (slot = firstSlot; slot >= 0; slot--) {
                ArmLIR *curLIR = prevInstList[slot];
                ArmLIR *prevLIR = prevInstList[slot+1];

                /*
                 * Check the highest instruction.
                 * ENCODE_ALL represents a scheduling barrier.
                 */
                if (prevLIR->defMask == ENCODE_ALL) {
                    /*
                     * If the first instruction is a load, don't hoist anything
                     * above it since it is unlikely to be beneficial.
                     */
                    if (EncodingMap[curLIR->opcode].flags & IS_LOAD) continue;
                    /*
                     * Need to unconditionally break here even if the hoisted
                     * distance is greater than LD_LATENCY (ie more than enough
                     * cycles are inserted to hide the load latency) since theu
                     * subsequent code doesn't expect to compare against a
                     * pseudo opcode (whose opcode value is negative).
                     */
                    break;
                }

                /*
                 * NOTE: now prevLIR is guaranteed to be a non-pseudo
                 * instruction (ie accessing EncodingMap[prevLIR->opcode] is
                 * safe).
                 *
                 * Try to find two instructions with load/use dependency until
                 * the remaining instructions are less than LD_LATENCY.
                 */
                if (((curLIR->useMask & prevLIR->defMask) &&
                     (EncodingMap[prevLIR->opcode].flags & IS_LOAD)) ||
                    (slot < LD_LATENCY)) {
                    break;
                }
            }

            /* Found a slot to hoist to */
            if (slot >= 0) {
                ArmLIR *curLIR = prevInstList[slot];
                ArmLIR *newLoadLIR = (ArmLIR *) dvmCompilerNew(sizeof(ArmLIR),
                                                               true);
                *newLoadLIR = *thisLIR;
                /*
                 * Insertion is guaranteed to succeed since checkLIR
                 * is never the first LIR on the list
                 */
                dvmCompilerInsertLIRBefore((LIR *) curLIR,
                                           (LIR *) newLoadLIR);
                thisLIR->flags.isNop = true;
            }
        }
    }
}

void dvmCompilerApplyLocalOptimizations(CompilationUnit *cUnit, LIR *headLIR,
                                        LIR *tailLIR)
{
    if (!(gDvmJit.disableOpt & (1 << kLoadStoreElimination))) {
        applyLoadStoreElimination(cUnit, (ArmLIR *) headLIR,
                                  (ArmLIR *) tailLIR);
    }
    if (!(gDvmJit.disableOpt & (1 << kLoadHoisting))) {
        applyLoadHoisting(cUnit, (ArmLIR *) headLIR, (ArmLIR *) tailLIR);
    }
}
