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

/*
 * This file contains register alloction support and is intended to be
 * included by:
 *
 *        Codegen-$(TARGET_ARCH_VARIANT).c
 *
 */

#include "compiler/CompilerUtility.h"
#include "compiler/CompilerIR.h"
#include "compiler/Dataflow.h"
#include "compiler/codegen/mips/MipsLIR.h"

/*
 * Return most flexible allowed register class based on size.
 * Bug: 2813841
 * Must use a core register for data types narrower than word (due
 * to possible unaligned load/store.
 */
static inline RegisterClass dvmCompilerRegClassBySize(OpSize size)
{
    return (size == kUnsignedHalf ||
            size == kSignedHalf ||
            size == kUnsignedByte ||
            size == kSignedByte ) ? kCoreReg : kAnyReg;
}

static inline int dvmCompilerS2VReg(CompilationUnit *cUnit, int sReg)
{
    assert(sReg != INVALID_SREG);
    return DECODE_REG(dvmConvertSSARegToDalvik(cUnit, sReg));
}

/* Reset the tracker to unknown state */
static inline void dvmCompilerResetNullCheck(CompilationUnit *cUnit)
{
    dvmClearAllBits(cUnit->regPool->nullCheckedRegs);
}

/*
 * Get the "real" sreg number associated with an sReg slot.  In general,
 * sReg values passed through codegen are the SSA names created by
 * dataflow analysis and refer to slot numbers in the cUnit->regLocation
 * array.  However, renaming is accomplished by simply replacing RegLocation
 * entries in the cUnit->reglocation[] array.  Therefore, when location
 * records for operands are first created, we need to ask the locRecord
 * identified by the dataflow pass what it's new name is.
 */

static inline int dvmCompilerSRegHi(int lowSreg) {
    return (lowSreg == INVALID_SREG) ? INVALID_SREG : lowSreg + 1;
}


static inline bool dvmCompilerLiveOut(CompilationUnit *cUnit, int sReg)
{
    //TODO: fully implement
    return true;
}

static inline int dvmCompilerSSASrc(MIR *mir, int num)
{
    assert(mir->ssaRep->numUses > num);
    return mir->ssaRep->uses[num];
}

extern RegLocation dvmCompilerEvalLoc(CompilationUnit *cUnit, RegLocation loc,
                                      int regClass, bool update);
/* Mark a temp register as dead.  Does not affect allocation state. */
extern void dvmCompilerClobber(CompilationUnit *cUnit, int reg);

extern RegLocation dvmCompilerUpdateLoc(CompilationUnit *cUnit,
                                        RegLocation loc);

/* see comments for updateLoc */
extern RegLocation dvmCompilerUpdateLocWide(CompilationUnit *cUnit,
                                            RegLocation loc);

/* Clobber all of the temps that might be used by a handler. */
extern void dvmCompilerClobberHandlerRegs(CompilationUnit *cUnit);

extern void dvmCompilerMarkLive(CompilationUnit *cUnit, int reg, int sReg);

extern void dvmCompilerMarkDirty(CompilationUnit *cUnit, int reg);

extern void dvmCompilerMarkPair(CompilationUnit *cUnit, int lowReg,
                                int highReg);

extern void dvmCompilerMarkClean(CompilationUnit *cUnit, int reg);

extern void dvmCompilerResetDef(CompilationUnit *cUnit, int reg);

extern void dvmCompilerResetDefLoc(CompilationUnit *cUnit, RegLocation rl);

/* Set up temp & preserved register pools specialized by target */
extern void dvmCompilerInitPool(RegisterInfo *regs, int *regNums, int num);

/*
 * Mark the beginning and end LIR of a def sequence.  Note that
 * on entry start points to the LIR prior to the beginning of the
 * sequence.
 */
extern void dvmCompilerMarkDef(CompilationUnit *cUnit, RegLocation rl,
                               LIR *start, LIR *finish);
/*
 * Mark the beginning and end LIR of a def sequence.  Note that
 * on entry start points to the LIR prior to the beginning of the
 * sequence.
 */
extern void dvmCompilerMarkDefWide(CompilationUnit *cUnit, RegLocation rl,
                                   LIR *start, LIR *finish);

extern RegLocation dvmCompilerGetSrcWide(CompilationUnit *cUnit, MIR *mir,
                                         int low, int high);

extern RegLocation dvmCompilerGetDestWide(CompilationUnit *cUnit, MIR *mir,
                                          int low, int high);
// Get the LocRecord associated with an SSA name use.
extern RegLocation dvmCompilerGetSrc(CompilationUnit *cUnit, MIR *mir, int num);

// Get the LocRecord associated with an SSA name def.
extern RegLocation dvmCompilerGetDest(CompilationUnit *cUnit, MIR *mir,
                                      int num);

extern RegLocation dvmCompilerGetReturnWide(CompilationUnit *cUnit);

/* Clobber all regs that might be used by an external C call */
extern void dvmCompilerClobberCallRegs(CompilationUnit *cUnit);

extern RegisterInfo *dvmCompilerIsTemp(CompilationUnit *cUnit, int reg);

extern void dvmCompilerMarkInUse(CompilationUnit *cUnit, int reg);

extern int dvmCompilerAllocTemp(CompilationUnit *cUnit);

extern int dvmCompilerAllocTempFloat(CompilationUnit *cUnit);

//REDO: too many assumptions.
extern int dvmCompilerAllocTempDouble(CompilationUnit *cUnit);

extern void dvmCompilerFreeTemp(CompilationUnit *cUnit, int reg);

extern void dvmCompilerResetDefLocWide(CompilationUnit *cUnit, RegLocation rl);

extern void dvmCompilerResetDefTracking(CompilationUnit *cUnit);

/* Kill the corresponding bit in the null-checked register list */
extern void dvmCompilerKillNullCheckedLoc(CompilationUnit *cUnit,
                                          RegLocation loc);

//FIXME - this needs to also check the preserved pool.
extern RegisterInfo *dvmCompilerIsLive(CompilationUnit *cUnit, int reg);

/* To be used when explicitly managing register use */
extern void dvmCompilerLockAllTemps(CompilationUnit *cUnit);

extern void dvmCompilerFlushAllRegs(CompilationUnit *cUnit);

extern RegLocation dvmCompilerGetReturnWideAlt(CompilationUnit *cUnit);

extern RegLocation dvmCompilerGetReturn(CompilationUnit *cUnit);

extern RegLocation dvmCompilerGetReturnAlt(CompilationUnit *cUnit);

/* Clobber any temp associated with an sReg.  Could be in either class */
extern void dvmCompilerClobberSReg(CompilationUnit *cUnit, int sReg);

/* Return a temp if one is available, -1 otherwise */
extern int dvmCompilerAllocFreeTemp(CompilationUnit *cUnit);

/*
 * Similar to dvmCompilerAllocTemp(), but forces the allocation of a specific
 * register.  No check is made to see if the register was previously
 * allocated.  Use with caution.
 */
extern void dvmCompilerLockTemp(CompilationUnit *cUnit, int reg);

extern RegLocation dvmCompilerWideToNarrow(CompilationUnit *cUnit,
                                           RegLocation rl);

/*
 * Free all allocated temps in the temp pools.  Note that this does
 * not affect the "liveness" of a temp register, which will stay
 * live until it is either explicitly killed or reallocated.
 */
extern void dvmCompilerResetRegPool(CompilationUnit *cUnit);

extern void dvmCompilerClobberAllRegs(CompilationUnit *cUnit);

extern void dvmCompilerResetDefTracking(CompilationUnit *cUnit);
