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
#include "MipsLIR.h"
#include "Codegen.h"
#include "Ralloc.h"

#define SREG(c, s) ((c)->regLocation[(s)].sRegLow)
/*
 * Get the "real" sreg number associated with an sReg slot.  In general,
 * sReg values passed through codegen are the SSA names created by
 * dataflow analysis and refer to slot numbers in the cUnit->regLocation
 * array.  However, renaming is accomplished by simply replacing RegLocation
 * entries in the cUnit->reglocation[] array.  Therefore, when location
 * records for operands are first created, we need to ask the locRecord
 * identified by the dataflow pass what it's new name is.
 */

/*
 * Free all allocated temps in the temp pools.  Note that this does
 * not affect the "liveness" of a temp register, which will stay
 * live until it is either explicitly killed or reallocated.
 */
extern void dvmCompilerResetRegPool(CompilationUnit *cUnit)
{
    int i;
    for (i=0; i < cUnit->regPool->numCoreTemps; i++) {
        cUnit->regPool->coreTemps[i].inUse = false;
    }
    for (i=0; i < cUnit->regPool->numFPTemps; i++) {
        cUnit->regPool->FPTemps[i].inUse = false;
    }
}

 /* Set up temp & preserved register pools specialized by target */
extern void dvmCompilerInitPool(RegisterInfo *regs, int *regNums, int num)
{
    int i;
    for (i=0; i < num; i++) {
        regs[i].reg = regNums[i];
        regs[i].inUse = false;
        regs[i].pair = false;
        regs[i].live = false;
        regs[i].dirty = false;
        regs[i].sReg = INVALID_SREG;
    }
}

static void dumpRegPool(RegisterInfo *p, int numRegs)
{
    int i;
    ALOGE("================================================");
    for (i=0; i < numRegs; i++ ){
        ALOGE("R[%d]: U:%d, P:%d, part:%d, LV:%d, D:%d, SR:%d, ST:%x, EN:%x",
           p[i].reg, p[i].inUse, p[i].pair, p[i].partner, p[i].live,
           p[i].dirty, p[i].sReg,(int)p[i].defStart, (int)p[i].defEnd);
    }
    ALOGE("================================================");
}

static RegisterInfo *getRegInfo(CompilationUnit *cUnit, int reg)
{
    int numTemps = cUnit->regPool->numCoreTemps;
    RegisterInfo *p = cUnit->regPool->coreTemps;
    int i;
    for (i=0; i< numTemps; i++) {
        if (p[i].reg == reg) {
            return &p[i];
        }
    }
    p = cUnit->regPool->FPTemps;
    numTemps = cUnit->regPool->numFPTemps;
    for (i=0; i< numTemps; i++) {
        if (p[i].reg == reg) {
            return &p[i];
        }
    }
    ALOGE("Tried to get info on a non-existant temp: r%d",reg);
    dvmCompilerAbort(cUnit);
    return NULL;
}

static void flushRegWide(CompilationUnit *cUnit, int reg1, int reg2)
{
    RegisterInfo *info1 = getRegInfo(cUnit, reg1);
    RegisterInfo *info2 = getRegInfo(cUnit, reg2);
    assert(info1 && info2 && info1->pair && info2->pair &&
           (info1->partner == info2->reg) &&
           (info2->partner == info1->reg));
    if ((info1->live && info1->dirty) || (info2->live && info2->dirty)) {
        info1->dirty = false;
        info2->dirty = false;
        if (dvmCompilerS2VReg(cUnit, info2->sReg) <
            dvmCompilerS2VReg(cUnit, info1->sReg))
            info1 = info2;
        dvmCompilerFlushRegWideImpl(cUnit, rFP,
                                    dvmCompilerS2VReg(cUnit, info1->sReg) << 2,
                                    info1->reg, info1->partner);
    }
}

static void flushReg(CompilationUnit *cUnit, int reg)
{
    RegisterInfo *info = getRegInfo(cUnit, reg);
    if (info->live && info->dirty) {
        info->dirty = false;
        dvmCompilerFlushRegImpl(cUnit, rFP,
                                dvmCompilerS2VReg(cUnit, info->sReg) << 2,
                                reg, kWord);
    }
}

/* return true if found reg to clobber */
static bool clobberRegBody(CompilationUnit *cUnit, RegisterInfo *p,
                           int numTemps, int reg)
{
    int i;
    for (i=0; i< numTemps; i++) {
        if (p[i].reg == reg) {
            if (p[i].live && p[i].dirty) {
                if (p[i].pair) {
                    flushRegWide(cUnit, p[i].reg, p[i].partner);
                } else {
                    flushReg(cUnit, p[i].reg);
                }
            }
            p[i].live = false;
            p[i].sReg = INVALID_SREG;
            p[i].defStart = NULL;
            p[i].defEnd = NULL;
            if (p[i].pair) {
                p[i].pair = false;
                /* partners should be in same pool */
                clobberRegBody(cUnit, p, numTemps, p[i].partner);
            }
            return true;
        }
    }
    return false;
}

/* Mark a temp register as dead.  Does not affect allocation state. */
void dvmCompilerClobber(CompilationUnit *cUnit, int reg)
{
    if (!clobberRegBody(cUnit, cUnit->regPool->coreTemps,
                        cUnit->regPool->numCoreTemps, reg)) {
        clobberRegBody(cUnit, cUnit->regPool->FPTemps,
                       cUnit->regPool->numFPTemps, reg);
    }
}

static void clobberSRegBody(RegisterInfo *p, int numTemps, int sReg)
{
    int i;
    for (i=0; i< numTemps; i++) {
        if (p[i].sReg == sReg) {
            p[i].live = false;
            p[i].defStart = NULL;
            p[i].defEnd = NULL;
        }
    }
}

/* Clobber any temp associated with an sReg.  Could be in either class */
extern void dvmCompilerClobberSReg(CompilationUnit *cUnit, int sReg)
{
    clobberSRegBody(cUnit->regPool->coreTemps, cUnit->regPool->numCoreTemps,
                    sReg);
    clobberSRegBody(cUnit->regPool->FPTemps, cUnit->regPool->numFPTemps,
                    sReg);
}

static int allocTempBody(CompilationUnit *cUnit, RegisterInfo *p, int numTemps,
                         int *nextTemp, bool required)
{
    int i;
    int next = *nextTemp;
    for (i=0; i< numTemps; i++) {
        if (next >= numTemps)
            next = 0;
        if (!p[next].inUse && !p[next].live) {
            dvmCompilerClobber(cUnit, p[next].reg);
            p[next].inUse = true;
            p[next].pair = false;
            *nextTemp = next + 1;
            return p[next].reg;
        }
        next++;
    }
    next = *nextTemp;
    for (i=0; i< numTemps; i++) {
        if (next >= numTemps)
            next = 0;
        if (!p[next].inUse) {
            dvmCompilerClobber(cUnit, p[next].reg);
            p[next].inUse = true;
            p[next].pair = false;
            *nextTemp = next + 1;
            return p[next].reg;
        }
        next++;
    }
    if (required) {
        ALOGE("No free temp registers");
        dvmCompilerAbort(cUnit);
    }
    return -1;  // No register available
}

//REDO: too many assumptions.
extern int dvmCompilerAllocTempDouble(CompilationUnit *cUnit)
{
    RegisterInfo *p = cUnit->regPool->FPTemps;
    int numTemps = cUnit->regPool->numFPTemps;
    /* Cleanup - not all targets need aligned regs */
    int start = cUnit->regPool->nextFPTemp + (cUnit->regPool->nextFPTemp & 1);
    int next = start;
    int i;

    for (i=0; i < numTemps; i+=2) {
        if (next >= numTemps)
            next = 0;
        if ((!p[next].inUse && !p[next].live) &&
            (!p[next+1].inUse && !p[next+1].live)) {
            dvmCompilerClobber(cUnit, p[next].reg);
            dvmCompilerClobber(cUnit, p[next+1].reg);
            p[next].inUse = true;
            p[next+1].inUse = true;
            assert((p[next].reg+1) == p[next+1].reg);
            assert((p[next].reg & 0x1) == 0);
            cUnit->regPool->nextFPTemp += 2;
            return p[next].reg;
        }
        next += 2;
    }
    next = start;
    for (i=0; i < numTemps; i+=2) {
        if (next >= numTemps)
            next = 0;
        if (!p[next].inUse && !p[next+1].inUse) {
            dvmCompilerClobber(cUnit, p[next].reg);
            dvmCompilerClobber(cUnit, p[next+1].reg);
            p[next].inUse = true;
            p[next+1].inUse = true;
            assert((p[next].reg+1) == p[next+1].reg);
            assert((p[next].reg & 0x1) == 0);
            cUnit->regPool->nextFPTemp += 2;
            return p[next].reg;
        }
        next += 2;
    }
    ALOGE("No free temp registers");
    dvmCompilerAbort(cUnit);
    return -1;
}

/* Return a temp if one is available, -1 otherwise */
extern int dvmCompilerAllocFreeTemp(CompilationUnit *cUnit)
{
    return allocTempBody(cUnit, cUnit->regPool->coreTemps,
                         cUnit->regPool->numCoreTemps,
                         &cUnit->regPool->nextCoreTemp, true);
}

extern int dvmCompilerAllocTemp(CompilationUnit *cUnit)
{
    return allocTempBody(cUnit, cUnit->regPool->coreTemps,
                         cUnit->regPool->numCoreTemps,
                         &cUnit->regPool->nextCoreTemp, true);
}

extern int dvmCompilerAllocTempFloat(CompilationUnit *cUnit)
{
    return allocTempBody(cUnit, cUnit->regPool->FPTemps,
                         cUnit->regPool->numFPTemps,
                         &cUnit->regPool->nextFPTemp, true);
}

static RegisterInfo *allocLiveBody(RegisterInfo *p, int numTemps, int sReg)
{
    int i;
    if (sReg == -1)
        return NULL;
    for (i=0; i < numTemps; i++) {
        if (p[i].live && (p[i].sReg == sReg)) {
            p[i].inUse = true;
            return &p[i];
        }
    }
    return NULL;
}

static RegisterInfo *allocLive(CompilationUnit *cUnit, int sReg,
                               int regClass)
{
    RegisterInfo *res = NULL;
    switch(regClass) {
        case kAnyReg:
            res = allocLiveBody(cUnit->regPool->FPTemps,
                                cUnit->regPool->numFPTemps, sReg);
            if (res)
                break;
            /* Intentional fallthrough */
        case kCoreReg:
            res = allocLiveBody(cUnit->regPool->coreTemps,
                                cUnit->regPool->numCoreTemps, sReg);
            break;
        case kFPReg:
            res = allocLiveBody(cUnit->regPool->FPTemps,
                                cUnit->regPool->numFPTemps, sReg);
            break;
        default:
            ALOGE("Invalid register type");
            dvmCompilerAbort(cUnit);
    }
    return res;
}

extern void dvmCompilerFreeTemp(CompilationUnit *cUnit, int reg)
{
    RegisterInfo *p = cUnit->regPool->coreTemps;
    int numTemps = cUnit->regPool->numCoreTemps;
    int i;
    for (i=0; i< numTemps; i++) {
        if (p[i].reg == reg) {
            p[i].inUse = false;
            p[i].pair = false;
            return;
        }
    }
    p = cUnit->regPool->FPTemps;
    numTemps = cUnit->regPool->numFPTemps;
    for (i=0; i< numTemps; i++) {
        if (p[i].reg == reg) {
            p[i].inUse = false;
            p[i].pair = false;
            return;
        }
    }
    ALOGE("Tried to free a non-existant temp: r%d",reg);
    dvmCompilerAbort(cUnit);
}

/*
 * FIXME - this needs to also check the preserved pool once we start
 * start using preserved registers.
 */
extern RegisterInfo *dvmCompilerIsLive(CompilationUnit *cUnit, int reg)
{
    RegisterInfo *p = cUnit->regPool->coreTemps;
    int numTemps = cUnit->regPool->numCoreTemps;
    int i;
    for (i=0; i< numTemps; i++) {
        if (p[i].reg == reg) {
            return p[i].live ? &p[i] : NULL;
        }
    }
    p = cUnit->regPool->FPTemps;
    numTemps = cUnit->regPool->numFPTemps;
    for (i=0; i< numTemps; i++) {
        if (p[i].reg == reg) {
            return p[i].live ? &p[i] : NULL;
        }
    }
    return NULL;
}

extern RegisterInfo *dvmCompilerIsTemp(CompilationUnit *cUnit, int reg)
{
    RegisterInfo *p = cUnit->regPool->coreTemps;
    int numTemps = cUnit->regPool->numCoreTemps;
    int i;
    for (i=0; i< numTemps; i++) {
        if (p[i].reg == reg) {
            return &p[i];
        }
    }
    p = cUnit->regPool->FPTemps;
    numTemps = cUnit->regPool->numFPTemps;
    for (i=0; i< numTemps; i++) {
        if (p[i].reg == reg) {
            return &p[i];
        }
    }
    return NULL;
}

/*
 * Similar to dvmCompilerAllocTemp(), but forces the allocation of a specific
 * register.  No check is made to see if the register was previously
 * allocated.  Use with caution.
 */
extern void dvmCompilerLockTemp(CompilationUnit *cUnit, int reg)
{
    RegisterInfo *p = cUnit->regPool->coreTemps;
    int numTemps = cUnit->regPool->numCoreTemps;
    int i;
    for (i=0; i< numTemps; i++) {
        if (p[i].reg == reg) {
            p[i].inUse = true;
            p[i].live = false;
            return;
        }
    }
    p = cUnit->regPool->FPTemps;
    numTemps = cUnit->regPool->numFPTemps;
    for (i=0; i< numTemps; i++) {
        if (p[i].reg == reg) {
            p[i].inUse = true;
            p[i].live = false;
            return;
        }
    }
    ALOGE("Tried to lock a non-existant temp: r%d",reg);
    dvmCompilerAbort(cUnit);
}

/* Clobber all regs that might be used by an external C call */
extern void dvmCompilerClobberCallRegs(CompilationUnit *cUnit)
{
    dvmCompilerClobber(cUnit, r_ZERO);
    dvmCompilerClobber(cUnit, r_AT);
    dvmCompilerClobber(cUnit, r_V0);
    dvmCompilerClobber(cUnit, r_V1);
    dvmCompilerClobber(cUnit, r_A0);
    dvmCompilerClobber(cUnit, r_A1);
    dvmCompilerClobber(cUnit, r_A2);
    dvmCompilerClobber(cUnit, r_A3);
    dvmCompilerClobber(cUnit, r_T0);
    dvmCompilerClobber(cUnit, r_T1);
    dvmCompilerClobber(cUnit, r_T2);
    dvmCompilerClobber(cUnit, r_T3);
    dvmCompilerClobber(cUnit, r_T4);
    dvmCompilerClobber(cUnit, r_T5);
    dvmCompilerClobber(cUnit, r_T6);
    dvmCompilerClobber(cUnit, r_T7);
    dvmCompilerClobber(cUnit, r_T8);
    dvmCompilerClobber(cUnit, r_T9);
    dvmCompilerClobber(cUnit, r_K0);
    dvmCompilerClobber(cUnit, r_K1);
    dvmCompilerClobber(cUnit, r_GP);
    dvmCompilerClobber(cUnit, r_FP);
    dvmCompilerClobber(cUnit, r_RA);
    dvmCompilerClobber(cUnit, r_HI);
    dvmCompilerClobber(cUnit, r_LO);
    dvmCompilerClobber(cUnit, r_F0);
    dvmCompilerClobber(cUnit, r_F1);
    dvmCompilerClobber(cUnit, r_F2);
    dvmCompilerClobber(cUnit, r_F3);
    dvmCompilerClobber(cUnit, r_F4);
    dvmCompilerClobber(cUnit, r_F5);
    dvmCompilerClobber(cUnit, r_F6);
    dvmCompilerClobber(cUnit, r_F7);
    dvmCompilerClobber(cUnit, r_F8);
    dvmCompilerClobber(cUnit, r_F9);
    dvmCompilerClobber(cUnit, r_F10);
    dvmCompilerClobber(cUnit, r_F11);
    dvmCompilerClobber(cUnit, r_F12);
    dvmCompilerClobber(cUnit, r_F13);
    dvmCompilerClobber(cUnit, r_F14);
    dvmCompilerClobber(cUnit, r_F15);
}

/* Clobber all of the temps that might be used by a handler. */
extern void dvmCompilerClobberHandlerRegs(CompilationUnit *cUnit)
{
    //TUNING: reduce the set of regs used by handlers.  Only a few need lots.
    dvmCompilerClobberCallRegs(cUnit);
    dvmCompilerClobber(cUnit, r_S0);
    dvmCompilerClobber(cUnit, r_S1);
    dvmCompilerClobber(cUnit, r_S2);
    dvmCompilerClobber(cUnit, r_S3);
    dvmCompilerClobber(cUnit, r_S4);
    dvmCompilerClobber(cUnit, r_S5);
    dvmCompilerClobber(cUnit, r_S6);
    dvmCompilerClobber(cUnit, r_S7);
}

extern void dvmCompilerResetDef(CompilationUnit *cUnit, int reg)
{
    RegisterInfo *p = getRegInfo(cUnit, reg);
    p->defStart = NULL;
    p->defEnd = NULL;
}

static void nullifyRange(CompilationUnit *cUnit, LIR *start, LIR *finish,
                         int sReg1, int sReg2)
{
    if (start && finish) {
        LIR *p;
        assert(sReg1 == sReg2);
        for (p = start; ;p = p->next) {
            ((MipsLIR *)p)->flags.isNop = true;
            if (p == finish)
                break;
        }
    }
}

/*
 * Mark the beginning and end LIR of a def sequence.  Note that
 * on entry start points to the LIR prior to the beginning of the
 * sequence.
 */
extern void dvmCompilerMarkDef(CompilationUnit *cUnit, RegLocation rl,
                    LIR *start, LIR *finish)
{
    assert(!rl.wide);
    assert(start && start->next);
    assert(finish);
    RegisterInfo *p = getRegInfo(cUnit, rl.lowReg);
    p->defStart = start->next;
    p->defEnd = finish;
}

/*
 * Mark the beginning and end LIR of a def sequence.  Note that
 * on entry start points to the LIR prior to the beginning of the
 * sequence.
 */
extern void dvmCompilerMarkDefWide(CompilationUnit *cUnit, RegLocation rl,
                        LIR *start, LIR *finish)
{
    assert(rl.wide);
    assert(start && start->next);
    assert(finish);
    RegisterInfo *p = getRegInfo(cUnit, rl.lowReg);
    dvmCompilerResetDef(cUnit, rl.highReg);  // Only track low of pair
    p->defStart = start->next;
    p->defEnd = finish;
}

extern RegLocation dvmCompilerWideToNarrow(CompilationUnit *cUnit,
                                           RegLocation rl)
{
    assert(rl.wide);
    if (rl.location == kLocPhysReg) {
        RegisterInfo *infoLo = getRegInfo(cUnit, rl.lowReg);
        RegisterInfo *infoHi = getRegInfo(cUnit, rl.highReg);
        if (!infoLo->pair) {
            dumpRegPool(cUnit->regPool->coreTemps,
                        cUnit->regPool->numCoreTemps);
            assert(infoLo->pair);
        }
        if (!infoHi->pair) {
            dumpRegPool(cUnit->regPool->coreTemps,
                        cUnit->regPool->numCoreTemps);
            assert(infoHi->pair);
        }
        assert(infoLo->pair);
        assert(infoHi->pair);
        assert(infoLo->partner == infoHi->reg);
        assert(infoHi->partner == infoLo->reg);
        infoLo->pair = false;
        infoHi->pair = false;
        infoLo->defStart = NULL;
        infoLo->defEnd = NULL;
        infoHi->defStart = NULL;
        infoHi->defEnd = NULL;
    }
#ifndef HAVE_LITTLE_ENDIAN
    else if (rl.location == kLocDalvikFrame) {
        rl.sRegLow = dvmCompilerSRegHi(rl.sRegLow);
    }
#endif

    rl.wide = false;
    return rl;
}

extern void dvmCompilerResetDefLoc(CompilationUnit *cUnit, RegLocation rl)
{
    assert(!rl.wide);
    if (!(gDvmJit.disableOpt & (1 << kSuppressLoads))) {
        RegisterInfo *p = getRegInfo(cUnit, rl.lowReg);
        assert(!p->pair);
        nullifyRange(cUnit, p->defStart, p->defEnd,
                     p->sReg, rl.sRegLow);
    }
    dvmCompilerResetDef(cUnit, rl.lowReg);
}

extern void dvmCompilerResetDefLocWide(CompilationUnit *cUnit, RegLocation rl)
{
    assert(rl.wide);
    if (!(gDvmJit.disableOpt & (1 << kSuppressLoads))) {
        RegisterInfo *p = getRegInfo(cUnit, rl.lowReg);
        assert(p->pair);
        nullifyRange(cUnit, p->defStart, p->defEnd,
                     p->sReg, rl.sRegLow);
    }
    dvmCompilerResetDef(cUnit, rl.lowReg);
    dvmCompilerResetDef(cUnit, rl.highReg);
}

extern void dvmCompilerResetDefTracking(CompilationUnit *cUnit)
{
    int i;
    for (i=0; i< cUnit->regPool->numCoreTemps; i++) {
        dvmCompilerResetDef(cUnit, cUnit->regPool->coreTemps[i].reg);
    }
    for (i=0; i< cUnit->regPool->numFPTemps; i++) {
        dvmCompilerResetDef(cUnit, cUnit->regPool->FPTemps[i].reg);
    }
}

extern void dvmCompilerClobberAllRegs(CompilationUnit *cUnit)
{
    int i;
    for (i=0; i< cUnit->regPool->numCoreTemps; i++) {
        dvmCompilerClobber(cUnit, cUnit->regPool->coreTemps[i].reg);
    }
    for (i=0; i< cUnit->regPool->numFPTemps; i++) {
        dvmCompilerClobber(cUnit, cUnit->regPool->FPTemps[i].reg);
    }
}

/* To be used when explicitly managing register use */
extern void dvmCompilerLockAllTemps(CompilationUnit *cUnit)
{
    int i;
    for (i=0; i< cUnit->regPool->numCoreTemps; i++) {
        dvmCompilerLockTemp(cUnit, cUnit->regPool->coreTemps[i].reg);
    }
}

// Make sure nothing is live and dirty
static void flushAllRegsBody(CompilationUnit *cUnit, RegisterInfo *info,
                             int numRegs)
{
    int i;
    for (i=0; i < numRegs; i++) {
        if (info[i].live && info[i].dirty) {
            if (info[i].pair) {
                flushRegWide(cUnit, info[i].reg, info[i].partner);
            } else {
                flushReg(cUnit, info[i].reg);
            }
        }
    }
}

extern void dvmCompilerFlushAllRegs(CompilationUnit *cUnit)
{
    flushAllRegsBody(cUnit, cUnit->regPool->coreTemps,
                     cUnit->regPool->numCoreTemps);
    flushAllRegsBody(cUnit, cUnit->regPool->FPTemps,
                     cUnit->regPool->numFPTemps);
    dvmCompilerClobberAllRegs(cUnit);
}


//TUNING: rewrite all of this reg stuff.  Probably use an attribute table
static bool regClassMatches(int regClass, int reg)
{
    if (regClass == kAnyReg) {
        return true;
    } else if (regClass == kCoreReg) {
        return !FPREG(reg);
    } else {
        return FPREG(reg);
    }
}

extern void dvmCompilerMarkLive(CompilationUnit *cUnit, int reg, int sReg)
{
    RegisterInfo *info = getRegInfo(cUnit, reg);
    if ((info->reg == reg) && (info->sReg == sReg) && info->live) {
        return;  /* already live */
    } else if (sReg != INVALID_SREG) {
        dvmCompilerClobberSReg(cUnit, sReg);
        info->live = true;
    } else {
        /* Can't be live if no associated sReg */
        info->live = false;
    }
    info->sReg = sReg;
}

extern void dvmCompilerMarkPair(CompilationUnit *cUnit, int lowReg, int highReg)
{
    RegisterInfo *infoLo = getRegInfo(cUnit, lowReg);
    RegisterInfo *infoHi = getRegInfo(cUnit, highReg);
    infoLo->pair = infoHi->pair = true;
    infoLo->partner = highReg;
    infoHi->partner = lowReg;
}

extern void dvmCompilerMarkClean(CompilationUnit *cUnit, int reg)
{
    RegisterInfo *info = getRegInfo(cUnit, reg);
    info->dirty = false;
}

extern void dvmCompilerMarkDirty(CompilationUnit *cUnit, int reg)
{
    RegisterInfo *info = getRegInfo(cUnit, reg);
    info->dirty = true;
}

extern void dvmCompilerMarkInUse(CompilationUnit *cUnit, int reg)
{
      RegisterInfo *info = getRegInfo(cUnit, reg);
          info->inUse = true;
}

void copyRegInfo(CompilationUnit *cUnit, int newReg, int oldReg)
{
    RegisterInfo *newInfo = getRegInfo(cUnit, newReg);
    RegisterInfo *oldInfo = getRegInfo(cUnit, oldReg);
    *newInfo = *oldInfo;
    newInfo->reg = newReg;
}

/*
 * Return an updated location record with current in-register status.
 * If the value lives in live temps, reflect that fact.  No code
 * is generated.  The the live value is part of an older pair,
 * clobber both low and high.
 * TUNING: clobbering both is a bit heavy-handed, but the alternative
 * is a bit complex when dealing with FP regs.  Examine code to see
 * if it's worthwhile trying to be more clever here.
 */
extern RegLocation dvmCompilerUpdateLoc(CompilationUnit *cUnit, RegLocation loc)
{
    assert(!loc.wide);
    if (loc.location == kLocDalvikFrame) {
        RegisterInfo *infoLo = allocLive(cUnit, loc.sRegLow, kAnyReg);
        if (infoLo) {
            if (infoLo->pair) {
                dvmCompilerClobber(cUnit, infoLo->reg);
                dvmCompilerClobber(cUnit, infoLo->partner);
            } else {
                loc.lowReg = infoLo->reg;
                loc.location = kLocPhysReg;
            }
        }
    }

    return loc;
}

/* see comments for updateLoc */
extern RegLocation dvmCompilerUpdateLocWide(CompilationUnit *cUnit,
                                            RegLocation loc)
{
    assert(loc.wide);
    if (loc.location == kLocDalvikFrame) {
        // Are the dalvik regs already live in physical registers?
        RegisterInfo *infoLo = allocLive(cUnit, loc.sRegLow, kAnyReg);
        RegisterInfo *infoHi = allocLive(cUnit,
              dvmCompilerSRegHi(loc.sRegLow), kAnyReg);
        bool match = true;
        match = match && (infoLo != NULL);
        match = match && (infoHi != NULL);
        // Are they both core or both FP?
        match = match && (FPREG(infoLo->reg) == FPREG(infoHi->reg));
        // If a pair of floating point singles, are they properly aligned?
        if (match && FPREG(infoLo->reg)) {
            match &= ((infoLo->reg & 0x1) == 0);
            match &= ((infoHi->reg - infoLo->reg) == 1);
        }
        // If previously used as a pair, it is the same pair?
        if (match && (infoLo->pair || infoHi->pair)) {
            match = (infoLo->pair == infoHi->pair);
            match &= ((infoLo->reg == infoHi->partner) &&
                      (infoHi->reg == infoLo->partner));
        }
        if (match) {
            // Can reuse - update the register usage info
            loc.lowReg = infoLo->reg;
            loc.highReg = infoHi->reg;
            loc.location = kLocPhysReg;
            dvmCompilerMarkPair(cUnit, loc.lowReg, loc.highReg);
            assert(!FPREG(loc.lowReg) || ((loc.lowReg & 0x1) == 0));
            return loc;
        }
        // Can't easily reuse - clobber any overlaps
        if (infoLo) {
            dvmCompilerClobber(cUnit, infoLo->reg);
            if (infoLo->pair)
                dvmCompilerClobber(cUnit, infoLo->partner);
        }
        if (infoHi) {
            dvmCompilerClobber(cUnit, infoHi->reg);
            if (infoHi->pair)
                dvmCompilerClobber(cUnit, infoHi->partner);
        }
    }

    return loc;
}

static RegLocation evalLocWide(CompilationUnit *cUnit, RegLocation loc,
                               int regClass, bool update)
{
    assert(loc.wide);
    int newRegs;
    int lowReg;
    int highReg;

    loc = dvmCompilerUpdateLocWide(cUnit, loc);

    /* If already in registers, we can assume proper form.  Right reg class? */
    if (loc.location == kLocPhysReg) {
        assert(FPREG(loc.lowReg) == FPREG(loc.highReg));
        assert(!FPREG(loc.lowReg) || ((loc.lowReg & 0x1) == 0));
        if (!regClassMatches(regClass, loc.lowReg)) {
            /* Wrong register class.  Reallocate and copy */
            newRegs = dvmCompilerAllocTypedTempPair(cUnit, loc.fp, regClass);
            lowReg = newRegs & 0xff;
            highReg = (newRegs >> 8) & 0xff;
            dvmCompilerRegCopyWide(cUnit, lowReg, highReg, loc.lowReg,
                                   loc.highReg);
            copyRegInfo(cUnit, lowReg, loc.lowReg);
            copyRegInfo(cUnit, highReg, loc.highReg);
            dvmCompilerClobber(cUnit, loc.lowReg);
            dvmCompilerClobber(cUnit, loc.highReg);
            loc.lowReg = lowReg;
            loc.highReg = highReg;
            dvmCompilerMarkPair(cUnit, loc.lowReg, loc.highReg);
            assert(!FPREG(loc.lowReg) || ((loc.lowReg & 0x1) == 0));
        }
        return loc;
    }

    assert((loc.location != kLocRetval) || (loc.sRegLow == INVALID_SREG));
    assert((loc.location != kLocRetval) ||
           (dvmCompilerSRegHi(loc.sRegLow) == INVALID_SREG));

    newRegs = dvmCompilerAllocTypedTempPair(cUnit, loc.fp, regClass);
    loc.lowReg = newRegs & 0xff;
    loc.highReg = (newRegs >> 8) & 0xff;

    dvmCompilerMarkPair(cUnit, loc.lowReg, loc.highReg);
    if (update) {
        loc.location = kLocPhysReg;
        dvmCompilerMarkLive(cUnit, loc.lowReg, loc.sRegLow);
        dvmCompilerMarkLive(cUnit, loc.highReg, dvmCompilerSRegHi(loc.sRegLow));
    }
    assert(!FPREG(loc.lowReg) || ((loc.lowReg & 0x1) == 0));
    return loc;
}

extern RegLocation dvmCompilerEvalLoc(CompilationUnit *cUnit, RegLocation loc,
                                      int regClass, bool update)
{
    int newReg;
    if (loc.wide)
        return evalLocWide(cUnit, loc, regClass, update);
    loc = dvmCompilerUpdateLoc(cUnit, loc);

    if (loc.location == kLocPhysReg) {
        if (!regClassMatches(regClass, loc.lowReg)) {
            /* Wrong register class.  Realloc, copy and transfer ownership */
            newReg = dvmCompilerAllocTypedTemp(cUnit, loc.fp, regClass);
            dvmCompilerRegCopy(cUnit, newReg, loc.lowReg);
            copyRegInfo(cUnit, newReg, loc.lowReg);
            dvmCompilerClobber(cUnit, loc.lowReg);
            loc.lowReg = newReg;
        }
        return loc;
    }

    assert((loc.location != kLocRetval) || (loc.sRegLow == INVALID_SREG));

    newReg = dvmCompilerAllocTypedTemp(cUnit, loc.fp, regClass);
    loc.lowReg = newReg;

    if (update) {
        loc.location = kLocPhysReg;
        dvmCompilerMarkLive(cUnit, loc.lowReg, loc.sRegLow);
    }
    return loc;
}

static inline int getDestSSAName(MIR *mir, int num)
{
    assert(mir->ssaRep->numDefs > num);
    return mir->ssaRep->defs[num];
}

// Get the LocRecord associated with an SSA name use.
extern RegLocation dvmCompilerGetSrc(CompilationUnit *cUnit, MIR *mir, int num)
{
    RegLocation loc = cUnit->regLocation[
         SREG(cUnit, dvmCompilerSSASrc(mir, num))];
    loc.fp = cUnit->regLocation[dvmCompilerSSASrc(mir, num)].fp;
    loc.wide = false;
    return loc;
}

// Get the LocRecord associated with an SSA name def.
extern RegLocation dvmCompilerGetDest(CompilationUnit *cUnit, MIR *mir,
                                      int num)
{
    RegLocation loc = cUnit->regLocation[SREG(cUnit, getDestSSAName(mir, num))];
    loc.fp = cUnit->regLocation[getDestSSAName(mir, num)].fp;
    loc.wide = false;
    return loc;
}

static RegLocation getLocWide(CompilationUnit *cUnit, MIR *mir,
                              int low, int high, bool isSrc)
{
    RegLocation lowLoc;
    RegLocation highLoc;
    /* Copy loc record for low word and patch in data from high word */
    if (isSrc) {
        lowLoc = dvmCompilerGetSrc(cUnit, mir, low);
        highLoc = dvmCompilerGetSrc(cUnit, mir, high);
    } else {
        lowLoc = dvmCompilerGetDest(cUnit, mir, low);
        highLoc = dvmCompilerGetDest(cUnit, mir, high);
    }
    /* Avoid this case by either promoting both or neither. */
    assert(lowLoc.location == highLoc.location);
    if (lowLoc.location == kLocPhysReg) {
        /* This case shouldn't happen if we've named correctly */
        assert(lowLoc.fp == highLoc.fp);
    }
    lowLoc.wide = true;
    lowLoc.highReg = highLoc.lowReg;
    return lowLoc;
}

extern RegLocation dvmCompilerGetDestWide(CompilationUnit *cUnit, MIR *mir,
                                          int low, int high)
{
    return getLocWide(cUnit, mir, low, high, false);
}

extern RegLocation dvmCompilerGetSrcWide(CompilationUnit *cUnit, MIR *mir,
                                         int low, int high)
{
    return getLocWide(cUnit, mir, low, high, true);
}

extern RegLocation dvmCompilerGetReturnWide(CompilationUnit *cUnit)
{
    RegLocation res = LOC_C_RETURN_WIDE;
    dvmCompilerClobber(cUnit, r_V0);
    dvmCompilerClobber(cUnit, r_V1);
    dvmCompilerMarkInUse(cUnit, r_V0);
    dvmCompilerMarkInUse(cUnit, r_V1);
    dvmCompilerMarkPair(cUnit, res.lowReg, res.highReg);
    return res;
}

extern RegLocation dvmCompilerGetReturn(CompilationUnit *cUnit)
{
    RegLocation res = LOC_C_RETURN;
    dvmCompilerClobber(cUnit, r_V0);
    dvmCompilerMarkInUse(cUnit, r_V0);
    return res;
}

extern RegLocation dvmCompilerGetReturnWideAlt(CompilationUnit *cUnit)
{
    RegLocation res = LOC_C_RETURN_WIDE_ALT;
    dvmCompilerClobber(cUnit, r_F0);
    dvmCompilerClobber(cUnit, r_F1);
    dvmCompilerMarkInUse(cUnit, r_F0);
    dvmCompilerMarkInUse(cUnit, r_F1);
    dvmCompilerMarkPair(cUnit, res.lowReg, res.highReg);
    return res;
}

extern RegLocation dvmCompilerGetReturnAlt(CompilationUnit *cUnit)
{
    RegLocation res = LOC_C_RETURN_ALT;
    dvmCompilerClobber(cUnit, r_F0);
    dvmCompilerMarkInUse(cUnit, r_F0);
    return res;
}

/* Kill the corresponding bit in the null-checked register list */
extern void dvmCompilerKillNullCheckedLoc(CompilationUnit *cUnit,
                                          RegLocation loc)
{
    if (loc.location != kLocRetval) {
        assert(loc.sRegLow != INVALID_SREG);
        dvmClearBit(cUnit->regPool->nullCheckedRegs, loc.sRegLow);
        if (loc.wide) {
            assert(dvmCompilerSRegHi(loc.sRegLow) != INVALID_SREG);
            dvmClearBit(cUnit->regPool->nullCheckedRegs,
                        dvmCompilerSRegHi(loc.sRegLow));
        }
    }
}

extern void dvmCompilerFlushRegWideForV5TEVFP(CompilationUnit *cUnit,
                                              int reg1, int reg2)
{
    flushRegWide(cUnit, reg1, reg2);
}

extern void dvmCompilerFlushRegForV5TEVFP(CompilationUnit *cUnit, int reg)
{
    flushReg(cUnit, reg);
}
