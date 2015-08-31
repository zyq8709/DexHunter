/*
 * Copyright (C) 2008 The Android Open Source Project
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
 * Dalvik bytecode structural verifier.  The only public entry point
 * (except for a few shared utility functions) is dvmVerifyCodeFlow().
 *
 * TODO: might benefit from a signature-->class lookup cache.  Could avoid
 * some string-peeling and wouldn't need to compute hashes.
 */
#include "Dalvik.h"
#include "analysis/Liveness.h"
#include "analysis/CodeVerify.h"
#include "analysis/Optimize.h"
#include "analysis/RegisterMap.h"
#include "libdex/DexCatch.h"
#include "libdex/InstrUtils.h"

#include <stddef.h>


/*
 * We don't need to store the register data for many instructions, because
 * we either only need it at branch points (for verification) or GC points
 * and branches (for verification + type-precise register analysis).
 */
enum RegisterTrackingMode {
    kTrackRegsBranches,
    kTrackRegsGcPoints,
    kTrackRegsAll
};

/*
 * Set this to enable dead code scanning.  This is not required, but it's
 * very useful when testing changes to the verifier (to make sure we're not
 * skipping over stuff) and for checking the optimized output from "dx".
 * The only reason not to do it is that it slightly increases the time
 * required to perform verification.
 */
#ifndef NDEBUG
# define DEAD_CODE_SCAN  true
#else
# define DEAD_CODE_SCAN  false
#endif

static bool gDebugVerbose = false;

#define SHOW_REG_DETAILS \
    (0 | DRT_SHOW_LIVENESS /*| DRT_SHOW_REF_TYPES | DRT_SHOW_LOCALS*/)

/*
 * We need an extra "pseudo register" to hold the return type briefly.  It
 * can be category 1 or 2, so we need two slots.
 */
#define kExtraRegs  2
#define RESULT_REGISTER(_insnRegCount)  (_insnRegCount)

/*
 * Big fat collection of register data.
 */
typedef struct RegisterTable {
    /*
     * Array of RegisterLine structs, one per address in the method.  We only
     * set the pointers for certain addresses, based on instruction widths
     * and what we're trying to accomplish.
     */
    RegisterLine* registerLines;

    /*
     * Number of registers we track for each instruction.  This is equal
     * to the method's declared "registersSize" plus kExtraRegs.
     */
    size_t      insnRegCountPlus;

    /*
     * Storage for a register line we're currently working on.
     */
    RegisterLine workLine;

    /*
     * Storage for a register line we're saving for later.
     */
    RegisterLine savedLine;

    /*
     * A single large alloc, with all of the storage needed for RegisterLine
     * data (RegType array, MonitorEntries array, monitor stack).
     */
    void*       lineAlloc;
} RegisterTable;


/* fwd */
#ifndef NDEBUG
static void checkMergeTab();
#endif
static bool isInitMethod(const Method* meth);
static RegType getInvocationThis(const RegisterLine* registerLine,\
    const DecodedInstruction* pDecInsn, VerifyError* pFailure);
static void verifyRegisterType(RegisterLine* registerLine, \
    u4 vsrc, RegType checkType, VerifyError* pFailure);
static bool doCodeVerification(VerifierData* vdata, RegisterTable* regTable);
static bool verifyInstruction(const Method* meth, InsnFlags* insnFlags,\
    RegisterTable* regTable, int insnIdx, UninitInstanceMap* uninitMap,
    int* pStartGuess);
static ClassObject* findCommonSuperclass(ClassObject* c1, ClassObject* c2);
static void dumpRegTypes(const VerifierData* vdata, \
    const RegisterLine* registerLine, int addr, const char* addrName,
    const UninitInstanceMap* uninitMap, int displayFlags);

/* bit values for dumpRegTypes() "displayFlags" */
enum {
    DRT_SIMPLE          = 0,
    DRT_SHOW_REF_TYPES  = 0x01,
    DRT_SHOW_LOCALS     = 0x02,
    DRT_SHOW_LIVENESS   = 0x04,
};


/*
 * ===========================================================================
 *      RegType and UninitInstanceMap utility functions
 * ===========================================================================
 */

#define __  kRegTypeUnknown
#define _U  kRegTypeUninit
#define _X  kRegTypeConflict
#define _0  kRegTypeZero
#define _1  kRegTypeOne
#define _Z  kRegTypeBoolean
#define _y  kRegTypeConstPosByte
#define _Y  kRegTypeConstByte
#define _h  kRegTypeConstPosShort
#define _H  kRegTypeConstShort
#define _c  kRegTypeConstChar
#define _i  kRegTypeConstInteger
#define _b  kRegTypePosByte
#define _B  kRegTypeByte
#define _s  kRegTypePosShort
#define _S  kRegTypeShort
#define _C  kRegTypeChar
#define _I  kRegTypeInteger
#define _F  kRegTypeFloat
#define _N  kRegTypeConstLo
#define _n  kRegTypeConstHi
#define _J  kRegTypeLongLo
#define _j  kRegTypeLongHi
#define _D  kRegTypeDoubleLo
#define _d  kRegTypeDoubleHi

/*
 * Merge result table for primitive values.  The table is symmetric along
 * the diagonal.
 *
 * Note that 32-bit int/float do not merge into 64-bit long/double.  This
 * is a register merge, not a widening conversion.  Only the "implicit"
 * widening within a category, e.g. byte to short, is allowed.
 *
 * Dalvik does not draw a distinction between int and float, but we enforce
 * that once a value is used as int, it can't be used as float, and vice
 * versa. We do not allow free exchange between 32-bit int/float and 64-bit
 * long/double.
 *
 * Note that Uninit+Uninit=Uninit.  This holds true because we only
 * use this when the RegType value is exactly equal to kRegTypeUninit, which
 * can only happen for the zeroeth entry in the table.
 *
 * "Unknown" never merges with anything known.  The only time a register
 * transitions from "unknown" to "known" is when we're executing code
 * for the first time, and we handle that with a simple copy.
 */
const char gDvmMergeTab[kRegTypeMAX][kRegTypeMAX] =
{
    /* chk:  _  U  X  0  1  Z  y  Y  h  H  c  i  b  B  s  S  C  I  F  N  n  J  j  D  d */
    { /*_*/ __,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X },
    { /*U*/ _X,_U,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X },
    { /*X*/ _X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X },
    { /*0*/ _X,_X,_X,_0,_Z,_Z,_y,_Y,_h,_H,_c,_i,_b,_B,_s,_S,_C,_I,_F,_X,_X,_X,_X,_X,_X },
    { /*1*/ _X,_X,_X,_Z,_1,_Z,_y,_Y,_h,_H,_c,_i,_b,_B,_s,_S,_C,_I,_F,_X,_X,_X,_X,_X,_X },
    { /*Z*/ _X,_X,_X,_Z,_Z,_Z,_y,_Y,_h,_H,_c,_i,_b,_B,_s,_S,_C,_I,_F,_X,_X,_X,_X,_X,_X },
    { /*y*/ _X,_X,_X,_y,_y,_y,_y,_Y,_h,_H,_c,_i,_b,_B,_s,_S,_C,_I,_F,_X,_X,_X,_X,_X,_X },
    { /*Y*/ _X,_X,_X,_Y,_Y,_Y,_Y,_Y,_h,_H,_c,_i,_B,_B,_S,_S,_I,_I,_F,_X,_X,_X,_X,_X,_X },
    { /*h*/ _X,_X,_X,_h,_h,_h,_h,_h,_h,_H,_c,_i,_s,_S,_s,_S,_C,_I,_F,_X,_X,_X,_X,_X,_X },
    { /*H*/ _X,_X,_X,_H,_H,_H,_H,_H,_H,_H,_c,_i,_S,_S,_S,_S,_I,_I,_F,_X,_X,_X,_X,_X,_X },
    { /*c*/ _X,_X,_X,_c,_c,_c,_c,_c,_c,_c,_c,_i,_C,_I,_C,_I,_C,_I,_F,_X,_X,_X,_X,_X,_X },
    { /*i*/ _X,_X,_X,_i,_i,_i,_i,_i,_i,_i,_i,_i,_I,_I,_I,_I,_I,_I,_F,_X,_X,_X,_X,_X,_X },
    { /*b*/ _X,_X,_X,_b,_b,_b,_b,_B,_s,_S,_C,_I,_b,_B,_s,_S,_C,_I,_X,_X,_X,_X,_X,_X,_X },
    { /*B*/ _X,_X,_X,_B,_B,_B,_B,_B,_S,_S,_I,_I,_B,_B,_S,_S,_I,_I,_X,_X,_X,_X,_X,_X,_X },
    { /*s*/ _X,_X,_X,_s,_s,_s,_s,_S,_s,_S,_C,_I,_s,_S,_s,_S,_C,_I,_X,_X,_X,_X,_X,_X,_X },
    { /*S*/ _X,_X,_X,_S,_S,_S,_S,_S,_S,_S,_I,_I,_S,_S,_S,_S,_I,_I,_X,_X,_X,_X,_X,_X,_X },
    { /*C*/ _X,_X,_X,_C,_C,_C,_C,_I,_C,_I,_C,_I,_C,_I,_C,_I,_C,_I,_X,_X,_X,_X,_X,_X,_X },
    { /*I*/ _X,_X,_X,_I,_I,_I,_I,_I,_I,_I,_I,_I,_I,_I,_I,_I,_I,_I,_X,_X,_X,_X,_X,_X,_X },
    { /*F*/ _X,_X,_X,_F,_F,_F,_F,_F,_F,_F,_F,_F,_X,_X,_X,_X,_X,_X,_F,_X,_X,_X,_X,_X,_X },
    { /*N*/ _X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_N,_X,_J,_X,_D,_X },
    { /*n*/ _X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_n,_X,_j,_X,_d },
    { /*J*/ _X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_J,_X,_J,_X,_X,_X },
    { /*j*/ _X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_j,_X,_j,_X,_X },
    { /*D*/ _X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_D,_X,_X,_X,_D,_X },
    { /*d*/ _X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_X,_d,_X,_X,_X,_d },
};

#undef __
#undef _U
#undef _X
#undef _0
#undef _1
#undef _Z
#undef _y
#undef _Y
#undef _h
#undef _H
#undef _c
#undef _i
#undef _b
#undef _B
#undef _s
#undef _S
#undef _C
#undef _I
#undef _F
#undef _N
#undef _n
#undef _J
#undef _j
#undef _D
#undef _d

#ifndef NDEBUG
/*
 * Verify symmetry in the conversion table.
 */
static void checkMergeTab()
{
    int i, j;

    for (i = 0; i < kRegTypeMAX; i++) {
        for (j = i; j < kRegTypeMAX; j++) {
            if (gDvmMergeTab[i][j] != gDvmMergeTab[j][i]) {
                ALOGE("Symmetry violation: %d,%d vs %d,%d", i, j, j, i);
                dvmAbort();
            }
        }
    }
}
#endif

/*
 * Determine whether we can convert "srcType" to "checkType", where
 * "checkType" is one of the category-1 non-reference types.
 *
 * Constant derived types may become floats, but other values may not.
 */
static bool canConvertTo1nr(RegType srcType, RegType checkType)
{
    static const char convTab
        [kRegType1nrEND-kRegType1nrSTART+1][kRegType1nrEND-kRegType1nrSTART+1] =
    {
        /* chk: 0  1  Z  y  Y  h  H  c  i  b  B  s  S  C  I  F */
        { /*0*/ 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { /*1*/ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { /*Z*/ 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { /*y*/ 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { /*Y*/ 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1 },
        { /*h*/ 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1 },
        { /*H*/ 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1 },
        { /*c*/ 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1 },
        { /*i*/ 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1 },
        { /*b*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0 },
        { /*B*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0 },
        { /*s*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0 },
        { /*S*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0 },
        { /*C*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0 },
        { /*I*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 },
        { /*F*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
    };

    assert(checkType >= kRegType1nrSTART && checkType <= kRegType1nrEND);
#if 0
    if (checkType < kRegType1nrSTART || checkType > kRegType1nrEND) {
        LOG_VFY("Unexpected checkType %d (srcType=%d)", checkType, srcType);
        assert(false);
        return false;
    }
#endif

    //printf("convTab[%d][%d] = %d\n", srcType, checkType,
    //    convTab[srcType-kRegType1nrSTART][checkType-kRegType1nrSTART]);
    if (srcType >= kRegType1nrSTART && srcType <= kRegType1nrEND)
        return (bool) convTab[srcType-kRegType1nrSTART][checkType-kRegType1nrSTART];

    return false;
}

/*
 * Determine whether the category-2 types are compatible.
 */
static bool canConvertTo2(RegType srcType, RegType checkType)
{
    return ((srcType == kRegTypeConstLo || srcType == checkType) &&
            (checkType == kRegTypeLongLo || checkType == kRegTypeDoubleLo));
}

/*
 * Determine whether or not "instrType" and "targetType" are compatible,
 * for purposes of getting or setting a value in a field or array.  The
 * idea is that an instruction with a category 1nr type (say, aget-short
 * or iput-boolean) is accessing a static field, instance field, or array
 * entry, and we want to make sure sure that the operation is legal.
 *
 * At a minimum, source and destination must have the same width.  We
 * further refine this to assert that "short" and "char" are not
 * compatible, because the sign-extension is different on the "get"
 * operations.
 *
 * We're not considering the actual contents of the register, so we'll
 * never get "pseudo-types" like kRegTypeZero or kRegTypePosShort.  We
 * could get kRegTypeUnknown in "targetType" if a field or array class
 * lookup failed.  Category 2 types and references are checked elsewhere.
 */
static bool checkFieldArrayStore1nr(RegType instrType, RegType targetType)
{
    return (instrType == targetType);
}

/*
 * Convert a VM PrimitiveType enum value to the equivalent RegType value.
 */
static RegType primitiveTypeToRegType(PrimitiveType primType)
{
    switch (primType) {
        case PRIM_BOOLEAN: return kRegTypeBoolean;
        case PRIM_BYTE:    return kRegTypeByte;
        case PRIM_SHORT:   return kRegTypeShort;
        case PRIM_CHAR:    return kRegTypeChar;
        case PRIM_INT:     return kRegTypeInteger;
        case PRIM_LONG:    return kRegTypeLongLo;
        case PRIM_FLOAT:   return kRegTypeFloat;
        case PRIM_DOUBLE:  return kRegTypeDoubleLo;
        case PRIM_VOID:
        default: {
            assert(false);
            return kRegTypeUnknown;
        }
    }
}

/*
 * Convert a const derived RegType to the equivalent non-const RegType value.
 * Does nothing if the argument type isn't const derived.
 */
static RegType constTypeToRegType(RegType constType)
{
    switch (constType) {
        case kRegTypeConstPosByte: return kRegTypePosByte;
        case kRegTypeConstByte: return kRegTypeByte;
        case kRegTypeConstPosShort: return kRegTypePosShort;
        case kRegTypeConstShort: return kRegTypeShort;
        case kRegTypeConstChar: return kRegTypeChar;
        case kRegTypeConstInteger: return kRegTypeInteger;
        default: {
            return constType;
        }
    }
}

/*
 * Given a 32-bit constant, return the most-restricted RegType enum entry
 * that can hold the value. The types used here indicate the value came
 * from a const instruction, and may not correctly represent the real type
 * of the value. Upon use, a constant derived type is updated with the
 * type from the use, which will be unambiguous.
 */
static char determineCat1Const(s4 value)
{
    if (value < -32768)
        return kRegTypeConstInteger;
    else if (value < -128)
        return kRegTypeConstShort;
    else if (value < 0)
        return kRegTypeConstByte;
    else if (value == 0)
        return kRegTypeZero;
    else if (value == 1)
        return kRegTypeOne;
    else if (value < 128)
        return kRegTypeConstPosByte;
    else if (value < 32768)
        return kRegTypeConstPosShort;
    else if (value < 65536)
        return kRegTypeConstChar;
    else
        return kRegTypeConstInteger;
}

/*
 * Create a new uninitialized instance map.
 *
 * The map is allocated and populated with address entries.  The addresses
 * appear in ascending order to allow binary searching.
 *
 * Very few methods have 10 or more new-instance instructions; the
 * majority have 0 or 1.  Occasionally a static initializer will have 200+.
 *
 * TODO: merge this into the static pass or initRegisterTable; want to
 * avoid walking through the instructions yet again just to set up this table
 */
UninitInstanceMap* dvmCreateUninitInstanceMap(const Method* meth,
    const InsnFlags* insnFlags, int newInstanceCount)
{
    const int insnsSize = dvmGetMethodInsnsSize(meth);
    const u2* insns = meth->insns;
    UninitInstanceMap* uninitMap;
    bool isInit = false;
    int idx, addr;

    if (isInitMethod(meth)) {
        newInstanceCount++;
        isInit = true;
    }

    /*
     * Allocate the header and map as a single unit.
     *
     * TODO: consider having a static instance so we can avoid allocations.
     * I don't think the verifier is guaranteed to be single-threaded when
     * running in the VM (rather than dexopt), so that must be taken into
     * account.
     */
    int size = offsetof(UninitInstanceMap, map) +
                newInstanceCount * sizeof(uninitMap->map[0]);
    uninitMap = (UninitInstanceMap*)calloc(1, size);
    if (uninitMap == NULL)
        return NULL;
    uninitMap->numEntries = newInstanceCount;

    idx = 0;
    if (isInit) {
        uninitMap->map[idx++].addr = kUninitThisArgAddr;
    }

    /*
     * Run through and find the new-instance instructions.
     */
    for (addr = 0; addr < insnsSize; /**/) {
        int width = dvmInsnGetWidth(insnFlags, addr);

        Opcode opcode = dexOpcodeFromCodeUnit(*insns);
        if (opcode == OP_NEW_INSTANCE)
            uninitMap->map[idx++].addr = addr;

        addr += width;
        insns += width;
    }

    assert(idx == newInstanceCount);
    return uninitMap;
}

/*
 * Free the map.
 */
void dvmFreeUninitInstanceMap(UninitInstanceMap* uninitMap)
{
    free(uninitMap);
}

/*
 * Set the class object associated with the instruction at "addr".
 *
 * Returns the map slot index, or -1 if the address isn't listed in the map
 * (shouldn't happen) or if a class is already associated with the address
 * (bad bytecode).
 *
 * Entries, once set, do not change -- a given address can only allocate
 * one type of object.
 */
static int setUninitInstance(UninitInstanceMap* uninitMap, int addr,
    ClassObject* clazz)
{
    int idx;

    assert(clazz != NULL);

#ifdef VERIFIER_STATS
    gDvm.verifierStats.uninitSearches++;
#endif

    /* TODO: binary search when numEntries > 8 */
    for (idx = uninitMap->numEntries - 1; idx >= 0; idx--) {
        if (uninitMap->map[idx].addr == addr) {
            if (uninitMap->map[idx].clazz != NULL &&
                uninitMap->map[idx].clazz != clazz)
            {
                LOG_VFY("VFY: addr %d already set to %p, not setting to %p",
                    addr, uninitMap->map[idx].clazz, clazz);
                return -1;          // already set to something else??
            }
            uninitMap->map[idx].clazz = clazz;
            return idx;
        }
    }

    LOG_VFY("VFY: addr %d not found in uninit map", addr);
    assert(false);      // shouldn't happen
    return -1;
}

/*
 * Get the class object at the specified index.
 */
static ClassObject* getUninitInstance(const UninitInstanceMap* uninitMap,
    int idx)
{
    assert(idx >= 0 && idx < uninitMap->numEntries);
    return uninitMap->map[idx].clazz;
}

/* determine if "type" is actually an object reference (init/uninit/zero) */
static inline bool regTypeIsReference(RegType type) {
    return (type > kRegTypeMAX || type == kRegTypeUninit ||
            type == kRegTypeZero);
}

/* determine if "type" is an uninitialized object reference */
static inline bool regTypeIsUninitReference(RegType type) {
    return ((type & kRegTypeUninitMask) == kRegTypeUninit);
}

/* convert the initialized reference "type" to a ClassObject pointer */
/* (does not expect uninit ref types or "zero") */
static ClassObject* regTypeInitializedReferenceToClass(RegType type)
{
    assert(regTypeIsReference(type) && type != kRegTypeZero);
    if ((type & 0x01) == 0) {
        return (ClassObject*) type;
    } else {
        //LOG_VFY("VFY: attempted to use uninitialized reference");
        return NULL;
    }
}

/* extract the index into the uninitialized instance map table */
static inline int regTypeToUninitIndex(RegType type) {
    assert(regTypeIsUninitReference(type));
    return (type & ~kRegTypeUninitMask) >> kRegTypeUninitShift;
}

/* convert the reference "type" to a ClassObject pointer */
static ClassObject* regTypeReferenceToClass(RegType type,
    const UninitInstanceMap* uninitMap)
{
    assert(regTypeIsReference(type) && type != kRegTypeZero);
    if (regTypeIsUninitReference(type)) {
        assert(uninitMap != NULL);
        return getUninitInstance(uninitMap, regTypeToUninitIndex(type));
    } else {
        return (ClassObject*) type;
    }
}

/* convert the ClassObject pointer to an (initialized) register type */
static inline RegType regTypeFromClass(ClassObject* clazz) {
    return (u4) clazz;
}

/* return the RegType for the uninitialized reference in slot "uidx" */
static RegType regTypeFromUninitIndex(int uidx) {
    return (u4) (kRegTypeUninit | (uidx << kRegTypeUninitShift));
}


/*
 * ===========================================================================
 *      Signature operations
 * ===========================================================================
 */

/*
 * Is this method a constructor?
 */
static bool isInitMethod(const Method* meth)
{
    return (*meth->name == '<' && strcmp(meth->name+1, "init>") == 0);
}

/*
 * Is this method a class initializer?
 */
#if 0
static bool isClassInitMethod(const Method* meth)
{
    return (*meth->name == '<' && strcmp(meth->name+1, "clinit>") == 0);
}
#endif

/*
 * Look up a class reference given as a simple string descriptor.
 *
 * If we can't find it, return a generic substitute when possible.
 */
static ClassObject* lookupClassByDescriptor(const Method* meth,
    const char* pDescriptor, VerifyError* pFailure)
{
    /*
     * The javac compiler occasionally puts references to nonexistent
     * classes in signatures.  For example, if you have a non-static
     * inner class with no constructor, the compiler provides
     * a private <init> for you.  Constructing the class
     * requires <init>(parent), but the outer class can't call
     * that because the method is private.  So the compiler
     * generates a package-scope <init>(parent,bogus) method that
     * just calls the regular <init> (the "bogus" part being necessary
     * to distinguish the signature of the synthetic method).
     * Treating the bogus class as an instance of java.lang.Object
     * allows the verifier to process the class successfully.
     */

    //ALOGI("Looking up '%s'", typeStr);
    ClassObject* clazz;
    clazz = dvmFindClassNoInit(pDescriptor, meth->clazz->classLoader);
    if (clazz == NULL) {
        dvmClearOptException(dvmThreadSelf());
        if (strchr(pDescriptor, '$') != NULL) {
            ALOGV("VFY: unable to find class referenced in signature (%s)",
                pDescriptor);
        } else {
            LOG_VFY("VFY: unable to find class referenced in signature (%s)",
                pDescriptor);
        }

        if (pDescriptor[0] == '[') {
            /* We are looking at an array descriptor. */

            /*
             * There should never be a problem loading primitive arrays.
             */
            if (pDescriptor[1] != 'L' && pDescriptor[1] != '[') {
                LOG_VFY("VFY: invalid char in signature in '%s'",
                    pDescriptor);
                *pFailure = VERIFY_ERROR_GENERIC;
            }

            /*
             * Try to continue with base array type.  This will let
             * us pass basic stuff (e.g. get array len) that wouldn't
             * fly with an Object.  This is NOT correct if the
             * missing type is a primitive array, but we should never
             * have a problem loading those.  (I'm not convinced this
             * is correct or even useful.  Just use Object here?)
             */
            clazz = dvmFindClassNoInit("[Ljava/lang/Object;",
                meth->clazz->classLoader);
        } else if (pDescriptor[0] == 'L') {
            /*
             * We are looking at a non-array reference descriptor;
             * try to continue with base reference type.
             */
            clazz = gDvm.classJavaLangObject;
        } else {
            /* We are looking at a primitive type. */
            LOG_VFY("VFY: invalid char in signature in '%s'", pDescriptor);
            *pFailure = VERIFY_ERROR_GENERIC;
        }

        if (clazz == NULL) {
            *pFailure = VERIFY_ERROR_GENERIC;
        }
    }

    if (dvmIsPrimitiveClass(clazz)) {
        LOG_VFY("VFY: invalid use of primitive type '%s'", pDescriptor);
        *pFailure = VERIFY_ERROR_GENERIC;
        clazz = NULL;
    }

    return clazz;
}

/*
 * Look up a class reference in a signature.  Could be an arg or the
 * return value.
 *
 * Advances "*pSig" to the last character in the signature (that is, to
 * the ';').
 *
 * NOTE: this is also expected to verify the signature.
 */
static ClassObject* lookupSignatureClass(const Method* meth, const char** pSig,
    VerifyError* pFailure)
{
    const char* sig = *pSig;
    const char* endp = sig;

    assert(sig != NULL && *sig == 'L');

    while (*++endp != ';' && *endp != '\0')
        ;
    if (*endp != ';') {
        LOG_VFY("VFY: bad signature component '%s' (missing ';')", sig);
        *pFailure = VERIFY_ERROR_GENERIC;
        return NULL;
    }

    endp++;    /* Advance past the ';'. */
    int typeLen = endp - sig;
    char typeStr[typeLen+1]; /* +1 for the '\0' */
    memcpy(typeStr, sig, typeLen);
    typeStr[typeLen] = '\0';

    *pSig = endp - 1; /* - 1 so that *pSig points at, not past, the ';' */

    return lookupClassByDescriptor(meth, typeStr, pFailure);
}

/*
 * Look up an array class reference in a signature.  Could be an arg or the
 * return value.
 *
 * Advances "*pSig" to the last character in the signature.
 *
 * NOTE: this is also expected to verify the signature.
 */
static ClassObject* lookupSignatureArrayClass(const Method* meth,
    const char** pSig, VerifyError* pFailure)
{
    const char* sig = *pSig;
    const char* endp = sig;

    assert(sig != NULL && *sig == '[');

    /* find the end */
    while (*++endp == '[' && *endp != '\0')
        ;

    if (*endp == 'L') {
        while (*++endp != ';' && *endp != '\0')
            ;
        if (*endp != ';') {
            LOG_VFY("VFY: bad signature component '%s' (missing ';')", sig);
            *pFailure = VERIFY_ERROR_GENERIC;
            return NULL;
        }
    }

    int typeLen = endp - sig +1;
    char typeStr[typeLen+1];
    memcpy(typeStr, sig, typeLen);
    typeStr[typeLen] = '\0';

    *pSig = endp;

    return lookupClassByDescriptor(meth, typeStr, pFailure);
}

/*
 * Set the register types for the first instruction in the method based on
 * the method signature.
 *
 * This has the side-effect of validating the signature.
 *
 * Returns "true" on success.
 */
static bool setTypesFromSignature(const Method* meth, RegType* regTypes,
    UninitInstanceMap* uninitMap)
{
    DexParameterIterator iterator;
    int actualArgs, expectedArgs, argStart;
    VerifyError failure = VERIFY_ERROR_NONE;
    const char* descriptor;

    dexParameterIteratorInit(&iterator, &meth->prototype);
    argStart = meth->registersSize - meth->insSize;
    expectedArgs = meth->insSize;     /* long/double count as two */
    actualArgs = 0;

    assert(argStart >= 0);      /* should have been verified earlier */

    /*
     * Include the "this" pointer.
     */
    if (!dvmIsStaticMethod(meth)) {
        /*
         * If this is a constructor for a class other than java.lang.Object,
         * mark the first ("this") argument as uninitialized.  This restricts
         * field access until the superclass constructor is called.
         */
        if (isInitMethod(meth) && meth->clazz != gDvm.classJavaLangObject) {
            int uidx = setUninitInstance(uninitMap, kUninitThisArgAddr,
                            meth->clazz);
            assert(uidx == 0);
            regTypes[argStart + actualArgs] = regTypeFromUninitIndex(uidx);
        } else {
            regTypes[argStart + actualArgs] = regTypeFromClass(meth->clazz);
        }
        actualArgs++;
    }

    for (;;) {
        descriptor = dexParameterIteratorNextDescriptor(&iterator);

        if (descriptor == NULL) {
            break;
        }

        if (actualArgs >= expectedArgs) {
            LOG_VFY("VFY: expected %d args, found more (%s)",
                expectedArgs, descriptor);
            goto bad_sig;
        }

        switch (*descriptor) {
        case 'L':
        case '[':
            /*
             * We assume that reference arguments are initialized.  The
             * only way it could be otherwise (assuming the caller was
             * verified) is if the current method is <init>, but in that
             * case it's effectively considered initialized the instant
             * we reach here (in the sense that we can return without
             * doing anything or call virtual methods).
             */
            {
                ClassObject* clazz =
                    lookupClassByDescriptor(meth, descriptor, &failure);
                if (!VERIFY_OK(failure))
                    goto bad_sig;
                regTypes[argStart + actualArgs] = regTypeFromClass(clazz);
            }
            actualArgs++;
            break;
        case 'Z':
            regTypes[argStart + actualArgs] = kRegTypeBoolean;
            actualArgs++;
            break;
        case 'C':
            regTypes[argStart + actualArgs] = kRegTypeChar;
            actualArgs++;
            break;
        case 'B':
            regTypes[argStart + actualArgs] = kRegTypeByte;
            actualArgs++;
            break;
        case 'I':
            regTypes[argStart + actualArgs] = kRegTypeInteger;
            actualArgs++;
            break;
        case 'S':
            regTypes[argStart + actualArgs] = kRegTypeShort;
            actualArgs++;
            break;
        case 'F':
            regTypes[argStart + actualArgs] = kRegTypeFloat;
            actualArgs++;
            break;
        case 'D':
            regTypes[argStart + actualArgs] = kRegTypeDoubleLo;
            regTypes[argStart + actualArgs +1] = kRegTypeDoubleHi;
            actualArgs += 2;
            break;
        case 'J':
            regTypes[argStart + actualArgs] = kRegTypeLongLo;
            regTypes[argStart + actualArgs +1] = kRegTypeLongHi;
            actualArgs += 2;
            break;
        default:
            LOG_VFY("VFY: unexpected signature type char '%c'", *descriptor);
            goto bad_sig;
        }
    }

    if (actualArgs != expectedArgs) {
        LOG_VFY("VFY: expected %d args, found %d", expectedArgs, actualArgs);
        goto bad_sig;
    }

    descriptor = dexProtoGetReturnType(&meth->prototype);

    /*
     * Validate return type.  We don't do the type lookup; just want to make
     * sure that it has the right format.  Only major difference from the
     * method argument format is that 'V' is supported.
     */
    switch (*descriptor) {
    case 'I':
    case 'C':
    case 'S':
    case 'B':
    case 'Z':
    case 'V':
    case 'F':
    case 'D':
    case 'J':
        if (*(descriptor+1) != '\0')
            goto bad_sig;
        break;
    case '[':
        /* single/multi, object/primitive */
        while (*++descriptor == '[')
            ;
        if (*descriptor == 'L') {
            while (*++descriptor != ';' && *descriptor != '\0')
                ;
            if (*descriptor != ';')
                goto bad_sig;
        } else {
            if (*(descriptor+1) != '\0')
                goto bad_sig;
        }
        break;
    case 'L':
        /* could be more thorough here, but shouldn't be required */
        while (*++descriptor != ';' && *descriptor != '\0')
            ;
        if (*descriptor != ';')
            goto bad_sig;
        break;
    default:
        goto bad_sig;
    }

    return true;

//fail:
//    LOG_VFY_METH(meth, "VFY:  bad sig");
//    return false;

bad_sig:
    {
        char* desc = dexProtoCopyMethodDescriptor(&meth->prototype);
        LOG_VFY("VFY: bad signature '%s' for %s.%s",
            desc, meth->clazz->descriptor, meth->name);
        free(desc);
    }
    return false;
}

/*
 * Return the register type for the method.  We can't just use the
 * already-computed DalvikJniReturnType, because if it's a reference type
 * we need to do the class lookup.
 *
 * Returned references are assumed to be initialized.
 *
 * Returns kRegTypeUnknown for "void".
 */
static RegType getMethodReturnType(const Method* meth)
{
    RegType type;
    const char* descriptor = dexProtoGetReturnType(&meth->prototype);

    switch (*descriptor) {
    case 'I':
        type = kRegTypeInteger;
        break;
    case 'C':
        type = kRegTypeChar;
        break;
    case 'S':
        type = kRegTypeShort;
        break;
    case 'B':
        type = kRegTypeByte;
        break;
    case 'Z':
        type = kRegTypeBoolean;
        break;
    case 'V':
        type = kRegTypeUnknown;
        break;
    case 'F':
        type = kRegTypeFloat;
        break;
    case 'D':
        type = kRegTypeDoubleLo;
        break;
    case 'J':
        type = kRegTypeLongLo;
        break;
    case 'L':
    case '[':
        {
            VerifyError failure = VERIFY_ERROR_NONE;
            ClassObject* clazz =
                lookupClassByDescriptor(meth, descriptor, &failure);
            assert(VERIFY_OK(failure));
            type = regTypeFromClass(clazz);
        }
        break;
    default:
        /* we verified signature return type earlier, so this is impossible */
        assert(false);
        type = kRegTypeConflict;
        break;
    }

    return type;
}

/*
 * Convert a single-character signature value (i.e. a primitive type) to
 * the corresponding RegType.  This is intended for access to object fields
 * holding primitive types.
 *
 * Returns kRegTypeUnknown for objects, arrays, and void.
 */
static RegType primSigCharToRegType(char sigChar)
{
    RegType type;

    switch (sigChar) {
    case 'I':
        type = kRegTypeInteger;
        break;
    case 'C':
        type = kRegTypeChar;
        break;
    case 'S':
        type = kRegTypeShort;
        break;
    case 'B':
        type = kRegTypeByte;
        break;
    case 'Z':
        type = kRegTypeBoolean;
        break;
    case 'F':
        type = kRegTypeFloat;
        break;
    case 'D':
        type = kRegTypeDoubleLo;
        break;
    case 'J':
        type = kRegTypeLongLo;
        break;
    case 'V':
    case 'L':
    case '[':
        type = kRegTypeUnknown;
        break;
    default:
        assert(false);
        type = kRegTypeUnknown;
        break;
    }

    return type;
}

/*
 * See if the method matches the MethodType.
 */
static bool isCorrectInvokeKind(MethodType methodType, Method* resMethod)
{
    switch (methodType) {
    case METHOD_DIRECT:
        return dvmIsDirectMethod(resMethod);
    case METHOD_STATIC:
        return dvmIsStaticMethod(resMethod);
    case METHOD_VIRTUAL:
    case METHOD_INTERFACE:
        return !dvmIsDirectMethod(resMethod);
    default:
        return false;
    }
}

/*
 * Verify the arguments to a method.  We're executing in "method", making
 * a call to the method reference in vB.
 *
 * If this is a "direct" invoke, we allow calls to <init>.  For calls to
 * <init>, the first argument may be an uninitialized reference.  Otherwise,
 * calls to anything starting with '<' will be rejected, as will any
 * uninitialized reference arguments.
 *
 * For non-static method calls, this will verify that the method call is
 * appropriate for the "this" argument.
 *
 * The method reference is in vBBBB.  The "isRange" parameter determines
 * whether we use 0-4 "args" values or a range of registers defined by
 * vAA and vCCCC.
 *
 * Widening conversions on integers and references are allowed, but
 * narrowing conversions are not.
 *
 * Returns the resolved method on success, NULL on failure (with *pFailure
 * set appropriately).
 */
static Method* verifyInvocationArgs(const Method* meth,
    RegisterLine* registerLine, const int insnRegCount,
    const DecodedInstruction* pDecInsn, UninitInstanceMap* uninitMap,
    MethodType methodType, bool isRange, bool isSuper, VerifyError* pFailure)
{
    Method* resMethod;
    char* sigOriginal = NULL;
    const char* sig;
    int expectedArgs;
    int actualArgs;

    /*
     * Resolve the method.  This could be an abstract or concrete method
     * depending on what sort of call we're making.
     */
    if (methodType == METHOD_INTERFACE) {
        resMethod = dvmOptResolveInterfaceMethod(meth->clazz, pDecInsn->vB);
    } else {
        resMethod = dvmOptResolveMethod(meth->clazz, pDecInsn->vB, methodType,
            pFailure);
    }
    if (resMethod == NULL) {
        /* failed; print a meaningful failure message */
        DexFile* pDexFile = meth->clazz->pDvmDex->pDexFile;

        const DexMethodId* pMethodId = dexGetMethodId(pDexFile, pDecInsn->vB);
        const char* methodName = dexStringById(pDexFile, pMethodId->nameIdx);
        char* methodDesc = dexCopyDescriptorFromMethodId(pDexFile, pMethodId);
        const char* classDescriptor = dexStringByTypeIdx(pDexFile, pMethodId->classIdx);

        if (!gDvm.optimizing) {
            std::string dotMissingClass =
                dvmHumanReadableDescriptor(classDescriptor);
            std::string dotMethClass =
                dvmHumanReadableDescriptor(meth->clazz->descriptor);

            ALOGI("Could not find method %s.%s, referenced from method %s.%s",
                    dotMissingClass.c_str(), methodName,
                    dotMethClass.c_str(), meth->name);
        }

        LOG_VFY("VFY: unable to resolve %s method %u: %s.%s %s",
            dvmMethodTypeStr(methodType), pDecInsn->vB,
            classDescriptor, methodName, methodDesc);
        free(methodDesc);
        if (VERIFY_OK(*pFailure))       /* not set for interface resolve */
            *pFailure = VERIFY_ERROR_NO_METHOD;
        goto fail;
    }

    /*
     * Only time you can explicitly call a method starting with '<' is when
     * making a "direct" invocation on "<init>".  There are additional
     * restrictions but we don't enforce them here.
     */
    if (resMethod->name[0] == '<') {
        if (methodType != METHOD_DIRECT || !isInitMethod(resMethod)) {
            LOG_VFY("VFY: invalid call to %s.%s",
                    resMethod->clazz->descriptor, resMethod->name);
            goto bad_sig;
        }
    }

    /*
     * See if the method type implied by the invoke instruction matches the
     * access flags for the target method.
     */
    if (!isCorrectInvokeKind(methodType, resMethod)) {
        LOG_VFY("VFY: invoke type does not match method type of %s.%s",
            resMethod->clazz->descriptor, resMethod->name);
        goto fail;
    }

    /*
     * If we're using invoke-super(method), make sure that the executing
     * method's class' superclass has a vtable entry for the target method.
     */
    if (isSuper) {
        assert(methodType == METHOD_VIRTUAL);
        ClassObject* super = meth->clazz->super;
        if (super == NULL || resMethod->methodIndex > super->vtableCount) {
            char* desc = dexProtoCopyMethodDescriptor(&resMethod->prototype);
            LOG_VFY("VFY: invalid invoke-super from %s.%s to super %s.%s %s",
                    meth->clazz->descriptor, meth->name,
                    (super == NULL) ? "-" : super->descriptor,
                    resMethod->name, desc);
            free(desc);
            *pFailure = VERIFY_ERROR_NO_METHOD;
            goto fail;
        }
    }

    /*
     * We use vAA as our expected arg count, rather than resMethod->insSize,
     * because we need to match the call to the signature.  Also, we might
     * might be calling through an abstract method definition (which doesn't
     * have register count values).
     */
    sigOriginal = dexProtoCopyMethodDescriptor(&resMethod->prototype);
    sig = sigOriginal;
    expectedArgs = pDecInsn->vA;
    actualArgs = 0;

    /* caught by static verifier */
    assert(isRange || expectedArgs <= 5);

    if (expectedArgs > meth->outsSize) {
        LOG_VFY("VFY: invalid arg count (%d) exceeds outsSize (%d)",
            expectedArgs, meth->outsSize);
        goto fail;
    }

    if (*sig++ != '(')
        goto bad_sig;

    /*
     * Check the "this" argument, which must be an instance of the class
     * that declared the method.  For an interface class, we don't do the
     * full interface merge, so we can't do a rigorous check here (which
     * is okay since we have to do it at runtime).
     */
    if (!dvmIsStaticMethod(resMethod)) {
        ClassObject* actualThisRef;
        RegType actualArgType;

        actualArgType = getInvocationThis(registerLine, pDecInsn, pFailure);
        if (!VERIFY_OK(*pFailure))
            goto fail;

        if (regTypeIsUninitReference(actualArgType) && resMethod->name[0] != '<')
        {
            LOG_VFY("VFY: 'this' arg must be initialized");
            goto fail;
        }
        if (methodType != METHOD_INTERFACE && actualArgType != kRegTypeZero) {
            actualThisRef = regTypeReferenceToClass(actualArgType, uninitMap);
            if (!dvmInstanceof(actualThisRef, resMethod->clazz)) {
                LOG_VFY("VFY: 'this' arg '%s' not instance of '%s'",
                        actualThisRef->descriptor,
                        resMethod->clazz->descriptor);
                goto fail;
            }
        }
        actualArgs++;
    }

    /*
     * Process the target method's signature.  This signature may or may not
     * have been verified, so we can't assume it's properly formed.
     */
    while (*sig != '\0' && *sig != ')') {
        if (actualArgs >= expectedArgs) {
            LOG_VFY("VFY: expected %d args, found more (%c)",
                expectedArgs, *sig);
            goto bad_sig;
        }

        u4 getReg;
        if (isRange)
            getReg = pDecInsn->vC + actualArgs;
        else
            getReg = pDecInsn->arg[actualArgs];

        switch (*sig) {
        case 'L':
            {
                ClassObject* clazz = lookupSignatureClass(meth, &sig, pFailure);
                if (!VERIFY_OK(*pFailure))
                    goto bad_sig;
                verifyRegisterType(registerLine, getReg,
                    regTypeFromClass(clazz), pFailure);
                if (!VERIFY_OK(*pFailure)) {
                    LOG_VFY("VFY: bad arg %d (into %s)",
                            actualArgs, clazz->descriptor);
                    goto bad_sig;
                }
            }
            actualArgs++;
            break;
        case '[':
            {
                ClassObject* clazz =
                    lookupSignatureArrayClass(meth, &sig, pFailure);
                if (!VERIFY_OK(*pFailure))
                    goto bad_sig;
                verifyRegisterType(registerLine, getReg,
                    regTypeFromClass(clazz), pFailure);
                if (!VERIFY_OK(*pFailure)) {
                    LOG_VFY("VFY: bad arg %d (into %s)",
                            actualArgs, clazz->descriptor);
                    goto bad_sig;
                }
            }
            actualArgs++;
            break;
        case 'Z':
            verifyRegisterType(registerLine, getReg, kRegTypeBoolean, pFailure);
            actualArgs++;
            break;
        case 'C':
            verifyRegisterType(registerLine, getReg, kRegTypeChar, pFailure);
            actualArgs++;
            break;
        case 'B':
            verifyRegisterType(registerLine, getReg, kRegTypeByte, pFailure);
            actualArgs++;
            break;
        case 'I':
            verifyRegisterType(registerLine, getReg, kRegTypeInteger, pFailure);
            actualArgs++;
            break;
        case 'S':
            verifyRegisterType(registerLine, getReg, kRegTypeShort, pFailure);
            actualArgs++;
            break;
        case 'F':
            verifyRegisterType(registerLine, getReg, kRegTypeFloat, pFailure);
            actualArgs++;
            break;
        case 'D':
            verifyRegisterType(registerLine, getReg, kRegTypeDoubleLo, pFailure);
            actualArgs += 2;
            break;
        case 'J':
            verifyRegisterType(registerLine, getReg, kRegTypeLongLo, pFailure);
            actualArgs += 2;
            break;
        default:
            LOG_VFY("VFY: invocation target: bad signature type char '%c'",
                *sig);
            goto bad_sig;
        }

        sig++;
    }
    if (*sig != ')') {
        char* desc = dexProtoCopyMethodDescriptor(&resMethod->prototype);
        LOG_VFY("VFY: invocation target: bad signature '%s'", desc);
        free(desc);
        goto bad_sig;
    }

    if (actualArgs != expectedArgs) {
        LOG_VFY("VFY: expected %d args, found %d", expectedArgs, actualArgs);
        goto bad_sig;
    }

    free(sigOriginal);
    return resMethod;

bad_sig:
    if (resMethod != NULL) {
        char* desc = dexProtoCopyMethodDescriptor(&resMethod->prototype);
        LOG_VFY("VFY:  rejecting call to %s.%s %s",
            resMethod->clazz->descriptor, resMethod->name, desc);
        free(desc);
    }

fail:
    free(sigOriginal);
    if (*pFailure == VERIFY_ERROR_NONE)
        *pFailure = VERIFY_ERROR_GENERIC;
    return NULL;
}

/*
 * Get the class object for the type of data stored in a field.  This isn't
 * stored in the Field struct, so we have to recover it from the signature.
 *
 * This only works for reference types.  Don't call this for primitive types.
 *
 * If we can't find the class, we return java.lang.Object, so that
 * verification can continue if a field is only accessed in trivial ways.
 */
static ClassObject* getFieldClass(const Method* meth, const Field* field)
{
    ClassObject* fieldClass;
    const char* signature = field->signature;

    if ((*signature == 'L') || (*signature == '[')) {
        fieldClass = dvmFindClassNoInit(signature,
                meth->clazz->classLoader);
    } else {
        return NULL;
    }

    if (fieldClass == NULL) {
        dvmClearOptException(dvmThreadSelf());
        ALOGV("VFY: unable to find class '%s' for field %s.%s, trying Object",
            field->signature, meth->clazz->descriptor, field->name);
        fieldClass = gDvm.classJavaLangObject;
    } else {
        assert(!dvmIsPrimitiveClass(fieldClass));
    }
    return fieldClass;
}


/*
 * ===========================================================================
 *      Register operations
 * ===========================================================================
 */

/*
 * Get the type of register N.
 *
 * The register index was validated during the static pass, so we don't
 * need to check it here.
 */
static inline RegType getRegisterType(const RegisterLine* registerLine, u4 vsrc)
{
    return registerLine->regTypes[vsrc];
}

/*
 * Get the value from a register, and cast it to a ClassObject.  Sets
 * "*pFailure" if something fails.
 *
 * This fails if the register holds an uninitialized class.
 *
 * If the register holds kRegTypeZero, this returns a NULL pointer.
 */
static ClassObject* getClassFromRegister(const RegisterLine* registerLine,
    u4 vsrc, VerifyError* pFailure)
{
    ClassObject* clazz = NULL;
    RegType type;

    /* get the element type of the array held in vsrc */
    type = getRegisterType(registerLine, vsrc);

    /* if "always zero", we allow it to fail at runtime */
    if (type == kRegTypeZero)
        goto bail;

    if (!regTypeIsReference(type)) {
        LOG_VFY("VFY: tried to get class from non-ref register v%d (type=%d)",
            vsrc, type);
        *pFailure = VERIFY_ERROR_GENERIC;
        goto bail;
    }
    if (regTypeIsUninitReference(type)) {
        LOG_VFY("VFY: register %u holds uninitialized reference", vsrc);
        *pFailure = VERIFY_ERROR_GENERIC;
        goto bail;
    }

    clazz = regTypeInitializedReferenceToClass(type);

bail:
    return clazz;
}

/*
 * Get the "this" pointer from a non-static method invocation.  This
 * returns the RegType so the caller can decide whether it needs the
 * reference to be initialized or not.  (Can also return kRegTypeZero
 * if the reference can only be zero at this point.)
 *
 * The argument count is in vA, and the first argument is in vC, for both
 * "simple" and "range" versions.  We just need to make sure vA is >= 1
 * and then return vC.
 */
static RegType getInvocationThis(const RegisterLine* registerLine,
    const DecodedInstruction* pDecInsn, VerifyError* pFailure)
{
    RegType thisType = kRegTypeUnknown;

    if (pDecInsn->vA < 1) {
        LOG_VFY("VFY: invoke lacks 'this'");
        *pFailure = VERIFY_ERROR_GENERIC;
        goto bail;
    }

    /* get the element type of the array held in vsrc */
    thisType = getRegisterType(registerLine, pDecInsn->vC);
    if (!regTypeIsReference(thisType)) {
        LOG_VFY("VFY: tried to get class from non-ref register v%d (type=%d)",
            pDecInsn->vC, thisType);
        *pFailure = VERIFY_ERROR_GENERIC;
        goto bail;
    }

bail:
    return thisType;
}

/*
 * Set the type of register N, verifying that the register is valid.  If
 * "newType" is the "Lo" part of a 64-bit value, register N+1 will be
 * set to "newType+1".
 *
 * The register index was validated during the static pass, so we don't
 * need to check it here.
 *
 * TODO: clear mon stack bits
 */
static void setRegisterType(RegisterLine* registerLine, u4 vdst,
    RegType newType)
{
    RegType* insnRegs = registerLine->regTypes;

    switch (newType) {
    case kRegTypeUnknown:
    case kRegTypeBoolean:
    case kRegTypeOne:
    case kRegTypeConstByte:
    case kRegTypeConstPosByte:
    case kRegTypeConstShort:
    case kRegTypeConstPosShort:
    case kRegTypeConstChar:
    case kRegTypeConstInteger:
    case kRegTypeByte:
    case kRegTypePosByte:
    case kRegTypeShort:
    case kRegTypePosShort:
    case kRegTypeChar:
    case kRegTypeInteger:
    case kRegTypeFloat:
    case kRegTypeZero:
    case kRegTypeUninit:
        insnRegs[vdst] = newType;
        break;
    case kRegTypeConstLo:
    case kRegTypeLongLo:
    case kRegTypeDoubleLo:
        insnRegs[vdst] = newType;
        insnRegs[vdst+1] = newType+1;
        break;
    case kRegTypeConstHi:
    case kRegTypeLongHi:
    case kRegTypeDoubleHi:
        /* should never set these explicitly */
        ALOGE("BUG: explicit set of high register type");
        dvmAbort();
        break;

    default:
        /* can't switch for ref types, so we check explicitly */
        if (regTypeIsReference(newType)) {
            insnRegs[vdst] = newType;

            /*
             * In most circumstances we won't see a reference to a primitive
             * class here (e.g. "D"), since that would mean the object in the
             * register is actually a primitive type.  It can happen as the
             * result of an assumed-successful check-cast instruction in
             * which the second argument refers to a primitive class.  (In
             * practice, such an instruction will always throw an exception.)
             *
             * This is not an issue for instructions like const-class, where
             * the object in the register is a java.lang.Class instance.
             */
            break;
        }
        /* bad type - fall through */

    case kRegTypeConflict:      // should only be set during a merge
        ALOGE("BUG: set register to unknown type %d", newType);
        dvmAbort();
        break;
    }

    /*
     * Clear the monitor entry bits for this register.
     */
    if (registerLine->monitorEntries != NULL)
        registerLine->monitorEntries[vdst] = 0;
}

/*
 * Verify that the contents of the specified register have the specified
 * type (or can be converted to it through an implicit widening conversion).
 *
 * This will modify the type of the source register if it was originally
 * derived from a constant to prevent mixing of int/float and long/double.
 *
 * If "vsrc" is a reference, both it and the "vsrc" register must be
 * initialized ("vsrc" may be Zero).  This will verify that the value in
 * the register is an instance of checkType, or if checkType is an
 * interface, verify that the register implements checkType.
 */
static void verifyRegisterType(RegisterLine* registerLine, u4 vsrc,
    RegType checkType, VerifyError* pFailure)
{
    const RegType* insnRegs = registerLine->regTypes;
    RegType srcType = insnRegs[vsrc];

    //ALOGD("check-reg v%u = %d", vsrc, checkType);
    switch (checkType) {
    case kRegTypeFloat:
    case kRegTypeBoolean:
    case kRegTypePosByte:
    case kRegTypeByte:
    case kRegTypePosShort:
    case kRegTypeShort:
    case kRegTypeChar:
    case kRegTypeInteger:
        if (!canConvertTo1nr(srcType, checkType)) {
            LOG_VFY("VFY: register1 v%u type %d, wanted %d",
                vsrc, srcType, checkType);
            *pFailure = VERIFY_ERROR_GENERIC;
            break;
        }
        /* Update type if result is float */
        if (checkType == kRegTypeFloat) {
            setRegisterType(registerLine, vsrc, checkType);
        } else {
            /* Update const type to actual type after use */
            setRegisterType(registerLine, vsrc, constTypeToRegType(srcType));
        }
        break;
    case kRegTypeLongLo:
    case kRegTypeDoubleLo:
        if (insnRegs[vsrc+1] != srcType+1) {
            LOG_VFY("VFY: register2 v%u-%u values %d,%d",
                vsrc, vsrc+1, insnRegs[vsrc], insnRegs[vsrc+1]);
            *pFailure = VERIFY_ERROR_GENERIC;
            break;
        } else if (!canConvertTo2(srcType, checkType)) {
            LOG_VFY("VFY: register2 v%u type %d, wanted %d",
                vsrc, srcType, checkType);
            *pFailure = VERIFY_ERROR_GENERIC;
            break;
        }
        /* Update type if source is from const */
        if (srcType == kRegTypeConstLo) {
            setRegisterType(registerLine, vsrc, checkType);
        }
        break;
    case kRegTypeConstLo:
    case kRegTypeConstHi:
    case kRegTypeLongHi:
    case kRegTypeDoubleHi:
    case kRegTypeZero:
    case kRegTypeOne:
    case kRegTypeUnknown:
    case kRegTypeConflict:
        /* should never be checking for these explicitly */
        assert(false);
        *pFailure = VERIFY_ERROR_GENERIC;
        return;
    case kRegTypeUninit:
    default:
        /* make sure checkType is initialized reference */
        if (!regTypeIsReference(checkType)) {
            LOG_VFY("VFY: unexpected check type %d", checkType);
            assert(false);
            *pFailure = VERIFY_ERROR_GENERIC;
            break;
        }
        if (regTypeIsUninitReference(checkType)) {
            LOG_VFY("VFY: uninitialized ref not expected as reg check");
            *pFailure = VERIFY_ERROR_GENERIC;
            break;
        }
        /* make sure srcType is initialized reference or always-NULL */
        if (!regTypeIsReference(srcType)) {
            LOG_VFY("VFY: register1 v%u type %d, wanted ref", vsrc, srcType);
            *pFailure = VERIFY_ERROR_GENERIC;
            break;
        }
        if (regTypeIsUninitReference(srcType)) {
            LOG_VFY("VFY: register1 v%u holds uninitialized ref", vsrc);
            *pFailure = VERIFY_ERROR_GENERIC;
            break;
        }
        /* if the register isn't Zero, make sure it's an instance of check */
        if (srcType != kRegTypeZero) {
            ClassObject* srcClass = regTypeInitializedReferenceToClass(srcType);
            ClassObject* checkClass = regTypeInitializedReferenceToClass(checkType);
            assert(srcClass != NULL);
            assert(checkClass != NULL);

            if (dvmIsInterfaceClass(checkClass)) {
                /*
                 * All objects implement all interfaces as far as the
                 * verifier is concerned.  The runtime has to sort it out.
                 * See comments above findCommonSuperclass.
                 */
                /*
                if (srcClass != checkClass &&
                    !dvmImplements(srcClass, checkClass))
                {
                    LOG_VFY("VFY: %s does not implement %s",
                            srcClass->descriptor, checkClass->descriptor);
                    *pFailure = VERIFY_ERROR_GENERIC;
                }
                */
            } else {
                if (!dvmInstanceof(srcClass, checkClass)) {
                    LOG_VFY("VFY: %s is not instance of %s",
                            srcClass->descriptor, checkClass->descriptor);
                    *pFailure = VERIFY_ERROR_GENERIC;
                }
            }
        }
        break;
    }
}

/*
 * Set the type of the "result" register.
 */
static void setResultRegisterType(RegisterLine* registerLine,
    const int insnRegCount, RegType newType)
{
    setRegisterType(registerLine, RESULT_REGISTER(insnRegCount), newType);
}


/*
 * Update all registers holding "uninitType" to instead hold the
 * corresponding initialized reference type.  This is called when an
 * appropriate <init> method is invoked -- all copies of the reference
 * must be marked as initialized.
 */
static void markRefsAsInitialized(RegisterLine* registerLine, int insnRegCount,
    UninitInstanceMap* uninitMap, RegType uninitType, VerifyError* pFailure)
{
    RegType* insnRegs = registerLine->regTypes;
    ClassObject* clazz;
    RegType initType;
    int i, changed;

    clazz = getUninitInstance(uninitMap, regTypeToUninitIndex(uninitType));
    if (clazz == NULL) {
        ALOGE("VFY: unable to find type=%#x (idx=%d)",
            uninitType, regTypeToUninitIndex(uninitType));
        *pFailure = VERIFY_ERROR_GENERIC;
        return;
    }
    initType = regTypeFromClass(clazz);

    changed = 0;
    for (i = 0; i < insnRegCount; i++) {
        if (insnRegs[i] == uninitType) {
            insnRegs[i] = initType;
            changed++;
        }
    }
    //ALOGD("VFY: marked %d registers as initialized", changed);
    assert(changed > 0);

    return;
}

/*
 * We're creating a new instance of class C at address A.  Any registers
 * holding instances previously created at address A must be initialized
 * by now.  If not, we mark them as "conflict" to prevent them from being
 * used (otherwise, markRefsAsInitialized would mark the old ones and the
 * new ones at the same time).
 */
static void markUninitRefsAsInvalid(RegisterLine* registerLine,
    int insnRegCount, UninitInstanceMap* uninitMap, RegType uninitType)
{
    RegType* insnRegs = registerLine->regTypes;
    int i, changed;

    changed = 0;
    for (i = 0; i < insnRegCount; i++) {
        if (insnRegs[i] == uninitType) {
            insnRegs[i] = kRegTypeConflict;
            if (registerLine->monitorEntries != NULL)
                registerLine->monitorEntries[i] = 0;
            changed++;
        }
    }

    //if (changed)
    //    ALOGD("VFY: marked %d uninitialized registers as invalid", changed);
}

/*
 * Find the register line for the specified instruction in the current method.
 */
static inline RegisterLine* getRegisterLine(const RegisterTable* regTable,
    int insnIdx)
{
    return &regTable->registerLines[insnIdx];
}

/*
 * Copy a register line.
 */
static inline void copyRegisterLine(RegisterLine* dst, const RegisterLine* src,
    size_t numRegs)
{
    memcpy(dst->regTypes, src->regTypes, numRegs * sizeof(RegType));

    assert((src->monitorEntries == NULL && dst->monitorEntries == NULL) ||
           (src->monitorEntries != NULL && dst->monitorEntries != NULL));
    if (dst->monitorEntries != NULL) {
        assert(dst->monitorStack != NULL);
        memcpy(dst->monitorEntries, src->monitorEntries,
            numRegs * sizeof(MonitorEntries));
        memcpy(dst->monitorStack, src->monitorStack,
            kMaxMonitorStackDepth * sizeof(u4));
        dst->monitorStackTop = src->monitorStackTop;
    }
}

/*
 * Copy a register line into the table.
 */
static inline void copyLineToTable(RegisterTable* regTable, int insnIdx,
    const RegisterLine* src)
{
    RegisterLine* dst = getRegisterLine(regTable, insnIdx);
    assert(dst->regTypes != NULL);
    copyRegisterLine(dst, src, regTable->insnRegCountPlus);
}

/*
 * Copy a register line out of the table.
 */
static inline void copyLineFromTable(RegisterLine* dst,
    const RegisterTable* regTable, int insnIdx)
{
    RegisterLine* src = getRegisterLine(regTable, insnIdx);
    assert(src->regTypes != NULL);
    copyRegisterLine(dst, src, regTable->insnRegCountPlus);
}


#ifndef NDEBUG
/*
 * Compare two register lines.  Returns 0 if they match.
 *
 * Using this for a sort is unwise, since the value can change based on
 * machine endianness.
 */
static inline int compareLineToTable(const RegisterTable* regTable,
    int insnIdx, const RegisterLine* line2)
{
    const RegisterLine* line1 = getRegisterLine(regTable, insnIdx);
    if (line1->monitorEntries != NULL) {
        int result;

        if (line2->monitorEntries == NULL)
            return 1;
        result = memcmp(line1->monitorEntries, line2->monitorEntries,
            regTable->insnRegCountPlus * sizeof(MonitorEntries));
        if (result != 0) {
            LOG_VFY("monitorEntries mismatch");
            return result;
        }
        result = line1->monitorStackTop - line2->monitorStackTop;
        if (result != 0) {
            LOG_VFY("monitorStackTop mismatch");
            return result;
        }
        result = memcmp(line1->monitorStack, line2->monitorStack,
            line1->monitorStackTop);
        if (result != 0) {
            LOG_VFY("monitorStack mismatch");
            return result;
        }
    }
    return memcmp(line1->regTypes, line2->regTypes,
            regTable->insnRegCountPlus * sizeof(RegType));
}
#endif

/*
 * Register type categories, for type checking.
 *
 * The spec says category 1 includes boolean, byte, char, short, int, float,
 * reference, and returnAddress.  Category 2 includes long and double.
 *
 * We treat object references separately, so we have "category1nr".  We
 * don't support jsr/ret, so there is no "returnAddress" type.
 */
enum TypeCategory {
    kTypeCategoryUnknown = 0,
    kTypeCategory1nr,           // boolean, byte, char, short, int, float
    kTypeCategory2,             // long, double
    kTypeCategoryRef,           // object reference
};

/*
 * See if "type" matches "cat".  All we're really looking for here is that
 * we're not mixing and matching 32-bit and 64-bit quantities, and we're
 * not mixing references with numerics.  (For example, the arguments to
 * "a < b" could be integers of different sizes, but they must both be
 * integers.  Dalvik is less specific about int vs. float, so we treat them
 * as equivalent here.)
 *
 * For category 2 values, "type" must be the "low" half of the value.
 *
 * Sets "*pFailure" if something looks wrong.
 */
static void checkTypeCategory(RegType type, TypeCategory cat,
    VerifyError* pFailure)
{
    switch (cat) {
    case kTypeCategory1nr:
        switch (type) {
        case kRegTypeZero:
        case kRegTypeOne:
        case kRegTypeBoolean:
        case kRegTypeConstPosByte:
        case kRegTypeConstByte:
        case kRegTypeConstPosShort:
        case kRegTypeConstShort:
        case kRegTypeConstChar:
        case kRegTypeConstInteger:
        case kRegTypePosByte:
        case kRegTypeByte:
        case kRegTypePosShort:
        case kRegTypeShort:
        case kRegTypeChar:
        case kRegTypeInteger:
        case kRegTypeFloat:
            break;
        default:
            *pFailure = VERIFY_ERROR_GENERIC;
            break;
        }
        break;

    case kTypeCategory2:
        switch (type) {
        case kRegTypeConstLo:
        case kRegTypeLongLo:
        case kRegTypeDoubleLo:
            break;
        default:
            *pFailure = VERIFY_ERROR_GENERIC;
            break;
        }
        break;

    case kTypeCategoryRef:
        if (type != kRegTypeZero && !regTypeIsReference(type))
            *pFailure = VERIFY_ERROR_GENERIC;
        break;

    default:
        assert(false);
        *pFailure = VERIFY_ERROR_GENERIC;
        break;
    }
}

/*
 * For a category 2 register pair, verify that "typeh" is the appropriate
 * high part for "typel".
 *
 * Does not verify that "typel" is in fact the low part of a 64-bit
 * register pair.
 */
static void checkWidePair(RegType typel, RegType typeh, VerifyError* pFailure)
{
    if ((typeh != typel+1))
        *pFailure = VERIFY_ERROR_GENERIC;
}

/*
 * Implement category-1 "move" instructions.  Copy a 32-bit value from
 * "vsrc" to "vdst".
 */
static void copyRegister1(RegisterLine* registerLine, u4 vdst, u4 vsrc,
    TypeCategory cat, VerifyError* pFailure)
{
    assert(cat == kTypeCategory1nr || cat == kTypeCategoryRef);
    RegType type = getRegisterType(registerLine, vsrc);
    checkTypeCategory(type, cat, pFailure);
    if (!VERIFY_OK(*pFailure)) {
        LOG_VFY("VFY: copy1 v%u<-v%u type=%d cat=%d", vdst, vsrc, type, cat);
    } else {
        setRegisterType(registerLine, vdst, type);
        if (cat == kTypeCategoryRef && registerLine->monitorEntries != NULL) {
            registerLine->monitorEntries[vdst] =
                registerLine->monitorEntries[vsrc];
        }
    }
}

/*
 * Implement category-2 "move" instructions.  Copy a 64-bit value from
 * "vsrc" to "vdst".  This copies both halves of the register.
 */
static void copyRegister2(RegisterLine* registerLine, u4 vdst, u4 vsrc,
    VerifyError* pFailure)
{
    RegType typel = getRegisterType(registerLine, vsrc);
    RegType typeh = getRegisterType(registerLine, vsrc+1);

    checkTypeCategory(typel, kTypeCategory2, pFailure);
    checkWidePair(typel, typeh, pFailure);
    if (!VERIFY_OK(*pFailure)) {
        LOG_VFY("VFY: copy2 v%u<-v%u type=%d/%d", vdst, vsrc, typel, typeh);
    } else {
        setRegisterType(registerLine, vdst, typel);
        /* target monitor stack bits will be cleared */
    }
}

/*
 * Implement "move-result".  Copy the category-1 value from the result
 * register to another register, and reset the result register.
 */
static void copyResultRegister1(RegisterLine* registerLine,
    const int insnRegCount, u4 vdst, TypeCategory cat, VerifyError* pFailure)
{
    RegType type;
    u4 vsrc;

    assert(vdst < (u4) insnRegCount);

    vsrc = RESULT_REGISTER(insnRegCount);
    type = getRegisterType(registerLine, vsrc);
    checkTypeCategory(type, cat, pFailure);
    if (!VERIFY_OK(*pFailure)) {
        LOG_VFY("VFY: copyRes1 v%u<-v%u cat=%d type=%d",
            vdst, vsrc, cat, type);
    } else {
        setRegisterType(registerLine, vdst, type);
        setRegisterType(registerLine, vsrc, kRegTypeUnknown);
        /* target monitor stack bits will be cleared */
    }
}

/*
 * Implement "move-result-wide".  Copy the category-2 value from the result
 * register to another register, and reset the result register.
 */
static void copyResultRegister2(RegisterLine* registerLine,
    const int insnRegCount, u4 vdst, VerifyError* pFailure)
{
    RegType typel, typeh;
    u4 vsrc;

    assert(vdst < (u4) insnRegCount);

    vsrc = RESULT_REGISTER(insnRegCount);
    typel = getRegisterType(registerLine, vsrc);
    typeh = getRegisterType(registerLine, vsrc+1);
    checkTypeCategory(typel, kTypeCategory2, pFailure);
    checkWidePair(typel, typeh, pFailure);
    if (!VERIFY_OK(*pFailure)) {
        LOG_VFY("VFY: copyRes2 v%u<-v%u type=%d/%d",
            vdst, vsrc, typel, typeh);
    } else {
        setRegisterType(registerLine, vdst, typel);
        setRegisterType(registerLine, vsrc, kRegTypeUnknown);
        setRegisterType(registerLine, vsrc+1, kRegTypeUnknown);
        /* target monitor stack bits will be cleared */
    }
}

/*
 * Verify types for a simple two-register instruction (e.g. "neg-int").
 * "dstType" is stored into vA, and "srcType" is verified against vB.
 */
static void checkUnop(RegisterLine* registerLine, DecodedInstruction* pDecInsn,
    RegType dstType, RegType srcType, VerifyError* pFailure)
{
    verifyRegisterType(registerLine, pDecInsn->vB, srcType, pFailure);
    setRegisterType(registerLine, pDecInsn->vA, dstType);
}

/*
 * We're performing an operation like "and-int/2addr" that can be
 * performed on booleans as well as integers.  We get no indication of
 * boolean-ness, but we can infer it from the types of the arguments.
 *
 * Assumes we've already validated reg1/reg2.
 *
 * TODO: consider generalizing this.  The key principle is that the
 * result of a bitwise operation can only be as wide as the widest of
 * the operands.  You can safely AND/OR/XOR two chars together and know
 * you still have a char, so it's reasonable for the compiler or "dx"
 * to skip the int-to-char instruction.  (We need to do this for boolean
 * because there is no int-to-boolean operation.)
 *
 * Returns true if both args are Boolean, Zero, or One.
 */
static bool upcastBooleanOp(RegisterLine* registerLine, u4 reg1, u4 reg2)
{
    RegType type1, type2;

    type1 = getRegisterType(registerLine, reg1);
    type2 = getRegisterType(registerLine, reg2);

    if ((type1 == kRegTypeBoolean || type1 == kRegTypeZero ||
            type1 == kRegTypeOne) &&
        (type2 == kRegTypeBoolean || type2 == kRegTypeZero ||
            type2 == kRegTypeOne))
    {
        return true;
    }
    return false;
}

/*
 * Verify types for A two-register instruction with a literal constant
 * (e.g. "add-int/lit8").  "dstType" is stored into vA, and "srcType" is
 * verified against vB.
 *
 * If "checkBooleanOp" is set, we use the constant value in vC.
 */
static void checkLitop(RegisterLine* registerLine, DecodedInstruction* pDecInsn,
    RegType dstType, RegType srcType, bool checkBooleanOp,
    VerifyError* pFailure)
{
    verifyRegisterType(registerLine, pDecInsn->vB, srcType, pFailure);
    if (VERIFY_OK(*pFailure) && checkBooleanOp) {
        assert(dstType == kRegTypeInteger);
        /* check vB with the call, then check the constant manually */
        if (upcastBooleanOp(registerLine, pDecInsn->vB, pDecInsn->vB)
            && (pDecInsn->vC == 0 || pDecInsn->vC == 1))
        {
            dstType = kRegTypeBoolean;
        }
    }
    setRegisterType(registerLine, pDecInsn->vA, dstType);
}

/*
 * Verify types for a simple three-register instruction (e.g. "add-int").
 * "dstType" is stored into vA, and "srcType1"/"srcType2" are verified
 * against vB/vC.
 */
static void checkBinop(RegisterLine* registerLine, DecodedInstruction* pDecInsn,
    RegType dstType, RegType srcType1, RegType srcType2, bool checkBooleanOp,
    VerifyError* pFailure)
{
    verifyRegisterType(registerLine, pDecInsn->vB, srcType1, pFailure);
    verifyRegisterType(registerLine, pDecInsn->vC, srcType2, pFailure);
    if (VERIFY_OK(*pFailure) && checkBooleanOp) {
        assert(dstType == kRegTypeInteger);
        if (upcastBooleanOp(registerLine, pDecInsn->vB, pDecInsn->vC))
            dstType = kRegTypeBoolean;
    }
    setRegisterType(registerLine, pDecInsn->vA, dstType);
}

/*
 * Verify types for a binary "2addr" operation.  "srcType1"/"srcType2"
 * are verified against vA/vB, then "dstType" is stored into vA.
 */
static void checkBinop2addr(RegisterLine* registerLine,
    DecodedInstruction* pDecInsn, RegType dstType, RegType srcType1,
    RegType srcType2, bool checkBooleanOp, VerifyError* pFailure)
{
    verifyRegisterType(registerLine, pDecInsn->vA, srcType1, pFailure);
    verifyRegisterType(registerLine, pDecInsn->vB, srcType2, pFailure);
    if (VERIFY_OK(*pFailure) && checkBooleanOp) {
        assert(dstType == kRegTypeInteger);
        if (upcastBooleanOp(registerLine, pDecInsn->vA, pDecInsn->vB))
            dstType = kRegTypeBoolean;
    }
    setRegisterType(registerLine, pDecInsn->vA, dstType);
}

/*
 * Treat right-shifting as a narrowing conversion when possible.
 *
 * For example, right-shifting an int 24 times results in a value that can
 * be treated as a byte.
 *
 * Things get interesting when contemplating sign extension.  Right-
 * shifting an integer by 16 yields a value that can be represented in a
 * "short" but not a "char", but an unsigned right shift by 16 yields a
 * value that belongs in a char rather than a short.  (Consider what would
 * happen if the result of the shift were cast to a char or short and then
 * cast back to an int.  If sign extension, or the lack thereof, causes
 * a change in the 32-bit representation, then the conversion was lossy.)
 *
 * A signed right shift by 17 on an integer results in a short.  An unsigned
 * right shfit by 17 on an integer results in a posshort, which can be
 * assigned to a short or a char.
 *
 * An unsigned right shift on a short can actually expand the result into
 * a 32-bit integer.  For example, 0xfffff123 >>> 8 becomes 0x00fffff1,
 * which can't be represented in anything smaller than an int.
 *
 * javac does not generate code that takes advantage of this, but some
 * of the code optimizers do.  It's generally a peephole optimization
 * that replaces a particular sequence, e.g. (bipush 24, ishr, i2b) is
 * replaced by (bipush 24, ishr).  Knowing that shifting a short 8 times
 * to the right yields a byte is really more than we need to handle the
 * code that's out there, but support is not much more complex than just
 * handling integer.
 *
 * Right-shifting never yields a boolean value.
 *
 * Returns the new register type.
 */
static RegType adjustForRightShift(RegisterLine* registerLine, int reg,
    unsigned int shiftCount, bool isUnsignedShift, VerifyError* pFailure)
{
    RegType srcType = getRegisterType(registerLine, reg);
    RegType newType;

    /* convert const derived types to their actual types */
    srcType = constTypeToRegType(srcType);

    /* no-op */
    if (shiftCount == 0)
        return srcType;

    /* safe defaults */
    if (isUnsignedShift)
        newType = kRegTypeInteger;
    else
        newType = srcType;

    if (shiftCount >= 32) {
        LOG_VFY("Got unexpectedly large shift count %u", shiftCount);
        /* fail? */
        return newType;
    }

    switch (srcType) {
    case kRegTypeInteger:               /* 32-bit signed value */
        if (isUnsignedShift) {
            if (shiftCount > 24)
                newType = kRegTypePosByte;
            else if (shiftCount >= 16)
                newType = kRegTypeChar;
        } else {
            if (shiftCount >= 24)
                newType = kRegTypeByte;
            else if (shiftCount >= 16)
                newType = kRegTypeShort;
        }
        break;
    case kRegTypeShort:                 /* 16-bit signed value */
        if (isUnsignedShift) {
            /* default (kRegTypeInteger) is correct */
        } else {
            if (shiftCount >= 8)
                newType = kRegTypeByte;
        }
        break;
    case kRegTypePosShort:              /* 15-bit unsigned value */
        if (shiftCount >= 8)
            newType = kRegTypePosByte;
        break;
    case kRegTypeChar:                  /* 16-bit unsigned value */
        if (shiftCount > 8)
            newType = kRegTypePosByte;
        break;
    case kRegTypeByte:                  /* 8-bit signed value */
        /* defaults (u=kRegTypeInteger / s=srcType) are correct */
        break;
    case kRegTypePosByte:               /* 7-bit unsigned value */
        /* always use newType=srcType */
        newType = srcType;
        break;
    case kRegTypeZero:                  /* 1-bit unsigned value */
    case kRegTypeOne:
    case kRegTypeBoolean:
        /* unnecessary? */
        newType = kRegTypeZero;
        break;
    default:
        /* long, double, references; shouldn't be here! */
        assert(false);
        break;
    }

    if (newType != srcType) {
        LOGVV("narrowing: %d(%d) --> %d to %d",
            shiftCount, isUnsignedShift, srcType, newType);
    } else {
        LOGVV("not narrowed: %d(%d) --> %d",
            shiftCount, isUnsignedShift, srcType);
    }
    return newType;
}


/*
 * ===========================================================================
 *      Register merge
 * ===========================================================================
 */

/*
 * Compute the "class depth" of a class.  This is the distance from the
 * class to the top of the tree, chasing superclass links.  java.lang.Object
 * has a class depth of 0.
 */
static int getClassDepth(ClassObject* clazz)
{
    int depth = 0;

    while (clazz->super != NULL) {
        clazz = clazz->super;
        depth++;
    }
    return depth;
}

/*
 * Given two classes, walk up the superclass tree to find a common
 * ancestor.  (Called from findCommonSuperclass().)
 *
 * TODO: consider caching the class depth in the class object so we don't
 * have to search for it here.
 */
static ClassObject* digForSuperclass(ClassObject* c1, ClassObject* c2)
{
    int depth1, depth2;

    depth1 = getClassDepth(c1);
    depth2 = getClassDepth(c2);

    if (gDebugVerbose) {
        LOGVV("COMMON: %s(%d) + %s(%d)",
            c1->descriptor, depth1, c2->descriptor, depth2);
    }

    /* pull the deepest one up */
    if (depth1 > depth2) {
        while (depth1 > depth2) {
            c1 = c1->super;
            depth1--;
        }
    } else {
        while (depth2 > depth1) {
            c2 = c2->super;
            depth2--;
        }
    }

    /* walk up in lock-step */
    while (c1 != c2) {
        c1 = c1->super;
        c2 = c2->super;

        assert(c1 != NULL && c2 != NULL);
    }

    if (gDebugVerbose) {
        LOGVV("      : --> %s", c1->descriptor);
    }
    return c1;
}

/*
 * Merge two array classes.  We can't use the general "walk up to the
 * superclass" merge because the superclass of an array is always Object.
 * We want String[] + Integer[] = Object[].  This works for higher dimensions
 * as well, e.g. String[][] + Integer[][] = Object[][].
 *
 * If Foo1 and Foo2 are subclasses of Foo, Foo1[] + Foo2[] = Foo[].
 *
 * If Class implements Type, Class[] + Type[] = Type[].
 *
 * If the dimensions don't match, we want to convert to an array of Object
 * with the least dimension, e.g. String[][] + String[][][][] = Object[][].
 *
 * Arrays of primitive types effectively have one less dimension when
 * merging.  int[] + float[] = Object, int[] + String[] = Object,
 * int[][] + float[][] = Object[], int[][] + String[] = Object[].  (The
 * only time this function doesn't return an array class is when one of
 * the arguments is a 1-dimensional primitive array.)
 *
 * This gets a little awkward because we may have to ask the VM to create
 * a new array type with the appropriate element and dimensions.  However, we
 * shouldn't be doing this often.
 */
static ClassObject* findCommonArraySuperclass(ClassObject* c1, ClassObject* c2)
{
    ClassObject* arrayClass = NULL;
    ClassObject* commonElem;
    int arrayDim1, arrayDim2;
    int i, numDims;
    bool hasPrimitive = false;

    arrayDim1 = c1->arrayDim;
    arrayDim2 = c2->arrayDim;
    assert(c1->arrayDim > 0);
    assert(c2->arrayDim > 0);

    if (dvmIsPrimitiveClass(c1->elementClass)) {
        arrayDim1--;
        hasPrimitive = true;
    }
    if (dvmIsPrimitiveClass(c2->elementClass)) {
        arrayDim2--;
        hasPrimitive = true;
    }

    if (!hasPrimitive && arrayDim1 == arrayDim2) {
        /*
         * Two arrays of reference types with equal dimensions.  Try to
         * find a good match.
         */
        commonElem = findCommonSuperclass(c1->elementClass, c2->elementClass);
        numDims = arrayDim1;
    } else {
        /*
         * Mismatched array depths and/or array(s) of primitives.  We want
         * Object, or an Object array with appropriate dimensions.
         *
         * We initialize arrayClass to Object here, because it's possible
         * for us to set numDims=0.
         */
        if (arrayDim1 < arrayDim2)
            numDims = arrayDim1;
        else
            numDims = arrayDim2;
        arrayClass = commonElem = c1->super;     // == java.lang.Object
    }

    /*
     * Find an appropriately-dimensioned array class.  This is easiest
     * to do iteratively, using the array class found by the current round
     * as the element type for the next round.
     */
    for (i = 0; i < numDims; i++) {
        arrayClass = dvmFindArrayClassForElement(commonElem);
        commonElem = arrayClass;
    }
    assert(arrayClass != NULL);

    LOGVV("ArrayMerge '%s' + '%s' --> '%s'",
        c1->descriptor, c2->descriptor, arrayClass->descriptor);
    return arrayClass;
}

/*
 * Find the first common superclass of the two classes.  We're not
 * interested in common interfaces.
 *
 * The easiest way to do this for concrete classes is to compute the "class
 * depth" of each, move up toward the root of the deepest one until they're
 * at the same depth, then walk both up to the root until they match.
 *
 * If both classes are arrays, we need to merge based on array depth and
 * element type.
 *
 * If one class is an interface, we check to see if the other class/interface
 * (or one of its predecessors) implements the interface.  If so, we return
 * the interface; otherwise, we return Object.
 *
 * NOTE: we continue the tradition of "lazy interface handling".  To wit,
 * suppose we have three classes:
 *   One implements Fancy, Free
 *   Two implements Fancy, Free
 *   Three implements Free
 * where Fancy and Free are unrelated interfaces.  The code requires us
 * to merge One into Two.  Ideally we'd use a common interface, which
 * gives us a choice between Fancy and Free, and no guidance on which to
 * use.  If we use Free, we'll be okay when Three gets merged in, but if
 * we choose Fancy, we're hosed.  The "ideal" solution is to create a
 * set of common interfaces and carry that around, merging further references
 * into it.  This is a pain.  The easy solution is to simply boil them
 * down to Objects and let the runtime invokeinterface call fail, which
 * is what we do.
 */
static ClassObject* findCommonSuperclass(ClassObject* c1, ClassObject* c2)
{
    assert(!dvmIsPrimitiveClass(c1) && !dvmIsPrimitiveClass(c2));

    if (c1 == c2)
        return c1;

    if (dvmIsInterfaceClass(c1) && dvmImplements(c2, c1)) {
        if (gDebugVerbose)
            LOGVV("COMMON/I1: %s + %s --> %s",
                c1->descriptor, c2->descriptor, c1->descriptor);
        return c1;
    }
    if (dvmIsInterfaceClass(c2) && dvmImplements(c1, c2)) {
        if (gDebugVerbose)
            LOGVV("COMMON/I2: %s + %s --> %s",
                c1->descriptor, c2->descriptor, c2->descriptor);
        return c2;
    }

    if (dvmIsArrayClass(c1) && dvmIsArrayClass(c2)) {
        return findCommonArraySuperclass(c1, c2);
    }

    return digForSuperclass(c1, c2);
}

/*
 * Merge two RegType values.
 *
 * Sets "*pChanged" to "true" if the result doesn't match "type1".
 */
static RegType mergeTypes(RegType type1, RegType type2, bool* pChanged)
{
    RegType result;

    /*
     * Check for trivial case so we don't have to hit memory.
     */
    if (type1 == type2)
        return type1;

    /*
     * Use the table if we can, and reject any attempts to merge something
     * from the table with a reference type.
     *
     * Uninitialized references are composed of the enum ORed with an
     * index value.  The uninitialized table entry at index zero *will*
     * show up as a simple kRegTypeUninit value.  Since this cannot be
     * merged with anything but itself, the rules do the right thing.
     */
    if (type1 < kRegTypeMAX) {
        if (type2 < kRegTypeMAX) {
            result = gDvmMergeTab[type1][type2];
        } else {
            /* simple + reference == conflict, usually */
            if (type1 == kRegTypeZero)
                result = type2;
            else
                result = kRegTypeConflict;
        }
    } else {
        if (type2 < kRegTypeMAX) {
            /* reference + simple == conflict, usually */
            if (type2 == kRegTypeZero)
                result = type1;
            else
                result = kRegTypeConflict;
        } else {
            /* merging two references */
            if (regTypeIsUninitReference(type1) ||
                regTypeIsUninitReference(type2))
            {
                /* can't merge uninit with anything but self */
                result = kRegTypeConflict;
            } else {
                ClassObject* clazz1 = regTypeInitializedReferenceToClass(type1);
                ClassObject* clazz2 = regTypeInitializedReferenceToClass(type2);
                ClassObject* mergedClass;

                mergedClass = findCommonSuperclass(clazz1, clazz2);
                assert(mergedClass != NULL);
                result = regTypeFromClass(mergedClass);
            }
        }
    }

    if (result != type1)
        *pChanged = true;
    return result;
}

/*
 * Merge the bits that indicate which monitor entry addresses on the stack
 * are associated with this register.
 *
 * The merge is a simple bitwise AND.
 *
 * Sets "*pChanged" to "true" if the result doesn't match "ents1".
 */
static MonitorEntries mergeMonitorEntries(MonitorEntries ents1,
    MonitorEntries ents2, bool* pChanged)
{
    MonitorEntries result = ents1 & ents2;
    if (result != ents1)
        *pChanged = true;
    return result;
}

/*
 * Control can transfer to "nextInsn".
 *
 * Merge the registers from "workLine" into "regTable" at "nextInsn", and
 * set the "changed" flag on the target address if any of the registers
 * has changed.
 *
 * Returns "false" if we detect mis-matched monitor stacks.
 */
static bool updateRegisters(const Method* meth, InsnFlags* insnFlags,
    RegisterTable* regTable, int nextInsn, const RegisterLine* workLine)
{
    const size_t insnRegCountPlus = regTable->insnRegCountPlus;
    assert(workLine != NULL);
    const RegType* workRegs = workLine->regTypes;

    if (!dvmInsnIsVisitedOrChanged(insnFlags, nextInsn)) {
        /*
         * We haven't processed this instruction before, and we haven't
         * touched the registers here, so there's nothing to "merge".  Copy
         * the registers over and mark it as changed.  (This is the only
         * way a register can transition out of "unknown", so this is not
         * just an optimization.)
         */
        LOGVV("COPY into 0x%04x", nextInsn);
        copyLineToTable(regTable, nextInsn, workLine);
        dvmInsnSetChanged(insnFlags, nextInsn, true);
#ifdef VERIFIER_STATS
        gDvm.verifierStats.copyRegCount++;
#endif
    } else {
        if (gDebugVerbose) {
            LOGVV("MERGE into 0x%04x", nextInsn);
            //dumpRegTypes(vdata, targetRegs, 0, "targ", NULL, 0);
            //dumpRegTypes(vdata, workRegs, 0, "work", NULL, 0);
        }
        /* merge registers, set Changed only if different */
        RegisterLine* targetLine = getRegisterLine(regTable, nextInsn);
        RegType* targetRegs = targetLine->regTypes;
        MonitorEntries* workMonEnts = workLine->monitorEntries;
        MonitorEntries* targetMonEnts = targetLine->monitorEntries;
        bool changed = false;
        unsigned int idx;

        assert(targetRegs != NULL);

        if (targetMonEnts != NULL) {
            /*
             * Monitor stacks must be identical.
             */
            if (targetLine->monitorStackTop != workLine->monitorStackTop) {
                LOG_VFY_METH(meth,
                    "VFY: mismatched stack depth %d vs. %d at 0x%04x",
                    targetLine->monitorStackTop, workLine->monitorStackTop,
                    nextInsn);
                return false;
            }
            if (memcmp(targetLine->monitorStack, workLine->monitorStack,
                    targetLine->monitorStackTop * sizeof(u4)) != 0)
            {
                LOG_VFY_METH(meth, "VFY: mismatched monitor stacks at 0x%04x",
                    nextInsn);
                return false;
            }
        }

        for (idx = 0; idx < insnRegCountPlus; idx++) {
            targetRegs[idx] =
                    mergeTypes(targetRegs[idx], workRegs[idx], &changed);

            if (targetMonEnts != NULL) {
                targetMonEnts[idx] = mergeMonitorEntries(targetMonEnts[idx],
                    workMonEnts[idx], &changed);
            }
        }

        if (gDebugVerbose) {
            //ALOGI(" RESULT (changed=%d)", changed);
            //dumpRegTypes(vdata, targetRegs, 0, "rslt", NULL, 0);
        }
#ifdef VERIFIER_STATS
        gDvm.verifierStats.mergeRegCount++;
        if (changed)
            gDvm.verifierStats.mergeRegChanged++;
#endif

        if (changed)
            dvmInsnSetChanged(insnFlags, nextInsn, true);
    }

    return true;
}


/*
 * ===========================================================================
 *      Utility functions
 * ===========================================================================
 */

/*
 * Look up an instance field, specified by "fieldIdx", that is going to be
 * accessed in object "objType".  This resolves the field and then verifies
 * that the class containing the field is an instance of the reference in
 * "objType".
 *
 * It is possible for "objType" to be kRegTypeZero, meaning that we might
 * have a null reference.  This is a runtime problem, so we allow it,
 * skipping some of the type checks.
 *
 * In general, "objType" must be an initialized reference.  However, we
 * allow it to be uninitialized if this is an "<init>" method and the field
 * is declared within the "objType" class.
 *
 * Returns an InstField on success, returns NULL and sets "*pFailure"
 * on failure.
 */
static InstField* getInstField(const Method* meth,
    const UninitInstanceMap* uninitMap, RegType objType, int fieldIdx,
    VerifyError* pFailure)
{
    InstField* instField = NULL;
    ClassObject* objClass;
    bool mustBeLocal = false;

    if (!regTypeIsReference(objType)) {
        LOG_VFY("VFY: attempt to access field in non-reference type %d",
            objType);
        *pFailure = VERIFY_ERROR_GENERIC;
        goto bail;
    }

    instField = dvmOptResolveInstField(meth->clazz, fieldIdx, pFailure);
    if (instField == NULL) {
        LOG_VFY("VFY: unable to resolve instance field %u", fieldIdx);
        assert(!VERIFY_OK(*pFailure));
        goto bail;
    }

    if (objType == kRegTypeZero)
        goto bail;

    /*
     * Access to fields in uninitialized objects is allowed if this is
     * the <init> method for the object and the field in question is
     * declared by this class.
     */
    objClass = regTypeReferenceToClass(objType, uninitMap);
    assert(objClass != NULL);
    if (regTypeIsUninitReference(objType)) {
        if (!isInitMethod(meth) || meth->clazz != objClass) {
            LOG_VFY("VFY: attempt to access field via uninitialized ref");
            *pFailure = VERIFY_ERROR_GENERIC;
            goto bail;
        }
        mustBeLocal = true;
    }

    if (!dvmInstanceof(objClass, instField->clazz)) {
        LOG_VFY("VFY: invalid field access (field %s.%s, through %s ref)",
                instField->clazz->descriptor, instField->name,
                objClass->descriptor);
        *pFailure = VERIFY_ERROR_NO_FIELD;
        goto bail;
    }

    if (mustBeLocal) {
        /* for uninit ref, make sure it's defined by this class, not super */
        if (instField < objClass->ifields ||
            instField >= objClass->ifields + objClass->ifieldCount)
        {
            LOG_VFY("VFY: invalid constructor field access (field %s in %s)",
                    instField->name, objClass->descriptor);
            *pFailure = VERIFY_ERROR_GENERIC;
            goto bail;
        }
    }

bail:
    return instField;
}

/*
 * Look up a static field.
 *
 * Returns a StaticField on success, returns NULL and sets "*pFailure"
 * on failure.
 */
static StaticField* getStaticField(const Method* meth, int fieldIdx,
    VerifyError* pFailure)
{
    StaticField* staticField;

    staticField = dvmOptResolveStaticField(meth->clazz, fieldIdx, pFailure);
    if (staticField == NULL) {
        DexFile* pDexFile = meth->clazz->pDvmDex->pDexFile;
        const DexFieldId* pFieldId;

        pFieldId = dexGetFieldId(pDexFile, fieldIdx);

        LOG_VFY("VFY: unable to resolve static field %u (%s) in %s", fieldIdx,
            dexStringById(pDexFile, pFieldId->nameIdx),
            dexStringByTypeIdx(pDexFile, pFieldId->classIdx));
        assert(!VERIFY_OK(*pFailure));
        goto bail;
    }

bail:
    return staticField;
}

/*
 * If "field" is marked "final", make sure this is the either <clinit>
 * or <init> as appropriate.
 *
 * Sets "*pFailure" on failure.
 */
static void checkFinalFieldAccess(const Method* meth, const Field* field,
    VerifyError* pFailure)
{
    if (!dvmIsFinalField(field))
        return;

    /* make sure we're in the same class */
    if (meth->clazz != field->clazz) {
        LOG_VFY_METH(meth, "VFY: can't modify final field %s.%s",
            field->clazz->descriptor, field->name);
        *pFailure = VERIFY_ERROR_ACCESS_FIELD;
        return;
    }

    /*
     * The VM spec descriptions of putfield and putstatic say that
     * IllegalAccessError is only thrown when the instructions appear
     * outside the declaring class.  Our earlier attempts to restrict
     * final field modification to constructors are, therefore, wrong.
     */
#if 0
    /* make sure we're in the right kind of constructor */
    if (dvmIsStaticField(field)) {
        if (!isClassInitMethod(meth)) {
            LOG_VFY_METH(meth,
                "VFY: can't modify final static field outside <clinit>");
            *pFailure = VERIFY_ERROR_GENERIC;
        }
    } else {
        if (!isInitMethod(meth)) {
            LOG_VFY_METH(meth,
                "VFY: can't modify final field outside <init>");
            *pFailure = VERIFY_ERROR_GENERIC;
        }
    }
#endif
}

/*
 * Make sure that the register type is suitable for use as an array index.
 *
 * Sets "*pFailure" if not.
 */
static void checkArrayIndexType(const Method* meth, RegType regType,
    VerifyError* pFailure)
{
    if (VERIFY_OK(*pFailure)) {
        /*
         * The 1nr types are interchangeable at this level. However,
         * check that a float is not used as the index.
         */
        checkTypeCategory(regType, kTypeCategory1nr, pFailure);
        if (regType == kRegTypeFloat) {
          *pFailure = VERIFY_ERROR_GENERIC;
        }
        if (!VERIFY_OK(*pFailure)) {
            LOG_VFY_METH(meth, "Invalid reg type for array index (%d)",
                regType);
        }
    }
}

/*
 * Check constraints on constructor return.  Specifically, make sure that
 * the "this" argument got initialized.
 *
 * The "this" argument to <init> uses code offset kUninitThisArgAddr, which
 * puts it at the start of the list in slot 0.  If we see a register with
 * an uninitialized slot 0 reference, we know it somehow didn't get
 * initialized.
 *
 * Returns "true" if all is well.
 */
static bool checkConstructorReturn(const Method* meth,
    const RegisterLine* registerLine, const int insnRegCount)
{
    const RegType* insnRegs = registerLine->regTypes;
    int i;

    if (!isInitMethod(meth))
        return true;

    RegType uninitThis = regTypeFromUninitIndex(kUninitThisArgSlot);

    for (i = 0; i < insnRegCount; i++) {
        if (insnRegs[i] == uninitThis) {
            LOG_VFY("VFY: <init> returning without calling superclass init");
            return false;
        }
    }
    return true;
}

/*
 * Verify that the target instruction is not "move-exception".  It's important
 * that the only way to execute a move-exception is as the first instruction
 * of an exception handler.
 *
 * Returns "true" if all is well, "false" if the target instruction is
 * move-exception.
 */
static bool checkMoveException(const Method* meth, int insnIdx,
    const char* logNote)
{
    assert(insnIdx >= 0 && insnIdx < (int)dvmGetMethodInsnsSize(meth));

    if ((meth->insns[insnIdx] & 0xff) == OP_MOVE_EXCEPTION) {
        LOG_VFY("VFY: invalid use of move-exception");
        return false;
    }
    return true;
}

/*
 * For the "move-exception" instruction at "insnIdx", which must be at an
 * exception handler address, determine the first common superclass of
 * all exceptions that can land here.  (For javac output, we're probably
 * looking at multiple spans of bytecode covered by one "try" that lands
 * at an exception-specific "catch", but in general the handler could be
 * shared for multiple exceptions.)
 *
 * Returns NULL if no matching exception handler can be found, or if the
 * exception is not a subclass of Throwable.
 */
static ClassObject* getCaughtExceptionType(const Method* meth, int insnIdx,
    VerifyError* pFailure)
{
    VerifyError localFailure;
    const DexCode* pCode;
    DexFile* pDexFile;
    ClassObject* commonSuper = NULL;
    u4 handlersSize;
    u4 offset;
    u4 i;

    pDexFile = meth->clazz->pDvmDex->pDexFile;
    pCode = dvmGetMethodCode(meth);

    if (pCode->triesSize != 0) {
        handlersSize = dexGetHandlersSize(pCode);
        offset = dexGetFirstHandlerOffset(pCode);
    } else {
        handlersSize = 0;
        offset = 0;
    }

    for (i = 0; i < handlersSize; i++) {
        DexCatchIterator iterator;
        dexCatchIteratorInit(&iterator, pCode, offset);

        for (;;) {
            const DexCatchHandler* handler = dexCatchIteratorNext(&iterator);

            if (handler == NULL) {
                break;
            }

            if (handler->address == (u4) insnIdx) {
                ClassObject* clazz;

                if (handler->typeIdx == kDexNoIndex)
                    clazz = gDvm.exThrowable;
                else
                    clazz = dvmOptResolveClass(meth->clazz, handler->typeIdx,
                                &localFailure);

                if (clazz == NULL) {
                    LOG_VFY("VFY: unable to resolve exception class %u (%s)",
                        handler->typeIdx,
                        dexStringByTypeIdx(pDexFile, handler->typeIdx));
                    /* TODO: do we want to keep going?  If we don't fail
                     * this we run the risk of having a non-Throwable
                     * introduced at runtime.  However, that won't pass
                     * an instanceof test, so is essentially harmless. */
                } else {
                    if (commonSuper == NULL)
                        commonSuper = clazz;
                    else
                        commonSuper = findCommonSuperclass(clazz, commonSuper);
                }
            }
        }

        offset = dexCatchIteratorGetEndOffset(&iterator, pCode);
    }

    if (commonSuper == NULL) {
        /* no catch blocks, or no catches with classes we can find */
        LOG_VFY_METH(meth,
            "VFY: unable to find exception handler at addr %#x", insnIdx);
        *pFailure = VERIFY_ERROR_GENERIC;
    } else {
        // TODO: verify the class is an instance of Throwable?
    }

    return commonSuper;
}

/*
 * Helper for initRegisterTable.
 *
 * Returns an updated copy of "storage".
 */
static u1* assignLineStorage(u1* storage, RegisterLine* line,
    bool trackMonitors, size_t regTypeSize, size_t monEntSize, size_t stackSize)
{
    line->regTypes = (RegType*) storage;
    storage += regTypeSize;

    if (trackMonitors) {
        line->monitorEntries = (MonitorEntries*) storage;
        storage += monEntSize;
        line->monitorStack = (u4*) storage;
        storage += stackSize;

        assert(line->monitorStackTop == 0);
    }

    return storage;
}

/*
 * Initialize the RegisterTable.
 *
 * Every instruction address can have a different set of information about
 * what's in which register, but for verification purposes we only need to
 * store it at branch target addresses (because we merge into that).
 *
 * By zeroing out the regType storage we are effectively initializing the
 * register information to kRegTypeUnknown.
 *
 * We jump through some hoops here to minimize the total number of
 * allocations we have to perform per method verified.
 */
static bool initRegisterTable(const VerifierData* vdata,
    RegisterTable* regTable, RegisterTrackingMode trackRegsFor)
{
    const Method* meth = vdata->method;
    const int insnsSize = vdata->insnsSize;
    const InsnFlags* insnFlags = vdata->insnFlags;
    const int kExtraLines = 2;  /* workLine, savedLine */
    int i;

    /*
     * Every address gets a RegisterLine struct.  This is wasteful, but
     * not so much that it's worth chasing through an extra level of
     * indirection.
     */
    regTable->insnRegCountPlus = meth->registersSize + kExtraRegs;
    regTable->registerLines =
        (RegisterLine*) calloc(insnsSize, sizeof(RegisterLine));
    if (regTable->registerLines == NULL)
        return false;

    assert(insnsSize > 0);

    /*
     * Count up the number of "interesting" instructions.
     *
     * "All" means "every address that holds the start of an instruction".
     * "Branches" and "GcPoints" mean just those addresses.
     *
     * "GcPoints" fills about half the addresses, "Branches" about 15%.
     */
    int interestingCount = kExtraLines;

    for (i = 0; i < insnsSize; i++) {
        bool interesting;

        switch (trackRegsFor) {
        case kTrackRegsAll:
            interesting = dvmInsnIsOpcode(insnFlags, i);
            break;
        case kTrackRegsGcPoints:
            interesting = dvmInsnIsGcPoint(insnFlags, i) ||
                          dvmInsnIsBranchTarget(insnFlags, i);
            break;
        case kTrackRegsBranches:
            interesting = dvmInsnIsBranchTarget(insnFlags, i);
            break;
        default:
            dvmAbort();
            return false;
        }

        if (interesting)
            interestingCount++;

        /* count instructions, for display only */
        //if (dvmInsnIsOpcode(insnFlags, i))
        //    insnCount++;
    }

    /*
     * Allocate storage for the register type arrays.
     * TODO: set trackMonitors based on global config option
     */
    size_t regTypeSize = regTable->insnRegCountPlus * sizeof(RegType);
    size_t monEntSize = regTable->insnRegCountPlus * sizeof(MonitorEntries);
    size_t stackSize = kMaxMonitorStackDepth * sizeof(u4);
    bool trackMonitors;

    if (gDvm.monitorVerification) {
        trackMonitors = (vdata->monitorEnterCount != 0);
    } else {
        trackMonitors = false;
    }

    size_t spacePerEntry = regTypeSize +
        (trackMonitors ? monEntSize + stackSize : 0);
    regTable->lineAlloc = calloc(interestingCount, spacePerEntry);
    if (regTable->lineAlloc == NULL)
        return false;

#ifdef VERIFIER_STATS
    size_t totalSpace = interestingCount * spacePerEntry +
        insnsSize * sizeof(RegisterLine);
    if (gDvm.verifierStats.biggestAlloc < totalSpace)
        gDvm.verifierStats.biggestAlloc = totalSpace;
#endif

    /*
     * Populate the sparse register line table.
     *
     * There is a RegisterLine associated with every address, but not
     * every RegisterLine has non-NULL pointers to storage for its fields.
     */
    u1* storage = (u1*)regTable->lineAlloc;
    for (i = 0; i < insnsSize; i++) {
        bool interesting;

        switch (trackRegsFor) {
        case kTrackRegsAll:
            interesting = dvmInsnIsOpcode(insnFlags, i);
            break;
        case kTrackRegsGcPoints:
            interesting = dvmInsnIsGcPoint(insnFlags, i) ||
                          dvmInsnIsBranchTarget(insnFlags, i);
            break;
        case kTrackRegsBranches:
            interesting = dvmInsnIsBranchTarget(insnFlags, i);
            break;
        default:
            dvmAbort();
            return false;
        }

        if (interesting) {
            storage = assignLineStorage(storage, &regTable->registerLines[i],
                trackMonitors, regTypeSize, monEntSize, stackSize);
        }
    }

    /*
     * Grab storage for our "temporary" register lines.
     */
    storage = assignLineStorage(storage, &regTable->workLine,
        trackMonitors, regTypeSize, monEntSize, stackSize);
    storage = assignLineStorage(storage, &regTable->savedLine,
        trackMonitors, regTypeSize, monEntSize, stackSize);

    //ALOGD("Tracking registers for [%d], total %d in %d units",
    //    trackRegsFor, interestingCount-kExtraLines, insnsSize);

    assert(storage - (u1*)regTable->lineAlloc ==
        (int) (interestingCount * spacePerEntry));
    assert(regTable->registerLines[0].regTypes != NULL);
    return true;
}

/*
 * Free up any "hairy" structures associated with register lines.
 */
static void freeRegisterLineInnards(VerifierData* vdata)
{
    unsigned int idx;

    if (vdata->registerLines == NULL)
        return;

    for (idx = 0; idx < vdata->insnsSize; idx++) {
        BitVector* liveRegs = vdata->registerLines[idx].liveRegs;
        if (liveRegs != NULL)
            dvmFreeBitVector(liveRegs);
    }
}


/*
 * Verify that the arguments in a filled-new-array instruction are valid.
 *
 * "resClass" is the class refered to by pDecInsn->vB.
 */
static void verifyFilledNewArrayRegs(const Method* meth,
    RegisterLine* registerLine, const DecodedInstruction* pDecInsn,
    ClassObject* resClass, bool isRange, VerifyError* pFailure)
{
    u4 argCount = pDecInsn->vA;
    RegType expectedType;
    PrimitiveType elemType;
    unsigned int ui;

    assert(dvmIsArrayClass(resClass));
    elemType = resClass->elementClass->primitiveType;
    if (elemType == PRIM_NOT) {
        expectedType = regTypeFromClass(resClass->elementClass);
    } else {
        expectedType = primitiveTypeToRegType(elemType);
    }
    //ALOGI("filled-new-array: %s -> %d", resClass->descriptor, expectedType);

    /*
     * Verify each register.  If "argCount" is bad, verifyRegisterType()
     * will run off the end of the list and fail.  It's legal, if silly,
     * for argCount to be zero.
     */
    for (ui = 0; ui < argCount; ui++) {
        u4 getReg;

        if (isRange)
            getReg = pDecInsn->vC + ui;
        else
            getReg = pDecInsn->arg[ui];

        verifyRegisterType(registerLine, getReg, expectedType, pFailure);
        if (!VERIFY_OK(*pFailure)) {
            LOG_VFY("VFY: filled-new-array arg %u(%u) not valid", ui, getReg);
            return;
        }
    }
}


/*
 * Replace an instruction with "throw-verification-error".  This allows us to
 * defer error reporting until the code path is first used.
 *
 * This is expected to be called during "just in time" verification, not
 * from within dexopt.  (Verification failures in dexopt will result in
 * postponement of verification to first use of the class.)
 *
 * The throw-verification-error instruction requires two code units.  Some
 * of the replaced instructions require three; the third code unit will
 * receive a "nop".  The instruction's length will be left unchanged
 * in "insnFlags".
 *
 * The VM postpones setting of debugger breakpoints in unverified classes,
 * so there should be no clashes with the debugger.
 *
 * Returns "true" on success.
 */
static bool replaceFailingInstruction(const Method* meth, InsnFlags* insnFlags,
    int insnIdx, VerifyError failure)
{
    VerifyErrorRefType refType;
    u2* oldInsns = (u2*) meth->insns + insnIdx;
    int width;

    if (gDvm.optimizing)
        ALOGD("Weird: RFI during dexopt?");

    /*
     * Generate the new instruction out of the old.
     *
     * First, make sure this is an instruction we're expecting to stomp on.
     */
    Opcode opcode = dexOpcodeFromCodeUnit(*oldInsns);
    switch (opcode) {
    case OP_CONST_CLASS:                // insn[1] == class ref, 2 bytes
    case OP_CHECK_CAST:
    case OP_INSTANCE_OF:
    case OP_NEW_INSTANCE:
    case OP_NEW_ARRAY:
    case OP_FILLED_NEW_ARRAY:           // insn[1] == class ref, 3 bytes
    case OP_FILLED_NEW_ARRAY_RANGE:
        refType = VERIFY_ERROR_REF_CLASS;
        break;

    case OP_IGET:                       // insn[1] == field ref, 2 bytes
    case OP_IGET_BOOLEAN:
    case OP_IGET_BYTE:
    case OP_IGET_CHAR:
    case OP_IGET_SHORT:
    case OP_IGET_WIDE:
    case OP_IGET_OBJECT:
    case OP_IPUT:
    case OP_IPUT_BOOLEAN:
    case OP_IPUT_BYTE:
    case OP_IPUT_CHAR:
    case OP_IPUT_SHORT:
    case OP_IPUT_WIDE:
    case OP_IPUT_OBJECT:
    case OP_SGET:
    case OP_SGET_BOOLEAN:
    case OP_SGET_BYTE:
    case OP_SGET_CHAR:
    case OP_SGET_SHORT:
    case OP_SGET_WIDE:
    case OP_SGET_OBJECT:
    case OP_SPUT:
    case OP_SPUT_BOOLEAN:
    case OP_SPUT_BYTE:
    case OP_SPUT_CHAR:
    case OP_SPUT_SHORT:
    case OP_SPUT_WIDE:
    case OP_SPUT_OBJECT:
        refType = VERIFY_ERROR_REF_FIELD;
        break;

    case OP_INVOKE_VIRTUAL:             // insn[1] == method ref, 3 bytes
    case OP_INVOKE_VIRTUAL_RANGE:
    case OP_INVOKE_SUPER:
    case OP_INVOKE_SUPER_RANGE:
    case OP_INVOKE_DIRECT:
    case OP_INVOKE_DIRECT_RANGE:
    case OP_INVOKE_STATIC:
    case OP_INVOKE_STATIC_RANGE:
    case OP_INVOKE_INTERFACE:
    case OP_INVOKE_INTERFACE_RANGE:
        refType = VERIFY_ERROR_REF_METHOD;
        break;

    default:
        /* could handle this in a generic way, but this is probably safer */
        LOG_VFY("GLITCH: verifier asked to replace opcode 0x%02x", opcode);
        return false;
    }

    assert((dexGetFlagsFromOpcode(opcode) & kInstrCanThrow) != 0);

    /* write a NOP over the third code unit, if necessary */
    width = dvmInsnGetWidth(insnFlags, insnIdx);
    switch (width) {
    case 2:
    case 4:
        /* nothing to do */
        break;
    case 3:
        dvmUpdateCodeUnit(meth, oldInsns+2, OP_NOP);
        break;
    default:
        /* whoops */
        ALOGE("ERROR: stomped a %d-unit instruction with a verifier error",
            width);
        dvmAbort();
    }

    /* encode the opcode, with the failure code in the high byte */
    assert(width == 2 || width == 3);
    u2 newVal = OP_THROW_VERIFICATION_ERROR |
        (failure << 8) | (refType << (8 + kVerifyErrorRefTypeShift));
    dvmUpdateCodeUnit(meth, oldInsns, newVal);

    return true;
}

/*
 * Handle a monitor-enter instruction.
 */
void handleMonitorEnter(RegisterLine* workLine, u4 regIdx, u4 insnIdx,
    VerifyError* pFailure)
{
    if (!regTypeIsReference(getRegisterType(workLine, regIdx))) {
        LOG_VFY("VFY: monitor-enter on non-object");
        *pFailure = VERIFY_ERROR_GENERIC;
        return;
    }

    if (workLine->monitorEntries == NULL) {
        /* should only be true if monitor verification is disabled */
        assert(!gDvm.monitorVerification);
        return;
    }

    if (workLine->monitorStackTop == kMaxMonitorStackDepth) {
        LOG_VFY("VFY: monitor-enter stack overflow (%d)",
            kMaxMonitorStackDepth);
        *pFailure = VERIFY_ERROR_GENERIC;
        return;
    }

    /*
     * Push an entry on the stack, and set a bit in the register flags to
     * indicate that it's associated with this register.
     */
    workLine->monitorEntries[regIdx] |= 1 << workLine->monitorStackTop;
    workLine->monitorStack[workLine->monitorStackTop++] = insnIdx;
}

/*
 * Handle a monitor-exit instruction.
 */
void handleMonitorExit(RegisterLine* workLine, u4 regIdx, u4 insnIdx,
    VerifyError* pFailure)
{
    if (!regTypeIsReference(getRegisterType(workLine, regIdx))) {
        LOG_VFY("VFY: monitor-exit on non-object");
        *pFailure = VERIFY_ERROR_GENERIC;
        return;
    }

    if (workLine->monitorEntries == NULL) {
        /* should only be true if monitor verification is disabled */
        assert(!gDvm.monitorVerification);
        return;
    }

    if (workLine->monitorStackTop == 0) {
        LOG_VFY("VFY: monitor-exit stack underflow");
        *pFailure = VERIFY_ERROR_GENERIC;
        return;
    }

    /*
     * Confirm that the entry at the top of the stack is associated with
     * the register.  Pop the top entry off.
     */
    workLine->monitorStackTop--;
#ifdef BUG_3215458_FIXED
    /*
     * TODO: This code can safely be enabled if know we are working on
     * a dex file of format version 036 or later. (That is, we'll need to
     * add a check for the version number.)
     */
    if ((workLine->monitorEntries[regIdx] & (1 << workLine->monitorStackTop))
            == 0)
    {
        LOG_VFY("VFY: monitor-exit bit %d not set: addr=0x%04x (bits[%d]=%#x)",
            workLine->monitorStackTop, insnIdx, regIdx,
            workLine->monitorEntries[regIdx]);
        *pFailure = VERIFY_ERROR_GENERIC;
        return;
    }
#endif
    workLine->monitorStack[workLine->monitorStackTop] = 0;

    /*
     * Clear the bit from the register flags.
     */
    workLine->monitorEntries[regIdx] &= ~(1 << workLine->monitorStackTop);
}


/*
 * ===========================================================================
 *      Entry point and driver loop
 * ===========================================================================
 */

/*
 * One-time preparation.
 */
static void verifyPrep()
{
#ifndef NDEBUG
    /* only need to do this if the table was updated */
    checkMergeTab();
#endif
}

/*
 * Entry point for the detailed code-flow analysis of a single method.
 */
bool dvmVerifyCodeFlow(VerifierData* vdata)
{
    bool result = false;
    const Method* meth = vdata->method;
    const int insnsSize = vdata->insnsSize;
    const bool generateRegisterMap = gDvm.generateRegisterMaps;
    RegisterTable regTable;

    memset(&regTable, 0, sizeof(regTable));

#ifdef VERIFIER_STATS
    gDvm.verifierStats.methodsExamined++;
    if (vdata->monitorEnterCount)
        gDvm.verifierStats.monEnterMethods++;
#endif

    /* TODO: move this elsewhere -- we don't need to do this for every method */
    verifyPrep();

    if (meth->registersSize * insnsSize > 4*1024*1024) {
        LOG_VFY_METH(meth,
            "VFY: warning: method is huge (regs=%d insnsSize=%d)",
            meth->registersSize, insnsSize);
        /* might be bogus data, might be some huge generated method */
    }

    /*
     * Create register lists, and initialize them to "Unknown".  If we're
     * also going to create the register map, we need to retain the
     * register lists for a larger set of addresses.
     */
    if (!initRegisterTable(vdata, &regTable,
            generateRegisterMap ? kTrackRegsGcPoints : kTrackRegsBranches))
        goto bail;

    vdata->registerLines = regTable.registerLines;

    /*
     * Perform liveness analysis.
     *
     * We can do this before or after the main verifier pass.  The choice
     * affects whether or not we see the effects of verifier instruction
     * changes, i.e. substitution of throw-verification-error.
     *
     * In practice the ordering doesn't really matter, because T-V-E
     * just prunes "can continue", creating regions of dead code (with
     * corresponding register map data that will never be used).
     */
    if (generateRegisterMap &&
        gDvm.registerMapMode == kRegisterMapModeLivePrecise)
    {
        /*
         * Compute basic blocks and predecessor lists.
         */
        if (!dvmComputeVfyBasicBlocks(vdata))
            goto bail;

        /*
         * Compute liveness.
         */
        if (!dvmComputeLiveness(vdata))
            goto bail;
    }

    /*
     * Initialize the types of the registers that correspond to the
     * method arguments.  We can determine this from the method signature.
     */
    if (!setTypesFromSignature(meth, regTable.registerLines[0].regTypes,
            vdata->uninitMap))
        goto bail;

    /*
     * Run the verifier.
     */
    if (!doCodeVerification(vdata, &regTable))
        goto bail;

    /*
     * Generate a register map.
     */
    if (generateRegisterMap) {
        RegisterMap* pMap = dvmGenerateRegisterMapV(vdata);
        if (pMap != NULL) {
            /*
             * Tuck it into the Method struct.  It will either get used
             * directly or, if we're in dexopt, will be packed up and
             * appended to the DEX file.
             */
            dvmSetRegisterMap((Method*)meth, pMap);
        }
    }

    /*
     * Success.
     */
    result = true;

bail:
    freeRegisterLineInnards(vdata);
    free(regTable.registerLines);
    free(regTable.lineAlloc);
    return result;
}

/*
 * Grind through the instructions.
 *
 * The basic strategy is as outlined in v3 4.11.1.2: set the "changed" bit
 * on the first instruction, process it (setting additional "changed" bits),
 * and repeat until there are no more.
 *
 * v3 4.11.1.1
 * - (N/A) operand stack is always the same size
 * - operand stack [registers] contain the correct types of values
 * - local variables [registers] contain the correct types of values
 * - methods are invoked with the appropriate arguments
 * - fields are assigned using values of appropriate types
 * - opcodes have the correct type values in operand registers
 * - there is never an uninitialized class instance in a local variable in
 *   code protected by an exception handler (operand stack is okay, because
 *   the operand stack is discarded when an exception is thrown) [can't
 *   know what's a local var w/o the debug info -- should fall out of
 *   register typing]
 *
 * v3 4.11.1.2
 * - execution cannot fall off the end of the code
 *
 * (We also do many of the items described in the "static checks" sections,
 * because it's easier to do them here.)
 *
 * We need an array of RegType values, one per register, for every
 * instruction.  If the method uses monitor-enter, we need extra data
 * for every register, and a stack for every "interesting" instruction.
 * In theory this could become quite large -- up to several megabytes for
 * a monster function.
 *
 * NOTE:
 * The spec forbids backward branches when there's an uninitialized reference
 * in a register.  The idea is to prevent something like this:
 *   loop:
 *     move r1, r0
 *     new-instance r0, MyClass
 *     ...
 *     if-eq rN, loop  // once
 *   initialize r0
 *
 * This leaves us with two different instances, both allocated by the
 * same instruction, but only one is initialized.  The scheme outlined in
 * v3 4.11.1.4 wouldn't catch this, so they work around it by preventing
 * backward branches.  We achieve identical results without restricting
 * code reordering by specifying that you can't execute the new-instance
 * instruction if a register contains an uninitialized instance created
 * by that same instrutcion.
 */
static bool doCodeVerification(VerifierData* vdata, RegisterTable* regTable)
{
    const Method* meth = vdata->method;
    InsnFlags* insnFlags = vdata->insnFlags;
    UninitInstanceMap* uninitMap = vdata->uninitMap;
    const int insnsSize = dvmGetMethodInsnsSize(meth);
    bool result = false;
    bool debugVerbose = false;
    int insnIdx, startGuess;

    /*
     * Begin by marking the first instruction as "changed".
     */
    dvmInsnSetChanged(insnFlags, 0, true);

    if (dvmWantVerboseVerification(meth)) {
        IF_ALOGI() {
            char* desc = dexProtoCopyMethodDescriptor(&meth->prototype);
            ALOGI("Now verifying: %s.%s %s (ins=%d regs=%d)",
                meth->clazz->descriptor, meth->name, desc,
                meth->insSize, meth->registersSize);
            ALOGI(" ------ [0    4    8    12   16   20   24   28   32   36");
            free(desc);
        }
        debugVerbose = true;
        gDebugVerbose = true;
    } else {
        gDebugVerbose = false;
    }

    startGuess = 0;

    /*
     * Continue until no instructions are marked "changed".
     */
    while (true) {
        /*
         * Find the first marked one.  Use "startGuess" as a way to find
         * one quickly.
         */
        for (insnIdx = startGuess; insnIdx < insnsSize; insnIdx++) {
            if (dvmInsnIsChanged(insnFlags, insnIdx))
                break;
        }

        if (insnIdx == insnsSize) {
            if (startGuess != 0) {
                /* try again, starting from the top */
                startGuess = 0;
                continue;
            } else {
                /* all flags are clear */
                break;
            }
        }

        /*
         * We carry the working set of registers from instruction to
         * instruction.  If this address can be the target of a branch
         * (or throw) instruction, or if we're skipping around chasing
         * "changed" flags, we need to load the set of registers from
         * the table.
         *
         * Because we always prefer to continue on to the next instruction,
         * we should never have a situation where we have a stray
         * "changed" flag set on an instruction that isn't a branch target.
         */
        if (dvmInsnIsBranchTarget(insnFlags, insnIdx)) {
            RegisterLine* workLine = &regTable->workLine;

            copyLineFromTable(workLine, regTable, insnIdx);
        } else {
#ifndef NDEBUG
            /*
             * Sanity check: retrieve the stored register line (assuming
             * a full table) and make sure it actually matches.
             */
            RegisterLine* registerLine = getRegisterLine(regTable, insnIdx);
            if (registerLine->regTypes != NULL &&
                compareLineToTable(regTable, insnIdx, &regTable->workLine) != 0)
            {
                char* desc = dexProtoCopyMethodDescriptor(&meth->prototype);
                LOG_VFY("HUH? workLine diverged in %s.%s %s",
                        meth->clazz->descriptor, meth->name, desc);
                free(desc);
                dumpRegTypes(vdata, registerLine, 0, "work",
                    uninitMap, DRT_SHOW_REF_TYPES | DRT_SHOW_LOCALS);
                dumpRegTypes(vdata, registerLine, 0, "insn",
                    uninitMap, DRT_SHOW_REF_TYPES | DRT_SHOW_LOCALS);
            }
#endif
        }
        if (debugVerbose) {
            dumpRegTypes(vdata, &regTable->workLine, insnIdx,
                NULL, uninitMap, SHOW_REG_DETAILS);
        }

        //ALOGI("process %s.%s %s %d",
        //    meth->clazz->descriptor, meth->name, meth->descriptor, insnIdx);
        if (!verifyInstruction(meth, insnFlags, regTable, insnIdx,
                uninitMap, &startGuess))
        {
            //ALOGD("+++ %s bailing at %d", meth->name, insnIdx);
            goto bail;
        }

        /*
         * Clear "changed" and mark as visited.
         */
        dvmInsnSetVisited(insnFlags, insnIdx, true);
        dvmInsnSetChanged(insnFlags, insnIdx, false);
    }

    if (DEAD_CODE_SCAN && !IS_METHOD_FLAG_SET(meth, METHOD_ISWRITABLE)) {
        /*
         * Scan for dead code.  There's nothing "evil" about dead code
         * (besides the wasted space), but it indicates a flaw somewhere
         * down the line, possibly in the verifier.
         *
         * If we've substituted "always throw" instructions into the stream,
         * we are almost certainly going to have some dead code.
         */
        int deadStart = -1;
        for (insnIdx = 0; insnIdx < insnsSize;
            insnIdx += dvmInsnGetWidth(insnFlags, insnIdx))
        {
            /*
             * Switch-statement data doesn't get "visited" by scanner.  It
             * may or may not be preceded by a padding NOP (for alignment).
             */
            int instr = meth->insns[insnIdx];
            if (instr == kPackedSwitchSignature ||
                instr == kSparseSwitchSignature ||
                instr == kArrayDataSignature ||
                (instr == OP_NOP && (insnIdx + 1 < insnsSize) &&
                 (meth->insns[insnIdx+1] == kPackedSwitchSignature ||
                  meth->insns[insnIdx+1] == kSparseSwitchSignature ||
                  meth->insns[insnIdx+1] == kArrayDataSignature)))
            {
                dvmInsnSetVisited(insnFlags, insnIdx, true);
            }

            if (!dvmInsnIsVisited(insnFlags, insnIdx)) {
                if (deadStart < 0)
                    deadStart = insnIdx;
            } else if (deadStart >= 0) {
                IF_ALOGD() {
                    char* desc =
                        dexProtoCopyMethodDescriptor(&meth->prototype);
                    ALOGD("VFY: dead code 0x%04x-%04x in %s.%s %s",
                        deadStart, insnIdx-1,
                        meth->clazz->descriptor, meth->name, desc);
                    free(desc);
                }

                deadStart = -1;
            }
        }
        if (deadStart >= 0) {
            IF_ALOGD() {
                char* desc = dexProtoCopyMethodDescriptor(&meth->prototype);
                ALOGD("VFY: dead code 0x%04x-%04x in %s.%s %s",
                    deadStart, insnIdx-1,
                    meth->clazz->descriptor, meth->name, desc);
                free(desc);
            }
        }
    }

    result = true;

bail:
    return result;
}


/*
 * Perform verification for a single instruction.
 *
 * This requires fully decoding the instruction to determine the effect
 * it has on registers.
 *
 * Finds zero or more following instructions and sets the "changed" flag
 * if execution at that point needs to be (re-)evaluated.  Register changes
 * are merged into "regTypes" at the target addresses.  Does not set or
 * clear any other flags in "insnFlags".
 *
 * This may alter meth->insns if we need to replace an instruction with
 * throw-verification-error.
 */
static bool verifyInstruction(const Method* meth, InsnFlags* insnFlags,
    RegisterTable* regTable, int insnIdx, UninitInstanceMap* uninitMap,
    int* pStartGuess)
{
    const int insnsSize = dvmGetMethodInsnsSize(meth);
    const u2* insns = meth->insns + insnIdx;
    bool result = false;

#ifdef VERIFIER_STATS
    if (dvmInsnIsVisited(insnFlags, insnIdx)) {
        gDvm.verifierStats.instrsReexamined++;
    } else {
        gDvm.verifierStats.instrsExamined++;
    }
#endif

    /*
     * Once we finish decoding the instruction, we need to figure out where
     * we can go from here.  There are three possible ways to transfer
     * control to another statement:
     *
     * (1) Continue to the next instruction.  Applies to all but
     *     unconditional branches, method returns, and exception throws.
     * (2) Branch to one or more possible locations.  Applies to branches
     *     and switch statements.
     * (3) Exception handlers.  Applies to any instruction that can
     *     throw an exception that is handled by an encompassing "try"
     *     block.
     *
     * We can also return, in which case there is no successor instruction
     * from this point.
     *
     * The behavior can be determined from the OpcodeFlags.
     */

    RegisterLine* workLine = &regTable->workLine;
    const DexFile* pDexFile = meth->clazz->pDvmDex->pDexFile;
    ClassObject* resClass;
    s4 branchTarget = 0;
    const int insnRegCount = meth->registersSize;
    RegType tmpType;
    DecodedInstruction decInsn;
    bool justSetResult = false;
    VerifyError failure = VERIFY_ERROR_NONE;

#ifndef NDEBUG
    memset(&decInsn, 0x81, sizeof(decInsn));
#endif
    dexDecodeInstruction(insns, &decInsn);

    int nextFlags = dexGetFlagsFromOpcode(decInsn.opcode);

    /*
     * Make a copy of the previous register state.  If the instruction
     * can throw an exception, we will copy/merge this into the "catch"
     * address rather than workLine, because we don't want the result
     * from the "successful" code path (e.g. a check-cast that "improves"
     * a type) to be visible to the exception handler.
     */
    if ((nextFlags & kInstrCanThrow) != 0 && dvmInsnIsInTry(insnFlags, insnIdx))
    {
        copyRegisterLine(&regTable->savedLine, workLine,
            regTable->insnRegCountPlus);
    } else {
#ifndef NDEBUG
        memset(regTable->savedLine.regTypes, 0xdd,
            regTable->insnRegCountPlus * sizeof(RegType));
#endif
    }

    switch (decInsn.opcode) {
    case OP_NOP:
        /*
         * A "pure" NOP has no effect on anything.  Data tables start with
         * a signature that looks like a NOP; if we see one of these in
         * the course of executing code then we have a problem.
         */
        if (decInsn.vA != 0) {
            LOG_VFY("VFY: encountered data table in instruction stream");
            failure = VERIFY_ERROR_GENERIC;
        }
        break;

    case OP_MOVE:
    case OP_MOVE_FROM16:
    case OP_MOVE_16:
        copyRegister1(workLine, decInsn.vA, decInsn.vB, kTypeCategory1nr,
            &failure);
        break;
    case OP_MOVE_WIDE:
    case OP_MOVE_WIDE_FROM16:
    case OP_MOVE_WIDE_16:
        copyRegister2(workLine, decInsn.vA, decInsn.vB, &failure);
        break;
    case OP_MOVE_OBJECT:
    case OP_MOVE_OBJECT_FROM16:
    case OP_MOVE_OBJECT_16:
        copyRegister1(workLine, decInsn.vA, decInsn.vB, kTypeCategoryRef,
            &failure);
        break;

    /*
     * The move-result instructions copy data out of a "pseudo-register"
     * with the results from the last method invocation.  In practice we
     * might want to hold the result in an actual CPU register, so the
     * Dalvik spec requires that these only appear immediately after an
     * invoke or filled-new-array.
     *
     * These calls invalidate the "result" register.  (This is now
     * redundant with the reset done below, but it can make the debug info
     * easier to read in some cases.)
     */
    case OP_MOVE_RESULT:
        copyResultRegister1(workLine, insnRegCount, decInsn.vA,
            kTypeCategory1nr, &failure);
        break;
    case OP_MOVE_RESULT_WIDE:
        copyResultRegister2(workLine, insnRegCount, decInsn.vA, &failure);
        break;
    case OP_MOVE_RESULT_OBJECT:
        copyResultRegister1(workLine, insnRegCount, decInsn.vA,
            kTypeCategoryRef, &failure);
        break;

    case OP_MOVE_EXCEPTION:
        /*
         * This statement can only appear as the first instruction in an
         * exception handler (though not all exception handlers need to
         * have one of these).  We verify that as part of extracting the
         * exception type from the catch block list.
         *
         * "resClass" will hold the closest common superclass of all
         * exceptions that can be handled here.
         */
        resClass = getCaughtExceptionType(meth, insnIdx, &failure);
        if (resClass == NULL) {
            assert(!VERIFY_OK(failure));
        } else {
            setRegisterType(workLine, decInsn.vA, regTypeFromClass(resClass));
        }
        break;

    case OP_RETURN_VOID:
        if (!checkConstructorReturn(meth, workLine, insnRegCount)) {
            failure = VERIFY_ERROR_GENERIC;
        } else if (getMethodReturnType(meth) != kRegTypeUnknown) {
            LOG_VFY("VFY: return-void not expected");
            failure = VERIFY_ERROR_GENERIC;
        }
        break;
    case OP_RETURN:
        if (!checkConstructorReturn(meth, workLine, insnRegCount)) {
            failure = VERIFY_ERROR_GENERIC;
        } else {
            /* check the method signature */
            RegType returnType = getMethodReturnType(meth);
            checkTypeCategory(returnType, kTypeCategory1nr, &failure);
            if (!VERIFY_OK(failure))
                LOG_VFY("VFY: return-1nr not expected");

            /*
             * javac generates synthetic functions that write byte values
             * into boolean fields. Also, it may use integer values for
             * boolean, byte, short, and character return types.
             */
            RegType srcType = getRegisterType(workLine, decInsn.vA);
            if ((returnType == kRegTypeBoolean && srcType == kRegTypeByte) ||
                ((returnType == kRegTypeBoolean || returnType == kRegTypeByte ||
                  returnType == kRegTypeShort || returnType == kRegTypeChar) &&
                 srcType == kRegTypeInteger))
                returnType = srcType;

            /* check the register contents */
            verifyRegisterType(workLine, decInsn.vA, returnType, &failure);
            if (!VERIFY_OK(failure)) {
                LOG_VFY("VFY: return-1nr on invalid register v%d",
                    decInsn.vA);
            }
        }
        break;
    case OP_RETURN_WIDE:
        if (!checkConstructorReturn(meth, workLine, insnRegCount)) {
            failure = VERIFY_ERROR_GENERIC;
        } else {
            RegType returnType;

            /* check the method signature */
            returnType = getMethodReturnType(meth);
            checkTypeCategory(returnType, kTypeCategory2, &failure);
            if (!VERIFY_OK(failure))
                LOG_VFY("VFY: return-wide not expected");

            /* check the register contents */
            verifyRegisterType(workLine, decInsn.vA, returnType, &failure);
            if (!VERIFY_OK(failure)) {
                LOG_VFY("VFY: return-wide on invalid register pair v%d",
                    decInsn.vA);
            }
        }
        break;
    case OP_RETURN_OBJECT:
        if (!checkConstructorReturn(meth, workLine, insnRegCount)) {
            failure = VERIFY_ERROR_GENERIC;
        } else {
            RegType returnType = getMethodReturnType(meth);
            checkTypeCategory(returnType, kTypeCategoryRef, &failure);
            if (!VERIFY_OK(failure)) {
                LOG_VFY("VFY: return-object not expected");
                break;
            }

            /* returnType is the *expected* return type, not register value */
            assert(returnType != kRegTypeZero);
            assert(!regTypeIsUninitReference(returnType));

            /*
             * Verify that the reference in vAA is an instance of the type
             * in "returnType".  The Zero type is allowed here.  If the
             * method is declared to return an interface, then any
             * initialized reference is acceptable.
             *
             * Note getClassFromRegister fails if the register holds an
             * uninitialized reference, so we do not allow them to be
             * returned.
             */
            ClassObject* declClass;

            declClass = regTypeInitializedReferenceToClass(returnType);
            resClass = getClassFromRegister(workLine, decInsn.vA, &failure);
            if (!VERIFY_OK(failure))
                break;
            if (resClass != NULL) {
                if (!dvmIsInterfaceClass(declClass) &&
                    !dvmInstanceof(resClass, declClass))
                {
                    LOG_VFY("VFY: returning %s (cl=%p), declared %s (cl=%p)",
                            resClass->descriptor, resClass->classLoader,
                            declClass->descriptor, declClass->classLoader);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }
            }
        }
        break;

    case OP_CONST_4:
    case OP_CONST_16:
    case OP_CONST:
        /* could be boolean, int, float, or a null reference */
        setRegisterType(workLine, decInsn.vA,
            determineCat1Const((s4)decInsn.vB));
        break;
    case OP_CONST_HIGH16:
        /* could be boolean, int, float, or a null reference */
        setRegisterType(workLine, decInsn.vA,
            determineCat1Const((s4) decInsn.vB << 16));
        break;
    case OP_CONST_WIDE_16:
    case OP_CONST_WIDE_32:
    case OP_CONST_WIDE:
    case OP_CONST_WIDE_HIGH16:
        /* could be long or double; resolved upon use */
        setRegisterType(workLine, decInsn.vA, kRegTypeConstLo);
        break;
    case OP_CONST_STRING:
    case OP_CONST_STRING_JUMBO:
        assert(gDvm.classJavaLangString != NULL);
        setRegisterType(workLine, decInsn.vA,
            regTypeFromClass(gDvm.classJavaLangString));
        break;
    case OP_CONST_CLASS:
        assert(gDvm.classJavaLangClass != NULL);
        /* make sure we can resolve the class; access check is important */
        resClass = dvmOptResolveClass(meth->clazz, decInsn.vB, &failure);
        if (resClass == NULL) {
            const char* badClassDesc = dexStringByTypeIdx(pDexFile, decInsn.vB);
            dvmLogUnableToResolveClass(badClassDesc, meth);
            LOG_VFY("VFY: unable to resolve const-class %d (%s) in %s",
                decInsn.vB, badClassDesc, meth->clazz->descriptor);
            assert(failure != VERIFY_ERROR_GENERIC);
        } else {
            setRegisterType(workLine, decInsn.vA,
                regTypeFromClass(gDvm.classJavaLangClass));
        }
        break;

    case OP_MONITOR_ENTER:
        handleMonitorEnter(workLine, decInsn.vA, insnIdx, &failure);
        break;
    case OP_MONITOR_EXIT:
        /*
         * monitor-exit instructions are odd.  They can throw exceptions,
         * but when they do they act as if they succeeded and the PC is
         * pointing to the following instruction.  (This behavior goes back
         * to the need to handle asynchronous exceptions, a now-deprecated
         * feature that Dalvik doesn't support.)
         *
         * In practice we don't need to worry about this.  The only
         * exceptions that can be thrown from monitor-exit are for a
         * null reference and -exit without a matching -enter.  If the
         * structured locking checks are working, the former would have
         * failed on the -enter instruction, and the latter is impossible.
         *
         * This is fortunate, because issue 3221411 prevents us from
         * chasing the "can throw" path when monitor verification is
         * enabled.  If we can fully verify the locking we can ignore
         * some catch blocks (which will show up as "dead" code when
         * we skip them here); if we can't, then the code path could be
         * "live" so we still need to check it.
         */
        if (workLine->monitorEntries != NULL)
            nextFlags &= ~kInstrCanThrow;
        handleMonitorExit(workLine, decInsn.vA, insnIdx, &failure);
        break;

    case OP_CHECK_CAST:
        /*
         * If this instruction succeeds, we will promote register vA to
         * the type in vB.  (This could be a demotion -- not expected, so
         * we don't try to address it.)
         *
         * If it fails, an exception is thrown, which we deal with later
         * by ignoring the update to decInsn.vA when branching to a handler.
         */
        resClass = dvmOptResolveClass(meth->clazz, decInsn.vB, &failure);
        if (resClass == NULL) {
            const char* badClassDesc = dexStringByTypeIdx(pDexFile, decInsn.vB);
            dvmLogUnableToResolveClass(badClassDesc, meth);
            LOG_VFY("VFY: unable to resolve check-cast %d (%s) in %s",
                decInsn.vB, badClassDesc, meth->clazz->descriptor);
            assert(failure != VERIFY_ERROR_GENERIC);
        } else {
            RegType origType;

            origType = getRegisterType(workLine, decInsn.vA);
            if (!regTypeIsReference(origType)) {
                LOG_VFY("VFY: check-cast on non-reference in v%u",decInsn.vA);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
            setRegisterType(workLine, decInsn.vA, regTypeFromClass(resClass));
        }
        break;
    case OP_INSTANCE_OF:
        /* make sure we're checking a reference type */
        tmpType = getRegisterType(workLine, decInsn.vB);
        if (!regTypeIsReference(tmpType)) {
            LOG_VFY("VFY: vB not a reference (%d)", tmpType);
            failure = VERIFY_ERROR_GENERIC;
            break;
        }

        /* make sure we can resolve the class; access check is important */
        resClass = dvmOptResolveClass(meth->clazz, decInsn.vC, &failure);
        if (resClass == NULL) {
            const char* badClassDesc = dexStringByTypeIdx(pDexFile, decInsn.vC);
            dvmLogUnableToResolveClass(badClassDesc, meth);
            LOG_VFY("VFY: unable to resolve instanceof %d (%s) in %s",
                decInsn.vC, badClassDesc, meth->clazz->descriptor);
            assert(failure != VERIFY_ERROR_GENERIC);
        } else {
            /* result is boolean */
            setRegisterType(workLine, decInsn.vA, kRegTypeBoolean);
        }
        break;

    case OP_ARRAY_LENGTH:
        resClass = getClassFromRegister(workLine, decInsn.vB, &failure);
        if (!VERIFY_OK(failure))
            break;
        if (resClass != NULL && !dvmIsArrayClass(resClass)) {
            LOG_VFY("VFY: array-length on non-array");
            failure = VERIFY_ERROR_GENERIC;
            break;
        }
        setRegisterType(workLine, decInsn.vA, kRegTypeInteger);
        break;

    case OP_NEW_INSTANCE:
        resClass = dvmOptResolveClass(meth->clazz, decInsn.vB, &failure);
        if (resClass == NULL) {
            const char* badClassDesc = dexStringByTypeIdx(pDexFile, decInsn.vB);
            dvmLogUnableToResolveClass(badClassDesc, meth);
            LOG_VFY("VFY: unable to resolve new-instance %d (%s) in %s",
                decInsn.vB, badClassDesc, meth->clazz->descriptor);
            assert(failure != VERIFY_ERROR_GENERIC);
        } else {
            RegType uninitType;

            /* can't create an instance of an interface or abstract class */
            if (dvmIsAbstractClass(resClass) || dvmIsInterfaceClass(resClass)) {
                LOG_VFY("VFY: new-instance on interface or abstract class %s",
                    resClass->descriptor);
                failure = VERIFY_ERROR_INSTANTIATION;
                break;
            }

            /* add resolved class to uninit map if not already there */
            int uidx = setUninitInstance(uninitMap, insnIdx, resClass);
            assert(uidx >= 0);
            uninitType = regTypeFromUninitIndex(uidx);

            /*
             * Any registers holding previous allocations from this address
             * that have not yet been initialized must be marked invalid.
             */
            markUninitRefsAsInvalid(workLine, insnRegCount, uninitMap,
                uninitType);

            /* add the new uninitialized reference to the register ste */
            setRegisterType(workLine, decInsn.vA, uninitType);
        }
        break;
    case OP_NEW_ARRAY:
        resClass = dvmOptResolveClass(meth->clazz, decInsn.vC, &failure);
        if (resClass == NULL) {
            const char* badClassDesc = dexStringByTypeIdx(pDexFile, decInsn.vC);
            dvmLogUnableToResolveClass(badClassDesc, meth);
            LOG_VFY("VFY: unable to resolve new-array %d (%s) in %s",
                decInsn.vC, badClassDesc, meth->clazz->descriptor);
            assert(failure != VERIFY_ERROR_GENERIC);
        } else if (!dvmIsArrayClass(resClass)) {
            LOG_VFY("VFY: new-array on non-array class");
            failure = VERIFY_ERROR_GENERIC;
        } else {
            /* make sure "size" register is valid type */
            verifyRegisterType(workLine, decInsn.vB, kRegTypeInteger, &failure);
            /* set register type to array class */
            setRegisterType(workLine, decInsn.vA, regTypeFromClass(resClass));
        }
        break;
    case OP_FILLED_NEW_ARRAY:
    case OP_FILLED_NEW_ARRAY_RANGE:
        resClass = dvmOptResolveClass(meth->clazz, decInsn.vB, &failure);
        if (resClass == NULL) {
            const char* badClassDesc = dexStringByTypeIdx(pDexFile, decInsn.vB);
            dvmLogUnableToResolveClass(badClassDesc, meth);
            LOG_VFY("VFY: unable to resolve filled-array %d (%s) in %s",
                decInsn.vB, badClassDesc, meth->clazz->descriptor);
            assert(failure != VERIFY_ERROR_GENERIC);
        } else if (!dvmIsArrayClass(resClass)) {
            LOG_VFY("VFY: filled-new-array on non-array class");
            failure = VERIFY_ERROR_GENERIC;
        } else {
            bool isRange = (decInsn.opcode == OP_FILLED_NEW_ARRAY_RANGE);

            /* check the arguments to the instruction */
            verifyFilledNewArrayRegs(meth, workLine, &decInsn,
                resClass, isRange, &failure);
            /* filled-array result goes into "result" register */
            setResultRegisterType(workLine, insnRegCount,
                regTypeFromClass(resClass));
            justSetResult = true;
        }
        break;

    case OP_CMPL_FLOAT:
    case OP_CMPG_FLOAT:
        verifyRegisterType(workLine, decInsn.vB, kRegTypeFloat, &failure);
        verifyRegisterType(workLine, decInsn.vC, kRegTypeFloat, &failure);
        setRegisterType(workLine, decInsn.vA, kRegTypeBoolean);
        break;
    case OP_CMPL_DOUBLE:
    case OP_CMPG_DOUBLE:
        verifyRegisterType(workLine, decInsn.vB, kRegTypeDoubleLo, &failure);
        verifyRegisterType(workLine, decInsn.vC, kRegTypeDoubleLo, &failure);
        setRegisterType(workLine, decInsn.vA, kRegTypeBoolean);
        break;
    case OP_CMP_LONG:
        verifyRegisterType(workLine, decInsn.vB, kRegTypeLongLo, &failure);
        verifyRegisterType(workLine, decInsn.vC, kRegTypeLongLo, &failure);
        setRegisterType(workLine, decInsn.vA, kRegTypeBoolean);
        break;

    case OP_THROW:
        resClass = getClassFromRegister(workLine, decInsn.vA, &failure);
        if (VERIFY_OK(failure) && resClass != NULL) {
            if (!dvmInstanceof(resClass, gDvm.exThrowable)) {
                LOG_VFY("VFY: thrown class %s not instanceof Throwable",
                        resClass->descriptor);
                failure = VERIFY_ERROR_GENERIC;
            }
        }
        break;

    case OP_GOTO:
    case OP_GOTO_16:
    case OP_GOTO_32:
        /* no effect on or use of registers */
        break;

    case OP_PACKED_SWITCH:
    case OP_SPARSE_SWITCH:
        /* verify that vAA is an integer, or can be converted to one */
        verifyRegisterType(workLine, decInsn.vA, kRegTypeInteger, &failure);
        break;

    case OP_FILL_ARRAY_DATA:
        {
            RegType valueType;
            const u2 *arrayData;
            u2 elemWidth;

            /* Similar to the verification done for APUT */
            resClass = getClassFromRegister(workLine, decInsn.vA, &failure);
            if (!VERIFY_OK(failure))
                break;

            /* resClass can be null if the reg type is Zero */
            if (resClass == NULL)
                break;

            if (!dvmIsArrayClass(resClass) || resClass->arrayDim != 1 ||
                resClass->elementClass->primitiveType == PRIM_NOT ||
                resClass->elementClass->primitiveType == PRIM_VOID)
            {
                LOG_VFY("VFY: invalid fill-array-data on %s",
                        resClass->descriptor);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }

            valueType = primitiveTypeToRegType(
                                    resClass->elementClass->primitiveType);
            assert(valueType != kRegTypeUnknown);

            /*
             * Now verify if the element width in the table matches the element
             * width declared in the array
             */
            arrayData = insns + (insns[1] | (((s4)insns[2]) << 16));
            if (arrayData[0] != kArrayDataSignature) {
                LOG_VFY("VFY: invalid magic for array-data");
                failure = VERIFY_ERROR_GENERIC;
                break;
            }

            switch (resClass->elementClass->primitiveType) {
                case PRIM_BOOLEAN:
                case PRIM_BYTE:
                     elemWidth = 1;
                     break;
                case PRIM_CHAR:
                case PRIM_SHORT:
                     elemWidth = 2;
                     break;
                case PRIM_FLOAT:
                case PRIM_INT:
                     elemWidth = 4;
                     break;
                case PRIM_DOUBLE:
                case PRIM_LONG:
                     elemWidth = 8;
                     break;
                default:
                     elemWidth = 0;
                     break;
            }

            /*
             * Since we don't compress the data in Dex, expect to see equal
             * width of data stored in the table and expected from the array
             * class.
             */
            if (arrayData[1] != elemWidth) {
                LOG_VFY("VFY: array-data size mismatch (%d vs %d)",
                        arrayData[1], elemWidth);
                failure = VERIFY_ERROR_GENERIC;
            }
        }
        break;

    case OP_IF_EQ:
    case OP_IF_NE:
        {
            RegType type1, type2;

            type1 = getRegisterType(workLine, decInsn.vA);
            type2 = getRegisterType(workLine, decInsn.vB);

            /* both references? */
            if (regTypeIsReference(type1) && regTypeIsReference(type2))
                break;

            /* both category-1nr? */
            checkTypeCategory(type1, kTypeCategory1nr, &failure);
            checkTypeCategory(type2, kTypeCategory1nr, &failure);
            if (type1 == kRegTypeFloat || type2 == kRegTypeFloat) {
              failure = VERIFY_ERROR_GENERIC;
            }
            if (!VERIFY_OK(failure)) {
                LOG_VFY("VFY: args to if-eq/if-ne must both be refs or cat1");
                break;
            }
        }
        break;
    case OP_IF_LT:
    case OP_IF_GE:
    case OP_IF_GT:
    case OP_IF_LE:
        tmpType = getRegisterType(workLine, decInsn.vA);
        checkTypeCategory(tmpType, kTypeCategory1nr, &failure);
        if (tmpType == kRegTypeFloat) {
          failure = VERIFY_ERROR_GENERIC;
        }
        if (!VERIFY_OK(failure)) {
            LOG_VFY("VFY: args to 'if' must be cat-1nr and not float");
            break;
        }
        tmpType = getRegisterType(workLine, decInsn.vB);
        checkTypeCategory(tmpType, kTypeCategory1nr, &failure);
        if (tmpType == kRegTypeFloat) {
          failure = VERIFY_ERROR_GENERIC;
        }
        if (!VERIFY_OK(failure)) {
            LOG_VFY("VFY: args to 'if' must be cat-1nr and not float");
            break;
        }
        break;
    case OP_IF_EQZ:
    case OP_IF_NEZ:
        tmpType = getRegisterType(workLine, decInsn.vA);
        if (regTypeIsReference(tmpType))
            break;
        checkTypeCategory(tmpType, kTypeCategory1nr, &failure);
        if (tmpType == kRegTypeFloat) {
          failure = VERIFY_ERROR_GENERIC;
        }
        if (!VERIFY_OK(failure))
            LOG_VFY("VFY: expected non-float cat-1 arg to if");
        break;
    case OP_IF_LTZ:
    case OP_IF_GEZ:
    case OP_IF_GTZ:
    case OP_IF_LEZ:
        tmpType = getRegisterType(workLine, decInsn.vA);
        checkTypeCategory(tmpType, kTypeCategory1nr, &failure);
        if (tmpType == kRegTypeFloat) {
          failure = VERIFY_ERROR_GENERIC;
        }
        if (!VERIFY_OK(failure))
            LOG_VFY("VFY: expected non-float cat-1 arg to if");
        break;

    case OP_AGET:
        tmpType = kRegTypeInteger;
        goto aget_1nr_common;
    case OP_AGET_BOOLEAN:
        tmpType = kRegTypeBoolean;
        goto aget_1nr_common;
    case OP_AGET_BYTE:
        tmpType = kRegTypeByte;
        goto aget_1nr_common;
    case OP_AGET_CHAR:
        tmpType = kRegTypeChar;
        goto aget_1nr_common;
    case OP_AGET_SHORT:
        tmpType = kRegTypeShort;
        goto aget_1nr_common;
aget_1nr_common:
        {
            RegType srcType, indexType;

            indexType = getRegisterType(workLine, decInsn.vC);
            checkArrayIndexType(meth, indexType, &failure);
            if (!VERIFY_OK(failure))
                break;

            resClass = getClassFromRegister(workLine, decInsn.vB, &failure);
            if (!VERIFY_OK(failure))
                break;
            if (resClass != NULL) {
                /* verify the class */
                if (!dvmIsArrayClass(resClass) || resClass->arrayDim != 1 ||
                    resClass->elementClass->primitiveType == PRIM_NOT)
                {
                    LOG_VFY("VFY: invalid aget-1nr target %s",
                        resClass->descriptor);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }

                /* make sure array type matches instruction */
                srcType = primitiveTypeToRegType(
                                        resClass->elementClass->primitiveType);

                /* correct if float */
                if (srcType == kRegTypeFloat && tmpType == kRegTypeInteger)
                    tmpType = kRegTypeFloat;

                if (!checkFieldArrayStore1nr(tmpType, srcType)) {
                    LOG_VFY("VFY: invalid aget-1nr, array type=%d with"
                            " inst type=%d (on %s)",
                        srcType, tmpType, resClass->descriptor);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }
            } else {
                /*
                 * Null array ref; this code path will fail at runtime. Label
                 * result as zero to allow it to remain mergeable.
                 */
                tmpType = kRegTypeZero;
            }
            setRegisterType(workLine, decInsn.vA, tmpType);
        }
        break;

    case OP_AGET_WIDE:
        {
            RegType dstType, indexType;

            indexType = getRegisterType(workLine, decInsn.vC);
            checkArrayIndexType(meth, indexType, &failure);
            if (!VERIFY_OK(failure))
                break;

            resClass = getClassFromRegister(workLine, decInsn.vB, &failure);
            if (!VERIFY_OK(failure))
                break;
            if (resClass != NULL) {
                /* verify the class */
                if (!dvmIsArrayClass(resClass) || resClass->arrayDim != 1 ||
                    resClass->elementClass->primitiveType == PRIM_NOT)
                {
                    LOG_VFY("VFY: invalid aget-wide target %s",
                        resClass->descriptor);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }

                /* try to refine "dstType" */
                switch (resClass->elementClass->primitiveType) {
                case PRIM_LONG:
                    dstType = kRegTypeLongLo;
                    break;
                case PRIM_DOUBLE:
                    dstType = kRegTypeDoubleLo;
                    break;
                default:
                    LOG_VFY("VFY: invalid aget-wide on %s",
                        resClass->descriptor);
                    dstType = kRegTypeUnknown;
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }
            } else {
                /*
                 * Null array ref; this code path will fail at runtime.  We
                 * know this is either long or double, so label it const.
                 */
                dstType = kRegTypeConstLo;
            }
            setRegisterType(workLine, decInsn.vA, dstType);
        }
        break;

    case OP_AGET_OBJECT:
        {
            RegType dstType, indexType;

            indexType = getRegisterType(workLine, decInsn.vC);
            checkArrayIndexType(meth, indexType, &failure);
            if (!VERIFY_OK(failure))
                break;

            /* get the class of the array we're pulling an object from */
            resClass = getClassFromRegister(workLine, decInsn.vB, &failure);
            if (!VERIFY_OK(failure))
                break;
            if (resClass != NULL) {
                ClassObject* elementClass;

                assert(resClass != NULL);
                if (!dvmIsArrayClass(resClass)) {
                    LOG_VFY("VFY: aget-object on non-array class");
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }
                assert(resClass->elementClass != NULL);

                /*
                 * Find the element class.  resClass->elementClass indicates
                 * the basic type, which won't be what we want for a
                 * multi-dimensional array.
                 */
                if (resClass->descriptor[1] == '[') {
                    assert(resClass->arrayDim > 1);
                    elementClass = dvmFindArrayClass(&resClass->descriptor[1],
                                        resClass->classLoader);
                } else if (resClass->descriptor[1] == 'L') {
                    assert(resClass->arrayDim == 1);
                    elementClass = resClass->elementClass;
                } else {
                    LOG_VFY("VFY: aget-object on non-ref array class (%s)",
                        resClass->descriptor);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }

                dstType = regTypeFromClass(elementClass);
            } else {
                /*
                 * The array reference is NULL, so the current code path will
                 * throw an exception.  For proper merging with later code
                 * paths, and correct handling of "if-eqz" tests on the
                 * result of the array get, we want to treat this as a null
                 * reference.
                 */
                dstType = kRegTypeZero;
            }
            setRegisterType(workLine, decInsn.vA, dstType);
        }
        break;
    case OP_APUT:
        tmpType = kRegTypeInteger;
        goto aput_1nr_common;
    case OP_APUT_BOOLEAN:
        tmpType = kRegTypeBoolean;
        goto aput_1nr_common;
    case OP_APUT_BYTE:
        tmpType = kRegTypeByte;
        goto aput_1nr_common;
    case OP_APUT_CHAR:
        tmpType = kRegTypeChar;
        goto aput_1nr_common;
    case OP_APUT_SHORT:
        tmpType = kRegTypeShort;
        goto aput_1nr_common;
aput_1nr_common:
        {
            RegType srcType, dstType, indexType;

            indexType = getRegisterType(workLine, decInsn.vC);
            checkArrayIndexType(meth, indexType, &failure);
            if (!VERIFY_OK(failure))
                break;

            srcType = getRegisterType(workLine, decInsn.vA);

            /* correct if float */
            if (srcType == kRegTypeFloat && tmpType == kRegTypeInteger)
                tmpType = kRegTypeFloat;

            /* make sure the source register has the correct type */
            if (!canConvertTo1nr(srcType, tmpType)) {
                LOG_VFY("VFY: invalid reg type %d on aput instr (need %d)",
                    srcType, tmpType);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }

            resClass = getClassFromRegister(workLine, decInsn.vB, &failure);
            if (!VERIFY_OK(failure))
                break;

            /* resClass can be null if the reg type is Zero */
            if (resClass == NULL)
                break;

            if (!dvmIsArrayClass(resClass) || resClass->arrayDim != 1 ||
                resClass->elementClass->primitiveType == PRIM_NOT)
            {
                LOG_VFY("VFY: invalid aput-1nr on %s", resClass->descriptor);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }

            /* verify that instruction matches array */
            dstType = primitiveTypeToRegType(
                                    resClass->elementClass->primitiveType);

            /* correct if float */
            if (dstType == kRegTypeFloat && tmpType == kRegTypeInteger)
                tmpType = kRegTypeFloat;

            verifyRegisterType(workLine, decInsn.vA, dstType, &failure);

            if (dstType == kRegTypeUnknown ||
                !checkFieldArrayStore1nr(tmpType, dstType)) {
                LOG_VFY("VFY: invalid aput-1nr on %s (inst=%d dst=%d)",
                        resClass->descriptor, tmpType, dstType);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
        }
        break;
    case OP_APUT_WIDE:
        tmpType = getRegisterType(workLine, decInsn.vC);
        checkArrayIndexType(meth, tmpType, &failure);
        if (!VERIFY_OK(failure))
            break;

        resClass = getClassFromRegister(workLine, decInsn.vB, &failure);
        if (!VERIFY_OK(failure))
            break;
        if (resClass != NULL) {
            /* verify the class and try to refine "dstType" */
            if (!dvmIsArrayClass(resClass) || resClass->arrayDim != 1 ||
                resClass->elementClass->primitiveType == PRIM_NOT)
            {
                LOG_VFY("VFY: invalid aput-wide on %s",
                        resClass->descriptor);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }

            switch (resClass->elementClass->primitiveType) {
            case PRIM_LONG:
                verifyRegisterType(workLine, decInsn.vA, kRegTypeLongLo, &failure);
                break;
            case PRIM_DOUBLE:
                verifyRegisterType(workLine, decInsn.vA, kRegTypeDoubleLo, &failure);
                break;
            default:
                LOG_VFY("VFY: invalid aput-wide on %s",
                        resClass->descriptor);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
        }
        break;
    case OP_APUT_OBJECT:
        tmpType = getRegisterType(workLine, decInsn.vC);
        checkArrayIndexType(meth, tmpType, &failure);
        if (!VERIFY_OK(failure))
            break;

        /* get the ref we're storing; Zero is okay, Uninit is not */
        resClass = getClassFromRegister(workLine, decInsn.vA, &failure);
        if (!VERIFY_OK(failure))
            break;
        if (resClass != NULL) {
            ClassObject* arrayClass;
            ClassObject* elementClass;

            /*
             * Get the array class.  If the array ref is null, we won't
             * have type information (and we'll crash at runtime with a
             * null pointer exception).
             */
            arrayClass = getClassFromRegister(workLine, decInsn.vB, &failure);

            if (arrayClass != NULL) {
                /* see if the array holds a compatible type */
                if (!dvmIsArrayClass(arrayClass)) {
                    LOG_VFY("VFY: invalid aput-object on %s",
                            arrayClass->descriptor);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }

                /*
                 * Find the element class.  resClass->elementClass indicates
                 * the basic type, which won't be what we want for a
                 * multi-dimensional array.
                 *
                 * All we want to check here is that the element type is a
                 * reference class.  We *don't* check instanceof here, because
                 * you can still put a String into a String[] after the latter
                 * has been cast to an Object[].
                 */
                if (arrayClass->descriptor[1] == '[') {
                    assert(arrayClass->arrayDim > 1);
                    elementClass = dvmFindArrayClass(&arrayClass->descriptor[1],
                                        arrayClass->classLoader);
                } else {
                    assert(arrayClass->arrayDim == 1);
                    elementClass = arrayClass->elementClass;
                }
                if (elementClass->primitiveType != PRIM_NOT) {
                    LOG_VFY("VFY: invalid aput-object of %s into %s",
                            resClass->descriptor, arrayClass->descriptor);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }
            }
        }
        break;

    case OP_IGET:
        tmpType = kRegTypeInteger;
        goto iget_1nr_common;
    case OP_IGET_BOOLEAN:
        tmpType = kRegTypeBoolean;
        goto iget_1nr_common;
    case OP_IGET_BYTE:
        tmpType = kRegTypeByte;
        goto iget_1nr_common;
    case OP_IGET_CHAR:
        tmpType = kRegTypeChar;
        goto iget_1nr_common;
    case OP_IGET_SHORT:
        tmpType = kRegTypeShort;
        goto iget_1nr_common;
iget_1nr_common:
        {
            InstField* instField;
            RegType objType, fieldType;

            objType = getRegisterType(workLine, decInsn.vB);
            instField = getInstField(meth, uninitMap, objType, decInsn.vC,
                            &failure);
            if (!VERIFY_OK(failure))
                break;

            /* make sure the field's type is compatible with expectation */
            fieldType = primSigCharToRegType(instField->signature[0]);

            /* correct if float */
            if (fieldType == kRegTypeFloat && tmpType == kRegTypeInteger)
                tmpType = kRegTypeFloat;

            if (fieldType == kRegTypeUnknown ||
                !checkFieldArrayStore1nr(tmpType, fieldType))
            {
                LOG_VFY("VFY: invalid iget-1nr of %s.%s (inst=%d field=%d)",
                        instField->clazz->descriptor,
                        instField->name, tmpType, fieldType);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }

            setRegisterType(workLine, decInsn.vA, tmpType);
        }
        break;
    case OP_IGET_WIDE:
        {
            RegType dstType;
            InstField* instField;
            RegType objType;

            objType = getRegisterType(workLine, decInsn.vB);
            instField = getInstField(meth, uninitMap, objType, decInsn.vC,
                            &failure);
            if (!VERIFY_OK(failure))
                break;
            /* check the type, which should be prim */
            switch (instField->signature[0]) {
            case 'D':
                dstType = kRegTypeDoubleLo;
                break;
            case 'J':
                dstType = kRegTypeLongLo;
                break;
            default:
                LOG_VFY("VFY: invalid iget-wide of %s.%s",
                        instField->clazz->descriptor,
                        instField->name);
                dstType = kRegTypeUnknown;
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
            if (VERIFY_OK(failure)) {
                setRegisterType(workLine, decInsn.vA, dstType);
            }
        }
        break;
    case OP_IGET_OBJECT:
        {
            ClassObject* fieldClass;
            InstField* instField;
            RegType objType;

            objType = getRegisterType(workLine, decInsn.vB);
            instField = getInstField(meth, uninitMap, objType, decInsn.vC,
                            &failure);
            if (!VERIFY_OK(failure))
                break;
            fieldClass = getFieldClass(meth, instField);
            if (fieldClass == NULL) {
                /* class not found or primitive type */
                LOG_VFY("VFY: unable to recover field class from '%s'",
                    instField->signature);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
            if (VERIFY_OK(failure)) {
                assert(!dvmIsPrimitiveClass(fieldClass));
                setRegisterType(workLine, decInsn.vA,
                    regTypeFromClass(fieldClass));
            }
        }
        break;
    case OP_IPUT:
        tmpType = kRegTypeInteger;
        goto iput_1nr_common;
    case OP_IPUT_BOOLEAN:
        tmpType = kRegTypeBoolean;
        goto iput_1nr_common;
    case OP_IPUT_BYTE:
        tmpType = kRegTypeByte;
        goto iput_1nr_common;
    case OP_IPUT_CHAR:
        tmpType = kRegTypeChar;
        goto iput_1nr_common;
    case OP_IPUT_SHORT:
        tmpType = kRegTypeShort;
        goto iput_1nr_common;
iput_1nr_common:
        {
            RegType srcType, fieldType, objType;
            InstField* instField;

            srcType = getRegisterType(workLine, decInsn.vA);

            /*
             * javac generates synthetic functions that write byte values
             * into boolean fields.
             */
            if (tmpType == kRegTypeBoolean && srcType == kRegTypeByte)
                tmpType = kRegTypeByte;

            /* correct if float */
            if (srcType == kRegTypeFloat && tmpType == kRegTypeInteger)
              tmpType = kRegTypeFloat;

            /* make sure the source register has the correct type */
            if (!canConvertTo1nr(srcType, tmpType)) {
                LOG_VFY("VFY: invalid reg type %d on iput instr (need %d)",
                    srcType, tmpType);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }

            objType = getRegisterType(workLine, decInsn.vB);
            instField = getInstField(meth, uninitMap, objType, decInsn.vC,
                            &failure);
            if (!VERIFY_OK(failure))
                break;
            checkFinalFieldAccess(meth, instField, &failure);
            if (!VERIFY_OK(failure))
                break;

            /* get type of field we're storing into */
            fieldType = primSigCharToRegType(instField->signature[0]);

            /* correct if float */
            if (fieldType == kRegTypeFloat && tmpType == kRegTypeInteger)
                tmpType = kRegTypeFloat;

            if (fieldType == kRegTypeBoolean && srcType == kRegTypeByte)
                fieldType = kRegTypeByte;

            verifyRegisterType(workLine, decInsn.vA, fieldType, &failure);

            if (fieldType == kRegTypeUnknown ||
                !checkFieldArrayStore1nr(tmpType, fieldType))
            {
                LOG_VFY("VFY: invalid iput-1nr of %s.%s (inst=%d field=%d)",
                        instField->clazz->descriptor,
                        instField->name, tmpType, fieldType);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
        }
        break;
    case OP_IPUT_WIDE:
        tmpType = getRegisterType(workLine, decInsn.vA);
        {
            RegType typeHi = getRegisterType(workLine, decInsn.vA + 1);
            checkTypeCategory(tmpType, kTypeCategory2, &failure);
            checkWidePair(tmpType, typeHi, &failure);
        }
        if (!VERIFY_OK(failure))
            break;

        InstField* instField;
        RegType objType;

        objType = getRegisterType(workLine, decInsn.vB);
        instField = getInstField(meth, uninitMap, objType, decInsn.vC,
                        &failure);
        if (!VERIFY_OK(failure))
            break;
        checkFinalFieldAccess(meth, instField, &failure);
        if (!VERIFY_OK(failure))
            break;

        /* check the type, which should be prim */
        switch (instField->signature[0]) {
        case 'D':
            verifyRegisterType(workLine, decInsn.vA, kRegTypeDoubleLo, &failure);
            break;
        case 'J':
            verifyRegisterType(workLine, decInsn.vA, kRegTypeLongLo, &failure);
            break;
        default:
            LOG_VFY("VFY: invalid iput-wide of %s.%s",
                    instField->clazz->descriptor,
                    instField->name);
            failure = VERIFY_ERROR_GENERIC;
            break;
        }
        break;
    case OP_IPUT_OBJECT:
        {
            ClassObject* fieldClass;
            ClassObject* valueClass;
            InstField* instField;
            RegType objType, valueType;

            objType = getRegisterType(workLine, decInsn.vB);
            instField = getInstField(meth, uninitMap, objType, decInsn.vC,
                            &failure);
            if (!VERIFY_OK(failure))
                break;
            checkFinalFieldAccess(meth, instField, &failure);
            if (!VERIFY_OK(failure))
                break;

            fieldClass = getFieldClass(meth, instField);
            if (fieldClass == NULL) {
                LOG_VFY("VFY: unable to recover field class from '%s'",
                    instField->signature);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }

            valueType = getRegisterType(workLine, decInsn.vA);
            if (!regTypeIsReference(valueType)) {
                LOG_VFY("VFY: storing non-ref v%d into ref field '%s' (%s)",
                        decInsn.vA, instField->name,
                        fieldClass->descriptor);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
            if (valueType != kRegTypeZero) {
                valueClass = regTypeInitializedReferenceToClass(valueType);
                if (valueClass == NULL) {
                    LOG_VFY("VFY: storing uninit ref v%d into ref field",
                        decInsn.vA);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }
                /* allow if field is any interface or field is base class */
                if (!dvmIsInterfaceClass(fieldClass) &&
                    !dvmInstanceof(valueClass, fieldClass))
                {
                    LOG_VFY("VFY: storing type '%s' into field type '%s' (%s.%s)",
                            valueClass->descriptor, fieldClass->descriptor,
                            instField->clazz->descriptor,
                            instField->name);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }
            }
        }
        break;

    case OP_SGET:
        tmpType = kRegTypeInteger;
        goto sget_1nr_common;
    case OP_SGET_BOOLEAN:
        tmpType = kRegTypeBoolean;
        goto sget_1nr_common;
    case OP_SGET_BYTE:
        tmpType = kRegTypeByte;
        goto sget_1nr_common;
    case OP_SGET_CHAR:
        tmpType = kRegTypeChar;
        goto sget_1nr_common;
    case OP_SGET_SHORT:
        tmpType = kRegTypeShort;
        goto sget_1nr_common;
sget_1nr_common:
        {
            StaticField* staticField;
            RegType fieldType;

            staticField = getStaticField(meth, decInsn.vB, &failure);
            if (!VERIFY_OK(failure))
                break;

            /*
             * Make sure the field's type is compatible with expectation.
             * We can get ourselves into trouble if we mix & match loads
             * and stores with different widths, so rather than just checking
             * "canConvertTo1nr" we require that the field types have equal
             * widths.
             */
            fieldType = primSigCharToRegType(staticField->signature[0]);

            /* correct if float */
            if (fieldType == kRegTypeFloat && tmpType == kRegTypeInteger)
                tmpType = kRegTypeFloat;

            if (!checkFieldArrayStore1nr(tmpType, fieldType)) {
                LOG_VFY("VFY: invalid sget-1nr of %s.%s (inst=%d actual=%d)",
                    staticField->clazz->descriptor,
                    staticField->name, tmpType, fieldType);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }

            setRegisterType(workLine, decInsn.vA, tmpType);
        }
        break;
    case OP_SGET_WIDE:
        {
            StaticField* staticField;
            RegType dstType;

            staticField = getStaticField(meth, decInsn.vB, &failure);
            if (!VERIFY_OK(failure))
                break;
            /* check the type, which should be prim */
            switch (staticField->signature[0]) {
            case 'D':
                dstType = kRegTypeDoubleLo;
                break;
            case 'J':
                dstType = kRegTypeLongLo;
                break;
            default:
                LOG_VFY("VFY: invalid sget-wide of %s.%s",
                        staticField->clazz->descriptor,
                        staticField->name);
                dstType = kRegTypeUnknown;
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
            if (VERIFY_OK(failure)) {
                setRegisterType(workLine, decInsn.vA, dstType);
            }
        }
        break;
    case OP_SGET_OBJECT:
        {
            StaticField* staticField;
            ClassObject* fieldClass;

            staticField = getStaticField(meth, decInsn.vB, &failure);
            if (!VERIFY_OK(failure))
                break;
            fieldClass = getFieldClass(meth, staticField);
            if (fieldClass == NULL) {
                LOG_VFY("VFY: unable to recover field class from '%s'",
                    staticField->signature);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
            if (dvmIsPrimitiveClass(fieldClass)) {
                LOG_VFY("VFY: attempt to get prim field with sget-object");
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
            setRegisterType(workLine, decInsn.vA, regTypeFromClass(fieldClass));
        }
        break;
    case OP_SPUT:
        tmpType = kRegTypeInteger;
        goto sput_1nr_common;
    case OP_SPUT_BOOLEAN:
        tmpType = kRegTypeBoolean;
        goto sput_1nr_common;
    case OP_SPUT_BYTE:
        tmpType = kRegTypeByte;
        goto sput_1nr_common;
    case OP_SPUT_CHAR:
        tmpType = kRegTypeChar;
        goto sput_1nr_common;
    case OP_SPUT_SHORT:
        tmpType = kRegTypeShort;
        goto sput_1nr_common;
sput_1nr_common:
        {
            RegType srcType, fieldType;
            StaticField* staticField;

            srcType = getRegisterType(workLine, decInsn.vA);

            /*
             * javac generates synthetic functions that write byte values
             * into boolean fields.
             */
            if (tmpType == kRegTypeBoolean && srcType == kRegTypeByte)
                tmpType = kRegTypeByte;

            /* correct if float */
            if (srcType == kRegTypeFloat && tmpType == kRegTypeInteger)
              tmpType = kRegTypeFloat;

            /* make sure the source register has the correct type */
            if (!canConvertTo1nr(srcType, tmpType)) {
                LOG_VFY("VFY: invalid reg type %d on sput instr (need %d)",
                    srcType, tmpType);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }

            staticField = getStaticField(meth, decInsn.vB, &failure);
            if (!VERIFY_OK(failure))
                break;
            checkFinalFieldAccess(meth, staticField, &failure);
            if (!VERIFY_OK(failure))
                break;

            /*
             * Get type of field we're storing into.  We know that the
             * contents of the register match the instruction, but we also
             * need to ensure that the instruction matches the field type.
             * Using e.g. sput-short to write into a 32-bit integer field
             * can lead to trouble if we do 16-bit writes.
             */
            fieldType = primSigCharToRegType(staticField->signature[0]);

            /* correct if float */
            if (fieldType == kRegTypeFloat && tmpType == kRegTypeInteger)
                tmpType = kRegTypeFloat;

            if (fieldType == kRegTypeBoolean && srcType == kRegTypeByte)
                fieldType = kRegTypeByte;

            verifyRegisterType(workLine, decInsn.vA, fieldType, &failure);

            if (fieldType == kRegTypeUnknown ||
                !checkFieldArrayStore1nr(tmpType, fieldType)) {
                LOG_VFY("VFY: invalid sput-1nr of %s.%s (inst=%d actual=%d)",
                    staticField->clazz->descriptor,
                    staticField->name, tmpType, fieldType);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
        }
        break;
    case OP_SPUT_WIDE:
        tmpType = getRegisterType(workLine, decInsn.vA);
        {
            RegType typeHi = getRegisterType(workLine, decInsn.vA + 1);
            checkTypeCategory(tmpType, kTypeCategory2, &failure);
            checkWidePair(tmpType, typeHi, &failure);
        }
        if (!VERIFY_OK(failure))
            break;

        StaticField* staticField;

        staticField = getStaticField(meth, decInsn.vB, &failure);
        if (!VERIFY_OK(failure))
            break;
        checkFinalFieldAccess(meth, staticField, &failure);
        if (!VERIFY_OK(failure))
            break;

        /* check the type, which should be prim */
        switch (staticField->signature[0]) {
        case 'D':
            verifyRegisterType(workLine, decInsn.vA, kRegTypeDoubleLo, &failure);
            break;
        case 'J':
            verifyRegisterType(workLine, decInsn.vA, kRegTypeLongLo, &failure);
            break;
        default:
            LOG_VFY("VFY: invalid sput-wide of %s.%s",
                    staticField->clazz->descriptor,
                    staticField->name);
            failure = VERIFY_ERROR_GENERIC;
            break;
        }
        break;
    case OP_SPUT_OBJECT:
        {
            ClassObject* fieldClass;
            ClassObject* valueClass;
            StaticField* staticField;
            RegType valueType;

            staticField = getStaticField(meth, decInsn.vB, &failure);
            if (!VERIFY_OK(failure))
                break;
            checkFinalFieldAccess(meth, staticField, &failure);
            if (!VERIFY_OK(failure))
                break;

            fieldClass = getFieldClass(meth, staticField);
            if (fieldClass == NULL) {
                LOG_VFY("VFY: unable to recover field class from '%s'",
                    staticField->signature);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }

            valueType = getRegisterType(workLine, decInsn.vA);
            if (!regTypeIsReference(valueType)) {
                LOG_VFY("VFY: storing non-ref v%d into ref field '%s' (%s)",
                        decInsn.vA, staticField->name,
                        fieldClass->descriptor);
                failure = VERIFY_ERROR_GENERIC;
                break;
            }
            if (valueType != kRegTypeZero) {
                valueClass = regTypeInitializedReferenceToClass(valueType);
                if (valueClass == NULL) {
                    LOG_VFY("VFY: storing uninit ref v%d into ref field",
                        decInsn.vA);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }
                /* allow if field is any interface or field is base class */
                if (!dvmIsInterfaceClass(fieldClass) &&
                    !dvmInstanceof(valueClass, fieldClass))
                {
                    LOG_VFY("VFY: storing type '%s' into field type '%s' (%s.%s)",
                            valueClass->descriptor, fieldClass->descriptor,
                            staticField->clazz->descriptor,
                            staticField->name);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }
            }
        }
        break;

    case OP_INVOKE_VIRTUAL:
    case OP_INVOKE_VIRTUAL_RANGE:
    case OP_INVOKE_SUPER:
    case OP_INVOKE_SUPER_RANGE:
        {
            Method* calledMethod;
            RegType returnType;
            bool isRange;
            bool isSuper;

            isRange =  (decInsn.opcode == OP_INVOKE_VIRTUAL_RANGE ||
                        decInsn.opcode == OP_INVOKE_SUPER_RANGE);
            isSuper =  (decInsn.opcode == OP_INVOKE_SUPER ||
                        decInsn.opcode == OP_INVOKE_SUPER_RANGE);

            calledMethod = verifyInvocationArgs(meth, workLine, insnRegCount,
                            &decInsn, uninitMap, METHOD_VIRTUAL, isRange,
                            isSuper, &failure);
            if (!VERIFY_OK(failure))
                break;
            returnType = getMethodReturnType(calledMethod);
            setResultRegisterType(workLine, insnRegCount, returnType);
            justSetResult = true;
        }
        break;
    case OP_INVOKE_DIRECT:
    case OP_INVOKE_DIRECT_RANGE:
        {
            RegType returnType;
            Method* calledMethod;
            bool isRange;

            isRange =  (decInsn.opcode == OP_INVOKE_DIRECT_RANGE);
            calledMethod = verifyInvocationArgs(meth, workLine, insnRegCount,
                            &decInsn, uninitMap, METHOD_DIRECT, isRange,
                            false, &failure);
            if (!VERIFY_OK(failure))
                break;

            /*
             * Some additional checks when calling <init>.  We know from
             * the invocation arg check that the "this" argument is an
             * instance of calledMethod->clazz.  Now we further restrict
             * that to require that calledMethod->clazz is the same as
             * this->clazz or this->super, allowing the latter only if
             * the "this" argument is the same as the "this" argument to
             * this method (which implies that we're in <init> ourselves).
             */
            if (isInitMethod(calledMethod)) {
                RegType thisType;
                thisType = getInvocationThis(workLine, &decInsn, &failure);
                if (!VERIFY_OK(failure))
                    break;

                /* no null refs allowed (?) */
                if (thisType == kRegTypeZero) {
                    LOG_VFY("VFY: unable to initialize null ref");
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }

                ClassObject* thisClass;

                thisClass = regTypeReferenceToClass(thisType, uninitMap);
                assert(thisClass != NULL);

                /* must be in same class or in superclass */
                if (calledMethod->clazz == thisClass->super) {
                    if (thisClass != meth->clazz) {
                        LOG_VFY("VFY: invoke-direct <init> on super only "
                            "allowed for 'this' in <init>");
                        failure = VERIFY_ERROR_GENERIC;
                        break;
                    }
                }  else if (calledMethod->clazz != thisClass) {
                    LOG_VFY("VFY: invoke-direct <init> must be on current "
                            "class or super");
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }

                /* arg must be an uninitialized reference */
                if (!regTypeIsUninitReference(thisType)) {
                    LOG_VFY("VFY: can only initialize the uninitialized");
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }

                /*
                 * Replace the uninitialized reference with an initialized
                 * one, and clear the entry in the uninit map.  We need to
                 * do this for all registers that have the same object
                 * instance in them, not just the "this" register.
                 */
                markRefsAsInitialized(workLine, insnRegCount, uninitMap,
                    thisType, &failure);
                if (!VERIFY_OK(failure))
                    break;
            }
            returnType = getMethodReturnType(calledMethod);
            setResultRegisterType(workLine, insnRegCount, returnType);
            justSetResult = true;
        }
        break;
    case OP_INVOKE_STATIC:
    case OP_INVOKE_STATIC_RANGE:
        {
            RegType returnType;
            Method* calledMethod;
            bool isRange;

            isRange =  (decInsn.opcode == OP_INVOKE_STATIC_RANGE);
            calledMethod = verifyInvocationArgs(meth, workLine, insnRegCount,
                            &decInsn, uninitMap, METHOD_STATIC, isRange,
                            false, &failure);
            if (!VERIFY_OK(failure))
                break;

            returnType = getMethodReturnType(calledMethod);
            setResultRegisterType(workLine, insnRegCount, returnType);
            justSetResult = true;
        }
        break;
    case OP_INVOKE_INTERFACE:
    case OP_INVOKE_INTERFACE_RANGE:
        {
            RegType /*thisType,*/ returnType;
            Method* absMethod;
            bool isRange;

            isRange =  (decInsn.opcode == OP_INVOKE_INTERFACE_RANGE);
            absMethod = verifyInvocationArgs(meth, workLine, insnRegCount,
                            &decInsn, uninitMap, METHOD_INTERFACE, isRange,
                            false, &failure);
            if (!VERIFY_OK(failure))
                break;

#if 0       /* can't do this here, fails on dalvik test 052-verifier-fun */
            /*
             * Get the type of the "this" arg, which should always be an
             * interface class.  Because we don't do a full merge on
             * interface classes, this might have reduced to Object.
             */
            thisType = getInvocationThis(workLine, &decInsn, &failure);
            if (!VERIFY_OK(failure))
                break;

            if (thisType == kRegTypeZero) {
                /* null pointer always passes (and always fails at runtime) */
            } else {
                ClassObject* thisClass;

                thisClass = regTypeInitializedReferenceToClass(thisType);
                if (thisClass == NULL) {
                    LOG_VFY("VFY: interface call on uninitialized");
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }

                /*
                 * Either "thisClass" needs to be the interface class that
                 * defined absMethod, or absMethod's class needs to be one
                 * of the interfaces implemented by "thisClass".  (Or, if
                 * we couldn't complete the merge, this will be Object.)
                 */
                if (thisClass != absMethod->clazz &&
                    thisClass != gDvm.classJavaLangObject &&
                    !dvmImplements(thisClass, absMethod->clazz))
                {
                    LOG_VFY("VFY: unable to match absMethod '%s' with %s interfaces",
                            absMethod->name, thisClass->descriptor);
                    failure = VERIFY_ERROR_GENERIC;
                    break;
                }
            }
#endif

            /*
             * We don't have an object instance, so we can't find the
             * concrete method.  However, all of the type information is
             * in the abstract method, so we're good.
             */
            returnType = getMethodReturnType(absMethod);
            setResultRegisterType(workLine, insnRegCount, returnType);
            justSetResult = true;
        }
        break;

    case OP_NEG_INT:
    case OP_NOT_INT:
        checkUnop(workLine, &decInsn,
            kRegTypeInteger, kRegTypeInteger, &failure);
        break;
    case OP_NEG_LONG:
    case OP_NOT_LONG:
        checkUnop(workLine, &decInsn,
            kRegTypeLongLo, kRegTypeLongLo, &failure);
        break;
    case OP_NEG_FLOAT:
        checkUnop(workLine, &decInsn,
            kRegTypeFloat, kRegTypeFloat, &failure);
        break;
    case OP_NEG_DOUBLE:
        checkUnop(workLine, &decInsn,
            kRegTypeDoubleLo, kRegTypeDoubleLo, &failure);
        break;
    case OP_INT_TO_LONG:
        checkUnop(workLine, &decInsn,
            kRegTypeLongLo, kRegTypeInteger, &failure);
        break;
    case OP_INT_TO_FLOAT:
        checkUnop(workLine, &decInsn,
            kRegTypeFloat, kRegTypeInteger, &failure);
        break;
    case OP_INT_TO_DOUBLE:
        checkUnop(workLine, &decInsn,
            kRegTypeDoubleLo, kRegTypeInteger, &failure);
        break;
    case OP_LONG_TO_INT:
        checkUnop(workLine, &decInsn,
            kRegTypeInteger, kRegTypeLongLo, &failure);
        break;
    case OP_LONG_TO_FLOAT:
        checkUnop(workLine, &decInsn,
            kRegTypeFloat, kRegTypeLongLo, &failure);
        break;
    case OP_LONG_TO_DOUBLE:
        checkUnop(workLine, &decInsn,
            kRegTypeDoubleLo, kRegTypeLongLo, &failure);
        break;
    case OP_FLOAT_TO_INT:
        checkUnop(workLine, &decInsn,
            kRegTypeInteger, kRegTypeFloat, &failure);
        break;
    case OP_FLOAT_TO_LONG:
        checkUnop(workLine, &decInsn,
            kRegTypeLongLo, kRegTypeFloat, &failure);
        break;
    case OP_FLOAT_TO_DOUBLE:
        checkUnop(workLine, &decInsn,
            kRegTypeDoubleLo, kRegTypeFloat, &failure);
        break;
    case OP_DOUBLE_TO_INT:
        checkUnop(workLine, &decInsn,
            kRegTypeInteger, kRegTypeDoubleLo, &failure);
        break;
    case OP_DOUBLE_TO_LONG:
        checkUnop(workLine, &decInsn,
            kRegTypeLongLo, kRegTypeDoubleLo, &failure);
        break;
    case OP_DOUBLE_TO_FLOAT:
        checkUnop(workLine, &decInsn,
            kRegTypeFloat, kRegTypeDoubleLo, &failure);
        break;
    case OP_INT_TO_BYTE:
        checkUnop(workLine, &decInsn,
            kRegTypeByte, kRegTypeInteger, &failure);
        break;
    case OP_INT_TO_CHAR:
        checkUnop(workLine, &decInsn,
            kRegTypeChar, kRegTypeInteger, &failure);
        break;
    case OP_INT_TO_SHORT:
        checkUnop(workLine, &decInsn,
            kRegTypeShort, kRegTypeInteger, &failure);
        break;

    case OP_ADD_INT:
    case OP_SUB_INT:
    case OP_MUL_INT:
    case OP_REM_INT:
    case OP_DIV_INT:
    case OP_SHL_INT:
    case OP_SHR_INT:
    case OP_USHR_INT:
        checkBinop(workLine, &decInsn,
            kRegTypeInteger, kRegTypeInteger, kRegTypeInteger, false, &failure);
        break;
    case OP_AND_INT:
    case OP_OR_INT:
    case OP_XOR_INT:
        checkBinop(workLine, &decInsn,
            kRegTypeInteger, kRegTypeInteger, kRegTypeInteger, true, &failure);
        break;
    case OP_ADD_LONG:
    case OP_SUB_LONG:
    case OP_MUL_LONG:
    case OP_DIV_LONG:
    case OP_REM_LONG:
    case OP_AND_LONG:
    case OP_OR_LONG:
    case OP_XOR_LONG:
        checkBinop(workLine, &decInsn,
            kRegTypeLongLo, kRegTypeLongLo, kRegTypeLongLo, false, &failure);
        break;
    case OP_SHL_LONG:
    case OP_SHR_LONG:
    case OP_USHR_LONG:
        /* shift distance is Int, making these different from other binops */
        checkBinop(workLine, &decInsn,
            kRegTypeLongLo, kRegTypeLongLo, kRegTypeInteger, false, &failure);
        break;
    case OP_ADD_FLOAT:
    case OP_SUB_FLOAT:
    case OP_MUL_FLOAT:
    case OP_DIV_FLOAT:
    case OP_REM_FLOAT:
        checkBinop(workLine, &decInsn,
            kRegTypeFloat, kRegTypeFloat, kRegTypeFloat, false, &failure);
        break;
    case OP_ADD_DOUBLE:
    case OP_SUB_DOUBLE:
    case OP_MUL_DOUBLE:
    case OP_DIV_DOUBLE:
    case OP_REM_DOUBLE:
        checkBinop(workLine, &decInsn,
            kRegTypeDoubleLo, kRegTypeDoubleLo, kRegTypeDoubleLo, false,
            &failure);
        break;
    case OP_ADD_INT_2ADDR:
    case OP_SUB_INT_2ADDR:
    case OP_MUL_INT_2ADDR:
    case OP_REM_INT_2ADDR:
    case OP_SHL_INT_2ADDR:
    case OP_SHR_INT_2ADDR:
    case OP_USHR_INT_2ADDR:
        checkBinop2addr(workLine, &decInsn,
            kRegTypeInteger, kRegTypeInteger, kRegTypeInteger, false, &failure);
        break;
    case OP_AND_INT_2ADDR:
    case OP_OR_INT_2ADDR:
    case OP_XOR_INT_2ADDR:
        checkBinop2addr(workLine, &decInsn,
            kRegTypeInteger, kRegTypeInteger, kRegTypeInteger, true, &failure);
        break;
    case OP_DIV_INT_2ADDR:
        checkBinop2addr(workLine, &decInsn,
            kRegTypeInteger, kRegTypeInteger, kRegTypeInteger, false, &failure);
        break;
    case OP_ADD_LONG_2ADDR:
    case OP_SUB_LONG_2ADDR:
    case OP_MUL_LONG_2ADDR:
    case OP_DIV_LONG_2ADDR:
    case OP_REM_LONG_2ADDR:
    case OP_AND_LONG_2ADDR:
    case OP_OR_LONG_2ADDR:
    case OP_XOR_LONG_2ADDR:
        checkBinop2addr(workLine, &decInsn,
            kRegTypeLongLo, kRegTypeLongLo, kRegTypeLongLo, false, &failure);
        break;
    case OP_SHL_LONG_2ADDR:
    case OP_SHR_LONG_2ADDR:
    case OP_USHR_LONG_2ADDR:
        checkBinop2addr(workLine, &decInsn,
            kRegTypeLongLo, kRegTypeLongLo, kRegTypeInteger, false, &failure);
        break;
    case OP_ADD_FLOAT_2ADDR:
    case OP_SUB_FLOAT_2ADDR:
    case OP_MUL_FLOAT_2ADDR:
    case OP_DIV_FLOAT_2ADDR:
    case OP_REM_FLOAT_2ADDR:
        checkBinop2addr(workLine, &decInsn,
            kRegTypeFloat, kRegTypeFloat, kRegTypeFloat, false, &failure);
        break;
    case OP_ADD_DOUBLE_2ADDR:
    case OP_SUB_DOUBLE_2ADDR:
    case OP_MUL_DOUBLE_2ADDR:
    case OP_DIV_DOUBLE_2ADDR:
    case OP_REM_DOUBLE_2ADDR:
        checkBinop2addr(workLine, &decInsn,
            kRegTypeDoubleLo, kRegTypeDoubleLo, kRegTypeDoubleLo, false,
            &failure);
        break;
    case OP_ADD_INT_LIT16:
    case OP_RSUB_INT:
    case OP_MUL_INT_LIT16:
    case OP_DIV_INT_LIT16:
    case OP_REM_INT_LIT16:
        checkLitop(workLine, &decInsn,
            kRegTypeInteger, kRegTypeInteger, false, &failure);
        break;
    case OP_AND_INT_LIT16:
    case OP_OR_INT_LIT16:
    case OP_XOR_INT_LIT16:
        checkLitop(workLine, &decInsn,
            kRegTypeInteger, kRegTypeInteger, true, &failure);
        break;
    case OP_ADD_INT_LIT8:
    case OP_RSUB_INT_LIT8:
    case OP_MUL_INT_LIT8:
    case OP_DIV_INT_LIT8:
    case OP_REM_INT_LIT8:
    case OP_SHL_INT_LIT8:
        checkLitop(workLine, &decInsn,
            kRegTypeInteger, kRegTypeInteger, false, &failure);
        break;
    case OP_SHR_INT_LIT8:
        tmpType = adjustForRightShift(workLine,
            decInsn.vB, decInsn.vC, false, &failure);
        checkLitop(workLine, &decInsn,
            tmpType, kRegTypeInteger, false, &failure);
        break;
    case OP_USHR_INT_LIT8:
        tmpType = adjustForRightShift(workLine,
            decInsn.vB, decInsn.vC, true, &failure);
        checkLitop(workLine, &decInsn,
            tmpType, kRegTypeInteger, false, &failure);
        break;
    case OP_AND_INT_LIT8:
    case OP_OR_INT_LIT8:
    case OP_XOR_INT_LIT8:
        checkLitop(workLine, &decInsn,
            kRegTypeInteger, kRegTypeInteger, true, &failure);
        break;

    /*
     * This falls into the general category of "optimized" instructions,
     * which don't generally appear during verification.  Because it's
     * inserted in the course of verification, we can expect to see it here.
     */
    case OP_THROW_VERIFICATION_ERROR:
        break;

    /*
     * Verifying "quickened" instructions is tricky, because we have
     * discarded the original field/method information.  The byte offsets
     * and vtable indices only have meaning in the context of an object
     * instance.
     *
     * If a piece of code declares a local reference variable, assigns
     * null to it, and then issues a virtual method call on it, we
     * cannot evaluate the method call during verification.  This situation
     * isn't hard to handle, since we know the call will always result in an
     * NPE, and the arguments and return value don't matter.  Any code that
     * depends on the result of the method call is inaccessible, so the
     * fact that we can't fully verify anything that comes after the bad
     * call is not a problem.
     *
     * We must also consider the case of multiple code paths, only some of
     * which involve a null reference.  We can completely verify the method
     * if we sidestep the results of executing with a null reference.
     * For example, if on the first pass through the code we try to do a
     * virtual method invocation through a null ref, we have to skip the
     * method checks and have the method return a "wildcard" type (which
     * merges with anything to become that other thing).  The move-result
     * will tell us if it's a reference, single-word numeric, or double-word
     * value.  We continue to perform the verification, and at the end of
     * the function any invocations that were never fully exercised are
     * marked as null-only.
     *
     * We would do something similar for the field accesses.  The field's
     * type, once known, can be used to recover the width of short integers.
     * If the object reference was null, the field-get returns the "wildcard"
     * type, which is acceptable for any operation.
     */
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

    /*
     * These instructions are equivalent (from the verifier's point of view)
     * to the original form.  The change was made for correctness rather
     * than improved performance (except for invoke-object-init, which
     * provides both).  The substitution takes place after verification
     * completes, though, so we don't expect to see them here.
     */
    case OP_INVOKE_OBJECT_INIT_RANGE:
    case OP_RETURN_VOID_BARRIER:
    case OP_IGET_VOLATILE:
    case OP_IGET_WIDE_VOLATILE:
    case OP_IGET_OBJECT_VOLATILE:
    case OP_IPUT_VOLATILE:
    case OP_IPUT_WIDE_VOLATILE:
    case OP_IPUT_OBJECT_VOLATILE:
    case OP_SGET_VOLATILE:
    case OP_SGET_WIDE_VOLATILE:
    case OP_SGET_OBJECT_VOLATILE:
    case OP_SPUT_VOLATILE:
    case OP_SPUT_WIDE_VOLATILE:
    case OP_SPUT_OBJECT_VOLATILE:
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
        failure = VERIFY_ERROR_GENERIC;
        break;

    /*
     * DO NOT add a "default" clause here.  Without it the compiler will
     * complain if an instruction is missing (which is desirable).
     */
    }

    if (!VERIFY_OK(failure)) {
        if (failure == VERIFY_ERROR_GENERIC || gDvm.optimizing) {
            /* immediate failure, reject class */
            LOG_VFY_METH(meth, "VFY:  rejecting opcode 0x%02x at 0x%04x",
                decInsn.opcode, insnIdx);
            goto bail;
        } else {
            /* replace opcode and continue on */
            ALOGD("VFY: replacing opcode 0x%02x at 0x%04x",
                decInsn.opcode, insnIdx);
            if (!replaceFailingInstruction(meth, insnFlags, insnIdx, failure)) {
                LOG_VFY_METH(meth, "VFY:  rejecting opcode 0x%02x at 0x%04x",
                    decInsn.opcode, insnIdx);
                goto bail;
            }
            /* IMPORTANT: meth->insns may have been changed */
            insns = meth->insns + insnIdx;

            /* continue on as if we just handled a throw-verification-error */
            failure = VERIFY_ERROR_NONE;
            nextFlags = kInstrCanThrow;
        }
    }

    /*
     * If we didn't just set the result register, clear it out.  This
     * ensures that you can only use "move-result" immediately after the
     * result is set.  (We could check this statically, but it's not
     * expensive and it makes our debugging output cleaner.)
     */
    if (!justSetResult) {
        int reg = RESULT_REGISTER(insnRegCount);
        setRegisterType(workLine, reg, kRegTypeUnknown);
        setRegisterType(workLine, reg+1, kRegTypeUnknown);
    }

    /*
     * Handle "continue".  Tag the next consecutive instruction.
     */
    if ((nextFlags & kInstrCanContinue) != 0) {
        int insnWidth = dvmInsnGetWidth(insnFlags, insnIdx);
        if (insnIdx+insnWidth >= insnsSize) {
            LOG_VFY_METH(meth,
                "VFY: execution can walk off end of code area (from %#x)",
                insnIdx);
            goto bail;
        }

        /*
         * The only way to get to a move-exception instruction is to get
         * thrown there.  Make sure the next instruction isn't one.
         */
        if (!checkMoveException(meth, insnIdx+insnWidth, "next"))
            goto bail;

        if (getRegisterLine(regTable, insnIdx+insnWidth)->regTypes != NULL) {
            /*
             * Merge registers into what we have for the next instruction,
             * and set the "changed" flag if needed.
             */
            if (!updateRegisters(meth, insnFlags, regTable, insnIdx+insnWidth,
                    workLine))
                goto bail;
        } else {
            /*
             * We're not recording register data for the next instruction,
             * so we don't know what the prior state was.  We have to
             * assume that something has changed and re-evaluate it.
             */
            dvmInsnSetChanged(insnFlags, insnIdx+insnWidth, true);
        }
    }

    /*
     * Handle "branch".  Tag the branch target.
     *
     * NOTE: instructions like OP_EQZ provide information about the state
     * of the register when the branch is taken or not taken.  For example,
     * somebody could get a reference field, check it for zero, and if the
     * branch is taken immediately store that register in a boolean field
     * since the value is known to be zero.  We do not currently account for
     * that, and will reject the code.
     *
     * TODO: avoid re-fetching the branch target
     */
    if ((nextFlags & kInstrCanBranch) != 0) {
        bool isConditional;

        if (!dvmGetBranchOffset(meth, insnFlags, insnIdx, &branchTarget,
                &isConditional))
        {
            /* should never happen after static verification */
            LOG_VFY_METH(meth, "VFY: bad branch at %d", insnIdx);
            goto bail;
        }
        assert(isConditional || (nextFlags & kInstrCanContinue) == 0);
        assert(!isConditional || (nextFlags & kInstrCanContinue) != 0);

        if (!checkMoveException(meth, insnIdx+branchTarget, "branch"))
            goto bail;

        /* update branch target, set "changed" if appropriate */
        if (!updateRegisters(meth, insnFlags, regTable, insnIdx+branchTarget,
                workLine))
            goto bail;
    }

    /*
     * Handle "switch".  Tag all possible branch targets.
     *
     * We've already verified that the table is structurally sound, so we
     * just need to walk through and tag the targets.
     */
    if ((nextFlags & kInstrCanSwitch) != 0) {
        int offsetToSwitch = insns[1] | (((s4)insns[2]) << 16);
        const u2* switchInsns = insns + offsetToSwitch;
        int switchCount = switchInsns[1];
        int offsetToTargets, targ;

        if ((*insns & 0xff) == OP_PACKED_SWITCH) {
            /* 0=sig, 1=count, 2/3=firstKey */
            offsetToTargets = 4;
        } else {
            /* 0=sig, 1=count, 2..count*2 = keys */
            assert((*insns & 0xff) == OP_SPARSE_SWITCH);
            offsetToTargets = 2 + 2*switchCount;
        }

        /* verify each switch target */
        for (targ = 0; targ < switchCount; targ++) {
            int offset, absOffset;

            /* offsets are 32-bit, and only partly endian-swapped */
            offset = switchInsns[offsetToTargets + targ*2] |
                     (((s4) switchInsns[offsetToTargets + targ*2 +1]) << 16);
            absOffset = insnIdx + offset;

            assert(absOffset >= 0 && absOffset < insnsSize);

            if (!checkMoveException(meth, absOffset, "switch"))
                goto bail;

            if (!updateRegisters(meth, insnFlags, regTable, absOffset,
                    workLine))
                goto bail;
        }
    }

    /*
     * Handle instructions that can throw and that are sitting in a
     * "try" block.  (If they're not in a "try" block when they throw,
     * control transfers out of the method.)
     */
    if ((nextFlags & kInstrCanThrow) != 0 && dvmInsnIsInTry(insnFlags, insnIdx))
    {
        const DexCode* pCode = dvmGetMethodCode(meth);
        DexCatchIterator iterator;
        bool hasCatchAll = false;

        if (dexFindCatchHandler(&iterator, pCode, insnIdx)) {
            for (;;) {
                DexCatchHandler* handler = dexCatchIteratorNext(&iterator);

                if (handler == NULL) {
                    break;
                }

                if (handler->typeIdx == kDexNoIndex)
                    hasCatchAll = true;

                /*
                 * Merge registers into the "catch" block.  We want to
                 * use the "savedRegs" rather than "workRegs", because
                 * at runtime the exception will be thrown before the
                 * instruction modifies any registers.
                 */
                if (!updateRegisters(meth, insnFlags, regTable,
                        handler->address, &regTable->savedLine))
                    goto bail;
            }
        }

        /*
         * If the monitor stack depth is nonzero, there must be a "catch all"
         * handler for this instruction.  This does apply to monitor-exit
         * because of async exception handling.
         */
        if (workLine->monitorStackTop != 0 && !hasCatchAll) {
            /*
             * The state in workLine reflects the post-execution state.
             * If the current instruction is a monitor-enter and the monitor
             * stack was empty, we don't need a catch-all (if it throws,
             * it will do so before grabbing the lock).
             */
            if (!(decInsn.opcode == OP_MONITOR_ENTER &&
                  workLine->monitorStackTop == 1))
            {
                LOG_VFY_METH(meth,
                    "VFY: no catch-all for instruction at 0x%04x", insnIdx);
                goto bail;
            }
        }
    }

    /*
     * If we're returning from the method, make sure our monitor stack
     * is empty.
     */
    if ((nextFlags & kInstrCanReturn) != 0 && workLine->monitorStackTop != 0) {
        LOG_VFY_METH(meth, "VFY: return with stack depth=%d at 0x%04x",
            workLine->monitorStackTop, insnIdx);
        goto bail;
    }

    /*
     * Update startGuess.  Advance to the next instruction of that's
     * possible, otherwise use the branch target if one was found.  If
     * neither of those exists we're in a return or throw; leave startGuess
     * alone and let the caller sort it out.
     */
    if ((nextFlags & kInstrCanContinue) != 0) {
        *pStartGuess = insnIdx + dvmInsnGetWidth(insnFlags, insnIdx);
    } else if ((nextFlags & kInstrCanBranch) != 0) {
        /* we're still okay if branchTarget is zero */
        *pStartGuess = insnIdx + branchTarget;
    }

    assert(*pStartGuess >= 0 && *pStartGuess < insnsSize &&
        dvmInsnGetWidth(insnFlags, *pStartGuess) != 0);

    result = true;

bail:
    return result;
}


/*
 * callback function used in dumpRegTypes to print local vars
 * valid at a given address.
 */
static void logLocalsCb(void *cnxt, u2 reg, u4 startAddress, u4 endAddress,
        const char *name, const char *descriptor,
        const char *signature)
{
    int addr = *((int *)cnxt);

    if (addr >= (int) startAddress && addr < (int) endAddress)
    {
        ALOGI("        %2d: '%s' %s", reg, name, descriptor);
    }
}

/*
 * Dump the register types for the specifed address to the log file.
 */
static void dumpRegTypes(const VerifierData* vdata,
    const RegisterLine* registerLine, int addr, const char* addrName,
    const UninitInstanceMap* uninitMap, int displayFlags)
{
    const Method* meth = vdata->method;
    const InsnFlags* insnFlags = vdata->insnFlags;
    const RegType* addrRegs = registerLine->regTypes;
    int regCount = meth->registersSize;
    int fullRegCount = regCount + kExtraRegs;
    bool branchTarget = dvmInsnIsBranchTarget(insnFlags, addr);
    int i;

    assert(addr >= 0 && addr < (int) dvmGetMethodInsnsSize(meth));

    int regCharSize = fullRegCount + (fullRegCount-1)/4 + 2 +1;
    char regChars[regCharSize +1];
    memset(regChars, ' ', regCharSize);
    regChars[0] = '[';
    if (regCount == 0)
        regChars[1] = ']';
    else
        regChars[1 + (regCount-1) + (regCount-1)/4 +1] = ']';
    regChars[regCharSize] = '\0';

    for (i = 0; i < regCount + kExtraRegs; i++) {
        char tch;

        switch (addrRegs[i]) {
        case kRegTypeUnknown:       tch = '.';  break;
        case kRegTypeConflict:      tch = 'X';  break;
        case kRegTypeZero:          tch = '0';  break;
        case kRegTypeOne:           tch = '1';  break;
        case kRegTypeBoolean:       tch = 'Z';  break;
        case kRegTypeConstPosByte:  tch = 'y';  break;
        case kRegTypeConstByte:     tch = 'Y';  break;
        case kRegTypeConstPosShort: tch = 'h';  break;
        case kRegTypeConstShort:    tch = 'H';  break;
        case kRegTypeConstChar:     tch = 'c';  break;
        case kRegTypeConstInteger:  tch = 'i';  break;
        case kRegTypePosByte:       tch = 'b';  break;
        case kRegTypeByte:          tch = 'B';  break;
        case kRegTypePosShort:      tch = 's';  break;
        case kRegTypeShort:         tch = 'S';  break;
        case kRegTypeChar:          tch = 'C';  break;
        case kRegTypeInteger:       tch = 'I';  break;
        case kRegTypeFloat:         tch = 'F';  break;
        case kRegTypeConstLo:       tch = 'N';  break;
        case kRegTypeConstHi:       tch = 'n';  break;
        case kRegTypeLongLo:        tch = 'J';  break;
        case kRegTypeLongHi:        tch = 'j';  break;
        case kRegTypeDoubleLo:      tch = 'D';  break;
        case kRegTypeDoubleHi:      tch = 'd';  break;
        default:
            if (regTypeIsReference(addrRegs[i])) {
                if (regTypeIsUninitReference(addrRegs[i]))
                    tch = 'U';
                else
                    tch = 'L';
            } else {
                tch = '*';
                assert(false);
            }
            break;
        }

        if (i < regCount)
            regChars[1 + i + (i/4)] = tch;
        else
            regChars[1 + i + (i/4) + 2] = tch;
    }

    if (addr == 0 && addrName != NULL) {
        ALOGI("%c%s %s mst=%d", branchTarget ? '>' : ' ',
            addrName, regChars, registerLine->monitorStackTop);
    } else {
        ALOGI("%c0x%04x %s mst=%d", branchTarget ? '>' : ' ',
            addr, regChars, registerLine->monitorStackTop);
    }
    if (displayFlags & DRT_SHOW_LIVENESS) {
        /*
         * We can't use registerLine->liveRegs because it might be the
         * "work line" rather than the copy from RegisterTable.
         */
        BitVector* liveRegs = vdata->registerLines[addr].liveRegs;
        if (liveRegs != NULL)  {
            char liveChars[regCharSize + 1];
            memset(liveChars, ' ', regCharSize);
            liveChars[regCharSize] = '\0';

            for (i = 0; i < regCount; i++) {
                bool isLive = dvmIsBitSet(liveRegs, i);
                liveChars[i + 1 + (i / 4)] = isLive ? '+' : '-';
            }
            ALOGI("        %s", liveChars);
        } else {
            ALOGI("        %c", '#');
        }
    }

    if (displayFlags & DRT_SHOW_REF_TYPES) {
        for (i = 0; i < regCount + kExtraRegs; i++) {
            if (regTypeIsReference(addrRegs[i]) && addrRegs[i] != kRegTypeZero)
            {
                ClassObject* clazz = regTypeReferenceToClass(addrRegs[i], uninitMap);
                assert(dvmIsHeapAddress((Object*)clazz));
                if (i < regCount) {
                    ALOGI("        %2d: 0x%08x %s%s",
                        i, addrRegs[i],
                        regTypeIsUninitReference(addrRegs[i]) ? "[U]" : "",
                        clazz->descriptor);
                } else {
                    ALOGI("        RS: 0x%08x %s%s",
                        addrRegs[i],
                        regTypeIsUninitReference(addrRegs[i]) ? "[U]" : "",
                        clazz->descriptor);
                }
            }
        }
    }
    if (displayFlags & DRT_SHOW_LOCALS) {
        dexDecodeDebugInfo(meth->clazz->pDvmDex->pDexFile,
                dvmGetMethodCode(meth),
                meth->clazz->descriptor,
                meth->prototype.protoIdx,
                meth->accessFlags,
                NULL, logLocalsCb, &addr);
    }
}
