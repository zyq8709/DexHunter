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
 * This file contains Arm-specific register alloction support.
 */

#include "compiler/CompilerUtility.h"
#include "compiler/CompilerIR.h"
#include "compiler/Dataflow.h"
#include "ArmLIR.h"
#include "Codegen.h"
#include "compiler/codegen/Ralloc.h"

/*
 * Register usage for 16-bit Thumb systems:
 *     r0-r3: Temp/argument
 *     lr(r14):      Temp for translations, return address for handlers
 *     rSELF(r6):    Pointer to Thread
 *     rFP(r5):      Dalvik frame pointer
 *     r4, r7:       Temp for translations
 *     r8, r9, r10:   Temp preserved across C calls
 *     r11, ip(r12):  Temp not preserved across C calls
 *
 * Register usage for 32-bit Thumb systems:
 *     r0-r3: Temp/argument
 *     lr(r14):      Temp for translations, return address for handlers
 *     rSELF(r6):    Pointer to Thread
 *     rFP(r5):      Dalvik frame pointer
 *     r4, r7:       Temp for translations
 *     r8, r9, r10   Temp preserved across C calls
 *     r11, ip(r12):      Temp not preserved across C calls
 *     fp0-fp15:     Hot temps, not preserved across C calls
 *     fp16-fp31:    Promotion pool
 *
 */

/* Clobber all regs that might be used by an external C call */
extern void dvmCompilerClobberCallRegs(CompilationUnit *cUnit)
{
    dvmCompilerClobber(cUnit, r0);
    dvmCompilerClobber(cUnit, r1);
    dvmCompilerClobber(cUnit, r2);
    dvmCompilerClobber(cUnit, r3);
    dvmCompilerClobber(cUnit, r9); // Need to do this?, be conservative
    dvmCompilerClobber(cUnit, r11);
    dvmCompilerClobber(cUnit, r12);
    dvmCompilerClobber(cUnit, r14lr);
}

/* Clobber all of the temps that might be used by a handler. */
extern void dvmCompilerClobberHandlerRegs(CompilationUnit *cUnit)
{
    //TUNING: reduce the set of regs used by handlers.  Only a few need lots.
    dvmCompilerClobberCallRegs(cUnit);
    dvmCompilerClobber(cUnit, r4PC);
    dvmCompilerClobber(cUnit, r7);
    dvmCompilerClobber(cUnit, r8);
    dvmCompilerClobber(cUnit, r9);
    dvmCompilerClobber(cUnit, r10);
}

extern RegLocation dvmCompilerGetReturnWide(CompilationUnit *cUnit)
{
    RegLocation res = LOC_C_RETURN_WIDE;
    dvmCompilerClobber(cUnit, r0);
    dvmCompilerClobber(cUnit, r1);
    dvmCompilerMarkInUse(cUnit, r0);
    dvmCompilerMarkInUse(cUnit, r1);
    dvmCompilerMarkPair(cUnit, res.lowReg, res.highReg);
    return res;
}

extern RegLocation dvmCompilerGetReturnWideAlt(CompilationUnit *cUnit)
{
    RegLocation res = LOC_C_RETURN_WIDE;
    res.lowReg = r2;
    res.highReg = r3;
    dvmCompilerClobber(cUnit, r2);
    dvmCompilerClobber(cUnit, r3);
    dvmCompilerMarkInUse(cUnit, r2);
    dvmCompilerMarkInUse(cUnit, r3);
    dvmCompilerMarkPair(cUnit, res.lowReg, res.highReg);
    return res;
}

extern RegLocation dvmCompilerGetReturn(CompilationUnit *cUnit)
{
    RegLocation res = LOC_C_RETURN;
    dvmCompilerClobber(cUnit, r0);
    dvmCompilerMarkInUse(cUnit, r0);
    return res;
}

extern RegLocation dvmCompilerGetReturnAlt(CompilationUnit *cUnit)
{
    RegLocation res = LOC_C_RETURN;
    res.lowReg = r1;
    dvmCompilerClobber(cUnit, r1);
    dvmCompilerMarkInUse(cUnit, r1);
    return res;
}
