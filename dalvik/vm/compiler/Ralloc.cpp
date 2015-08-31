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

/*
 * Quick & dirty - make FP usage sticky.  This is strictly a hint - local
 * code generation will handle misses.  It might be worthwhile to collaborate
 * with dx/dexopt to avoid reusing the same Dalvik temp for values of
 * different types.
 */
static void inferTypes(CompilationUnit *cUnit, BasicBlock *bb)
{
    MIR *mir;
    if (bb->blockType != kDalvikByteCode && bb->blockType != kEntryBlock)
        return;

    for (mir = bb->firstMIRInsn; mir; mir = mir->next) {
        SSARepresentation *ssaRep = mir->ssaRep;
        if (ssaRep) {
            int i;
            for (i=0; ssaRep->fpUse && i< ssaRep->numUses; i++) {
                if (ssaRep->fpUse[i])
                    cUnit->regLocation[ssaRep->uses[i]].fp = true;
            }
            for (i=0; ssaRep->fpDef && i< ssaRep->numDefs; i++) {
                if (ssaRep->fpDef[i])
                    cUnit->regLocation[ssaRep->defs[i]].fp = true;
            }
        }
    }
}

static const RegLocation freshLoc = {kLocDalvikFrame, 0, 0, INVALID_REG,
                                     INVALID_REG, INVALID_SREG};

/*
 * Local register allocation for simple traces.  Most of the work for
 * local allocation is done on the fly.  Here we do some initialization
 * and type inference.
 */
void dvmCompilerLocalRegAlloc(CompilationUnit *cUnit)
{
    int i;
    RegLocation *loc;

    /* Allocate the location map */
    loc = (RegLocation*)dvmCompilerNew(cUnit->numSSARegs * sizeof(*loc), true);
    for (i=0; i< cUnit->numSSARegs; i++) {
        loc[i] = freshLoc;
        loc[i].sRegLow = i;
    }
    cUnit->regLocation = loc;

    GrowableListIterator iterator;

    dvmGrowableListIteratorInit(&cUnit->blockList, &iterator);
    /* Do type inference pass */
    while (true) {
        BasicBlock *bb = (BasicBlock *) dvmGrowableListIteratorNext(&iterator);
        if (bb == NULL) break;
        inferTypes(cUnit, bb);
    }

    /* Remap SSA names back to original frame locations. */
    for (i=0; i < cUnit->numSSARegs; i++) {
        cUnit->regLocation[i].sRegLow =
                DECODE_REG(dvmConvertSSARegToDalvik(cUnit, loc[i].sRegLow));
    }
}
