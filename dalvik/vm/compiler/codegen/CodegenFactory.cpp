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
 * This file contains target-independent codegen and support, and is
 * included by:
 *
 *        $(TARGET_ARCH)/Codegen-$(TARGET_ARCH_VARIANT).c
 *
 * which combines this common code with specific support found in the
 * applicable directories below this one.
 *
 * Prior to including this file, TGT_LIR should be #defined.
 * For example, for arm:
 *    #define TGT_LIR ArmLIR
 * and for x86:
 *    #define TGT_LIR X86LIR
 */


/* Load a word at base + displacement.  Displacement must be word multiple */
static TGT_LIR *loadWordDisp(CompilationUnit *cUnit, int rBase,
                             int displacement, int rDest)
{
    return loadBaseDisp(cUnit, NULL, rBase, displacement, rDest, kWord,
                        INVALID_SREG);
}

static TGT_LIR *storeWordDisp(CompilationUnit *cUnit, int rBase,
                             int displacement, int rSrc)
{
    return storeBaseDisp(cUnit, rBase, displacement, rSrc, kWord);
}

/*
 * Load a Dalvik register into a physical register.  Take care when
 * using this routine, as it doesn't perform any bookkeeping regarding
 * register liveness.  That is the responsibility of the caller.
 */
static void loadValueDirect(CompilationUnit *cUnit, RegLocation rlSrc,
                            int reg1)
{
    rlSrc = dvmCompilerUpdateLoc(cUnit, rlSrc);
    if (rlSrc.location == kLocPhysReg) {
        genRegCopy(cUnit, reg1, rlSrc.lowReg);
    } else  if (rlSrc.location == kLocRetval) {
        loadWordDisp(cUnit, rSELF, offsetof(Thread, interpSave.retval), reg1);
    } else {
        assert(rlSrc.location == kLocDalvikFrame);
        loadWordDisp(cUnit, rFP, dvmCompilerS2VReg(cUnit, rlSrc.sRegLow) << 2,
                     reg1);
    }
}

/*
 * Similar to loadValueDirect, but clobbers and allocates the target
 * register.  Should be used when loading to a fixed register (for example,
 * loading arguments to an out of line call.
 */
static void loadValueDirectFixed(CompilationUnit *cUnit, RegLocation rlSrc,
                                 int reg1)
{
    dvmCompilerClobber(cUnit, reg1);
    dvmCompilerMarkInUse(cUnit, reg1);
    loadValueDirect(cUnit, rlSrc, reg1);
}

/*
 * Load a Dalvik register pair into a physical register[s].  Take care when
 * using this routine, as it doesn't perform any bookkeeping regarding
 * register liveness.  That is the responsibility of the caller.
 */
static void loadValueDirectWide(CompilationUnit *cUnit, RegLocation rlSrc,
                                int regLo, int regHi)
{
    rlSrc = dvmCompilerUpdateLocWide(cUnit, rlSrc);
    if (rlSrc.location == kLocPhysReg) {
        genRegCopyWide(cUnit, regLo, regHi, rlSrc.lowReg, rlSrc.highReg);
    } else if (rlSrc.location == kLocRetval) {
        loadBaseDispWide(cUnit, NULL, rSELF,
                         offsetof(Thread, interpSave.retval),
                         regLo, regHi, INVALID_SREG);
    } else {
        assert(rlSrc.location == kLocDalvikFrame);
            loadBaseDispWide(cUnit, NULL, rFP,
                             dvmCompilerS2VReg(cUnit, rlSrc.sRegLow) << 2,
                             regLo, regHi, INVALID_SREG);
    }
}

/*
 * Similar to loadValueDirect, but clobbers and allocates the target
 * registers.  Should be used when loading to a fixed registers (for example,
 * loading arguments to an out of line call.
 */
static void loadValueDirectWideFixed(CompilationUnit *cUnit, RegLocation rlSrc,
                                     int regLo, int regHi)
{
    dvmCompilerClobber(cUnit, regLo);
    dvmCompilerClobber(cUnit, regHi);
    dvmCompilerMarkInUse(cUnit, regLo);
    dvmCompilerMarkInUse(cUnit, regHi);
    loadValueDirectWide(cUnit, rlSrc, regLo, regHi);
}

static RegLocation loadValue(CompilationUnit *cUnit, RegLocation rlSrc,
                             RegisterClass opKind)
{
    rlSrc = dvmCompilerEvalLoc(cUnit, rlSrc, opKind, false);
    if (rlSrc.location == kLocDalvikFrame) {
        loadValueDirect(cUnit, rlSrc, rlSrc.lowReg);
        rlSrc.location = kLocPhysReg;
        dvmCompilerMarkLive(cUnit, rlSrc.lowReg, rlSrc.sRegLow);
    } else if (rlSrc.location == kLocRetval) {
        loadWordDisp(cUnit, rSELF, offsetof(Thread, interpSave.retval),
                     rlSrc.lowReg);
        rlSrc.location = kLocPhysReg;
        dvmCompilerClobber(cUnit, rlSrc.lowReg);
    }
    return rlSrc;
}

static void storeValue(CompilationUnit *cUnit, RegLocation rlDest,
                       RegLocation rlSrc)
{
    LIR *defStart;
    LIR *defEnd;
    assert(!rlDest.wide);
    assert(!rlSrc.wide);
    dvmCompilerKillNullCheckedLoc(cUnit, rlDest);
    rlSrc = dvmCompilerUpdateLoc(cUnit, rlSrc);
    rlDest = dvmCompilerUpdateLoc(cUnit, rlDest);
    if (rlSrc.location == kLocPhysReg) {
        if (dvmCompilerIsLive(cUnit, rlSrc.lowReg) ||
            (rlDest.location == kLocPhysReg)) {
            // Src is live or Dest has assigned reg.
            rlDest = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, false);
            genRegCopy(cUnit, rlDest.lowReg, rlSrc.lowReg);
        } else {
            // Just re-assign the registers.  Dest gets Src's regs
            rlDest.lowReg = rlSrc.lowReg;
            dvmCompilerClobber(cUnit, rlSrc.lowReg);
        }
    } else {
        // Load Src either into promoted Dest or temps allocated for Dest
        rlDest = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, false);
        loadValueDirect(cUnit, rlSrc, rlDest.lowReg);
    }

    // Dest is now live and dirty (until/if we flush it to home location)
    dvmCompilerMarkLive(cUnit, rlDest.lowReg, rlDest.sRegLow);
    dvmCompilerMarkDirty(cUnit, rlDest.lowReg);


    if (rlDest.location == kLocRetval) {
        storeBaseDisp(cUnit, rSELF, offsetof(Thread, interpSave.retval),
                      rlDest.lowReg, kWord);
        dvmCompilerClobber(cUnit, rlDest.lowReg);
    } else {
        dvmCompilerResetDefLoc(cUnit, rlDest);
        if (dvmCompilerLiveOut(cUnit, rlDest.sRegLow)) {
            defStart = (LIR *)cUnit->lastLIRInsn;
            int vReg = dvmCompilerS2VReg(cUnit, rlDest.sRegLow);
            storeBaseDisp(cUnit, rFP, vReg << 2, rlDest.lowReg, kWord);
            dvmCompilerMarkClean(cUnit, rlDest.lowReg);
            defEnd = (LIR *)cUnit->lastLIRInsn;
            dvmCompilerMarkDef(cUnit, rlDest, defStart, defEnd);
        }
    }
}

static RegLocation loadValueWide(CompilationUnit *cUnit, RegLocation rlSrc,
                                 RegisterClass opKind)
{
    assert(rlSrc.wide);
    rlSrc = dvmCompilerEvalLoc(cUnit, rlSrc, opKind, false);
    if (rlSrc.location == kLocDalvikFrame) {
        loadValueDirectWide(cUnit, rlSrc, rlSrc.lowReg, rlSrc.highReg);
        rlSrc.location = kLocPhysReg;
        dvmCompilerMarkLive(cUnit, rlSrc.lowReg, rlSrc.sRegLow);
        dvmCompilerMarkLive(cUnit, rlSrc.highReg,
                            dvmCompilerSRegHi(rlSrc.sRegLow));
    } else if (rlSrc.location == kLocRetval) {
        loadBaseDispWide(cUnit, NULL, rSELF,
                         offsetof(Thread, interpSave.retval),
                         rlSrc.lowReg, rlSrc.highReg, INVALID_SREG);
        rlSrc.location = kLocPhysReg;
        dvmCompilerClobber(cUnit, rlSrc.lowReg);
        dvmCompilerClobber(cUnit, rlSrc.highReg);
    }
    return rlSrc;
}

static void storeValueWide(CompilationUnit *cUnit, RegLocation rlDest,
                           RegLocation rlSrc)
{
    LIR *defStart;
    LIR *defEnd;
    assert(FPREG(rlSrc.lowReg)==FPREG(rlSrc.highReg));
    assert(rlDest.wide);
    assert(rlSrc.wide);
    dvmCompilerKillNullCheckedLoc(cUnit, rlDest);
    if (rlSrc.location == kLocPhysReg) {
        if (dvmCompilerIsLive(cUnit, rlSrc.lowReg) ||
            dvmCompilerIsLive(cUnit, rlSrc.highReg) ||
            (rlDest.location == kLocPhysReg)) {
            // Src is live or Dest has assigned reg.
            rlDest = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, false);
            genRegCopyWide(cUnit, rlDest.lowReg, rlDest.highReg,
                           rlSrc.lowReg, rlSrc.highReg);
        } else {
            // Just re-assign the registers.  Dest gets Src's regs
            rlDest.lowReg = rlSrc.lowReg;
            rlDest.highReg = rlSrc.highReg;
            dvmCompilerClobber(cUnit, rlSrc.lowReg);
            dvmCompilerClobber(cUnit, rlSrc.highReg);
        }
    } else {
        // Load Src either into promoted Dest or temps allocated for Dest
        rlDest = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, false);
        loadValueDirectWide(cUnit, rlSrc, rlDest.lowReg,
                            rlDest.highReg);
    }

    // Dest is now live and dirty (until/if we flush it to home location)
    dvmCompilerMarkLive(cUnit, rlDest.lowReg, rlDest.sRegLow);
    dvmCompilerMarkLive(cUnit, rlDest.highReg,
                        dvmCompilerSRegHi(rlDest.sRegLow));
    dvmCompilerMarkDirty(cUnit, rlDest.lowReg);
    dvmCompilerMarkDirty(cUnit, rlDest.highReg);
    dvmCompilerMarkPair(cUnit, rlDest.lowReg, rlDest.highReg);


    if (rlDest.location == kLocRetval) {
        storeBaseDispWide(cUnit, rSELF, offsetof(Thread, interpSave.retval),
                          rlDest.lowReg, rlDest.highReg);
        dvmCompilerClobber(cUnit, rlDest.lowReg);
        dvmCompilerClobber(cUnit, rlDest.highReg);
    } else {
        dvmCompilerResetDefLocWide(cUnit, rlDest);
        if (dvmCompilerLiveOut(cUnit, rlDest.sRegLow) ||
            dvmCompilerLiveOut(cUnit, dvmCompilerSRegHi(rlDest.sRegLow))) {
            defStart = (LIR *)cUnit->lastLIRInsn;
            int vReg = dvmCompilerS2VReg(cUnit, rlDest.sRegLow);
            assert((vReg+1) == dvmCompilerS2VReg(cUnit,
                                     dvmCompilerSRegHi(rlDest.sRegLow)));
            storeBaseDispWide(cUnit, rFP, vReg << 2, rlDest.lowReg,
                              rlDest.highReg);
            dvmCompilerMarkClean(cUnit, rlDest.lowReg);
            dvmCompilerMarkClean(cUnit, rlDest.highReg);
            defEnd = (LIR *)cUnit->lastLIRInsn;
            dvmCompilerMarkDefWide(cUnit, rlDest, defStart, defEnd);
        }
    }
}
