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

/*
 * Identify unconditional branches that jump to the immediate successor of the
 * branch itself.
 */
static void applyRedundantBranchElimination(CompilationUnit *cUnit)
{
    ArmLIR *thisLIR;

    for (thisLIR = (ArmLIR *) cUnit->firstLIRInsn;
         thisLIR != (ArmLIR *) cUnit->lastLIRInsn;
         thisLIR = NEXT_LIR(thisLIR)) {

        /* Branch to the next instruction */
        if (thisLIR->opcode == kThumbBUncond) {
            ArmLIR *nextLIR = thisLIR;

            while (true) {
                nextLIR = NEXT_LIR(nextLIR);

                /*
                 * Is the branch target the next instruction?
                 */
                if (nextLIR == (ArmLIR *) thisLIR->generic.target) {
                    thisLIR->flags.isNop = true;
                    break;
                }

                /*
                 * Found real useful stuff between the branch and the target.
                 * Need to explicitly check the lastLIRInsn here since with
                 * method-based JIT the branch might be the last real
                 * instruction.
                 */
                if (!isPseudoOpcode(nextLIR->opcode) ||
                    (nextLIR = (ArmLIR *) cUnit->lastLIRInsn))
                    break;
            }
        }
    }
}

void dvmCompilerApplyGlobalOptimizations(CompilationUnit *cUnit)
{
    applyRedundantBranchElimination(cUnit);
}
