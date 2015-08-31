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
#include "MipsLIR.h"

/*
 * Identify unconditional branches that jump to the immediate successor of the
 * branch itself.
 */
static void applyRedundantBranchElimination(CompilationUnit *cUnit)
{
    MipsLIR *thisLIR;

    for (thisLIR = (MipsLIR *) cUnit->firstLIRInsn;
         thisLIR != (MipsLIR *) cUnit->lastLIRInsn;
         thisLIR = NEXT_LIR(thisLIR)) {

        /* Branch to the next instruction */
        if (!thisLIR->flags.isNop && thisLIR->opcode == kMipsB) {
            MipsLIR *nextLIR = thisLIR;

            while (true) {
                nextLIR = NEXT_LIR(nextLIR);

                /*
                 * Is the branch target the next instruction?
                 */
                if (nextLIR == (MipsLIR *) thisLIR->generic.target) {
                    thisLIR->flags.isNop = true;
                    break;
                }

                /*
                 * Found real useful stuff between the branch and the target.
                 * Need to explicitly check the lastLIRInsn here since with
                 * method-based JIT the branch might be the last real
                 * instruction.
                 */
                if (!isPseudoOpCode(nextLIR->opcode) ||
                    (nextLIR = (MipsLIR *) cUnit->lastLIRInsn))
                    break;
            }
        }
    }
}

/*
 * Do simple a form of copy propagation and elimination.
 */
static void applyCopyPropagation(CompilationUnit *cUnit)
{
    MipsLIR *thisLIR;

    /* look for copies to possibly eliminate */
    for (thisLIR = (MipsLIR *) cUnit->firstLIRInsn;
         thisLIR != (MipsLIR *) cUnit->lastLIRInsn;
         thisLIR = NEXT_LIR(thisLIR)) {

        if (thisLIR->flags.isNop || thisLIR->opcode != kMipsMove)
            continue;

        const int max_insns = 10;
        MipsLIR *savedLIR[max_insns];
        int srcRedefined = 0;
        int insnCount = 0;
        MipsLIR *nextLIR;

        /* look for and record all uses of reg defined by the copy */
        for (nextLIR = (MipsLIR *) NEXT_LIR(thisLIR);
             nextLIR != (MipsLIR *) cUnit->lastLIRInsn;
             nextLIR = NEXT_LIR(nextLIR)) {

            if (nextLIR->flags.isNop || nextLIR->opcode == kMips32BitData)
                continue;

            if (isPseudoOpCode(nextLIR->opcode)) {
                if (nextLIR->opcode == kMipsPseudoDalvikByteCodeBoundary ||
                    nextLIR->opcode == kMipsPseudoBarrier ||
                    nextLIR->opcode == kMipsPseudoExtended ||
                    nextLIR->opcode == kMipsPseudoSSARep)
                    continue; /* these pseudos don't pose problems */
                else if (nextLIR->opcode == kMipsPseudoTargetLabel ||
                         nextLIR->opcode == kMipsPseudoEntryBlock ||
                         nextLIR->opcode == kMipsPseudoExitBlock)
                    insnCount = 0;  /* give up for these pseudos */
                break; /* reached end for copy propagation */
            }

            /* Since instructions with IS_BRANCH flag set will have its */
            /* useMask and defMask set to ENCODE_ALL, any checking of   */
            /* these flags must come after the branching checks.        */

            /* don't propagate across branch/jump and link case
               or jump via register */
            if (EncodingMap[nextLIR->opcode].flags & REG_DEF_LR ||
                nextLIR->opcode == kMipsJalr ||
                nextLIR->opcode == kMipsJr) {
                insnCount = 0;
                break;
            }

            /* branches with certain targets ok while others aren't */
            if (EncodingMap[nextLIR->opcode].flags & IS_BRANCH) {
                MipsLIR *targetLIR =  (MipsLIR *) nextLIR->generic.target;
                if (targetLIR->opcode != kMipsPseudoEHBlockLabel &&
                    targetLIR->opcode != kMipsPseudoChainingCellHot &&
                    targetLIR->opcode != kMipsPseudoChainingCellNormal &&
                    targetLIR->opcode != kMipsPseudoChainingCellInvokePredicted &&
                    targetLIR->opcode != kMipsPseudoChainingCellInvokeSingleton &&
                    targetLIR->opcode != kMipsPseudoPCReconstructionBlockLabel &&
                    targetLIR->opcode != kMipsPseudoPCReconstructionCell) {
                    insnCount = 0;
                    break;
                }
                /* FIXME - for now don't propagate across any branch/jump. */
                insnCount = 0;
                break;
            }

            /* copy def reg used here, so record insn for copy propagation */
            if (thisLIR->defMask & nextLIR->useMask) {
                if (insnCount == max_insns || srcRedefined) {
                    insnCount = 0;
                    break; /* just give up if too many or not possible */
                }
                savedLIR[insnCount++] = nextLIR;
            }

            if (thisLIR->defMask & nextLIR->defMask) {
		if (nextLIR->opcode == kMipsMovz)
		    insnCount = 0; /* movz relies on thisLIR setting dst reg so abandon propagation*/
                break;
            }

            /* copy src reg redefined here, so can't propagate further */
            if (thisLIR->useMask & nextLIR->defMask) {
                if (insnCount == 0)
                    break; /* nothing to propagate */
                srcRedefined = 1;
            }
       }

        /* conditions allow propagation and copy elimination */
        if (insnCount) {
            int i;
            for (i = 0; i < insnCount; i++) {
                int flags = EncodingMap[savedLIR[i]->opcode].flags;
                savedLIR[i]->useMask &= ~(1 << thisLIR->operands[0]);
                savedLIR[i]->useMask |= 1 << thisLIR->operands[1];
                if ((flags & REG_USE0) &&
                    savedLIR[i]->operands[0] == thisLIR->operands[0])
                    savedLIR[i]->operands[0] = thisLIR->operands[1];
                if ((flags & REG_USE1) &&
                    savedLIR[i]->operands[1] == thisLIR->operands[0])
                    savedLIR[i]->operands[1] = thisLIR->operands[1];
                if ((flags & REG_USE2) &&
                    savedLIR[i]->operands[2] == thisLIR->operands[0])
                    savedLIR[i]->operands[2] = thisLIR->operands[1];
                if ((flags & REG_USE3) &&
                    savedLIR[i]->operands[3] == thisLIR->operands[0])
                    savedLIR[i]->operands[3] = thisLIR->operands[1];
            }
            thisLIR->flags.isNop = true;
        }
    }
}

#ifdef __mips_hard_float
/*
 * Look for pairs of mov.s instructions that can be combined into mov.d
 */
static void mergeMovs(CompilationUnit *cUnit)
{
  MipsLIR *movsLIR = NULL;
  MipsLIR *thisLIR;

  for (thisLIR = (MipsLIR *) cUnit->firstLIRInsn;
       thisLIR != (MipsLIR *) cUnit->lastLIRInsn;
       thisLIR = NEXT_LIR(thisLIR)) {
    if (thisLIR->flags.isNop)
      continue;

    if (isPseudoOpCode(thisLIR->opcode)) {
      if (thisLIR->opcode == kMipsPseudoDalvikByteCodeBoundary ||
                thisLIR->opcode == kMipsPseudoExtended ||
	  thisLIR->opcode == kMipsPseudoSSARep)
	continue;  /* ok to move across these pseudos */
      movsLIR = NULL; /* don't merge across other pseudos */
      continue;
    }

    /* merge pairs of mov.s instructions */
    if (thisLIR->opcode == kMipsFmovs) {
      if (movsLIR == NULL)
	movsLIR = thisLIR;
      else if (((movsLIR->operands[0] & 1) == 0) &&
	       ((movsLIR->operands[1] & 1) == 0) &&
	       ((movsLIR->operands[0] + 1) == thisLIR->operands[0]) &&
	       ((movsLIR->operands[1] + 1) == thisLIR->operands[1])) {
	/* movsLIR is handling even register - upgrade to mov.d */
	movsLIR->opcode = kMipsFmovd;
	movsLIR->operands[0] = S2D(movsLIR->operands[0], movsLIR->operands[0]+1);
	movsLIR->operands[1] = S2D(movsLIR->operands[1], movsLIR->operands[1]+1);
	thisLIR->flags.isNop = true;
	movsLIR = NULL;
      }
      else if (((movsLIR->operands[0] & 1) == 1) &&
	       ((movsLIR->operands[1] & 1) == 1) &&
	       ((movsLIR->operands[0] - 1) == thisLIR->operands[0]) &&
	       ((movsLIR->operands[1] - 1) == thisLIR->operands[1])) {
	/* thissLIR is handling even register - upgrade to mov.d */
	thisLIR->opcode = kMipsFmovd;
	thisLIR->operands[0] = S2D(thisLIR->operands[0], thisLIR->operands[0]+1);
	thisLIR->operands[1] = S2D(thisLIR->operands[1], thisLIR->operands[1]+1);
	movsLIR->flags.isNop = true;
	movsLIR = NULL;
      }
      else
	/* carry on searching from here */
	movsLIR = thisLIR;
      continue;
    }

    /* intervening instruction - start search from scratch */
    movsLIR = NULL;
  }
}
#endif


/*
 * Look back first and then ahead to try to find an instruction to move into
 * the branch delay slot.  If the analysis can be done cheaply enough, it may be
 * be possible to tune this routine to be more beneficial (e.g., being more
 * particular about what instruction is speculated).
 */
static MipsLIR *delaySlotLIR(MipsLIR *firstLIR, MipsLIR *branchLIR)
{
    int isLoad;
    int loadVisited = 0;
    int isStore;
    int storeVisited = 0;
    u8 useMask = branchLIR->useMask;
    u8 defMask = branchLIR->defMask;
    MipsLIR *thisLIR;
    MipsLIR *newLIR = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);

    for (thisLIR = PREV_LIR(branchLIR);
         thisLIR != firstLIR;
         thisLIR = PREV_LIR(thisLIR)) {
        if (thisLIR->flags.isNop)
            continue;

        if (isPseudoOpCode(thisLIR->opcode)) {
            if (thisLIR->opcode == kMipsPseudoDalvikByteCodeBoundary ||
                thisLIR->opcode == kMipsPseudoExtended ||
                thisLIR->opcode == kMipsPseudoSSARep)
                continue;  /* ok to move across these pseudos */
            break; /* don't move across all other pseudos */
        }

        /* give up on moving previous instruction down into slot */
        if (thisLIR->opcode == kMipsNop ||
            thisLIR->opcode == kMips32BitData ||
            EncodingMap[thisLIR->opcode].flags & IS_BRANCH)
            break;

        /* don't reorder loads/stores (the alias info could
           possibly be used to allow as a future enhancement) */
        isLoad = EncodingMap[thisLIR->opcode].flags & IS_LOAD;
        isStore = EncodingMap[thisLIR->opcode].flags & IS_STORE;

        if (!(thisLIR->useMask & defMask) &&
            !(thisLIR->defMask & useMask) &&
            !(thisLIR->defMask & defMask) &&
            !(isLoad && storeVisited) &&
            !(isStore && loadVisited) &&
            !(isStore && storeVisited)) {
            *newLIR = *thisLIR;
            thisLIR->flags.isNop = true;
            return newLIR; /* move into delay slot succeeded */
        }

        loadVisited |= isLoad;
        storeVisited |= isStore;

        /* accumulate def/use constraints */
        useMask |= thisLIR->useMask;
        defMask |= thisLIR->defMask;
    }

    /* for unconditional branches try to copy the instruction at the
       branch target up into the delay slot and adjust the branch */
    if (branchLIR->opcode == kMipsB) {
        MipsLIR *targetLIR;
        for (targetLIR = (MipsLIR *) branchLIR->generic.target;
             targetLIR;
             targetLIR = NEXT_LIR(targetLIR)) {
            if (!targetLIR->flags.isNop &&
                (!isPseudoOpCode(targetLIR->opcode) || /* can't pull predicted up */
                 targetLIR->opcode == kMipsPseudoChainingCellInvokePredicted))
                break; /* try to get to next real op at branch target */
        }
        if (targetLIR && !isPseudoOpCode(targetLIR->opcode) &&
            !(EncodingMap[targetLIR->opcode].flags & IS_BRANCH)) {
            *newLIR = *targetLIR;
            branchLIR->generic.target = (LIR *) NEXT_LIR(targetLIR);
            return newLIR;
        }
    } else if (branchLIR->opcode >= kMipsBeq && branchLIR->opcode <= kMipsBne) {
        /* for conditional branches try to fill branch delay slot
           via speculative execution when safe */
        MipsLIR *targetLIR;
        for (targetLIR = (MipsLIR *) branchLIR->generic.target;
             targetLIR;
             targetLIR = NEXT_LIR(targetLIR)) {
            if (!targetLIR->flags.isNop && !isPseudoOpCode(targetLIR->opcode))
                break; /* try to get to next real op at branch target */
        }

        MipsLIR *nextLIR;
        for (nextLIR = NEXT_LIR(branchLIR);
             nextLIR;
             nextLIR = NEXT_LIR(nextLIR)) {
            if (!nextLIR->flags.isNop && !isPseudoOpCode(nextLIR->opcode))
                break; /* try to get to next real op for fall thru */
        }

        if (nextLIR && targetLIR) {
            int flags = EncodingMap[nextLIR->opcode].flags;
            int isLoad = flags & IS_LOAD;

            /* common branch and fall thru to normal chaining cells case */
            if (isLoad && nextLIR->opcode == targetLIR->opcode &&
                nextLIR->operands[0] == targetLIR->operands[0] &&
                nextLIR->operands[1] == targetLIR->operands[1] &&
                nextLIR->operands[2] == targetLIR->operands[2]) {
                *newLIR = *targetLIR;
                branchLIR->generic.target = (LIR *) NEXT_LIR(targetLIR);
                return newLIR;
            }

            /* try prefetching (maybe try speculating instructions along the
               trace like dalvik frame load which is common and may be safe) */
            int isStore = flags & IS_STORE;
            if (isLoad || isStore) {
                newLIR->opcode = kMipsPref;
                newLIR->operands[0] = isLoad ? 0 : 1;
                newLIR->operands[1] = nextLIR->operands[1];
                newLIR->operands[2] = nextLIR->operands[2];
                newLIR->defMask = nextLIR->defMask;
                newLIR->useMask = nextLIR->useMask;
                return newLIR;
            }
        }
    }

    /* couldn't find a useful instruction to move into the delay slot */
    newLIR->opcode = kMipsNop;
    return newLIR;
}

/*
 * The branch delay slot has been ignored thus far.  This is the point where
 * a useful instruction is moved into it or a nop is inserted.  Leave existing
 * NOPs alone -- these came from sparse and packed switch ops and are needed
 * to maintain the proper offset to the jump table.
 */
static void introduceBranchDelaySlot(CompilationUnit *cUnit)
{
    MipsLIR *thisLIR;
    MipsLIR *firstLIR =(MipsLIR *) cUnit->firstLIRInsn;
    MipsLIR *lastLIR =(MipsLIR *) cUnit->lastLIRInsn;

    for (thisLIR = lastLIR; thisLIR != firstLIR; thisLIR = PREV_LIR(thisLIR)) {
        if (thisLIR->flags.isNop ||
            isPseudoOpCode(thisLIR->opcode) ||
            !(EncodingMap[thisLIR->opcode].flags & IS_BRANCH)) {
            continue;
        } else if (thisLIR == lastLIR) {
            dvmCompilerAppendLIR(cUnit,
                (LIR *) delaySlotLIR(firstLIR, thisLIR));
        } else if (NEXT_LIR(thisLIR)->opcode != kMipsNop) {
            dvmCompilerInsertLIRAfter((LIR *) thisLIR,
                (LIR *) delaySlotLIR(firstLIR, thisLIR));
        }
    }

    if (!thisLIR->flags.isNop &&
        !isPseudoOpCode(thisLIR->opcode) &&
        EncodingMap[thisLIR->opcode].flags & IS_BRANCH) {
        /* nothing available to move, so insert nop */
        MipsLIR *nopLIR = (MipsLIR *) dvmCompilerNew(sizeof(MipsLIR), true);
        nopLIR->opcode = kMipsNop;
        dvmCompilerInsertLIRAfter((LIR *) thisLIR, (LIR *) nopLIR);
    }
}

void dvmCompilerApplyGlobalOptimizations(CompilationUnit *cUnit)
{
    applyRedundantBranchElimination(cUnit);
    applyCopyPropagation(cUnit);
#ifdef __mips_hard_float
    mergeMovs(cUnit);
#endif
    introduceBranchDelaySlot(cUnit);
}
