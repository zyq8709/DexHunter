/*
 * This file was generated automatically by gen-mterp.py for 'allstubs'.
 *
 * --> DO NOT EDIT <--
 */

/* File: c/header.cpp */
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

/* common includes */
#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "mterp/Mterp.h"
#include <math.h>                   // needed for fmod, fmodf
#include "mterp/common/FindInterface.h"

/*
 * Configuration defines.  These affect the C implementations, i.e. the
 * portable interpreter(s) and C stubs.
 *
 * Some defines are controlled by the Makefile, e.g.:
 *   WITH_INSTR_CHECKS
 *   WITH_TRACKREF_CHECKS
 *   EASY_GDB
 *   NDEBUG
 */

#ifdef WITH_INSTR_CHECKS            /* instruction-level paranoia (slow!) */
# define CHECK_BRANCH_OFFSETS
# define CHECK_REGISTER_INDICES
#endif

/*
 * Some architectures require 64-bit alignment for access to 64-bit data
 * types.  We can't just use pointers to copy 64-bit values out of our
 * interpreted register set, because gcc may assume the pointer target is
 * aligned and generate invalid code.
 *
 * There are two common approaches:
 *  (1) Use a union that defines a 32-bit pair and a 64-bit value.
 *  (2) Call memcpy().
 *
 * Depending upon what compiler you're using and what options are specified,
 * one may be faster than the other.  For example, the compiler might
 * convert a memcpy() of 8 bytes into a series of instructions and omit
 * the call.  The union version could cause some strange side-effects,
 * e.g. for a while ARM gcc thought it needed separate storage for each
 * inlined instance, and generated instructions to zero out ~700 bytes of
 * stack space at the top of the interpreter.
 *
 * The default is to use memcpy().  The current gcc for ARM seems to do
 * better with the union.
 */
#if defined(__ARM_EABI__)
# define NO_UNALIGN_64__UNION
#endif
/*
 * MIPS ABI requires 64-bit alignment for access to 64-bit data types.
 *
 * Use memcpy() to do the transfer
 */
#if defined(__mips__)
/* # define NO_UNALIGN_64__UNION */
#endif


//#define LOG_INSTR                   /* verbose debugging */
/* set and adjust ANDROID_LOG_TAGS='*:i jdwp:i dalvikvm:i dalvikvmi:i' */

/*
 * Export another copy of the PC on every instruction; this is largely
 * redundant with EXPORT_PC and the debugger code.  This value can be
 * compared against what we have stored on the stack with EXPORT_PC to
 * help ensure that we aren't missing any export calls.
 */
#if WITH_EXTRA_GC_CHECKS > 1
# define EXPORT_EXTRA_PC() (self->currentPc2 = pc)
#else
# define EXPORT_EXTRA_PC()
#endif

/*
 * Adjust the program counter.  "_offset" is a signed int, in 16-bit units.
 *
 * Assumes the existence of "const u2* pc" and "const u2* curMethod->insns".
 *
 * We don't advance the program counter until we finish an instruction or
 * branch, because we do want to have to unroll the PC if there's an
 * exception.
 */
#ifdef CHECK_BRANCH_OFFSETS
# define ADJUST_PC(_offset) do {                                            \
        int myoff = _offset;        /* deref only once */                   \
        if (pc + myoff < curMethod->insns ||                                \
            pc + myoff >= curMethod->insns + dvmGetMethodInsnsSize(curMethod)) \
        {                                                                   \
            char* desc;                                                     \
            desc = dexProtoCopyMethodDescriptor(&curMethod->prototype);     \
            ALOGE("Invalid branch %d at 0x%04x in %s.%s %s",                 \
                myoff, (int) (pc - curMethod->insns),                       \
                curMethod->clazz->descriptor, curMethod->name, desc);       \
            free(desc);                                                     \
            dvmAbort();                                                     \
        }                                                                   \
        pc += myoff;                                                        \
        EXPORT_EXTRA_PC();                                                  \
    } while (false)
#else
# define ADJUST_PC(_offset) do {                                            \
        pc += _offset;                                                      \
        EXPORT_EXTRA_PC();                                                  \
    } while (false)
#endif

/*
 * If enabled, log instructions as we execute them.
 */
#ifdef LOG_INSTR
# define ILOGD(...) ILOG(LOG_DEBUG, __VA_ARGS__)
# define ILOGV(...) ILOG(LOG_VERBOSE, __VA_ARGS__)
# define ILOG(_level, ...) do {                                             \
        char debugStrBuf[128];                                              \
        snprintf(debugStrBuf, sizeof(debugStrBuf), __VA_ARGS__);            \
        if (curMethod != NULL)                                              \
            ALOG(_level, LOG_TAG"i", "%-2d|%04x%s",                          \
                self->threadId, (int)(pc - curMethod->insns), debugStrBuf); \
        else                                                                \
            ALOG(_level, LOG_TAG"i", "%-2d|####%s",                          \
                self->threadId, debugStrBuf);                               \
    } while(false)
void dvmDumpRegs(const Method* method, const u4* framePtr, bool inOnly);
# define DUMP_REGS(_meth, _frame, _inOnly) dvmDumpRegs(_meth, _frame, _inOnly)
static const char kSpacing[] = "            ";
#else
# define ILOGD(...) ((void)0)
# define ILOGV(...) ((void)0)
# define DUMP_REGS(_meth, _frame, _inOnly) ((void)0)
#endif

/* get a long from an array of u4 */
static inline s8 getLongFromArray(const u4* ptr, int idx)
{
#if defined(NO_UNALIGN_64__UNION)
    union { s8 ll; u4 parts[2]; } conv;

    ptr += idx;
    conv.parts[0] = ptr[0];
    conv.parts[1] = ptr[1];
    return conv.ll;
#else
    s8 val;
    memcpy(&val, &ptr[idx], 8);
    return val;
#endif
}

/* store a long into an array of u4 */
static inline void putLongToArray(u4* ptr, int idx, s8 val)
{
#if defined(NO_UNALIGN_64__UNION)
    union { s8 ll; u4 parts[2]; } conv;

    ptr += idx;
    conv.ll = val;
    ptr[0] = conv.parts[0];
    ptr[1] = conv.parts[1];
#else
    memcpy(&ptr[idx], &val, 8);
#endif
}

/* get a double from an array of u4 */
static inline double getDoubleFromArray(const u4* ptr, int idx)
{
#if defined(NO_UNALIGN_64__UNION)
    union { double d; u4 parts[2]; } conv;

    ptr += idx;
    conv.parts[0] = ptr[0];
    conv.parts[1] = ptr[1];
    return conv.d;
#else
    double dval;
    memcpy(&dval, &ptr[idx], 8);
    return dval;
#endif
}

/* store a double into an array of u4 */
static inline void putDoubleToArray(u4* ptr, int idx, double dval)
{
#if defined(NO_UNALIGN_64__UNION)
    union { double d; u4 parts[2]; } conv;

    ptr += idx;
    conv.d = dval;
    ptr[0] = conv.parts[0];
    ptr[1] = conv.parts[1];
#else
    memcpy(&ptr[idx], &dval, 8);
#endif
}

/*
 * If enabled, validate the register number on every access.  Otherwise,
 * just do an array access.
 *
 * Assumes the existence of "u4* fp".
 *
 * "_idx" may be referenced more than once.
 */
#ifdef CHECK_REGISTER_INDICES
# define GET_REGISTER(_idx) \
    ( (_idx) < curMethod->registersSize ? \
        (fp[(_idx)]) : (assert(!"bad reg"),1969) )
# define SET_REGISTER(_idx, _val) \
    ( (_idx) < curMethod->registersSize ? \
        (fp[(_idx)] = (u4)(_val)) : (assert(!"bad reg"),1969) )
# define GET_REGISTER_AS_OBJECT(_idx)       ((Object *)GET_REGISTER(_idx))
# define SET_REGISTER_AS_OBJECT(_idx, _val) SET_REGISTER(_idx, (s4)_val)
# define GET_REGISTER_INT(_idx) ((s4) GET_REGISTER(_idx))
# define SET_REGISTER_INT(_idx, _val) SET_REGISTER(_idx, (s4)_val)
# define GET_REGISTER_WIDE(_idx) \
    ( (_idx) < curMethod->registersSize-1 ? \
        getLongFromArray(fp, (_idx)) : (assert(!"bad reg"),1969) )
# define SET_REGISTER_WIDE(_idx, _val) \
    ( (_idx) < curMethod->registersSize-1 ? \
        (void)putLongToArray(fp, (_idx), (_val)) : assert(!"bad reg") )
# define GET_REGISTER_FLOAT(_idx) \
    ( (_idx) < curMethod->registersSize ? \
        (*((float*) &fp[(_idx)])) : (assert(!"bad reg"),1969.0f) )
# define SET_REGISTER_FLOAT(_idx, _val) \
    ( (_idx) < curMethod->registersSize ? \
        (*((float*) &fp[(_idx)]) = (_val)) : (assert(!"bad reg"),1969.0f) )
# define GET_REGISTER_DOUBLE(_idx) \
    ( (_idx) < curMethod->registersSize-1 ? \
        getDoubleFromArray(fp, (_idx)) : (assert(!"bad reg"),1969.0) )
# define SET_REGISTER_DOUBLE(_idx, _val) \
    ( (_idx) < curMethod->registersSize-1 ? \
        (void)putDoubleToArray(fp, (_idx), (_val)) : assert(!"bad reg") )
#else
# define GET_REGISTER(_idx)                 (fp[(_idx)])
# define SET_REGISTER(_idx, _val)           (fp[(_idx)] = (_val))
# define GET_REGISTER_AS_OBJECT(_idx)       ((Object*) fp[(_idx)])
# define SET_REGISTER_AS_OBJECT(_idx, _val) (fp[(_idx)] = (u4)(_val))
# define GET_REGISTER_INT(_idx)             ((s4)GET_REGISTER(_idx))
# define SET_REGISTER_INT(_idx, _val)       SET_REGISTER(_idx, (s4)_val)
# define GET_REGISTER_WIDE(_idx)            getLongFromArray(fp, (_idx))
# define SET_REGISTER_WIDE(_idx, _val)      putLongToArray(fp, (_idx), (_val))
# define GET_REGISTER_FLOAT(_idx)           (*((float*) &fp[(_idx)]))
# define SET_REGISTER_FLOAT(_idx, _val)     (*((float*) &fp[(_idx)]) = (_val))
# define GET_REGISTER_DOUBLE(_idx)          getDoubleFromArray(fp, (_idx))
# define SET_REGISTER_DOUBLE(_idx, _val)    putDoubleToArray(fp, (_idx), (_val))
#endif

/*
 * Get 16 bits from the specified offset of the program counter.  We always
 * want to load 16 bits at a time from the instruction stream -- it's more
 * efficient than 8 and won't have the alignment problems that 32 might.
 *
 * Assumes existence of "const u2* pc".
 */
#define FETCH(_offset)     (pc[(_offset)])

/*
 * Extract instruction byte from 16-bit fetch (_inst is a u2).
 */
#define INST_INST(_inst)    ((_inst) & 0xff)

/*
 * Replace the opcode (used when handling breakpoints).  _opcode is a u1.
 */
#define INST_REPLACE_OP(_inst, _opcode) (((_inst) & 0xff00) | _opcode)

/*
 * Extract the "vA, vB" 4-bit registers from the instruction word (_inst is u2).
 */
#define INST_A(_inst)       (((_inst) >> 8) & 0x0f)
#define INST_B(_inst)       ((_inst) >> 12)

/*
 * Get the 8-bit "vAA" 8-bit register index from the instruction word.
 * (_inst is u2)
 */
#define INST_AA(_inst)      ((_inst) >> 8)

/*
 * The current PC must be available to Throwable constructors, e.g.
 * those created by the various exception throw routines, so that the
 * exception stack trace can be generated correctly.  If we don't do this,
 * the offset within the current method won't be shown correctly.  See the
 * notes in Exception.c.
 *
 * This is also used to determine the address for precise GC.
 *
 * Assumes existence of "u4* fp" and "const u2* pc".
 */
#define EXPORT_PC()         (SAVEAREA_FROM_FP(fp)->xtra.currentPc = pc)

/*
 * Check to see if "obj" is NULL.  If so, throw an exception.  Assumes the
 * pc has already been exported to the stack.
 *
 * Perform additional checks on debug builds.
 *
 * Use this to check for NULL when the instruction handler calls into
 * something that could throw an exception (so we have already called
 * EXPORT_PC at the top).
 */
static inline bool checkForNull(Object* obj)
{
    if (obj == NULL) {
        dvmThrowNullPointerException(NULL);
        return false;
    }
#ifdef WITH_EXTRA_OBJECT_VALIDATION
    if (!dvmIsHeapAddress(obj)) {
        ALOGE("Invalid object %p", obj);
        dvmAbort();
    }
#endif
#ifndef NDEBUG
    if (obj->clazz == NULL || ((u4) obj->clazz) <= 65536) {
        /* probable heap corruption */
        ALOGE("Invalid object class %p (in %p)", obj->clazz, obj);
        dvmAbort();
    }
#endif
    return true;
}

/*
 * Check to see if "obj" is NULL.  If so, export the PC into the stack
 * frame and throw an exception.
 *
 * Perform additional checks on debug builds.
 *
 * Use this to check for NULL when the instruction handler doesn't do
 * anything else that can throw an exception.
 */
static inline bool checkForNullExportPC(Object* obj, u4* fp, const u2* pc)
{
    if (obj == NULL) {
        EXPORT_PC();
        dvmThrowNullPointerException(NULL);
        return false;
    }
#ifdef WITH_EXTRA_OBJECT_VALIDATION
    if (!dvmIsHeapAddress(obj)) {
        ALOGE("Invalid object %p", obj);
        dvmAbort();
    }
#endif
#ifndef NDEBUG
    if (obj->clazz == NULL || ((u4) obj->clazz) <= 65536) {
        /* probable heap corruption */
        ALOGE("Invalid object class %p (in %p)", obj->clazz, obj);
        dvmAbort();
    }
#endif
    return true;
}

/* File: cstubs/stubdefs.cpp */
/*
 * In the C mterp stubs, "goto" is a function call followed immediately
 * by a return.
 */

#define GOTO_TARGET_DECL(_target, ...)                                      \
    extern "C" void dvmMterp_##_target(Thread* self, ## __VA_ARGS__);

/* (void)xxx to quiet unused variable compiler warnings. */
#define GOTO_TARGET(_target, ...)                                           \
    void dvmMterp_##_target(Thread* self, ## __VA_ARGS__) {                 \
        u2 ref, vsrc1, vsrc2, vdst;                                         \
        u2 inst = FETCH(0);                                                 \
        const Method* methodToCall;                                         \
        StackSaveArea* debugSaveArea;                                       \
        (void)ref; (void)vsrc1; (void)vsrc2; (void)vdst; (void)inst;        \
        (void)methodToCall; (void)debugSaveArea;

#define GOTO_TARGET_END }

/*
 * Redefine what used to be local variable accesses into Thread struct
 * references.  (These are undefined down in "footer.cpp".)
 */
#define retval                  self->interpSave.retval
#define pc                      self->interpSave.pc
#define fp                      self->interpSave.curFrame
#define curMethod               self->interpSave.method
#define methodClassDex          self->interpSave.methodClassDex
#define debugTrackedRefStart    self->interpSave.debugTrackedRefStart

/* ugh */
#define STUB_HACK(x) x
#if defined(WITH_JIT)
#define JIT_STUB_HACK(x) x
#else
#define JIT_STUB_HACK(x)
#endif

/*
 * InterpSave's pc and fp must be valid when breaking out to a
 * "Reportxxx" routine.  Because the portable interpreter uses local
 * variables for these, we must flush prior.  Stubs, however, use
 * the interpSave vars directly, so this is a nop for stubs.
 */
#define PC_FP_TO_SELF()
#define PC_TO_SELF()

/*
 * Opcode handler framing macros.  Here, each opcode is a separate function
 * that takes a "self" argument and returns void.  We can't declare
 * these "static" because they may be called from an assembly stub.
 * (void)xxx to quiet unused variable compiler warnings.
 */
#define HANDLE_OPCODE(_op)                                                  \
    extern "C" void dvmMterp_##_op(Thread* self);                           \
    void dvmMterp_##_op(Thread* self) {                                     \
        u4 ref;                                                             \
        u2 vsrc1, vsrc2, vdst;                                              \
        u2 inst = FETCH(0);                                                 \
        (void)ref; (void)vsrc1; (void)vsrc2; (void)vdst; (void)inst;

#define OP_END }

/*
 * Like the "portable" FINISH, but don't reload "inst", and return to caller
 * when done.  Further, debugger/profiler checks are handled
 * before handler execution in mterp, so we don't do them here either.
 */
#if defined(WITH_JIT)
#define FINISH(_offset) {                                                   \
        ADJUST_PC(_offset);                                                 \
        if (self->interpBreak.ctl.subMode & kSubModeJitTraceBuild) {        \
            dvmCheckJit(pc, self);                                          \
        }                                                                   \
        return;                                                             \
    }
#else
#define FINISH(_offset) {                                                   \
        ADJUST_PC(_offset);                                                 \
        return;                                                             \
    }
#endif

#define FINISH_BKPT(_opcode)       /* FIXME? */
#define DISPATCH_EXTENDED(_opcode) /* FIXME? */

/*
 * The "goto label" statements turn into function calls followed by
 * return statements.  Some of the functions take arguments, which in the
 * portable interpreter are handled by assigning values to globals.
 */

#define GOTO_exceptionThrown()                                              \
    do {                                                                    \
        dvmMterp_exceptionThrown(self);                                     \
        return;                                                             \
    } while(false)

#define GOTO_returnFromMethod()                                             \
    do {                                                                    \
        dvmMterp_returnFromMethod(self);                                    \
        return;                                                             \
    } while(false)

#define GOTO_invoke(_target, _methodCallRange)                              \
    do {                                                                    \
        dvmMterp_##_target(self, _methodCallRange);                         \
        return;                                                             \
    } while(false)

#define GOTO_invokeMethod(_methodCallRange, _methodToCall, _vsrc1, _vdst)   \
    do {                                                                    \
        dvmMterp_invokeMethod(self, _methodCallRange, _methodToCall,        \
            _vsrc1, _vdst);                                                 \
        return;                                                             \
    } while(false)

/*
 * As a special case, "goto bail" turns into a longjmp.
 */
#define GOTO_bail()                                                         \
    dvmMterpStdBail(self)

/*
 * Periodically check for thread suspension.
 *
 * While we're at it, see if a debugger has attached or the profiler has
 * started.
 */
#define PERIODIC_CHECKS(_pcadj) {                              \
        if (dvmCheckSuspendQuick(self)) {                                   \
            EXPORT_PC();  /* need for precise GC */                         \
            dvmCheckSuspendPending(self);                                   \
        }                                                                   \
    }

/* File: c/opcommon.cpp */
/* forward declarations of goto targets */
GOTO_TARGET_DECL(filledNewArray, bool methodCallRange);
GOTO_TARGET_DECL(invokeVirtual, bool methodCallRange);
GOTO_TARGET_DECL(invokeSuper, bool methodCallRange);
GOTO_TARGET_DECL(invokeInterface, bool methodCallRange);
GOTO_TARGET_DECL(invokeDirect, bool methodCallRange);
GOTO_TARGET_DECL(invokeStatic, bool methodCallRange);
GOTO_TARGET_DECL(invokeVirtualQuick, bool methodCallRange);
GOTO_TARGET_DECL(invokeSuperQuick, bool methodCallRange);
GOTO_TARGET_DECL(invokeMethod, bool methodCallRange, const Method* methodToCall,
    u2 count, u2 regs);
GOTO_TARGET_DECL(returnFromMethod);
GOTO_TARGET_DECL(exceptionThrown);

/*
 * ===========================================================================
 *
 * What follows are opcode definitions shared between multiple opcodes with
 * minor substitutions handled by the C pre-processor.  These should probably
 * use the mterp substitution mechanism instead, with the code here moved
 * into common fragment files (like the asm "binop.S"), although it's hard
 * to give up the C preprocessor in favor of the much simpler text subst.
 *
 * ===========================================================================
 */

#define HANDLE_NUMCONV(_opcode, _opname, _fromtype, _totype)                \
    HANDLE_OPCODE(_opcode /*vA, vB*/)                                       \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);                                               \
        ILOGV("|%s v%d,v%d", (_opname), vdst, vsrc1);                       \
        SET_REGISTER##_totype(vdst,                                         \
            GET_REGISTER##_fromtype(vsrc1));                                \
        FINISH(1);

#define HANDLE_FLOAT_TO_INT(_opcode, _opname, _fromvtype, _fromrtype,       \
        _tovtype, _tortype)                                                 \
    HANDLE_OPCODE(_opcode /*vA, vB*/)                                       \
    {                                                                       \
        /* spec defines specific handling for +/- inf and NaN values */     \
        _fromvtype val;                                                     \
        _tovtype intMin, intMax, result;                                    \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);                                               \
        ILOGV("|%s v%d,v%d", (_opname), vdst, vsrc1);                       \
        val = GET_REGISTER##_fromrtype(vsrc1);                              \
        intMin = (_tovtype) 1 << (sizeof(_tovtype) * 8 -1);                 \
        intMax = ~intMin;                                                   \
        result = (_tovtype) val;                                            \
        if (val >= intMax)          /* +inf */                              \
            result = intMax;                                                \
        else if (val <= intMin)     /* -inf */                              \
            result = intMin;                                                \
        else if (val != val)        /* NaN */                               \
            result = 0;                                                     \
        else                                                                \
            result = (_tovtype) val;                                        \
        SET_REGISTER##_tortype(vdst, result);                               \
    }                                                                       \
    FINISH(1);

#define HANDLE_INT_TO_SMALL(_opcode, _opname, _type)                        \
    HANDLE_OPCODE(_opcode /*vA, vB*/)                                       \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);                                               \
        ILOGV("|int-to-%s v%d,v%d", (_opname), vdst, vsrc1);                \
        SET_REGISTER(vdst, (_type) GET_REGISTER(vsrc1));                    \
        FINISH(1);

/* NOTE: the comparison result is always a signed 4-byte integer */
#define HANDLE_OP_CMPX(_opcode, _opname, _varType, _type, _nanVal)          \
    HANDLE_OPCODE(_opcode /*vAA, vBB, vCC*/)                                \
    {                                                                       \
        int result;                                                         \
        u2 regs;                                                            \
        _varType val1, val2;                                                \
        vdst = INST_AA(inst);                                               \
        regs = FETCH(1);                                                    \
        vsrc1 = regs & 0xff;                                                \
        vsrc2 = regs >> 8;                                                  \
        ILOGV("|cmp%s v%d,v%d,v%d", (_opname), vdst, vsrc1, vsrc2);         \
        val1 = GET_REGISTER##_type(vsrc1);                                  \
        val2 = GET_REGISTER##_type(vsrc2);                                  \
        if (val1 == val2)                                                   \
            result = 0;                                                     \
        else if (val1 < val2)                                               \
            result = -1;                                                    \
        else if (val1 > val2)                                               \
            result = 1;                                                     \
        else                                                                \
            result = (_nanVal);                                             \
        ILOGV("+ result=%d", result);                                       \
        SET_REGISTER(vdst, result);                                         \
    }                                                                       \
    FINISH(2);

#define HANDLE_OP_IF_XX(_opcode, _opname, _cmp)                             \
    HANDLE_OPCODE(_opcode /*vA, vB, +CCCC*/)                                \
        vsrc1 = INST_A(inst);                                               \
        vsrc2 = INST_B(inst);                                               \
        if ((s4) GET_REGISTER(vsrc1) _cmp (s4) GET_REGISTER(vsrc2)) {       \
            int branchOffset = (s2)FETCH(1);    /* sign-extended */         \
            ILOGV("|if-%s v%d,v%d,+0x%04x", (_opname), vsrc1, vsrc2,        \
                branchOffset);                                              \
            ILOGV("> branch taken");                                        \
            if (branchOffset < 0)                                           \
                PERIODIC_CHECKS(branchOffset);                              \
            FINISH(branchOffset);                                           \
        } else {                                                            \
            ILOGV("|if-%s v%d,v%d,-", (_opname), vsrc1, vsrc2);             \
            FINISH(2);                                                      \
        }

#define HANDLE_OP_IF_XXZ(_opcode, _opname, _cmp)                            \
    HANDLE_OPCODE(_opcode /*vAA, +BBBB*/)                                   \
        vsrc1 = INST_AA(inst);                                              \
        if ((s4) GET_REGISTER(vsrc1) _cmp 0) {                              \
            int branchOffset = (s2)FETCH(1);    /* sign-extended */         \
            ILOGV("|if-%s v%d,+0x%04x", (_opname), vsrc1, branchOffset);    \
            ILOGV("> branch taken");                                        \
            if (branchOffset < 0)                                           \
                PERIODIC_CHECKS(branchOffset);                              \
            FINISH(branchOffset);                                           \
        } else {                                                            \
            ILOGV("|if-%s v%d,-", (_opname), vsrc1);                        \
            FINISH(2);                                                      \
        }

#define HANDLE_UNOP(_opcode, _opname, _pfx, _sfx, _type)                    \
    HANDLE_OPCODE(_opcode /*vA, vB*/)                                       \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);                                               \
        ILOGV("|%s v%d,v%d", (_opname), vdst, vsrc1);                       \
        SET_REGISTER##_type(vdst, _pfx GET_REGISTER##_type(vsrc1) _sfx);    \
        FINISH(1);

#define HANDLE_OP_X_INT(_opcode, _opname, _op, _chkdiv)                     \
    HANDLE_OPCODE(_opcode /*vAA, vBB, vCC*/)                                \
    {                                                                       \
        u2 srcRegs;                                                         \
        vdst = INST_AA(inst);                                               \
        srcRegs = FETCH(1);                                                 \
        vsrc1 = srcRegs & 0xff;                                             \
        vsrc2 = srcRegs >> 8;                                               \
        ILOGV("|%s-int v%d,v%d", (_opname), vdst, vsrc1);                   \
        if (_chkdiv != 0) {                                                 \
            s4 firstVal, secondVal, result;                                 \
            firstVal = GET_REGISTER(vsrc1);                                 \
            secondVal = GET_REGISTER(vsrc2);                                \
            if (secondVal == 0) {                                           \
                EXPORT_PC();                                                \
                dvmThrowArithmeticException("divide by zero");              \
                GOTO_exceptionThrown();                                     \
            }                                                               \
            if ((u4)firstVal == 0x80000000 && secondVal == -1) {            \
                if (_chkdiv == 1)                                           \
                    result = firstVal;  /* division */                      \
                else                                                        \
                    result = 0;         /* remainder */                     \
            } else {                                                        \
                result = firstVal _op secondVal;                            \
            }                                                               \
            SET_REGISTER(vdst, result);                                     \
        } else {                                                            \
            /* non-div/rem case */                                          \
            SET_REGISTER(vdst,                                              \
                (s4) GET_REGISTER(vsrc1) _op (s4) GET_REGISTER(vsrc2));     \
        }                                                                   \
    }                                                                       \
    FINISH(2);

#define HANDLE_OP_SHX_INT(_opcode, _opname, _cast, _op)                     \
    HANDLE_OPCODE(_opcode /*vAA, vBB, vCC*/)                                \
    {                                                                       \
        u2 srcRegs;                                                         \
        vdst = INST_AA(inst);                                               \
        srcRegs = FETCH(1);                                                 \
        vsrc1 = srcRegs & 0xff;                                             \
        vsrc2 = srcRegs >> 8;                                               \
        ILOGV("|%s-int v%d,v%d", (_opname), vdst, vsrc1);                   \
        SET_REGISTER(vdst,                                                  \
            _cast GET_REGISTER(vsrc1) _op (GET_REGISTER(vsrc2) & 0x1f));    \
    }                                                                       \
    FINISH(2);

#define HANDLE_OP_X_INT_LIT16(_opcode, _opname, _op, _chkdiv)               \
    HANDLE_OPCODE(_opcode /*vA, vB, #+CCCC*/)                               \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);                                               \
        vsrc2 = FETCH(1);                                                   \
        ILOGV("|%s-int/lit16 v%d,v%d,#+0x%04x",                             \
            (_opname), vdst, vsrc1, vsrc2);                                 \
        if (_chkdiv != 0) {                                                 \
            s4 firstVal, result;                                            \
            firstVal = GET_REGISTER(vsrc1);                                 \
            if ((s2) vsrc2 == 0) {                                          \
                EXPORT_PC();                                                \
                dvmThrowArithmeticException("divide by zero");              \
                GOTO_exceptionThrown();                                     \
            }                                                               \
            if ((u4)firstVal == 0x80000000 && ((s2) vsrc2) == -1) {         \
                /* won't generate /lit16 instr for this; check anyway */    \
                if (_chkdiv == 1)                                           \
                    result = firstVal;  /* division */                      \
                else                                                        \
                    result = 0;         /* remainder */                     \
            } else {                                                        \
                result = firstVal _op (s2) vsrc2;                           \
            }                                                               \
            SET_REGISTER(vdst, result);                                     \
        } else {                                                            \
            /* non-div/rem case */                                          \
            SET_REGISTER(vdst, GET_REGISTER(vsrc1) _op (s2) vsrc2);         \
        }                                                                   \
        FINISH(2);

#define HANDLE_OP_X_INT_LIT8(_opcode, _opname, _op, _chkdiv)                \
    HANDLE_OPCODE(_opcode /*vAA, vBB, #+CC*/)                               \
    {                                                                       \
        u2 litInfo;                                                         \
        vdst = INST_AA(inst);                                               \
        litInfo = FETCH(1);                                                 \
        vsrc1 = litInfo & 0xff;                                             \
        vsrc2 = litInfo >> 8;       /* constant */                          \
        ILOGV("|%s-int/lit8 v%d,v%d,#+0x%02x",                              \
            (_opname), vdst, vsrc1, vsrc2);                                 \
        if (_chkdiv != 0) {                                                 \
            s4 firstVal, result;                                            \
            firstVal = GET_REGISTER(vsrc1);                                 \
            if ((s1) vsrc2 == 0) {                                          \
                EXPORT_PC();                                                \
                dvmThrowArithmeticException("divide by zero");              \
                GOTO_exceptionThrown();                                     \
            }                                                               \
            if ((u4)firstVal == 0x80000000 && ((s1) vsrc2) == -1) {         \
                if (_chkdiv == 1)                                           \
                    result = firstVal;  /* division */                      \
                else                                                        \
                    result = 0;         /* remainder */                     \
            } else {                                                        \
                result = firstVal _op ((s1) vsrc2);                         \
            }                                                               \
            SET_REGISTER(vdst, result);                                     \
        } else {                                                            \
            SET_REGISTER(vdst,                                              \
                (s4) GET_REGISTER(vsrc1) _op (s1) vsrc2);                   \
        }                                                                   \
    }                                                                       \
    FINISH(2);

#define HANDLE_OP_SHX_INT_LIT8(_opcode, _opname, _cast, _op)                \
    HANDLE_OPCODE(_opcode /*vAA, vBB, #+CC*/)                               \
    {                                                                       \
        u2 litInfo;                                                         \
        vdst = INST_AA(inst);                                               \
        litInfo = FETCH(1);                                                 \
        vsrc1 = litInfo & 0xff;                                             \
        vsrc2 = litInfo >> 8;       /* constant */                          \
        ILOGV("|%s-int/lit8 v%d,v%d,#+0x%02x",                              \
            (_opname), vdst, vsrc1, vsrc2);                                 \
        SET_REGISTER(vdst,                                                  \
            _cast GET_REGISTER(vsrc1) _op (vsrc2 & 0x1f));                  \
    }                                                                       \
    FINISH(2);

#define HANDLE_OP_X_INT_2ADDR(_opcode, _opname, _op, _chkdiv)               \
    HANDLE_OPCODE(_opcode /*vA, vB*/)                                       \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);                                               \
        ILOGV("|%s-int-2addr v%d,v%d", (_opname), vdst, vsrc1);             \
        if (_chkdiv != 0) {                                                 \
            s4 firstVal, secondVal, result;                                 \
            firstVal = GET_REGISTER(vdst);                                  \
            secondVal = GET_REGISTER(vsrc1);                                \
            if (secondVal == 0) {                                           \
                EXPORT_PC();                                                \
                dvmThrowArithmeticException("divide by zero");              \
                GOTO_exceptionThrown();                                     \
            }                                                               \
            if ((u4)firstVal == 0x80000000 && secondVal == -1) {            \
                if (_chkdiv == 1)                                           \
                    result = firstVal;  /* division */                      \
                else                                                        \
                    result = 0;         /* remainder */                     \
            } else {                                                        \
                result = firstVal _op secondVal;                            \
            }                                                               \
            SET_REGISTER(vdst, result);                                     \
        } else {                                                            \
            SET_REGISTER(vdst,                                              \
                (s4) GET_REGISTER(vdst) _op (s4) GET_REGISTER(vsrc1));      \
        }                                                                   \
        FINISH(1);

#define HANDLE_OP_SHX_INT_2ADDR(_opcode, _opname, _cast, _op)               \
    HANDLE_OPCODE(_opcode /*vA, vB*/)                                       \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);                                               \
        ILOGV("|%s-int-2addr v%d,v%d", (_opname), vdst, vsrc1);             \
        SET_REGISTER(vdst,                                                  \
            _cast GET_REGISTER(vdst) _op (GET_REGISTER(vsrc1) & 0x1f));     \
        FINISH(1);

#define HANDLE_OP_X_LONG(_opcode, _opname, _op, _chkdiv)                    \
    HANDLE_OPCODE(_opcode /*vAA, vBB, vCC*/)                                \
    {                                                                       \
        u2 srcRegs;                                                         \
        vdst = INST_AA(inst);                                               \
        srcRegs = FETCH(1);                                                 \
        vsrc1 = srcRegs & 0xff;                                             \
        vsrc2 = srcRegs >> 8;                                               \
        ILOGV("|%s-long v%d,v%d,v%d", (_opname), vdst, vsrc1, vsrc2);       \
        if (_chkdiv != 0) {                                                 \
            s8 firstVal, secondVal, result;                                 \
            firstVal = GET_REGISTER_WIDE(vsrc1);                            \
            secondVal = GET_REGISTER_WIDE(vsrc2);                           \
            if (secondVal == 0LL) {                                         \
                EXPORT_PC();                                                \
                dvmThrowArithmeticException("divide by zero");              \
                GOTO_exceptionThrown();                                     \
            }                                                               \
            if ((u8)firstVal == 0x8000000000000000ULL &&                    \
                secondVal == -1LL)                                          \
            {                                                               \
                if (_chkdiv == 1)                                           \
                    result = firstVal;  /* division */                      \
                else                                                        \
                    result = 0;         /* remainder */                     \
            } else {                                                        \
                result = firstVal _op secondVal;                            \
            }                                                               \
            SET_REGISTER_WIDE(vdst, result);                                \
        } else {                                                            \
            SET_REGISTER_WIDE(vdst,                                         \
                (s8) GET_REGISTER_WIDE(vsrc1) _op (s8) GET_REGISTER_WIDE(vsrc2)); \
        }                                                                   \
    }                                                                       \
    FINISH(2);

#define HANDLE_OP_SHX_LONG(_opcode, _opname, _cast, _op)                    \
    HANDLE_OPCODE(_opcode /*vAA, vBB, vCC*/)                                \
    {                                                                       \
        u2 srcRegs;                                                         \
        vdst = INST_AA(inst);                                               \
        srcRegs = FETCH(1);                                                 \
        vsrc1 = srcRegs & 0xff;                                             \
        vsrc2 = srcRegs >> 8;                                               \
        ILOGV("|%s-long v%d,v%d,v%d", (_opname), vdst, vsrc1, vsrc2);       \
        SET_REGISTER_WIDE(vdst,                                             \
            _cast GET_REGISTER_WIDE(vsrc1) _op (GET_REGISTER(vsrc2) & 0x3f)); \
    }                                                                       \
    FINISH(2);

#define HANDLE_OP_X_LONG_2ADDR(_opcode, _opname, _op, _chkdiv)              \
    HANDLE_OPCODE(_opcode /*vA, vB*/)                                       \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);                                               \
        ILOGV("|%s-long-2addr v%d,v%d", (_opname), vdst, vsrc1);            \
        if (_chkdiv != 0) {                                                 \
            s8 firstVal, secondVal, result;                                 \
            firstVal = GET_REGISTER_WIDE(vdst);                             \
            secondVal = GET_REGISTER_WIDE(vsrc1);                           \
            if (secondVal == 0LL) {                                         \
                EXPORT_PC();                                                \
                dvmThrowArithmeticException("divide by zero");              \
                GOTO_exceptionThrown();                                     \
            }                                                               \
            if ((u8)firstVal == 0x8000000000000000ULL &&                    \
                secondVal == -1LL)                                          \
            {                                                               \
                if (_chkdiv == 1)                                           \
                    result = firstVal;  /* division */                      \
                else                                                        \
                    result = 0;         /* remainder */                     \
            } else {                                                        \
                result = firstVal _op secondVal;                            \
            }                                                               \
            SET_REGISTER_WIDE(vdst, result);                                \
        } else {                                                            \
            SET_REGISTER_WIDE(vdst,                                         \
                (s8) GET_REGISTER_WIDE(vdst) _op (s8)GET_REGISTER_WIDE(vsrc1));\
        }                                                                   \
        FINISH(1);

#define HANDLE_OP_SHX_LONG_2ADDR(_opcode, _opname, _cast, _op)              \
    HANDLE_OPCODE(_opcode /*vA, vB*/)                                       \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);                                               \
        ILOGV("|%s-long-2addr v%d,v%d", (_opname), vdst, vsrc1);            \
        SET_REGISTER_WIDE(vdst,                                             \
            _cast GET_REGISTER_WIDE(vdst) _op (GET_REGISTER(vsrc1) & 0x3f)); \
        FINISH(1);

#define HANDLE_OP_X_FLOAT(_opcode, _opname, _op)                            \
    HANDLE_OPCODE(_opcode /*vAA, vBB, vCC*/)                                \
    {                                                                       \
        u2 srcRegs;                                                         \
        vdst = INST_AA(inst);                                               \
        srcRegs = FETCH(1);                                                 \
        vsrc1 = srcRegs & 0xff;                                             \
        vsrc2 = srcRegs >> 8;                                               \
        ILOGV("|%s-float v%d,v%d,v%d", (_opname), vdst, vsrc1, vsrc2);      \
        SET_REGISTER_FLOAT(vdst,                                            \
            GET_REGISTER_FLOAT(vsrc1) _op GET_REGISTER_FLOAT(vsrc2));       \
    }                                                                       \
    FINISH(2);

#define HANDLE_OP_X_DOUBLE(_opcode, _opname, _op)                           \
    HANDLE_OPCODE(_opcode /*vAA, vBB, vCC*/)                                \
    {                                                                       \
        u2 srcRegs;                                                         \
        vdst = INST_AA(inst);                                               \
        srcRegs = FETCH(1);                                                 \
        vsrc1 = srcRegs & 0xff;                                             \
        vsrc2 = srcRegs >> 8;                                               \
        ILOGV("|%s-double v%d,v%d,v%d", (_opname), vdst, vsrc1, vsrc2);     \
        SET_REGISTER_DOUBLE(vdst,                                           \
            GET_REGISTER_DOUBLE(vsrc1) _op GET_REGISTER_DOUBLE(vsrc2));     \
    }                                                                       \
    FINISH(2);

#define HANDLE_OP_X_FLOAT_2ADDR(_opcode, _opname, _op)                      \
    HANDLE_OPCODE(_opcode /*vA, vB*/)                                       \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);                                               \
        ILOGV("|%s-float-2addr v%d,v%d", (_opname), vdst, vsrc1);           \
        SET_REGISTER_FLOAT(vdst,                                            \
            GET_REGISTER_FLOAT(vdst) _op GET_REGISTER_FLOAT(vsrc1));        \
        FINISH(1);

#define HANDLE_OP_X_DOUBLE_2ADDR(_opcode, _opname, _op)                     \
    HANDLE_OPCODE(_opcode /*vA, vB*/)                                       \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);                                               \
        ILOGV("|%s-double-2addr v%d,v%d", (_opname), vdst, vsrc1);          \
        SET_REGISTER_DOUBLE(vdst,                                           \
            GET_REGISTER_DOUBLE(vdst) _op GET_REGISTER_DOUBLE(vsrc1));      \
        FINISH(1);

#define HANDLE_OP_AGET(_opcode, _opname, _type, _regsize)                   \
    HANDLE_OPCODE(_opcode /*vAA, vBB, vCC*/)                                \
    {                                                                       \
        ArrayObject* arrayObj;                                              \
        u2 arrayInfo;                                                       \
        EXPORT_PC();                                                        \
        vdst = INST_AA(inst);                                               \
        arrayInfo = FETCH(1);                                               \
        vsrc1 = arrayInfo & 0xff;    /* array ptr */                        \
        vsrc2 = arrayInfo >> 8;      /* index */                            \
        ILOGV("|aget%s v%d,v%d,v%d", (_opname), vdst, vsrc1, vsrc2);        \
        arrayObj = (ArrayObject*) GET_REGISTER(vsrc1);                      \
        if (!checkForNull((Object*) arrayObj))                              \
            GOTO_exceptionThrown();                                         \
        if (GET_REGISTER(vsrc2) >= arrayObj->length) {                      \
            dvmThrowArrayIndexOutOfBoundsException(                         \
                arrayObj->length, GET_REGISTER(vsrc2));                     \
            GOTO_exceptionThrown();                                         \
        }                                                                   \
        SET_REGISTER##_regsize(vdst,                                        \
            ((_type*)(void*)arrayObj->contents)[GET_REGISTER(vsrc2)]);      \
        ILOGV("+ AGET[%d]=%#x", GET_REGISTER(vsrc2), GET_REGISTER(vdst));   \
    }                                                                       \
    FINISH(2);

#define HANDLE_OP_APUT(_opcode, _opname, _type, _regsize)                   \
    HANDLE_OPCODE(_opcode /*vAA, vBB, vCC*/)                                \
    {                                                                       \
        ArrayObject* arrayObj;                                              \
        u2 arrayInfo;                                                       \
        EXPORT_PC();                                                        \
        vdst = INST_AA(inst);       /* AA: source value */                  \
        arrayInfo = FETCH(1);                                               \
        vsrc1 = arrayInfo & 0xff;   /* BB: array ptr */                     \
        vsrc2 = arrayInfo >> 8;     /* CC: index */                         \
        ILOGV("|aput%s v%d,v%d,v%d", (_opname), vdst, vsrc1, vsrc2);        \
        arrayObj = (ArrayObject*) GET_REGISTER(vsrc1);                      \
        if (!checkForNull((Object*) arrayObj))                              \
            GOTO_exceptionThrown();                                         \
        if (GET_REGISTER(vsrc2) >= arrayObj->length) {                      \
            dvmThrowArrayIndexOutOfBoundsException(                         \
                arrayObj->length, GET_REGISTER(vsrc2));                     \
            GOTO_exceptionThrown();                                         \
        }                                                                   \
        ILOGV("+ APUT[%d]=0x%08x", GET_REGISTER(vsrc2), GET_REGISTER(vdst));\
        ((_type*)(void*)arrayObj->contents)[GET_REGISTER(vsrc2)] =          \
            GET_REGISTER##_regsize(vdst);                                   \
    }                                                                       \
    FINISH(2);

/*
 * It's possible to get a bad value out of a field with sub-32-bit stores
 * because the -quick versions always operate on 32 bits.  Consider:
 *   short foo = -1  (sets a 32-bit register to 0xffffffff)
 *   iput-quick foo  (writes all 32 bits to the field)
 *   short bar = 1   (sets a 32-bit register to 0x00000001)
 *   iput-short      (writes the low 16 bits to the field)
 *   iget-quick foo  (reads all 32 bits from the field, yielding 0xffff0001)
 * This can only happen when optimized and non-optimized code has interleaved
 * access to the same field.  This is unlikely but possible.
 *
 * The easiest way to fix this is to always read/write 32 bits at a time.  On
 * a device with a 16-bit data bus this is sub-optimal.  (The alternative
 * approach is to have sub-int versions of iget-quick, but now we're wasting
 * Dalvik instruction space and making it less likely that handler code will
 * already be in the CPU i-cache.)
 */
#define HANDLE_IGET_X(_opcode, _opname, _ftype, _regsize)                   \
    HANDLE_OPCODE(_opcode /*vA, vB, field@CCCC*/)                           \
    {                                                                       \
        InstField* ifield;                                                  \
        Object* obj;                                                        \
        EXPORT_PC();                                                        \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);   /* object ptr */                            \
        ref = FETCH(1);         /* field ref */                             \
        ILOGV("|iget%s v%d,v%d,field@0x%04x", (_opname), vdst, vsrc1, ref); \
        obj = (Object*) GET_REGISTER(vsrc1);                                \
        if (!checkForNull(obj))                                             \
            GOTO_exceptionThrown();                                         \
        ifield = (InstField*) dvmDexGetResolvedField(methodClassDex, ref);  \
        if (ifield == NULL) {                                               \
            ifield = dvmResolveInstField(curMethod->clazz, ref);            \
            if (ifield == NULL)                                             \
                GOTO_exceptionThrown();                                     \
        }                                                                   \
        SET_REGISTER##_regsize(vdst,                                        \
            dvmGetField##_ftype(obj, ifield->byteOffset));                  \
        ILOGV("+ IGET '%s'=0x%08llx", ifield->name,                         \
            (u8) GET_REGISTER##_regsize(vdst));                             \
    }                                                                       \
    FINISH(2);

#define HANDLE_IGET_X_QUICK(_opcode, _opname, _ftype, _regsize)             \
    HANDLE_OPCODE(_opcode /*vA, vB, field@CCCC*/)                           \
    {                                                                       \
        Object* obj;                                                        \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);   /* object ptr */                            \
        ref = FETCH(1);         /* field offset */                          \
        ILOGV("|iget%s-quick v%d,v%d,field@+%u",                            \
            (_opname), vdst, vsrc1, ref);                                   \
        obj = (Object*) GET_REGISTER(vsrc1);                                \
        if (!checkForNullExportPC(obj, fp, pc))                             \
            GOTO_exceptionThrown();                                         \
        SET_REGISTER##_regsize(vdst, dvmGetField##_ftype(obj, ref));        \
        ILOGV("+ IGETQ %d=0x%08llx", ref,                                   \
            (u8) GET_REGISTER##_regsize(vdst));                             \
    }                                                                       \
    FINISH(2);

#define HANDLE_IPUT_X(_opcode, _opname, _ftype, _regsize)                   \
    HANDLE_OPCODE(_opcode /*vA, vB, field@CCCC*/)                           \
    {                                                                       \
        InstField* ifield;                                                  \
        Object* obj;                                                        \
        EXPORT_PC();                                                        \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);   /* object ptr */                            \
        ref = FETCH(1);         /* field ref */                             \
        ILOGV("|iput%s v%d,v%d,field@0x%04x", (_opname), vdst, vsrc1, ref); \
        obj = (Object*) GET_REGISTER(vsrc1);                                \
        if (!checkForNull(obj))                                             \
            GOTO_exceptionThrown();                                         \
        ifield = (InstField*) dvmDexGetResolvedField(methodClassDex, ref);  \
        if (ifield == NULL) {                                               \
            ifield = dvmResolveInstField(curMethod->clazz, ref);            \
            if (ifield == NULL)                                             \
                GOTO_exceptionThrown();                                     \
        }                                                                   \
        dvmSetField##_ftype(obj, ifield->byteOffset,                        \
            GET_REGISTER##_regsize(vdst));                                  \
        ILOGV("+ IPUT '%s'=0x%08llx", ifield->name,                         \
            (u8) GET_REGISTER##_regsize(vdst));                             \
    }                                                                       \
    FINISH(2);

#define HANDLE_IPUT_X_QUICK(_opcode, _opname, _ftype, _regsize)             \
    HANDLE_OPCODE(_opcode /*vA, vB, field@CCCC*/)                           \
    {                                                                       \
        Object* obj;                                                        \
        vdst = INST_A(inst);                                                \
        vsrc1 = INST_B(inst);   /* object ptr */                            \
        ref = FETCH(1);         /* field offset */                          \
        ILOGV("|iput%s-quick v%d,v%d,field@0x%04x",                         \
            (_opname), vdst, vsrc1, ref);                                   \
        obj = (Object*) GET_REGISTER(vsrc1);                                \
        if (!checkForNullExportPC(obj, fp, pc))                             \
            GOTO_exceptionThrown();                                         \
        dvmSetField##_ftype(obj, ref, GET_REGISTER##_regsize(vdst));        \
        ILOGV("+ IPUTQ %d=0x%08llx", ref,                                   \
            (u8) GET_REGISTER##_regsize(vdst));                             \
    }                                                                       \
    FINISH(2);

/*
 * The JIT needs dvmDexGetResolvedField() to return non-null.
 * Because the portable interpreter is not involved with the JIT
 * and trace building, we only need the extra check here when this
 * code is massaged into a stub called from an assembly interpreter.
 * This is controlled by the JIT_STUB_HACK maco.
 */

#define HANDLE_SGET_X(_opcode, _opname, _ftype, _regsize)                   \
    HANDLE_OPCODE(_opcode /*vAA, field@BBBB*/)                              \
    {                                                                       \
        StaticField* sfield;                                                \
        vdst = INST_AA(inst);                                               \
        ref = FETCH(1);         /* field ref */                             \
        ILOGV("|sget%s v%d,sfield@0x%04x", (_opname), vdst, ref);           \
        sfield = (StaticField*)dvmDexGetResolvedField(methodClassDex, ref); \
        if (sfield == NULL) {                                               \
            EXPORT_PC();                                                    \
            sfield = dvmResolveStaticField(curMethod->clazz, ref);          \
            if (sfield == NULL)                                             \
                GOTO_exceptionThrown();                                     \
            if (dvmDexGetResolvedField(methodClassDex, ref) == NULL) {      \
                JIT_STUB_HACK(dvmJitEndTraceSelect(self,pc));               \
            }                                                               \
        }                                                                   \
        SET_REGISTER##_regsize(vdst, dvmGetStaticField##_ftype(sfield));    \
        ILOGV("+ SGET '%s'=0x%08llx",                                       \
            sfield->name, (u8)GET_REGISTER##_regsize(vdst));                \
    }                                                                       \
    FINISH(2);

#define HANDLE_SPUT_X(_opcode, _opname, _ftype, _regsize)                   \
    HANDLE_OPCODE(_opcode /*vAA, field@BBBB*/)                              \
    {                                                                       \
        StaticField* sfield;                                                \
        vdst = INST_AA(inst);                                               \
        ref = FETCH(1);         /* field ref */                             \
        ILOGV("|sput%s v%d,sfield@0x%04x", (_opname), vdst, ref);           \
        sfield = (StaticField*)dvmDexGetResolvedField(methodClassDex, ref); \
        if (sfield == NULL) {                                               \
            EXPORT_PC();                                                    \
            sfield = dvmResolveStaticField(curMethod->clazz, ref);          \
            if (sfield == NULL)                                             \
                GOTO_exceptionThrown();                                     \
            if (dvmDexGetResolvedField(methodClassDex, ref) == NULL) {      \
                JIT_STUB_HACK(dvmJitEndTraceSelect(self,pc));               \
            }                                                               \
        }                                                                   \
        dvmSetStaticField##_ftype(sfield, GET_REGISTER##_regsize(vdst));    \
        ILOGV("+ SPUT '%s'=0x%08llx",                                       \
            sfield->name, (u8)GET_REGISTER##_regsize(vdst));                \
    }                                                                       \
    FINISH(2);

/* File: c/OP_NOP.cpp */
HANDLE_OPCODE(OP_NOP)
    FINISH(1);
OP_END

/* File: c/OP_MOVE.cpp */
HANDLE_OPCODE(OP_MOVE /*vA, vB*/)
    vdst = INST_A(inst);
    vsrc1 = INST_B(inst);
    ILOGV("|move%s v%d,v%d %s(v%d=0x%08x)",
        (INST_INST(inst) == OP_MOVE) ? "" : "-object", vdst, vsrc1,
        kSpacing, vdst, GET_REGISTER(vsrc1));
    SET_REGISTER(vdst, GET_REGISTER(vsrc1));
    FINISH(1);
OP_END

/* File: c/OP_MOVE_FROM16.cpp */
HANDLE_OPCODE(OP_MOVE_FROM16 /*vAA, vBBBB*/)
    vdst = INST_AA(inst);
    vsrc1 = FETCH(1);
    ILOGV("|move%s/from16 v%d,v%d %s(v%d=0x%08x)",
        (INST_INST(inst) == OP_MOVE_FROM16) ? "" : "-object", vdst, vsrc1,
        kSpacing, vdst, GET_REGISTER(vsrc1));
    SET_REGISTER(vdst, GET_REGISTER(vsrc1));
    FINISH(2);
OP_END

/* File: c/OP_MOVE_16.cpp */
HANDLE_OPCODE(OP_MOVE_16 /*vAAAA, vBBBB*/)
    vdst = FETCH(1);
    vsrc1 = FETCH(2);
    ILOGV("|move%s/16 v%d,v%d %s(v%d=0x%08x)",
        (INST_INST(inst) == OP_MOVE_16) ? "" : "-object", vdst, vsrc1,
        kSpacing, vdst, GET_REGISTER(vsrc1));
    SET_REGISTER(vdst, GET_REGISTER(vsrc1));
    FINISH(3);
OP_END

/* File: c/OP_MOVE_WIDE.cpp */
HANDLE_OPCODE(OP_MOVE_WIDE /*vA, vB*/)
    /* IMPORTANT: must correctly handle overlapping registers, e.g. both
     * "move-wide v6, v7" and "move-wide v7, v6" */
    vdst = INST_A(inst);
    vsrc1 = INST_B(inst);
    ILOGV("|move-wide v%d,v%d %s(v%d=0x%08llx)", vdst, vsrc1,
        kSpacing+5, vdst, GET_REGISTER_WIDE(vsrc1));
    SET_REGISTER_WIDE(vdst, GET_REGISTER_WIDE(vsrc1));
    FINISH(1);
OP_END

/* File: c/OP_MOVE_WIDE_FROM16.cpp */
HANDLE_OPCODE(OP_MOVE_WIDE_FROM16 /*vAA, vBBBB*/)
    vdst = INST_AA(inst);
    vsrc1 = FETCH(1);
    ILOGV("|move-wide/from16 v%d,v%d  (v%d=0x%08llx)", vdst, vsrc1,
        vdst, GET_REGISTER_WIDE(vsrc1));
    SET_REGISTER_WIDE(vdst, GET_REGISTER_WIDE(vsrc1));
    FINISH(2);
OP_END

/* File: c/OP_MOVE_WIDE_16.cpp */
HANDLE_OPCODE(OP_MOVE_WIDE_16 /*vAAAA, vBBBB*/)
    vdst = FETCH(1);
    vsrc1 = FETCH(2);
    ILOGV("|move-wide/16 v%d,v%d %s(v%d=0x%08llx)", vdst, vsrc1,
        kSpacing+8, vdst, GET_REGISTER_WIDE(vsrc1));
    SET_REGISTER_WIDE(vdst, GET_REGISTER_WIDE(vsrc1));
    FINISH(3);
OP_END

/* File: c/OP_MOVE_OBJECT.cpp */
/* File: c/OP_MOVE.cpp */
HANDLE_OPCODE(OP_MOVE_OBJECT /*vA, vB*/)
    vdst = INST_A(inst);
    vsrc1 = INST_B(inst);
    ILOGV("|move%s v%d,v%d %s(v%d=0x%08x)",
        (INST_INST(inst) == OP_MOVE) ? "" : "-object", vdst, vsrc1,
        kSpacing, vdst, GET_REGISTER(vsrc1));
    SET_REGISTER(vdst, GET_REGISTER(vsrc1));
    FINISH(1);
OP_END


/* File: c/OP_MOVE_OBJECT_FROM16.cpp */
/* File: c/OP_MOVE_FROM16.cpp */
HANDLE_OPCODE(OP_MOVE_OBJECT_FROM16 /*vAA, vBBBB*/)
    vdst = INST_AA(inst);
    vsrc1 = FETCH(1);
    ILOGV("|move%s/from16 v%d,v%d %s(v%d=0x%08x)",
        (INST_INST(inst) == OP_MOVE_FROM16) ? "" : "-object", vdst, vsrc1,
        kSpacing, vdst, GET_REGISTER(vsrc1));
    SET_REGISTER(vdst, GET_REGISTER(vsrc1));
    FINISH(2);
OP_END


/* File: c/OP_MOVE_OBJECT_16.cpp */
/* File: c/OP_MOVE_16.cpp */
HANDLE_OPCODE(OP_MOVE_OBJECT_16 /*vAAAA, vBBBB*/)
    vdst = FETCH(1);
    vsrc1 = FETCH(2);
    ILOGV("|move%s/16 v%d,v%d %s(v%d=0x%08x)",
        (INST_INST(inst) == OP_MOVE_16) ? "" : "-object", vdst, vsrc1,
        kSpacing, vdst, GET_REGISTER(vsrc1));
    SET_REGISTER(vdst, GET_REGISTER(vsrc1));
    FINISH(3);
OP_END


/* File: c/OP_MOVE_RESULT.cpp */
HANDLE_OPCODE(OP_MOVE_RESULT /*vAA*/)
    vdst = INST_AA(inst);
    ILOGV("|move-result%s v%d %s(v%d=0x%08x)",
         (INST_INST(inst) == OP_MOVE_RESULT) ? "" : "-object",
         vdst, kSpacing+4, vdst,retval.i);
    SET_REGISTER(vdst, retval.i);
    FINISH(1);
OP_END

/* File: c/OP_MOVE_RESULT_WIDE.cpp */
HANDLE_OPCODE(OP_MOVE_RESULT_WIDE /*vAA*/)
    vdst = INST_AA(inst);
    ILOGV("|move-result-wide v%d %s(0x%08llx)", vdst, kSpacing, retval.j);
    SET_REGISTER_WIDE(vdst, retval.j);
    FINISH(1);
OP_END

/* File: c/OP_MOVE_RESULT_OBJECT.cpp */
/* File: c/OP_MOVE_RESULT.cpp */
HANDLE_OPCODE(OP_MOVE_RESULT_OBJECT /*vAA*/)
    vdst = INST_AA(inst);
    ILOGV("|move-result%s v%d %s(v%d=0x%08x)",
         (INST_INST(inst) == OP_MOVE_RESULT) ? "" : "-object",
         vdst, kSpacing+4, vdst,retval.i);
    SET_REGISTER(vdst, retval.i);
    FINISH(1);
OP_END


/* File: c/OP_MOVE_EXCEPTION.cpp */
HANDLE_OPCODE(OP_MOVE_EXCEPTION /*vAA*/)
    vdst = INST_AA(inst);
    ILOGV("|move-exception v%d", vdst);
    assert(self->exception != NULL);
    SET_REGISTER(vdst, (u4)self->exception);
    dvmClearException(self);
    FINISH(1);
OP_END

/* File: c/OP_RETURN_VOID.cpp */
HANDLE_OPCODE(OP_RETURN_VOID /**/)
    ILOGV("|return-void");
#ifndef NDEBUG
    retval.j = 0xababababULL;    // placate valgrind
#endif
    GOTO_returnFromMethod();
OP_END

/* File: c/OP_RETURN.cpp */
HANDLE_OPCODE(OP_RETURN /*vAA*/)
    vsrc1 = INST_AA(inst);
    ILOGV("|return%s v%d",
        (INST_INST(inst) == OP_RETURN) ? "" : "-object", vsrc1);
    retval.i = GET_REGISTER(vsrc1);
    GOTO_returnFromMethod();
OP_END

/* File: c/OP_RETURN_WIDE.cpp */
HANDLE_OPCODE(OP_RETURN_WIDE /*vAA*/)
    vsrc1 = INST_AA(inst);
    ILOGV("|return-wide v%d", vsrc1);
    retval.j = GET_REGISTER_WIDE(vsrc1);
    GOTO_returnFromMethod();
OP_END

/* File: c/OP_RETURN_OBJECT.cpp */
/* File: c/OP_RETURN.cpp */
HANDLE_OPCODE(OP_RETURN_OBJECT /*vAA*/)
    vsrc1 = INST_AA(inst);
    ILOGV("|return%s v%d",
        (INST_INST(inst) == OP_RETURN) ? "" : "-object", vsrc1);
    retval.i = GET_REGISTER(vsrc1);
    GOTO_returnFromMethod();
OP_END


/* File: c/OP_CONST_4.cpp */
HANDLE_OPCODE(OP_CONST_4 /*vA, #+B*/)
    {
        s4 tmp;

        vdst = INST_A(inst);
        tmp = (s4) (INST_B(inst) << 28) >> 28;  // sign extend 4-bit value
        ILOGV("|const/4 v%d,#0x%02x", vdst, (s4)tmp);
        SET_REGISTER(vdst, tmp);
    }
    FINISH(1);
OP_END

/* File: c/OP_CONST_16.cpp */
HANDLE_OPCODE(OP_CONST_16 /*vAA, #+BBBB*/)
    vdst = INST_AA(inst);
    vsrc1 = FETCH(1);
    ILOGV("|const/16 v%d,#0x%04x", vdst, (s2)vsrc1);
    SET_REGISTER(vdst, (s2) vsrc1);
    FINISH(2);
OP_END

/* File: c/OP_CONST.cpp */
HANDLE_OPCODE(OP_CONST /*vAA, #+BBBBBBBB*/)
    {
        u4 tmp;

        vdst = INST_AA(inst);
        tmp = FETCH(1);
        tmp |= (u4)FETCH(2) << 16;
        ILOGV("|const v%d,#0x%08x", vdst, tmp);
        SET_REGISTER(vdst, tmp);
    }
    FINISH(3);
OP_END

/* File: c/OP_CONST_HIGH16.cpp */
HANDLE_OPCODE(OP_CONST_HIGH16 /*vAA, #+BBBB0000*/)
    vdst = INST_AA(inst);
    vsrc1 = FETCH(1);
    ILOGV("|const/high16 v%d,#0x%04x0000", vdst, vsrc1);
    SET_REGISTER(vdst, vsrc1 << 16);
    FINISH(2);
OP_END

/* File: c/OP_CONST_WIDE_16.cpp */
HANDLE_OPCODE(OP_CONST_WIDE_16 /*vAA, #+BBBB*/)
    vdst = INST_AA(inst);
    vsrc1 = FETCH(1);
    ILOGV("|const-wide/16 v%d,#0x%04x", vdst, (s2)vsrc1);
    SET_REGISTER_WIDE(vdst, (s2)vsrc1);
    FINISH(2);
OP_END

/* File: c/OP_CONST_WIDE_32.cpp */
HANDLE_OPCODE(OP_CONST_WIDE_32 /*vAA, #+BBBBBBBB*/)
    {
        u4 tmp;

        vdst = INST_AA(inst);
        tmp = FETCH(1);
        tmp |= (u4)FETCH(2) << 16;
        ILOGV("|const-wide/32 v%d,#0x%08x", vdst, tmp);
        SET_REGISTER_WIDE(vdst, (s4) tmp);
    }
    FINISH(3);
OP_END

/* File: c/OP_CONST_WIDE.cpp */
HANDLE_OPCODE(OP_CONST_WIDE /*vAA, #+BBBBBBBBBBBBBBBB*/)
    {
        u8 tmp;

        vdst = INST_AA(inst);
        tmp = FETCH(1);
        tmp |= (u8)FETCH(2) << 16;
        tmp |= (u8)FETCH(3) << 32;
        tmp |= (u8)FETCH(4) << 48;
        ILOGV("|const-wide v%d,#0x%08llx", vdst, tmp);
        SET_REGISTER_WIDE(vdst, tmp);
    }
    FINISH(5);
OP_END

/* File: c/OP_CONST_WIDE_HIGH16.cpp */
HANDLE_OPCODE(OP_CONST_WIDE_HIGH16 /*vAA, #+BBBB000000000000*/)
    vdst = INST_AA(inst);
    vsrc1 = FETCH(1);
    ILOGV("|const-wide/high16 v%d,#0x%04x000000000000", vdst, vsrc1);
    SET_REGISTER_WIDE(vdst, ((u8) vsrc1) << 48);
    FINISH(2);
OP_END

/* File: c/OP_CONST_STRING.cpp */
HANDLE_OPCODE(OP_CONST_STRING /*vAA, string@BBBB*/)
    {
        StringObject* strObj;

        vdst = INST_AA(inst);
        ref = FETCH(1);
        ILOGV("|const-string v%d string@0x%04x", vdst, ref);
        strObj = dvmDexGetResolvedString(methodClassDex, ref);
        if (strObj == NULL) {
            EXPORT_PC();
            strObj = dvmResolveString(curMethod->clazz, ref);
            if (strObj == NULL)
                GOTO_exceptionThrown();
        }
        SET_REGISTER(vdst, (u4) strObj);
    }
    FINISH(2);
OP_END

/* File: c/OP_CONST_STRING_JUMBO.cpp */
HANDLE_OPCODE(OP_CONST_STRING_JUMBO /*vAA, string@BBBBBBBB*/)
    {
        StringObject* strObj;
        u4 tmp;

        vdst = INST_AA(inst);
        tmp = FETCH(1);
        tmp |= (u4)FETCH(2) << 16;
        ILOGV("|const-string/jumbo v%d string@0x%08x", vdst, tmp);
        strObj = dvmDexGetResolvedString(methodClassDex, tmp);
        if (strObj == NULL) {
            EXPORT_PC();
            strObj = dvmResolveString(curMethod->clazz, tmp);
            if (strObj == NULL)
                GOTO_exceptionThrown();
        }
        SET_REGISTER(vdst, (u4) strObj);
    }
    FINISH(3);
OP_END

/* File: c/OP_CONST_CLASS.cpp */
HANDLE_OPCODE(OP_CONST_CLASS /*vAA, class@BBBB*/)
    {
        ClassObject* clazz;

        vdst = INST_AA(inst);
        ref = FETCH(1);
        ILOGV("|const-class v%d class@0x%04x", vdst, ref);
        clazz = dvmDexGetResolvedClass(methodClassDex, ref);
        if (clazz == NULL) {
            EXPORT_PC();
            clazz = dvmResolveClass(curMethod->clazz, ref, true);
            if (clazz == NULL)
                GOTO_exceptionThrown();
        }
        SET_REGISTER(vdst, (u4) clazz);
    }
    FINISH(2);
OP_END

/* File: c/OP_MONITOR_ENTER.cpp */
HANDLE_OPCODE(OP_MONITOR_ENTER /*vAA*/)
    {
        Object* obj;

        vsrc1 = INST_AA(inst);
        ILOGV("|monitor-enter v%d %s(0x%08x)",
            vsrc1, kSpacing+6, GET_REGISTER(vsrc1));
        obj = (Object*)GET_REGISTER(vsrc1);
        if (!checkForNullExportPC(obj, fp, pc))
            GOTO_exceptionThrown();
        ILOGV("+ locking %p %s", obj, obj->clazz->descriptor);
        EXPORT_PC();    /* need for precise GC */
        dvmLockObject(self, obj);
    }
    FINISH(1);
OP_END

/* File: c/OP_MONITOR_EXIT.cpp */
HANDLE_OPCODE(OP_MONITOR_EXIT /*vAA*/)
    {
        Object* obj;

        EXPORT_PC();

        vsrc1 = INST_AA(inst);
        ILOGV("|monitor-exit v%d %s(0x%08x)",
            vsrc1, kSpacing+5, GET_REGISTER(vsrc1));
        obj = (Object*)GET_REGISTER(vsrc1);
        if (!checkForNull(obj)) {
            /*
             * The exception needs to be processed at the *following*
             * instruction, not the current instruction (see the Dalvik
             * spec).  Because we're jumping to an exception handler,
             * we're not actually at risk of skipping an instruction
             * by doing so.
             */
            ADJUST_PC(1);           /* monitor-exit width is 1 */
            GOTO_exceptionThrown();
        }
        ILOGV("+ unlocking %p %s", obj, obj->clazz->descriptor);
        if (!dvmUnlockObject(self, obj)) {
            assert(dvmCheckException(self));
            ADJUST_PC(1);
            GOTO_exceptionThrown();
        }
    }
    FINISH(1);
OP_END

/* File: c/OP_CHECK_CAST.cpp */
HANDLE_OPCODE(OP_CHECK_CAST /*vAA, class@BBBB*/)
    {
        ClassObject* clazz;
        Object* obj;

        EXPORT_PC();

        vsrc1 = INST_AA(inst);
        ref = FETCH(1);         /* class to check against */
        ILOGV("|check-cast v%d,class@0x%04x", vsrc1, ref);

        obj = (Object*)GET_REGISTER(vsrc1);
        if (obj != NULL) {
#if defined(WITH_EXTRA_OBJECT_VALIDATION)
            if (!checkForNull(obj))
                GOTO_exceptionThrown();
#endif
            clazz = dvmDexGetResolvedClass(methodClassDex, ref);
            if (clazz == NULL) {
                clazz = dvmResolveClass(curMethod->clazz, ref, false);
                if (clazz == NULL)
                    GOTO_exceptionThrown();
            }
            if (!dvmInstanceof(obj->clazz, clazz)) {
                dvmThrowClassCastException(obj->clazz, clazz);
                GOTO_exceptionThrown();
            }
        }
    }
    FINISH(2);
OP_END

/* File: c/OP_INSTANCE_OF.cpp */
HANDLE_OPCODE(OP_INSTANCE_OF /*vA, vB, class@CCCC*/)
    {
        ClassObject* clazz;
        Object* obj;

        vdst = INST_A(inst);
        vsrc1 = INST_B(inst);   /* object to check */
        ref = FETCH(1);         /* class to check against */
        ILOGV("|instance-of v%d,v%d,class@0x%04x", vdst, vsrc1, ref);

        obj = (Object*)GET_REGISTER(vsrc1);
        if (obj == NULL) {
            SET_REGISTER(vdst, 0);
        } else {
#if defined(WITH_EXTRA_OBJECT_VALIDATION)
            if (!checkForNullExportPC(obj, fp, pc))
                GOTO_exceptionThrown();
#endif
            clazz = dvmDexGetResolvedClass(methodClassDex, ref);
            if (clazz == NULL) {
                EXPORT_PC();
                clazz = dvmResolveClass(curMethod->clazz, ref, true);
                if (clazz == NULL)
                    GOTO_exceptionThrown();
            }
            SET_REGISTER(vdst, dvmInstanceof(obj->clazz, clazz));
        }
    }
    FINISH(2);
OP_END

/* File: c/OP_ARRAY_LENGTH.cpp */
HANDLE_OPCODE(OP_ARRAY_LENGTH /*vA, vB*/)
    {
        ArrayObject* arrayObj;

        vdst = INST_A(inst);
        vsrc1 = INST_B(inst);
        arrayObj = (ArrayObject*) GET_REGISTER(vsrc1);
        ILOGV("|array-length v%d,v%d  (%p)", vdst, vsrc1, arrayObj);
        if (!checkForNullExportPC((Object*) arrayObj, fp, pc))
            GOTO_exceptionThrown();
        /* verifier guarantees this is an array reference */
        SET_REGISTER(vdst, arrayObj->length);
    }
    FINISH(1);
OP_END

/* File: c/OP_NEW_INSTANCE.cpp */
HANDLE_OPCODE(OP_NEW_INSTANCE /*vAA, class@BBBB*/)
    {
        ClassObject* clazz;
        Object* newObj;

        EXPORT_PC();

        vdst = INST_AA(inst);
        ref = FETCH(1);
        ILOGV("|new-instance v%d,class@0x%04x", vdst, ref);
        clazz = dvmDexGetResolvedClass(methodClassDex, ref);
        if (clazz == NULL) {
            clazz = dvmResolveClass(curMethod->clazz, ref, false);
            if (clazz == NULL)
                GOTO_exceptionThrown();
        }

        if (!dvmIsClassInitialized(clazz) && !dvmInitClass(clazz))
            GOTO_exceptionThrown();

#if defined(WITH_JIT)
        /*
         * The JIT needs dvmDexGetResolvedClass() to return non-null.
         * Since we use the portable interpreter to build the trace, this extra
         * check is not needed for mterp.
         */
        if ((self->interpBreak.ctl.subMode & kSubModeJitTraceBuild) &&
            (!dvmDexGetResolvedClass(methodClassDex, ref))) {
            /* Class initialization is still ongoing - end the trace */
            dvmJitEndTraceSelect(self,pc);
        }
#endif

        /*
         * Verifier now tests for interface/abstract class.
         */
        //if (dvmIsInterfaceClass(clazz) || dvmIsAbstractClass(clazz)) {
        //    dvmThrowExceptionWithClassMessage(gDvm.exInstantiationError,
        //        clazz->descriptor);
        //    GOTO_exceptionThrown();
        //}
        newObj = dvmAllocObject(clazz, ALLOC_DONT_TRACK);
        if (newObj == NULL)
            GOTO_exceptionThrown();
        SET_REGISTER(vdst, (u4) newObj);
    }
    FINISH(2);
OP_END

/* File: c/OP_NEW_ARRAY.cpp */
HANDLE_OPCODE(OP_NEW_ARRAY /*vA, vB, class@CCCC*/)
    {
        ClassObject* arrayClass;
        ArrayObject* newArray;
        s4 length;

        EXPORT_PC();

        vdst = INST_A(inst);
        vsrc1 = INST_B(inst);       /* length reg */
        ref = FETCH(1);
        ILOGV("|new-array v%d,v%d,class@0x%04x  (%d elements)",
            vdst, vsrc1, ref, (s4) GET_REGISTER(vsrc1));
        length = (s4) GET_REGISTER(vsrc1);
        if (length < 0) {
            dvmThrowNegativeArraySizeException(length);
            GOTO_exceptionThrown();
        }
        arrayClass = dvmDexGetResolvedClass(methodClassDex, ref);
        if (arrayClass == NULL) {
            arrayClass = dvmResolveClass(curMethod->clazz, ref, false);
            if (arrayClass == NULL)
                GOTO_exceptionThrown();
        }
        /* verifier guarantees this is an array class */
        assert(dvmIsArrayClass(arrayClass));
        assert(dvmIsClassInitialized(arrayClass));

        newArray = dvmAllocArrayByClass(arrayClass, length, ALLOC_DONT_TRACK);
        if (newArray == NULL)
            GOTO_exceptionThrown();
        SET_REGISTER(vdst, (u4) newArray);
    }
    FINISH(2);
OP_END

/* File: c/OP_FILLED_NEW_ARRAY.cpp */
HANDLE_OPCODE(OP_FILLED_NEW_ARRAY /*vB, {vD, vE, vF, vG, vA}, class@CCCC*/)
    GOTO_invoke(filledNewArray, false);
OP_END

/* File: c/OP_FILLED_NEW_ARRAY_RANGE.cpp */
HANDLE_OPCODE(OP_FILLED_NEW_ARRAY_RANGE /*{vCCCC..v(CCCC+AA-1)}, class@BBBB*/)
    GOTO_invoke(filledNewArray, true);
OP_END

/* File: c/OP_FILL_ARRAY_DATA.cpp */
HANDLE_OPCODE(OP_FILL_ARRAY_DATA)   /*vAA, +BBBBBBBB*/
    {
        const u2* arrayData;
        s4 offset;
        ArrayObject* arrayObj;

        EXPORT_PC();
        vsrc1 = INST_AA(inst);
        offset = FETCH(1) | (((s4) FETCH(2)) << 16);
        ILOGV("|fill-array-data v%d +0x%04x", vsrc1, offset);
        arrayData = pc + offset;       // offset in 16-bit units
#ifndef NDEBUG
        if (arrayData < curMethod->insns ||
            arrayData >= curMethod->insns + dvmGetMethodInsnsSize(curMethod))
        {
            /* should have been caught in verifier */
            dvmThrowInternalError("bad fill array data");
            GOTO_exceptionThrown();
        }
#endif
        arrayObj = (ArrayObject*) GET_REGISTER(vsrc1);
        if (!dvmInterpHandleFillArrayData(arrayObj, arrayData)) {
            GOTO_exceptionThrown();
        }
        FINISH(3);
    }
OP_END

/* File: c/OP_THROW.cpp */
HANDLE_OPCODE(OP_THROW /*vAA*/)
    {
        Object* obj;

        /*
         * We don't create an exception here, but the process of searching
         * for a catch block can do class lookups and throw exceptions.
         * We need to update the saved PC.
         */
        EXPORT_PC();

        vsrc1 = INST_AA(inst);
        ILOGV("|throw v%d  (%p)", vsrc1, (void*)GET_REGISTER(vsrc1));
        obj = (Object*) GET_REGISTER(vsrc1);
        if (!checkForNull(obj)) {
            /* will throw a null pointer exception */
            LOGVV("Bad exception");
        } else {
            /* use the requested exception */
            dvmSetException(self, obj);
        }
        GOTO_exceptionThrown();
    }
OP_END

/* File: c/OP_GOTO.cpp */
HANDLE_OPCODE(OP_GOTO /*+AA*/)
    vdst = INST_AA(inst);
    if ((s1)vdst < 0)
        ILOGV("|goto -0x%02x", -((s1)vdst));
    else
        ILOGV("|goto +0x%02x", ((s1)vdst));
    ILOGV("> branch taken");
    if ((s1)vdst < 0)
        PERIODIC_CHECKS((s1)vdst);
    FINISH((s1)vdst);
OP_END

/* File: c/OP_GOTO_16.cpp */
HANDLE_OPCODE(OP_GOTO_16 /*+AAAA*/)
    {
        s4 offset = (s2) FETCH(1);          /* sign-extend next code unit */

        if (offset < 0)
            ILOGV("|goto/16 -0x%04x", -offset);
        else
            ILOGV("|goto/16 +0x%04x", offset);
        ILOGV("> branch taken");
        if (offset < 0)
            PERIODIC_CHECKS(offset);
        FINISH(offset);
    }
OP_END

/* File: c/OP_GOTO_32.cpp */
HANDLE_OPCODE(OP_GOTO_32 /*+AAAAAAAA*/)
    {
        s4 offset = FETCH(1);               /* low-order 16 bits */
        offset |= ((s4) FETCH(2)) << 16;    /* high-order 16 bits */

        if (offset < 0)
            ILOGV("|goto/32 -0x%08x", -offset);
        else
            ILOGV("|goto/32 +0x%08x", offset);
        ILOGV("> branch taken");
        if (offset <= 0)    /* allowed to branch to self */
            PERIODIC_CHECKS(offset);
        FINISH(offset);
    }
OP_END

/* File: c/OP_PACKED_SWITCH.cpp */
HANDLE_OPCODE(OP_PACKED_SWITCH /*vAA, +BBBB*/)
    {
        const u2* switchData;
        u4 testVal;
        s4 offset;

        vsrc1 = INST_AA(inst);
        offset = FETCH(1) | (((s4) FETCH(2)) << 16);
        ILOGV("|packed-switch v%d +0x%04x", vsrc1, offset);
        switchData = pc + offset;       // offset in 16-bit units
#ifndef NDEBUG
        if (switchData < curMethod->insns ||
            switchData >= curMethod->insns + dvmGetMethodInsnsSize(curMethod))
        {
            /* should have been caught in verifier */
            EXPORT_PC();
            dvmThrowInternalError("bad packed switch");
            GOTO_exceptionThrown();
        }
#endif
        testVal = GET_REGISTER(vsrc1);

        offset = dvmInterpHandlePackedSwitch(switchData, testVal);
        ILOGV("> branch taken (0x%04x)", offset);
        if (offset <= 0)  /* uncommon */
            PERIODIC_CHECKS(offset);
        FINISH(offset);
    }
OP_END

/* File: c/OP_SPARSE_SWITCH.cpp */
HANDLE_OPCODE(OP_SPARSE_SWITCH /*vAA, +BBBB*/)
    {
        const u2* switchData;
        u4 testVal;
        s4 offset;

        vsrc1 = INST_AA(inst);
        offset = FETCH(1) | (((s4) FETCH(2)) << 16);
        ILOGV("|sparse-switch v%d +0x%04x", vsrc1, offset);
        switchData = pc + offset;       // offset in 16-bit units
#ifndef NDEBUG
        if (switchData < curMethod->insns ||
            switchData >= curMethod->insns + dvmGetMethodInsnsSize(curMethod))
        {
            /* should have been caught in verifier */
            EXPORT_PC();
            dvmThrowInternalError("bad sparse switch");
            GOTO_exceptionThrown();
        }
#endif
        testVal = GET_REGISTER(vsrc1);

        offset = dvmInterpHandleSparseSwitch(switchData, testVal);
        ILOGV("> branch taken (0x%04x)", offset);
        if (offset <= 0)  /* uncommon */
            PERIODIC_CHECKS(offset);
        FINISH(offset);
    }
OP_END

/* File: c/OP_CMPL_FLOAT.cpp */
HANDLE_OP_CMPX(OP_CMPL_FLOAT, "l-float", float, _FLOAT, -1)
OP_END

/* File: c/OP_CMPG_FLOAT.cpp */
HANDLE_OP_CMPX(OP_CMPG_FLOAT, "g-float", float, _FLOAT, 1)
OP_END

/* File: c/OP_CMPL_DOUBLE.cpp */
HANDLE_OP_CMPX(OP_CMPL_DOUBLE, "l-double", double, _DOUBLE, -1)
OP_END

/* File: c/OP_CMPG_DOUBLE.cpp */
HANDLE_OP_CMPX(OP_CMPG_DOUBLE, "g-double", double, _DOUBLE, 1)
OP_END

/* File: c/OP_CMP_LONG.cpp */
HANDLE_OP_CMPX(OP_CMP_LONG, "-long", s8, _WIDE, 0)
OP_END

/* File: c/OP_IF_EQ.cpp */
HANDLE_OP_IF_XX(OP_IF_EQ, "eq", ==)
OP_END

/* File: c/OP_IF_NE.cpp */
HANDLE_OP_IF_XX(OP_IF_NE, "ne", !=)
OP_END

/* File: c/OP_IF_LT.cpp */
HANDLE_OP_IF_XX(OP_IF_LT, "lt", <)
OP_END

/* File: c/OP_IF_GE.cpp */
HANDLE_OP_IF_XX(OP_IF_GE, "ge", >=)
OP_END

/* File: c/OP_IF_GT.cpp */
HANDLE_OP_IF_XX(OP_IF_GT, "gt", >)
OP_END

/* File: c/OP_IF_LE.cpp */
HANDLE_OP_IF_XX(OP_IF_LE, "le", <=)
OP_END

/* File: c/OP_IF_EQZ.cpp */
HANDLE_OP_IF_XXZ(OP_IF_EQZ, "eqz", ==)
OP_END

/* File: c/OP_IF_NEZ.cpp */
HANDLE_OP_IF_XXZ(OP_IF_NEZ, "nez", !=)
OP_END

/* File: c/OP_IF_LTZ.cpp */
HANDLE_OP_IF_XXZ(OP_IF_LTZ, "ltz", <)
OP_END

/* File: c/OP_IF_GEZ.cpp */
HANDLE_OP_IF_XXZ(OP_IF_GEZ, "gez", >=)
OP_END

/* File: c/OP_IF_GTZ.cpp */
HANDLE_OP_IF_XXZ(OP_IF_GTZ, "gtz", >)
OP_END

/* File: c/OP_IF_LEZ.cpp */
HANDLE_OP_IF_XXZ(OP_IF_LEZ, "lez", <=)
OP_END

/* File: c/OP_UNUSED_3E.cpp */
HANDLE_OPCODE(OP_UNUSED_3E)
OP_END

/* File: c/OP_UNUSED_3F.cpp */
HANDLE_OPCODE(OP_UNUSED_3F)
OP_END

/* File: c/OP_UNUSED_40.cpp */
HANDLE_OPCODE(OP_UNUSED_40)
OP_END

/* File: c/OP_UNUSED_41.cpp */
HANDLE_OPCODE(OP_UNUSED_41)
OP_END

/* File: c/OP_UNUSED_42.cpp */
HANDLE_OPCODE(OP_UNUSED_42)
OP_END

/* File: c/OP_UNUSED_43.cpp */
HANDLE_OPCODE(OP_UNUSED_43)
OP_END

/* File: c/OP_AGET.cpp */
HANDLE_OP_AGET(OP_AGET, "", u4, )
OP_END

/* File: c/OP_AGET_WIDE.cpp */
HANDLE_OP_AGET(OP_AGET_WIDE, "-wide", s8, _WIDE)
OP_END

/* File: c/OP_AGET_OBJECT.cpp */
HANDLE_OP_AGET(OP_AGET_OBJECT, "-object", u4, )
OP_END

/* File: c/OP_AGET_BOOLEAN.cpp */
HANDLE_OP_AGET(OP_AGET_BOOLEAN, "-boolean", u1, )
OP_END

/* File: c/OP_AGET_BYTE.cpp */
HANDLE_OP_AGET(OP_AGET_BYTE, "-byte", s1, )
OP_END

/* File: c/OP_AGET_CHAR.cpp */
HANDLE_OP_AGET(OP_AGET_CHAR, "-char", u2, )
OP_END

/* File: c/OP_AGET_SHORT.cpp */
HANDLE_OP_AGET(OP_AGET_SHORT, "-short", s2, )
OP_END

/* File: c/OP_APUT.cpp */
HANDLE_OP_APUT(OP_APUT, "", u4, )
OP_END

/* File: c/OP_APUT_WIDE.cpp */
HANDLE_OP_APUT(OP_APUT_WIDE, "-wide", s8, _WIDE)
OP_END

/* File: c/OP_APUT_OBJECT.cpp */
HANDLE_OPCODE(OP_APUT_OBJECT /*vAA, vBB, vCC*/)
    {
        ArrayObject* arrayObj;
        Object* obj;
        u2 arrayInfo;
        EXPORT_PC();
        vdst = INST_AA(inst);       /* AA: source value */
        arrayInfo = FETCH(1);
        vsrc1 = arrayInfo & 0xff;   /* BB: array ptr */
        vsrc2 = arrayInfo >> 8;     /* CC: index */
        ILOGV("|aput%s v%d,v%d,v%d", "-object", vdst, vsrc1, vsrc2);
        arrayObj = (ArrayObject*) GET_REGISTER(vsrc1);
        if (!checkForNull((Object*) arrayObj))
            GOTO_exceptionThrown();
        if (GET_REGISTER(vsrc2) >= arrayObj->length) {
            dvmThrowArrayIndexOutOfBoundsException(
                arrayObj->length, GET_REGISTER(vsrc2));
            GOTO_exceptionThrown();
        }
        obj = (Object*) GET_REGISTER(vdst);
        if (obj != NULL) {
            if (!checkForNull(obj))
                GOTO_exceptionThrown();
            if (!dvmCanPutArrayElement(obj->clazz, arrayObj->clazz)) {
                ALOGV("Can't put a '%s'(%p) into array type='%s'(%p)",
                    obj->clazz->descriptor, obj,
                    arrayObj->clazz->descriptor, arrayObj);
                dvmThrowArrayStoreExceptionIncompatibleElement(obj->clazz, arrayObj->clazz);
                GOTO_exceptionThrown();
            }
        }
        ILOGV("+ APUT[%d]=0x%08x", GET_REGISTER(vsrc2), GET_REGISTER(vdst));
        dvmSetObjectArrayElement(arrayObj,
                                 GET_REGISTER(vsrc2),
                                 (Object *)GET_REGISTER(vdst));
    }
    FINISH(2);
OP_END

/* File: c/OP_APUT_BOOLEAN.cpp */
HANDLE_OP_APUT(OP_APUT_BOOLEAN, "-boolean", u1, )
OP_END

/* File: c/OP_APUT_BYTE.cpp */
HANDLE_OP_APUT(OP_APUT_BYTE, "-byte", s1, )
OP_END

/* File: c/OP_APUT_CHAR.cpp */
HANDLE_OP_APUT(OP_APUT_CHAR, "-char", u2, )
OP_END

/* File: c/OP_APUT_SHORT.cpp */
HANDLE_OP_APUT(OP_APUT_SHORT, "-short", s2, )
OP_END

/* File: c/OP_IGET.cpp */
HANDLE_IGET_X(OP_IGET,                  "", Int, )
OP_END

/* File: c/OP_IGET_WIDE.cpp */
HANDLE_IGET_X(OP_IGET_WIDE,             "-wide", Long, _WIDE)
OP_END

/* File: c/OP_IGET_OBJECT.cpp */
HANDLE_IGET_X(OP_IGET_OBJECT,           "-object", Object, _AS_OBJECT)
OP_END

/* File: c/OP_IGET_BOOLEAN.cpp */
HANDLE_IGET_X(OP_IGET_BOOLEAN,          "", Int, )
OP_END

/* File: c/OP_IGET_BYTE.cpp */
HANDLE_IGET_X(OP_IGET_BYTE,             "", Int, )
OP_END

/* File: c/OP_IGET_CHAR.cpp */
HANDLE_IGET_X(OP_IGET_CHAR,             "", Int, )
OP_END

/* File: c/OP_IGET_SHORT.cpp */
HANDLE_IGET_X(OP_IGET_SHORT,            "", Int, )
OP_END

/* File: c/OP_IPUT.cpp */
HANDLE_IPUT_X(OP_IPUT,                  "", Int, )
OP_END

/* File: c/OP_IPUT_WIDE.cpp */
HANDLE_IPUT_X(OP_IPUT_WIDE,             "-wide", Long, _WIDE)
OP_END

/* File: c/OP_IPUT_OBJECT.cpp */
/*
 * The VM spec says we should verify that the reference being stored into
 * the field is assignment compatible.  In practice, many popular VMs don't
 * do this because it slows down a very common operation.  It's not so bad
 * for us, since "dexopt" quickens it whenever possible, but it's still an
 * issue.
 *
 * To make this spec-complaint, we'd need to add a ClassObject pointer to
 * the Field struct, resolve the field's type descriptor at link or class
 * init time, and then verify the type here.
 */
HANDLE_IPUT_X(OP_IPUT_OBJECT,           "-object", Object, _AS_OBJECT)
OP_END

/* File: c/OP_IPUT_BOOLEAN.cpp */
HANDLE_IPUT_X(OP_IPUT_BOOLEAN,          "", Int, )
OP_END

/* File: c/OP_IPUT_BYTE.cpp */
HANDLE_IPUT_X(OP_IPUT_BYTE,             "", Int, )
OP_END

/* File: c/OP_IPUT_CHAR.cpp */
HANDLE_IPUT_X(OP_IPUT_CHAR,             "", Int, )
OP_END

/* File: c/OP_IPUT_SHORT.cpp */
HANDLE_IPUT_X(OP_IPUT_SHORT,            "", Int, )
OP_END

/* File: c/OP_SGET.cpp */
HANDLE_SGET_X(OP_SGET,                  "", Int, )
OP_END

/* File: c/OP_SGET_WIDE.cpp */
HANDLE_SGET_X(OP_SGET_WIDE,             "-wide", Long, _WIDE)
OP_END

/* File: c/OP_SGET_OBJECT.cpp */
HANDLE_SGET_X(OP_SGET_OBJECT,           "-object", Object, _AS_OBJECT)
OP_END

/* File: c/OP_SGET_BOOLEAN.cpp */
HANDLE_SGET_X(OP_SGET_BOOLEAN,          "", Int, )
OP_END

/* File: c/OP_SGET_BYTE.cpp */
HANDLE_SGET_X(OP_SGET_BYTE,             "", Int, )
OP_END

/* File: c/OP_SGET_CHAR.cpp */
HANDLE_SGET_X(OP_SGET_CHAR,             "", Int, )
OP_END

/* File: c/OP_SGET_SHORT.cpp */
HANDLE_SGET_X(OP_SGET_SHORT,            "", Int, )
OP_END

/* File: c/OP_SPUT.cpp */
HANDLE_SPUT_X(OP_SPUT,                  "", Int, )
OP_END

/* File: c/OP_SPUT_WIDE.cpp */
HANDLE_SPUT_X(OP_SPUT_WIDE,             "-wide", Long, _WIDE)
OP_END

/* File: c/OP_SPUT_OBJECT.cpp */
HANDLE_SPUT_X(OP_SPUT_OBJECT,           "-object", Object, _AS_OBJECT)
OP_END

/* File: c/OP_SPUT_BOOLEAN.cpp */
HANDLE_SPUT_X(OP_SPUT_BOOLEAN,          "", Int, )
OP_END

/* File: c/OP_SPUT_BYTE.cpp */
HANDLE_SPUT_X(OP_SPUT_BYTE,             "", Int, )
OP_END

/* File: c/OP_SPUT_CHAR.cpp */
HANDLE_SPUT_X(OP_SPUT_CHAR,             "", Int, )
OP_END

/* File: c/OP_SPUT_SHORT.cpp */
HANDLE_SPUT_X(OP_SPUT_SHORT,            "", Int, )
OP_END

/* File: c/OP_INVOKE_VIRTUAL.cpp */
HANDLE_OPCODE(OP_INVOKE_VIRTUAL /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeVirtual, false);
OP_END

/* File: c/OP_INVOKE_SUPER.cpp */
HANDLE_OPCODE(OP_INVOKE_SUPER /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeSuper, false);
OP_END

/* File: c/OP_INVOKE_DIRECT.cpp */
HANDLE_OPCODE(OP_INVOKE_DIRECT /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeDirect, false);
OP_END

/* File: c/OP_INVOKE_STATIC.cpp */
HANDLE_OPCODE(OP_INVOKE_STATIC /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeStatic, false);
OP_END

/* File: c/OP_INVOKE_INTERFACE.cpp */
HANDLE_OPCODE(OP_INVOKE_INTERFACE /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeInterface, false);
OP_END

/* File: c/OP_UNUSED_73.cpp */
HANDLE_OPCODE(OP_UNUSED_73)
OP_END

/* File: c/OP_INVOKE_VIRTUAL_RANGE.cpp */
HANDLE_OPCODE(OP_INVOKE_VIRTUAL_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeVirtual, true);
OP_END

/* File: c/OP_INVOKE_SUPER_RANGE.cpp */
HANDLE_OPCODE(OP_INVOKE_SUPER_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeSuper, true);
OP_END

/* File: c/OP_INVOKE_DIRECT_RANGE.cpp */
HANDLE_OPCODE(OP_INVOKE_DIRECT_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeDirect, true);
OP_END

/* File: c/OP_INVOKE_STATIC_RANGE.cpp */
HANDLE_OPCODE(OP_INVOKE_STATIC_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeStatic, true);
OP_END

/* File: c/OP_INVOKE_INTERFACE_RANGE.cpp */
HANDLE_OPCODE(OP_INVOKE_INTERFACE_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeInterface, true);
OP_END

/* File: c/OP_UNUSED_79.cpp */
HANDLE_OPCODE(OP_UNUSED_79)
OP_END

/* File: c/OP_UNUSED_7A.cpp */
HANDLE_OPCODE(OP_UNUSED_7A)
OP_END

/* File: c/OP_NEG_INT.cpp */
HANDLE_UNOP(OP_NEG_INT, "neg-int", -, , )
OP_END

/* File: c/OP_NOT_INT.cpp */
HANDLE_UNOP(OP_NOT_INT, "not-int", , ^ 0xffffffff, )
OP_END

/* File: c/OP_NEG_LONG.cpp */
HANDLE_UNOP(OP_NEG_LONG, "neg-long", -, , _WIDE)
OP_END

/* File: c/OP_NOT_LONG.cpp */
HANDLE_UNOP(OP_NOT_LONG, "not-long", , ^ 0xffffffffffffffffULL, _WIDE)
OP_END

/* File: c/OP_NEG_FLOAT.cpp */
HANDLE_UNOP(OP_NEG_FLOAT, "neg-float", -, , _FLOAT)
OP_END

/* File: c/OP_NEG_DOUBLE.cpp */
HANDLE_UNOP(OP_NEG_DOUBLE, "neg-double", -, , _DOUBLE)
OP_END

/* File: c/OP_INT_TO_LONG.cpp */
HANDLE_NUMCONV(OP_INT_TO_LONG,          "int-to-long", _INT, _WIDE)
OP_END

/* File: c/OP_INT_TO_FLOAT.cpp */
HANDLE_NUMCONV(OP_INT_TO_FLOAT,         "int-to-float", _INT, _FLOAT)
OP_END

/* File: c/OP_INT_TO_DOUBLE.cpp */
HANDLE_NUMCONV(OP_INT_TO_DOUBLE,        "int-to-double", _INT, _DOUBLE)
OP_END

/* File: c/OP_LONG_TO_INT.cpp */
HANDLE_NUMCONV(OP_LONG_TO_INT,          "long-to-int", _WIDE, _INT)
OP_END

/* File: c/OP_LONG_TO_FLOAT.cpp */
HANDLE_NUMCONV(OP_LONG_TO_FLOAT,        "long-to-float", _WIDE, _FLOAT)
OP_END

/* File: c/OP_LONG_TO_DOUBLE.cpp */
HANDLE_NUMCONV(OP_LONG_TO_DOUBLE,       "long-to-double", _WIDE, _DOUBLE)
OP_END

/* File: c/OP_FLOAT_TO_INT.cpp */
HANDLE_FLOAT_TO_INT(OP_FLOAT_TO_INT,    "float-to-int",
    float, _FLOAT, s4, _INT)
OP_END

/* File: c/OP_FLOAT_TO_LONG.cpp */
HANDLE_FLOAT_TO_INT(OP_FLOAT_TO_LONG,   "float-to-long",
    float, _FLOAT, s8, _WIDE)
OP_END

/* File: c/OP_FLOAT_TO_DOUBLE.cpp */
HANDLE_NUMCONV(OP_FLOAT_TO_DOUBLE,      "float-to-double", _FLOAT, _DOUBLE)
OP_END

/* File: c/OP_DOUBLE_TO_INT.cpp */
HANDLE_FLOAT_TO_INT(OP_DOUBLE_TO_INT,   "double-to-int",
    double, _DOUBLE, s4, _INT)
OP_END

/* File: c/OP_DOUBLE_TO_LONG.cpp */
HANDLE_FLOAT_TO_INT(OP_DOUBLE_TO_LONG,  "double-to-long",
    double, _DOUBLE, s8, _WIDE)
OP_END

/* File: c/OP_DOUBLE_TO_FLOAT.cpp */
HANDLE_NUMCONV(OP_DOUBLE_TO_FLOAT,      "double-to-float", _DOUBLE, _FLOAT)
OP_END

/* File: c/OP_INT_TO_BYTE.cpp */
HANDLE_INT_TO_SMALL(OP_INT_TO_BYTE,     "byte", s1)
OP_END

/* File: c/OP_INT_TO_CHAR.cpp */
HANDLE_INT_TO_SMALL(OP_INT_TO_CHAR,     "char", u2)
OP_END

/* File: c/OP_INT_TO_SHORT.cpp */
HANDLE_INT_TO_SMALL(OP_INT_TO_SHORT,    "short", s2)    /* want sign bit */
OP_END

/* File: c/OP_ADD_INT.cpp */
HANDLE_OP_X_INT(OP_ADD_INT, "add", +, 0)
OP_END

/* File: c/OP_SUB_INT.cpp */
HANDLE_OP_X_INT(OP_SUB_INT, "sub", -, 0)
OP_END

/* File: c/OP_MUL_INT.cpp */
HANDLE_OP_X_INT(OP_MUL_INT, "mul", *, 0)
OP_END

/* File: c/OP_DIV_INT.cpp */
HANDLE_OP_X_INT(OP_DIV_INT, "div", /, 1)
OP_END

/* File: c/OP_REM_INT.cpp */
HANDLE_OP_X_INT(OP_REM_INT, "rem", %, 2)
OP_END

/* File: c/OP_AND_INT.cpp */
HANDLE_OP_X_INT(OP_AND_INT, "and", &, 0)
OP_END

/* File: c/OP_OR_INT.cpp */
HANDLE_OP_X_INT(OP_OR_INT,  "or",  |, 0)
OP_END

/* File: c/OP_XOR_INT.cpp */
HANDLE_OP_X_INT(OP_XOR_INT, "xor", ^, 0)
OP_END

/* File: c/OP_SHL_INT.cpp */
HANDLE_OP_SHX_INT(OP_SHL_INT, "shl", (s4), <<)
OP_END

/* File: c/OP_SHR_INT.cpp */
HANDLE_OP_SHX_INT(OP_SHR_INT, "shr", (s4), >>)
OP_END

/* File: c/OP_USHR_INT.cpp */
HANDLE_OP_SHX_INT(OP_USHR_INT, "ushr", (u4), >>)
OP_END

/* File: c/OP_ADD_LONG.cpp */
HANDLE_OP_X_LONG(OP_ADD_LONG, "add", +, 0)
OP_END

/* File: c/OP_SUB_LONG.cpp */
HANDLE_OP_X_LONG(OP_SUB_LONG, "sub", -, 0)
OP_END

/* File: c/OP_MUL_LONG.cpp */
HANDLE_OP_X_LONG(OP_MUL_LONG, "mul", *, 0)
OP_END

/* File: c/OP_DIV_LONG.cpp */
HANDLE_OP_X_LONG(OP_DIV_LONG, "div", /, 1)
OP_END

/* File: c/OP_REM_LONG.cpp */
HANDLE_OP_X_LONG(OP_REM_LONG, "rem", %, 2)
OP_END

/* File: c/OP_AND_LONG.cpp */
HANDLE_OP_X_LONG(OP_AND_LONG, "and", &, 0)
OP_END

/* File: c/OP_OR_LONG.cpp */
HANDLE_OP_X_LONG(OP_OR_LONG,  "or", |, 0)
OP_END

/* File: c/OP_XOR_LONG.cpp */
HANDLE_OP_X_LONG(OP_XOR_LONG, "xor", ^, 0)
OP_END

/* File: c/OP_SHL_LONG.cpp */
HANDLE_OP_SHX_LONG(OP_SHL_LONG, "shl", (s8), <<)
OP_END

/* File: c/OP_SHR_LONG.cpp */
HANDLE_OP_SHX_LONG(OP_SHR_LONG, "shr", (s8), >>)
OP_END

/* File: c/OP_USHR_LONG.cpp */
HANDLE_OP_SHX_LONG(OP_USHR_LONG, "ushr", (u8), >>)
OP_END

/* File: c/OP_ADD_FLOAT.cpp */
HANDLE_OP_X_FLOAT(OP_ADD_FLOAT, "add", +)
OP_END

/* File: c/OP_SUB_FLOAT.cpp */
HANDLE_OP_X_FLOAT(OP_SUB_FLOAT, "sub", -)
OP_END

/* File: c/OP_MUL_FLOAT.cpp */
HANDLE_OP_X_FLOAT(OP_MUL_FLOAT, "mul", *)
OP_END

/* File: c/OP_DIV_FLOAT.cpp */
HANDLE_OP_X_FLOAT(OP_DIV_FLOAT, "div", /)
OP_END

/* File: c/OP_REM_FLOAT.cpp */
HANDLE_OPCODE(OP_REM_FLOAT /*vAA, vBB, vCC*/)
    {
        u2 srcRegs;
        vdst = INST_AA(inst);
        srcRegs = FETCH(1);
        vsrc1 = srcRegs & 0xff;
        vsrc2 = srcRegs >> 8;
        ILOGV("|%s-float v%d,v%d,v%d", "mod", vdst, vsrc1, vsrc2);
        SET_REGISTER_FLOAT(vdst,
            fmodf(GET_REGISTER_FLOAT(vsrc1), GET_REGISTER_FLOAT(vsrc2)));
    }
    FINISH(2);
OP_END

/* File: c/OP_ADD_DOUBLE.cpp */
HANDLE_OP_X_DOUBLE(OP_ADD_DOUBLE, "add", +)
OP_END

/* File: c/OP_SUB_DOUBLE.cpp */
HANDLE_OP_X_DOUBLE(OP_SUB_DOUBLE, "sub", -)
OP_END

/* File: c/OP_MUL_DOUBLE.cpp */
HANDLE_OP_X_DOUBLE(OP_MUL_DOUBLE, "mul", *)
OP_END

/* File: c/OP_DIV_DOUBLE.cpp */
HANDLE_OP_X_DOUBLE(OP_DIV_DOUBLE, "div", /)
OP_END

/* File: c/OP_REM_DOUBLE.cpp */
HANDLE_OPCODE(OP_REM_DOUBLE /*vAA, vBB, vCC*/)
    {
        u2 srcRegs;
        vdst = INST_AA(inst);
        srcRegs = FETCH(1);
        vsrc1 = srcRegs & 0xff;
        vsrc2 = srcRegs >> 8;
        ILOGV("|%s-double v%d,v%d,v%d", "mod", vdst, vsrc1, vsrc2);
        SET_REGISTER_DOUBLE(vdst,
            fmod(GET_REGISTER_DOUBLE(vsrc1), GET_REGISTER_DOUBLE(vsrc2)));
    }
    FINISH(2);
OP_END

/* File: c/OP_ADD_INT_2ADDR.cpp */
HANDLE_OP_X_INT_2ADDR(OP_ADD_INT_2ADDR, "add", +, 0)
OP_END

/* File: c/OP_SUB_INT_2ADDR.cpp */
HANDLE_OP_X_INT_2ADDR(OP_SUB_INT_2ADDR, "sub", -, 0)
OP_END

/* File: c/OP_MUL_INT_2ADDR.cpp */
HANDLE_OP_X_INT_2ADDR(OP_MUL_INT_2ADDR, "mul", *, 0)
OP_END

/* File: c/OP_DIV_INT_2ADDR.cpp */
HANDLE_OP_X_INT_2ADDR(OP_DIV_INT_2ADDR, "div", /, 1)
OP_END

/* File: c/OP_REM_INT_2ADDR.cpp */
HANDLE_OP_X_INT_2ADDR(OP_REM_INT_2ADDR, "rem", %, 2)
OP_END

/* File: c/OP_AND_INT_2ADDR.cpp */
HANDLE_OP_X_INT_2ADDR(OP_AND_INT_2ADDR, "and", &, 0)
OP_END

/* File: c/OP_OR_INT_2ADDR.cpp */
HANDLE_OP_X_INT_2ADDR(OP_OR_INT_2ADDR,  "or", |, 0)
OP_END

/* File: c/OP_XOR_INT_2ADDR.cpp */
HANDLE_OP_X_INT_2ADDR(OP_XOR_INT_2ADDR, "xor", ^, 0)
OP_END

/* File: c/OP_SHL_INT_2ADDR.cpp */
HANDLE_OP_SHX_INT_2ADDR(OP_SHL_INT_2ADDR, "shl", (s4), <<)
OP_END

/* File: c/OP_SHR_INT_2ADDR.cpp */
HANDLE_OP_SHX_INT_2ADDR(OP_SHR_INT_2ADDR, "shr", (s4), >>)
OP_END

/* File: c/OP_USHR_INT_2ADDR.cpp */
HANDLE_OP_SHX_INT_2ADDR(OP_USHR_INT_2ADDR, "ushr", (u4), >>)
OP_END

/* File: c/OP_ADD_LONG_2ADDR.cpp */
HANDLE_OP_X_LONG_2ADDR(OP_ADD_LONG_2ADDR, "add", +, 0)
OP_END

/* File: c/OP_SUB_LONG_2ADDR.cpp */
HANDLE_OP_X_LONG_2ADDR(OP_SUB_LONG_2ADDR, "sub", -, 0)
OP_END

/* File: c/OP_MUL_LONG_2ADDR.cpp */
HANDLE_OP_X_LONG_2ADDR(OP_MUL_LONG_2ADDR, "mul", *, 0)
OP_END

/* File: c/OP_DIV_LONG_2ADDR.cpp */
HANDLE_OP_X_LONG_2ADDR(OP_DIV_LONG_2ADDR, "div", /, 1)
OP_END

/* File: c/OP_REM_LONG_2ADDR.cpp */
HANDLE_OP_X_LONG_2ADDR(OP_REM_LONG_2ADDR, "rem", %, 2)
OP_END

/* File: c/OP_AND_LONG_2ADDR.cpp */
HANDLE_OP_X_LONG_2ADDR(OP_AND_LONG_2ADDR, "and", &, 0)
OP_END

/* File: c/OP_OR_LONG_2ADDR.cpp */
HANDLE_OP_X_LONG_2ADDR(OP_OR_LONG_2ADDR,  "or", |, 0)
OP_END

/* File: c/OP_XOR_LONG_2ADDR.cpp */
HANDLE_OP_X_LONG_2ADDR(OP_XOR_LONG_2ADDR, "xor", ^, 0)
OP_END

/* File: c/OP_SHL_LONG_2ADDR.cpp */
HANDLE_OP_SHX_LONG_2ADDR(OP_SHL_LONG_2ADDR, "shl", (s8), <<)
OP_END

/* File: c/OP_SHR_LONG_2ADDR.cpp */
HANDLE_OP_SHX_LONG_2ADDR(OP_SHR_LONG_2ADDR, "shr", (s8), >>)
OP_END

/* File: c/OP_USHR_LONG_2ADDR.cpp */
HANDLE_OP_SHX_LONG_2ADDR(OP_USHR_LONG_2ADDR, "ushr", (u8), >>)
OP_END

/* File: c/OP_ADD_FLOAT_2ADDR.cpp */
HANDLE_OP_X_FLOAT_2ADDR(OP_ADD_FLOAT_2ADDR, "add", +)
OP_END

/* File: c/OP_SUB_FLOAT_2ADDR.cpp */
HANDLE_OP_X_FLOAT_2ADDR(OP_SUB_FLOAT_2ADDR, "sub", -)
OP_END

/* File: c/OP_MUL_FLOAT_2ADDR.cpp */
HANDLE_OP_X_FLOAT_2ADDR(OP_MUL_FLOAT_2ADDR, "mul", *)
OP_END

/* File: c/OP_DIV_FLOAT_2ADDR.cpp */
HANDLE_OP_X_FLOAT_2ADDR(OP_DIV_FLOAT_2ADDR, "div", /)
OP_END

/* File: c/OP_REM_FLOAT_2ADDR.cpp */
HANDLE_OPCODE(OP_REM_FLOAT_2ADDR /*vA, vB*/)
    vdst = INST_A(inst);
    vsrc1 = INST_B(inst);
    ILOGV("|%s-float-2addr v%d,v%d", "mod", vdst, vsrc1);
    SET_REGISTER_FLOAT(vdst,
        fmodf(GET_REGISTER_FLOAT(vdst), GET_REGISTER_FLOAT(vsrc1)));
    FINISH(1);
OP_END

/* File: c/OP_ADD_DOUBLE_2ADDR.cpp */
HANDLE_OP_X_DOUBLE_2ADDR(OP_ADD_DOUBLE_2ADDR, "add", +)
OP_END

/* File: c/OP_SUB_DOUBLE_2ADDR.cpp */
HANDLE_OP_X_DOUBLE_2ADDR(OP_SUB_DOUBLE_2ADDR, "sub", -)
OP_END

/* File: c/OP_MUL_DOUBLE_2ADDR.cpp */
HANDLE_OP_X_DOUBLE_2ADDR(OP_MUL_DOUBLE_2ADDR, "mul", *)
OP_END

/* File: c/OP_DIV_DOUBLE_2ADDR.cpp */
HANDLE_OP_X_DOUBLE_2ADDR(OP_DIV_DOUBLE_2ADDR, "div", /)
OP_END

/* File: c/OP_REM_DOUBLE_2ADDR.cpp */
HANDLE_OPCODE(OP_REM_DOUBLE_2ADDR /*vA, vB*/)
    vdst = INST_A(inst);
    vsrc1 = INST_B(inst);
    ILOGV("|%s-double-2addr v%d,v%d", "mod", vdst, vsrc1);
    SET_REGISTER_DOUBLE(vdst,
        fmod(GET_REGISTER_DOUBLE(vdst), GET_REGISTER_DOUBLE(vsrc1)));
    FINISH(1);
OP_END

/* File: c/OP_ADD_INT_LIT16.cpp */
HANDLE_OP_X_INT_LIT16(OP_ADD_INT_LIT16, "add", +, 0)
OP_END

/* File: c/OP_RSUB_INT.cpp */
HANDLE_OPCODE(OP_RSUB_INT /*vA, vB, #+CCCC*/)
    {
        vdst = INST_A(inst);
        vsrc1 = INST_B(inst);
        vsrc2 = FETCH(1);
        ILOGV("|rsub-int v%d,v%d,#+0x%04x", vdst, vsrc1, vsrc2);
        SET_REGISTER(vdst, (s2) vsrc2 - (s4) GET_REGISTER(vsrc1));
    }
    FINISH(2);
OP_END

/* File: c/OP_MUL_INT_LIT16.cpp */
HANDLE_OP_X_INT_LIT16(OP_MUL_INT_LIT16, "mul", *, 0)
OP_END

/* File: c/OP_DIV_INT_LIT16.cpp */
HANDLE_OP_X_INT_LIT16(OP_DIV_INT_LIT16, "div", /, 1)
OP_END

/* File: c/OP_REM_INT_LIT16.cpp */
HANDLE_OP_X_INT_LIT16(OP_REM_INT_LIT16, "rem", %, 2)
OP_END

/* File: c/OP_AND_INT_LIT16.cpp */
HANDLE_OP_X_INT_LIT16(OP_AND_INT_LIT16, "and", &, 0)
OP_END

/* File: c/OP_OR_INT_LIT16.cpp */
HANDLE_OP_X_INT_LIT16(OP_OR_INT_LIT16,  "or",  |, 0)
OP_END

/* File: c/OP_XOR_INT_LIT16.cpp */
HANDLE_OP_X_INT_LIT16(OP_XOR_INT_LIT16, "xor", ^, 0)
OP_END

/* File: c/OP_ADD_INT_LIT8.cpp */
HANDLE_OP_X_INT_LIT8(OP_ADD_INT_LIT8,   "add", +, 0)
OP_END

/* File: c/OP_RSUB_INT_LIT8.cpp */
HANDLE_OPCODE(OP_RSUB_INT_LIT8 /*vAA, vBB, #+CC*/)
    {
        u2 litInfo;
        vdst = INST_AA(inst);
        litInfo = FETCH(1);
        vsrc1 = litInfo & 0xff;
        vsrc2 = litInfo >> 8;
        ILOGV("|%s-int/lit8 v%d,v%d,#+0x%02x", "rsub", vdst, vsrc1, vsrc2);
        SET_REGISTER(vdst, (s1) vsrc2 - (s4) GET_REGISTER(vsrc1));
    }
    FINISH(2);
OP_END

/* File: c/OP_MUL_INT_LIT8.cpp */
HANDLE_OP_X_INT_LIT8(OP_MUL_INT_LIT8,   "mul", *, 0)
OP_END

/* File: c/OP_DIV_INT_LIT8.cpp */
HANDLE_OP_X_INT_LIT8(OP_DIV_INT_LIT8,   "div", /, 1)
OP_END

/* File: c/OP_REM_INT_LIT8.cpp */
HANDLE_OP_X_INT_LIT8(OP_REM_INT_LIT8,   "rem", %, 2)
OP_END

/* File: c/OP_AND_INT_LIT8.cpp */
HANDLE_OP_X_INT_LIT8(OP_AND_INT_LIT8,   "and", &, 0)
OP_END

/* File: c/OP_OR_INT_LIT8.cpp */
HANDLE_OP_X_INT_LIT8(OP_OR_INT_LIT8,    "or",  |, 0)
OP_END

/* File: c/OP_XOR_INT_LIT8.cpp */
HANDLE_OP_X_INT_LIT8(OP_XOR_INT_LIT8,   "xor", ^, 0)
OP_END

/* File: c/OP_SHL_INT_LIT8.cpp */
HANDLE_OP_SHX_INT_LIT8(OP_SHL_INT_LIT8,   "shl", (s4), <<)
OP_END

/* File: c/OP_SHR_INT_LIT8.cpp */
HANDLE_OP_SHX_INT_LIT8(OP_SHR_INT_LIT8,   "shr", (s4), >>)
OP_END

/* File: c/OP_USHR_INT_LIT8.cpp */
HANDLE_OP_SHX_INT_LIT8(OP_USHR_INT_LIT8,  "ushr", (u4), >>)
OP_END

/* File: c/OP_IGET_VOLATILE.cpp */
HANDLE_IGET_X(OP_IGET_VOLATILE,         "-volatile", IntVolatile, )
OP_END

/* File: c/OP_IPUT_VOLATILE.cpp */
HANDLE_IPUT_X(OP_IPUT_VOLATILE,         "-volatile", IntVolatile, )
OP_END

/* File: c/OP_SGET_VOLATILE.cpp */
HANDLE_SGET_X(OP_SGET_VOLATILE,         "-volatile", IntVolatile, )
OP_END

/* File: c/OP_SPUT_VOLATILE.cpp */
HANDLE_SPUT_X(OP_SPUT_VOLATILE,         "-volatile", IntVolatile, )
OP_END

/* File: c/OP_IGET_OBJECT_VOLATILE.cpp */
HANDLE_IGET_X(OP_IGET_OBJECT_VOLATILE,  "-object-volatile", ObjectVolatile, _AS_OBJECT)
OP_END

/* File: c/OP_IGET_WIDE_VOLATILE.cpp */
HANDLE_IGET_X(OP_IGET_WIDE_VOLATILE,    "-wide-volatile", LongVolatile, _WIDE)
OP_END

/* File: c/OP_IPUT_WIDE_VOLATILE.cpp */
HANDLE_IPUT_X(OP_IPUT_WIDE_VOLATILE,    "-wide-volatile", LongVolatile, _WIDE)
OP_END

/* File: c/OP_SGET_WIDE_VOLATILE.cpp */
HANDLE_SGET_X(OP_SGET_WIDE_VOLATILE,    "-wide-volatile", LongVolatile, _WIDE)
OP_END

/* File: c/OP_SPUT_WIDE_VOLATILE.cpp */
HANDLE_SPUT_X(OP_SPUT_WIDE_VOLATILE,    "-wide-volatile", LongVolatile, _WIDE)
OP_END

/* File: c/OP_BREAKPOINT.cpp */
HANDLE_OPCODE(OP_BREAKPOINT)
    {
        /*
         * Restart this instruction with the original opcode.  We do
         * this by simply jumping to the handler.
         *
         * It's probably not necessary to update "inst", but we do it
         * for the sake of anything that needs to do disambiguation in a
         * common handler with INST_INST.
         *
         * The breakpoint itself is handled over in updateDebugger(),
         * because we need to detect other events (method entry, single
         * step) and report them in the same event packet, and we're not
         * yet handling those through breakpoint instructions.  By the
         * time we get here, the breakpoint has already been handled and
         * the thread resumed.
         */
        u1 originalOpcode = dvmGetOriginalOpcode(pc);
        ALOGV("+++ break 0x%02x (0x%04x -> 0x%04x)", originalOpcode, inst,
            INST_REPLACE_OP(inst, originalOpcode));
        inst = INST_REPLACE_OP(inst, originalOpcode);
        FINISH_BKPT(originalOpcode);
    }
OP_END

/* File: c/OP_THROW_VERIFICATION_ERROR.cpp */
HANDLE_OPCODE(OP_THROW_VERIFICATION_ERROR)
    EXPORT_PC();
    vsrc1 = INST_AA(inst);
    ref = FETCH(1);             /* class/field/method ref */
    dvmThrowVerificationError(curMethod, vsrc1, ref);
    GOTO_exceptionThrown();
OP_END

/* File: c/OP_EXECUTE_INLINE.cpp */
HANDLE_OPCODE(OP_EXECUTE_INLINE /*vB, {vD, vE, vF, vG}, inline@CCCC*/)
    {
        /*
         * This has the same form as other method calls, but we ignore
         * the 5th argument (vA).  This is chiefly because the first four
         * arguments to a function on ARM are in registers.
         *
         * We only set the arguments that are actually used, leaving
         * the rest uninitialized.  We're assuming that, if the method
         * needs them, they'll be specified in the call.
         *
         * However, this annoys gcc when optimizations are enabled,
         * causing a "may be used uninitialized" warning.  Quieting
         * the warnings incurs a slight penalty (5%: 373ns vs. 393ns
         * on empty method).  Note that valgrind is perfectly happy
         * either way as the uninitialiezd values are never actually
         * used.
         */
        u4 arg0, arg1, arg2, arg3;
        arg0 = arg1 = arg2 = arg3 = 0;

        EXPORT_PC();

        vsrc1 = INST_B(inst);       /* #of args */
        ref = FETCH(1);             /* inline call "ref" */
        vdst = FETCH(2);            /* 0-4 register indices */
        ILOGV("|execute-inline args=%d @%d {regs=0x%04x}",
            vsrc1, ref, vdst);

        assert((vdst >> 16) == 0);  // 16-bit type -or- high 16 bits clear
        assert(vsrc1 <= 4);

        switch (vsrc1) {
        case 4:
            arg3 = GET_REGISTER(vdst >> 12);
            /* fall through */
        case 3:
            arg2 = GET_REGISTER((vdst & 0x0f00) >> 8);
            /* fall through */
        case 2:
            arg1 = GET_REGISTER((vdst & 0x00f0) >> 4);
            /* fall through */
        case 1:
            arg0 = GET_REGISTER(vdst & 0x0f);
            /* fall through */
        default:        // case 0
            ;
        }

        if (self->interpBreak.ctl.subMode & kSubModeDebugProfile) {
            if (!dvmPerformInlineOp4Dbg(arg0, arg1, arg2, arg3, &retval, ref))
                GOTO_exceptionThrown();
        } else {
            if (!dvmPerformInlineOp4Std(arg0, arg1, arg2, arg3, &retval, ref))
                GOTO_exceptionThrown();
        }
    }
    FINISH(3);
OP_END

/* File: c/OP_EXECUTE_INLINE_RANGE.cpp */
HANDLE_OPCODE(OP_EXECUTE_INLINE_RANGE /*{vCCCC..v(CCCC+AA-1)}, inline@BBBB*/)
    {
        u4 arg0, arg1, arg2, arg3;
        arg0 = arg1 = arg2 = arg3 = 0;      /* placate gcc */

        EXPORT_PC();

        vsrc1 = INST_AA(inst);      /* #of args */
        ref = FETCH(1);             /* inline call "ref" */
        vdst = FETCH(2);            /* range base */
        ILOGV("|execute-inline-range args=%d @%d {regs=v%d-v%d}",
            vsrc1, ref, vdst, vdst+vsrc1-1);

        assert((vdst >> 16) == 0);  // 16-bit type -or- high 16 bits clear
        assert(vsrc1 <= 4);

        switch (vsrc1) {
        case 4:
            arg3 = GET_REGISTER(vdst+3);
            /* fall through */
        case 3:
            arg2 = GET_REGISTER(vdst+2);
            /* fall through */
        case 2:
            arg1 = GET_REGISTER(vdst+1);
            /* fall through */
        case 1:
            arg0 = GET_REGISTER(vdst+0);
            /* fall through */
        default:        // case 0
            ;
        }

        if (self->interpBreak.ctl.subMode & kSubModeDebugProfile) {
            if (!dvmPerformInlineOp4Dbg(arg0, arg1, arg2, arg3, &retval, ref))
                GOTO_exceptionThrown();
        } else {
            if (!dvmPerformInlineOp4Std(arg0, arg1, arg2, arg3, &retval, ref))
                GOTO_exceptionThrown();
        }
    }
    FINISH(3);
OP_END

/* File: c/OP_INVOKE_OBJECT_INIT_RANGE.cpp */
HANDLE_OPCODE(OP_INVOKE_OBJECT_INIT_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    {
        Object* obj;

        vsrc1 = FETCH(2);               /* reg number of "this" pointer */
        obj = GET_REGISTER_AS_OBJECT(vsrc1);

        if (!checkForNullExportPC(obj, fp, pc))
            GOTO_exceptionThrown();

        /*
         * The object should be marked "finalizable" when Object.<init>
         * completes normally.  We're going to assume it does complete
         * (by virtue of being nothing but a return-void) and set it now.
         */
        if (IS_CLASS_FLAG_SET(obj->clazz, CLASS_ISFINALIZABLE)) {
            EXPORT_PC();
            dvmSetFinalizable(obj);
            if (dvmGetException(self))
                GOTO_exceptionThrown();
        }

        if (self->interpBreak.ctl.subMode & kSubModeDebuggerActive) {
            /* behave like OP_INVOKE_DIRECT_RANGE */
            GOTO_invoke(invokeDirect, true);
        }
        FINISH(3);
    }
OP_END

/* File: c/OP_RETURN_VOID_BARRIER.cpp */
HANDLE_OPCODE(OP_RETURN_VOID_BARRIER /**/)
    ILOGV("|return-void");
#ifndef NDEBUG
    retval.j = 0xababababULL;   /* placate valgrind */
#endif
    ANDROID_MEMBAR_STORE();
    GOTO_returnFromMethod();
OP_END

/* File: c/OP_IGET_QUICK.cpp */
HANDLE_IGET_X_QUICK(OP_IGET_QUICK,          "", Int, )
OP_END

/* File: c/OP_IGET_WIDE_QUICK.cpp */
HANDLE_IGET_X_QUICK(OP_IGET_WIDE_QUICK,     "-wide", Long, _WIDE)
OP_END

/* File: c/OP_IGET_OBJECT_QUICK.cpp */
HANDLE_IGET_X_QUICK(OP_IGET_OBJECT_QUICK,   "-object", Object, _AS_OBJECT)
OP_END

/* File: c/OP_IPUT_QUICK.cpp */
HANDLE_IPUT_X_QUICK(OP_IPUT_QUICK,          "", Int, )
OP_END

/* File: c/OP_IPUT_WIDE_QUICK.cpp */
HANDLE_IPUT_X_QUICK(OP_IPUT_WIDE_QUICK,     "-wide", Long, _WIDE)
OP_END

/* File: c/OP_IPUT_OBJECT_QUICK.cpp */
HANDLE_IPUT_X_QUICK(OP_IPUT_OBJECT_QUICK,   "-object", Object, _AS_OBJECT)
OP_END

/* File: c/OP_INVOKE_VIRTUAL_QUICK.cpp */
HANDLE_OPCODE(OP_INVOKE_VIRTUAL_QUICK /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeVirtualQuick, false);
OP_END

/* File: c/OP_INVOKE_VIRTUAL_QUICK_RANGE.cpp */
HANDLE_OPCODE(OP_INVOKE_VIRTUAL_QUICK_RANGE/*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeVirtualQuick, true);
OP_END

/* File: c/OP_INVOKE_SUPER_QUICK.cpp */
HANDLE_OPCODE(OP_INVOKE_SUPER_QUICK /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeSuperQuick, false);
OP_END

/* File: c/OP_INVOKE_SUPER_QUICK_RANGE.cpp */
HANDLE_OPCODE(OP_INVOKE_SUPER_QUICK_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeSuperQuick, true);
OP_END

/* File: c/OP_IPUT_OBJECT_VOLATILE.cpp */
HANDLE_IPUT_X(OP_IPUT_OBJECT_VOLATILE,  "-object-volatile", ObjectVolatile, _AS_OBJECT)
OP_END

/* File: c/OP_SGET_OBJECT_VOLATILE.cpp */
HANDLE_SGET_X(OP_SGET_OBJECT_VOLATILE,  "-object-volatile", ObjectVolatile, _AS_OBJECT)
OP_END

/* File: c/OP_SPUT_OBJECT_VOLATILE.cpp */
HANDLE_SPUT_X(OP_SPUT_OBJECT_VOLATILE,  "-object-volatile", ObjectVolatile, _AS_OBJECT)
OP_END

/* File: c/OP_UNUSED_FF.cpp */
HANDLE_OPCODE(OP_UNUSED_FF)
    /*
     * In portable interp, most unused opcodes will fall through to here.
     */
    ALOGE("unknown opcode 0x%02x\n", INST_INST(inst));
    dvmAbort();
    FINISH(1);
OP_END

/* File: cstubs/entry.cpp */
/*
 * Handler function table, one entry per opcode.
 */
#undef H
#define H(_op) (const void*) dvmMterp_##_op
DEFINE_GOTO_TABLE(gDvmMterpHandlers)

#undef H
#define H(_op) #_op
DEFINE_GOTO_TABLE(gDvmMterpHandlerNames)

#include <setjmp.h>

/*
 * C mterp entry point.  This just calls the various C fallbacks, making
 * this a slow but portable interpeter.
 *
 * This is only used for the "allstubs" variant.
 */
void dvmMterpStdRun(Thread* self)
{
    jmp_buf jmpBuf;

    self->interpSave.bailPtr = &jmpBuf;

    /* We exit via a longjmp */
    if (setjmp(jmpBuf)) {
        LOGVV("mterp threadid=%d returning", dvmThreadSelf()->threadId);
        return;
    }

    /* run until somebody longjmp()s out */
    while (true) {
        typedef void (*Handler)(Thread* self);

        u2 inst = /*self->interpSave.*/pc[0];
        /*
         * In mterp, dvmCheckBefore is handled via the altHandlerTable,
         * while in the portable interpreter it is part of the handler
         * FINISH code.  For allstubs, we must do an explicit check
         * in the interpretation loop.
         */
        if (self->interpBreak.ctl.subMode) {
            dvmCheckBefore(pc, fp, self);
        }
        Handler handler = (Handler) gDvmMterpHandlers[inst & 0xff];
        (void) gDvmMterpHandlerNames;   /* avoid gcc "defined but not used" */
        LOGVV("handler %p %s",
            handler, (const char*) gDvmMterpHandlerNames[inst & 0xff]);
        (*handler)(self);
    }
}

/*
 * C mterp exit point.  Call here to bail out of the interpreter.
 */
void dvmMterpStdBail(Thread* self)
{
    jmp_buf* pJmpBuf = (jmp_buf*) self->interpSave.bailPtr;
    longjmp(*pJmpBuf, 1);
}

/* File: c/gotoTargets.cpp */
/*
 * C footer.  This has some common code shared by the various targets.
 */

/*
 * Everything from here on is a "goto target".  In the basic interpreter
 * we jump into these targets and then jump directly to the handler for
 * next instruction.  Here, these are subroutines that return to the caller.
 */

GOTO_TARGET(filledNewArray, bool methodCallRange, bool)
    {
        ClassObject* arrayClass;
        ArrayObject* newArray;
        u4* contents;
        char typeCh;
        int i;
        u4 arg5;

        EXPORT_PC();

        ref = FETCH(1);             /* class ref */
        vdst = FETCH(2);            /* first 4 regs -or- range base */

        if (methodCallRange) {
            vsrc1 = INST_AA(inst);  /* #of elements */
            arg5 = -1;              /* silence compiler warning */
            ILOGV("|filled-new-array-range args=%d @0x%04x {regs=v%d-v%d}",
                vsrc1, ref, vdst, vdst+vsrc1-1);
        } else {
            arg5 = INST_A(inst);
            vsrc1 = INST_B(inst);   /* #of elements */
            ILOGV("|filled-new-array args=%d @0x%04x {regs=0x%04x %x}",
               vsrc1, ref, vdst, arg5);
        }

        /*
         * Resolve the array class.
         */
        arrayClass = dvmDexGetResolvedClass(methodClassDex, ref);
        if (arrayClass == NULL) {
            arrayClass = dvmResolveClass(curMethod->clazz, ref, false);
            if (arrayClass == NULL)
                GOTO_exceptionThrown();
        }
        /*
        if (!dvmIsArrayClass(arrayClass)) {
            dvmThrowRuntimeException(
                "filled-new-array needs array class");
            GOTO_exceptionThrown();
        }
        */
        /* verifier guarantees this is an array class */
        assert(dvmIsArrayClass(arrayClass));
        assert(dvmIsClassInitialized(arrayClass));

        /*
         * Create an array of the specified type.
         */
        LOGVV("+++ filled-new-array type is '%s'", arrayClass->descriptor);
        typeCh = arrayClass->descriptor[1];
        if (typeCh == 'D' || typeCh == 'J') {
            /* category 2 primitives not allowed */
            dvmThrowRuntimeException("bad filled array req");
            GOTO_exceptionThrown();
        } else if (typeCh != 'L' && typeCh != '[' && typeCh != 'I') {
            /* TODO: requires multiple "fill in" loops with different widths */
            ALOGE("non-int primitives not implemented");
            dvmThrowInternalError(
                "filled-new-array not implemented for anything but 'int'");
            GOTO_exceptionThrown();
        }

        newArray = dvmAllocArrayByClass(arrayClass, vsrc1, ALLOC_DONT_TRACK);
        if (newArray == NULL)
            GOTO_exceptionThrown();

        /*
         * Fill in the elements.  It's legal for vsrc1 to be zero.
         */
        contents = (u4*)(void*)newArray->contents;
        if (methodCallRange) {
            for (i = 0; i < vsrc1; i++)
                contents[i] = GET_REGISTER(vdst+i);
        } else {
            assert(vsrc1 <= 5);
            if (vsrc1 == 5) {
                contents[4] = GET_REGISTER(arg5);
                vsrc1--;
            }
            for (i = 0; i < vsrc1; i++) {
                contents[i] = GET_REGISTER(vdst & 0x0f);
                vdst >>= 4;
            }
        }
        if (typeCh == 'L' || typeCh == '[') {
            dvmWriteBarrierArray(newArray, 0, newArray->length);
        }

        retval.l = (Object*)newArray;
    }
    FINISH(3);
GOTO_TARGET_END


GOTO_TARGET(invokeVirtual, bool methodCallRange, bool)
    {
        Method* baseMethod;
        Object* thisPtr;

        EXPORT_PC();

        vsrc1 = INST_AA(inst);      /* AA (count) or BA (count + arg 5) */
        ref = FETCH(1);             /* method ref */
        vdst = FETCH(2);            /* 4 regs -or- first reg */

        /*
         * The object against which we are executing a method is always
         * in the first argument.
         */
        if (methodCallRange) {
            assert(vsrc1 > 0);
            ILOGV("|invoke-virtual-range args=%d @0x%04x {regs=v%d-v%d}",
                vsrc1, ref, vdst, vdst+vsrc1-1);
            thisPtr = (Object*) GET_REGISTER(vdst);
        } else {
            assert((vsrc1>>4) > 0);
            ILOGV("|invoke-virtual args=%d @0x%04x {regs=0x%04x %x}",
                vsrc1 >> 4, ref, vdst, vsrc1 & 0x0f);
            thisPtr = (Object*) GET_REGISTER(vdst & 0x0f);
        }

        if (!checkForNull(thisPtr))
            GOTO_exceptionThrown();

        /*
         * Resolve the method.  This is the correct method for the static
         * type of the object.  We also verify access permissions here.
         */
        baseMethod = dvmDexGetResolvedMethod(methodClassDex, ref);
        if (baseMethod == NULL) {
            baseMethod = dvmResolveMethod(curMethod->clazz, ref,METHOD_VIRTUAL);
            if (baseMethod == NULL) {
                ILOGV("+ unknown method or access denied");
                GOTO_exceptionThrown();
            }
        }

        /*
         * Combine the object we found with the vtable offset in the
         * method.
         */
        assert(baseMethod->methodIndex < thisPtr->clazz->vtableCount);
        methodToCall = thisPtr->clazz->vtable[baseMethod->methodIndex];

#if defined(WITH_JIT) && defined(MTERP_STUB)
        self->methodToCall = methodToCall;
        self->callsiteClass = thisPtr->clazz;
#endif

#if 0
        if (dvmIsAbstractMethod(methodToCall)) {
            /*
             * This can happen if you create two classes, Base and Sub, where
             * Sub is a sub-class of Base.  Declare a protected abstract
             * method foo() in Base, and invoke foo() from a method in Base.
             * Base is an "abstract base class" and is never instantiated
             * directly.  Now, Override foo() in Sub, and use Sub.  This
             * Works fine unless Sub stops providing an implementation of
             * the method.
             */
            dvmThrowAbstractMethodError("abstract method not implemented");
            GOTO_exceptionThrown();
        }
#else
        assert(!dvmIsAbstractMethod(methodToCall) ||
            methodToCall->nativeFunc != NULL);
#endif

        LOGVV("+++ base=%s.%s virtual[%d]=%s.%s",
            baseMethod->clazz->descriptor, baseMethod->name,
            (u4) baseMethod->methodIndex,
            methodToCall->clazz->descriptor, methodToCall->name);
        assert(methodToCall != NULL);

#if 0
        if (vsrc1 != methodToCall->insSize) {
            ALOGW("WRONG METHOD: base=%s.%s virtual[%d]=%s.%s",
                baseMethod->clazz->descriptor, baseMethod->name,
                (u4) baseMethod->methodIndex,
                methodToCall->clazz->descriptor, methodToCall->name);
            //dvmDumpClass(baseMethod->clazz);
            //dvmDumpClass(methodToCall->clazz);
            dvmDumpAllClasses(0);
        }
#endif

        GOTO_invokeMethod(methodCallRange, methodToCall, vsrc1, vdst);
    }
GOTO_TARGET_END

GOTO_TARGET(invokeSuper, bool methodCallRange)
    {
        Method* baseMethod;
        u2 thisReg;

        EXPORT_PC();

        vsrc1 = INST_AA(inst);      /* AA (count) or BA (count + arg 5) */
        ref = FETCH(1);             /* method ref */
        vdst = FETCH(2);            /* 4 regs -or- first reg */

        if (methodCallRange) {
            ILOGV("|invoke-super-range args=%d @0x%04x {regs=v%d-v%d}",
                vsrc1, ref, vdst, vdst+vsrc1-1);
            thisReg = vdst;
        } else {
            ILOGV("|invoke-super args=%d @0x%04x {regs=0x%04x %x}",
                vsrc1 >> 4, ref, vdst, vsrc1 & 0x0f);
            thisReg = vdst & 0x0f;
        }

        /* impossible in well-formed code, but we must check nevertheless */
        if (!checkForNull((Object*) GET_REGISTER(thisReg)))
            GOTO_exceptionThrown();

        /*
         * Resolve the method.  This is the correct method for the static
         * type of the object.  We also verify access permissions here.
         * The first arg to dvmResolveMethod() is just the referring class
         * (used for class loaders and such), so we don't want to pass
         * the superclass into the resolution call.
         */
        baseMethod = dvmDexGetResolvedMethod(methodClassDex, ref);
        if (baseMethod == NULL) {
            baseMethod = dvmResolveMethod(curMethod->clazz, ref,METHOD_VIRTUAL);
            if (baseMethod == NULL) {
                ILOGV("+ unknown method or access denied");
                GOTO_exceptionThrown();
            }
        }

        /*
         * Combine the object we found with the vtable offset in the
         * method's class.
         *
         * We're using the current method's class' superclass, not the
         * superclass of "this".  This is because we might be executing
         * in a method inherited from a superclass, and we want to run
         * in that class' superclass.
         */
        if (baseMethod->methodIndex >= curMethod->clazz->super->vtableCount) {
            /*
             * Method does not exist in the superclass.  Could happen if
             * superclass gets updated.
             */
            dvmThrowNoSuchMethodError(baseMethod->name);
            GOTO_exceptionThrown();
        }
        methodToCall = curMethod->clazz->super->vtable[baseMethod->methodIndex];

#if 0
        if (dvmIsAbstractMethod(methodToCall)) {
            dvmThrowAbstractMethodError("abstract method not implemented");
            GOTO_exceptionThrown();
        }
#else
        assert(!dvmIsAbstractMethod(methodToCall) ||
            methodToCall->nativeFunc != NULL);
#endif
        LOGVV("+++ base=%s.%s super-virtual=%s.%s",
            baseMethod->clazz->descriptor, baseMethod->name,
            methodToCall->clazz->descriptor, methodToCall->name);
        assert(methodToCall != NULL);

        GOTO_invokeMethod(methodCallRange, methodToCall, vsrc1, vdst);
    }
GOTO_TARGET_END

GOTO_TARGET(invokeInterface, bool methodCallRange)
    {
        Object* thisPtr;
        ClassObject* thisClass;

        EXPORT_PC();

        vsrc1 = INST_AA(inst);      /* AA (count) or BA (count + arg 5) */
        ref = FETCH(1);             /* method ref */
        vdst = FETCH(2);            /* 4 regs -or- first reg */

        /*
         * The object against which we are executing a method is always
         * in the first argument.
         */
        if (methodCallRange) {
            assert(vsrc1 > 0);
            ILOGV("|invoke-interface-range args=%d @0x%04x {regs=v%d-v%d}",
                vsrc1, ref, vdst, vdst+vsrc1-1);
            thisPtr = (Object*) GET_REGISTER(vdst);
        } else {
            assert((vsrc1>>4) > 0);
            ILOGV("|invoke-interface args=%d @0x%04x {regs=0x%04x %x}",
                vsrc1 >> 4, ref, vdst, vsrc1 & 0x0f);
            thisPtr = (Object*) GET_REGISTER(vdst & 0x0f);
        }

        if (!checkForNull(thisPtr))
            GOTO_exceptionThrown();

        thisClass = thisPtr->clazz;

        /*
         * Given a class and a method index, find the Method* with the
         * actual code we want to execute.
         */
        methodToCall = dvmFindInterfaceMethodInCache(thisClass, ref, curMethod,
                        methodClassDex);
#if defined(WITH_JIT) && defined(MTERP_STUB)
        self->callsiteClass = thisClass;
        self->methodToCall = methodToCall;
#endif
        if (methodToCall == NULL) {
            assert(dvmCheckException(self));
            GOTO_exceptionThrown();
        }

        GOTO_invokeMethod(methodCallRange, methodToCall, vsrc1, vdst);
    }
GOTO_TARGET_END

GOTO_TARGET(invokeDirect, bool methodCallRange)
    {
        u2 thisReg;

        EXPORT_PC();

        vsrc1 = INST_AA(inst);      /* AA (count) or BA (count + arg 5) */
        ref = FETCH(1);             /* method ref */
        vdst = FETCH(2);            /* 4 regs -or- first reg */

        if (methodCallRange) {
            ILOGV("|invoke-direct-range args=%d @0x%04x {regs=v%d-v%d}",
                vsrc1, ref, vdst, vdst+vsrc1-1);
            thisReg = vdst;
        } else {
            ILOGV("|invoke-direct args=%d @0x%04x {regs=0x%04x %x}",
                vsrc1 >> 4, ref, vdst, vsrc1 & 0x0f);
            thisReg = vdst & 0x0f;
        }

        if (!checkForNull((Object*) GET_REGISTER(thisReg)))
            GOTO_exceptionThrown();

        methodToCall = dvmDexGetResolvedMethod(methodClassDex, ref);
        if (methodToCall == NULL) {
            methodToCall = dvmResolveMethod(curMethod->clazz, ref,
                            METHOD_DIRECT);
            if (methodToCall == NULL) {
                ILOGV("+ unknown direct method");     // should be impossible
                GOTO_exceptionThrown();
            }
        }
        GOTO_invokeMethod(methodCallRange, methodToCall, vsrc1, vdst);
    }
GOTO_TARGET_END

GOTO_TARGET(invokeStatic, bool methodCallRange)
    EXPORT_PC();

    vsrc1 = INST_AA(inst);      /* AA (count) or BA (count + arg 5) */
    ref = FETCH(1);             /* method ref */
    vdst = FETCH(2);            /* 4 regs -or- first reg */

    if (methodCallRange)
        ILOGV("|invoke-static-range args=%d @0x%04x {regs=v%d-v%d}",
            vsrc1, ref, vdst, vdst+vsrc1-1);
    else
        ILOGV("|invoke-static args=%d @0x%04x {regs=0x%04x %x}",
            vsrc1 >> 4, ref, vdst, vsrc1 & 0x0f);

    methodToCall = dvmDexGetResolvedMethod(methodClassDex, ref);
    if (methodToCall == NULL) {
        methodToCall = dvmResolveMethod(curMethod->clazz, ref, METHOD_STATIC);
        if (methodToCall == NULL) {
            ILOGV("+ unknown method");
            GOTO_exceptionThrown();
        }

#if defined(WITH_JIT) && defined(MTERP_STUB)
        /*
         * The JIT needs dvmDexGetResolvedMethod() to return non-null.
         * Include the check if this code is being used as a stub
         * called from the assembly interpreter.
         */
        if ((self->interpBreak.ctl.subMode & kSubModeJitTraceBuild) &&
            (dvmDexGetResolvedMethod(methodClassDex, ref) == NULL)) {
            /* Class initialization is still ongoing */
            dvmJitEndTraceSelect(self,pc);
        }
#endif
    }
    GOTO_invokeMethod(methodCallRange, methodToCall, vsrc1, vdst);
GOTO_TARGET_END

GOTO_TARGET(invokeVirtualQuick, bool methodCallRange)
    {
        Object* thisPtr;

        EXPORT_PC();

        vsrc1 = INST_AA(inst);      /* AA (count) or BA (count + arg 5) */
        ref = FETCH(1);             /* vtable index */
        vdst = FETCH(2);            /* 4 regs -or- first reg */

        /*
         * The object against which we are executing a method is always
         * in the first argument.
         */
        if (methodCallRange) {
            assert(vsrc1 > 0);
            ILOGV("|invoke-virtual-quick-range args=%d @0x%04x {regs=v%d-v%d}",
                vsrc1, ref, vdst, vdst+vsrc1-1);
            thisPtr = (Object*) GET_REGISTER(vdst);
        } else {
            assert((vsrc1>>4) > 0);
            ILOGV("|invoke-virtual-quick args=%d @0x%04x {regs=0x%04x %x}",
                vsrc1 >> 4, ref, vdst, vsrc1 & 0x0f);
            thisPtr = (Object*) GET_REGISTER(vdst & 0x0f);
        }

        if (!checkForNull(thisPtr))
            GOTO_exceptionThrown();


        /*
         * Combine the object we found with the vtable offset in the
         * method.
         */
        assert(ref < (unsigned int) thisPtr->clazz->vtableCount);
        methodToCall = thisPtr->clazz->vtable[ref];
#if defined(WITH_JIT) && defined(MTERP_STUB)
        self->callsiteClass = thisPtr->clazz;
        self->methodToCall = methodToCall;
#endif

#if 0
        if (dvmIsAbstractMethod(methodToCall)) {
            dvmThrowAbstractMethodError("abstract method not implemented");
            GOTO_exceptionThrown();
        }
#else
        assert(!dvmIsAbstractMethod(methodToCall) ||
            methodToCall->nativeFunc != NULL);
#endif

        LOGVV("+++ virtual[%d]=%s.%s",
            ref, methodToCall->clazz->descriptor, methodToCall->name);
        assert(methodToCall != NULL);

        GOTO_invokeMethod(methodCallRange, methodToCall, vsrc1, vdst);
    }
GOTO_TARGET_END

GOTO_TARGET(invokeSuperQuick, bool methodCallRange)
    {
        u2 thisReg;

        EXPORT_PC();

        vsrc1 = INST_AA(inst);      /* AA (count) or BA (count + arg 5) */
        ref = FETCH(1);             /* vtable index */
        vdst = FETCH(2);            /* 4 regs -or- first reg */

        if (methodCallRange) {
            ILOGV("|invoke-super-quick-range args=%d @0x%04x {regs=v%d-v%d}",
                vsrc1, ref, vdst, vdst+vsrc1-1);
            thisReg = vdst;
        } else {
            ILOGV("|invoke-super-quick args=%d @0x%04x {regs=0x%04x %x}",
                vsrc1 >> 4, ref, vdst, vsrc1 & 0x0f);
            thisReg = vdst & 0x0f;
        }
        /* impossible in well-formed code, but we must check nevertheless */
        if (!checkForNull((Object*) GET_REGISTER(thisReg)))
            GOTO_exceptionThrown();

#if 0   /* impossible in optimized + verified code */
        if (ref >= curMethod->clazz->super->vtableCount) {
            dvmThrowNoSuchMethodError(NULL);
            GOTO_exceptionThrown();
        }
#else
        assert(ref < (unsigned int) curMethod->clazz->super->vtableCount);
#endif

        /*
         * Combine the object we found with the vtable offset in the
         * method's class.
         *
         * We're using the current method's class' superclass, not the
         * superclass of "this".  This is because we might be executing
         * in a method inherited from a superclass, and we want to run
         * in the method's class' superclass.
         */
        methodToCall = curMethod->clazz->super->vtable[ref];

#if 0
        if (dvmIsAbstractMethod(methodToCall)) {
            dvmThrowAbstractMethodError("abstract method not implemented");
            GOTO_exceptionThrown();
        }
#else
        assert(!dvmIsAbstractMethod(methodToCall) ||
            methodToCall->nativeFunc != NULL);
#endif
        LOGVV("+++ super-virtual[%d]=%s.%s",
            ref, methodToCall->clazz->descriptor, methodToCall->name);
        assert(methodToCall != NULL);
        GOTO_invokeMethod(methodCallRange, methodToCall, vsrc1, vdst);
    }
GOTO_TARGET_END


    /*
     * General handling for return-void, return, and return-wide.  Put the
     * return value in "retval" before jumping here.
     */
GOTO_TARGET(returnFromMethod)
    {
        StackSaveArea* saveArea;

        /*
         * We must do this BEFORE we pop the previous stack frame off, so
         * that the GC can see the return value (if any) in the local vars.
         *
         * Since this is now an interpreter switch point, we must do it before
         * we do anything at all.
         */
        PERIODIC_CHECKS(0);

        ILOGV("> retval=0x%llx (leaving %s.%s %s)",
            retval.j, curMethod->clazz->descriptor, curMethod->name,
            curMethod->shorty);
        //DUMP_REGS(curMethod, fp);

        saveArea = SAVEAREA_FROM_FP(fp);

#ifdef EASY_GDB
        debugSaveArea = saveArea;
#endif

        /* back up to previous frame and see if we hit a break */
        fp = (u4*)saveArea->prevFrame;
        assert(fp != NULL);

        /* Handle any special subMode requirements */
        if (self->interpBreak.ctl.subMode != 0) {
            PC_FP_TO_SELF();
            dvmReportReturn(self);
        }

        if (dvmIsBreakFrame(fp)) {
            /* bail without popping the method frame from stack */
            LOGVV("+++ returned into break frame");
            GOTO_bail();
        }

        /* update thread FP, and reset local variables */
        self->interpSave.curFrame = fp;
        curMethod = SAVEAREA_FROM_FP(fp)->method;
        self->interpSave.method = curMethod;
        //methodClass = curMethod->clazz;
        methodClassDex = curMethod->clazz->pDvmDex;
        pc = saveArea->savedPc;
        ILOGD("> (return to %s.%s %s)", curMethod->clazz->descriptor,
            curMethod->name, curMethod->shorty);

        /* use FINISH on the caller's invoke instruction */
        //u2 invokeInstr = INST_INST(FETCH(0));
        if (true /*invokeInstr >= OP_INVOKE_VIRTUAL &&
            invokeInstr <= OP_INVOKE_INTERFACE*/)
        {
            FINISH(3);
        } else {
            //ALOGE("Unknown invoke instr %02x at %d",
            //    invokeInstr, (int) (pc - curMethod->insns));
            assert(false);
        }
    }
GOTO_TARGET_END


    /*
     * Jump here when the code throws an exception.
     *
     * By the time we get here, the Throwable has been created and the stack
     * trace has been saved off.
     */
GOTO_TARGET(exceptionThrown)
    {
        Object* exception;
        int catchRelPc;

        PERIODIC_CHECKS(0);

        /*
         * We save off the exception and clear the exception status.  While
         * processing the exception we might need to load some Throwable
         * classes, and we don't want class loader exceptions to get
         * confused with this one.
         */
        assert(dvmCheckException(self));
        exception = dvmGetException(self);
        dvmAddTrackedAlloc(exception, self);
        dvmClearException(self);

        ALOGV("Handling exception %s at %s:%d",
            exception->clazz->descriptor, curMethod->name,
            dvmLineNumFromPC(curMethod, pc - curMethod->insns));

        /*
         * Report the exception throw to any "subMode" watchers.
         *
         * TODO: if the exception was thrown by interpreted code, control
         * fell through native, and then back to us, we will report the
         * exception at the point of the throw and again here.  We can avoid
         * this by not reporting exceptions when we jump here directly from
         * the native call code above, but then we won't report exceptions
         * that were thrown *from* the JNI code (as opposed to *through* it).
         *
         * The correct solution is probably to ignore from-native exceptions
         * here, and have the JNI exception code do the reporting to the
         * debugger.
         */
        if (self->interpBreak.ctl.subMode != 0) {
            PC_FP_TO_SELF();
            dvmReportExceptionThrow(self, exception);
        }

        /*
         * We need to unroll to the catch block or the nearest "break"
         * frame.
         *
         * A break frame could indicate that we have reached an intermediate
         * native call, or have gone off the top of the stack and the thread
         * needs to exit.  Either way, we return from here, leaving the
         * exception raised.
         *
         * If we do find a catch block, we want to transfer execution to
         * that point.
         *
         * Note this can cause an exception while resolving classes in
         * the "catch" blocks.
         */
        catchRelPc = dvmFindCatchBlock(self, pc - curMethod->insns,
                    exception, false, (void**)(void*)&fp);

        /*
         * Restore the stack bounds after an overflow.  This isn't going to
         * be correct in all circumstances, e.g. if JNI code devours the
         * exception this won't happen until some other exception gets
         * thrown.  If the code keeps pushing the stack bounds we'll end
         * up aborting the VM.
         *
         * Note we want to do this *after* the call to dvmFindCatchBlock,
         * because that may need extra stack space to resolve exception
         * classes (e.g. through a class loader).
         *
         * It's possible for the stack overflow handling to cause an
         * exception (specifically, class resolution in a "catch" block
         * during the call above), so we could see the thread's overflow
         * flag raised but actually be running in a "nested" interpreter
         * frame.  We don't allow doubled-up StackOverflowErrors, so
         * we can check for this by just looking at the exception type
         * in the cleanup function.  Also, we won't unroll past the SOE
         * point because the more-recent exception will hit a break frame
         * as it unrolls to here.
         */
        if (self->stackOverflowed)
            dvmCleanupStackOverflow(self, exception);

        if (catchRelPc < 0) {
            /* falling through to JNI code or off the bottom of the stack */
#if DVM_SHOW_EXCEPTION >= 2
            ALOGD("Exception %s from %s:%d not caught locally",
                exception->clazz->descriptor, dvmGetMethodSourceFile(curMethod),
                dvmLineNumFromPC(curMethod, pc - curMethod->insns));
#endif
            dvmSetException(self, exception);
            dvmReleaseTrackedAlloc(exception, self);
            GOTO_bail();
        }

#if DVM_SHOW_EXCEPTION >= 3
        {
            const Method* catchMethod = SAVEAREA_FROM_FP(fp)->method;
            ALOGD("Exception %s thrown from %s:%d to %s:%d",
                exception->clazz->descriptor, dvmGetMethodSourceFile(curMethod),
                dvmLineNumFromPC(curMethod, pc - curMethod->insns),
                dvmGetMethodSourceFile(catchMethod),
                dvmLineNumFromPC(catchMethod, catchRelPc));
        }
#endif

        /*
         * Adjust local variables to match self->interpSave.curFrame and the
         * updated PC.
         */
        //fp = (u4*) self->interpSave.curFrame;
        curMethod = SAVEAREA_FROM_FP(fp)->method;
        self->interpSave.method = curMethod;
        //methodClass = curMethod->clazz;
        methodClassDex = curMethod->clazz->pDvmDex;
        pc = curMethod->insns + catchRelPc;
        ILOGV("> pc <-- %s.%s %s", curMethod->clazz->descriptor,
            curMethod->name, curMethod->shorty);
        DUMP_REGS(curMethod, fp, false);            // show all regs

        /*
         * Restore the exception if the handler wants it.
         *
         * The Dalvik spec mandates that, if an exception handler wants to
         * do something with the exception, the first instruction executed
         * must be "move-exception".  We can pass the exception along
         * through the thread struct, and let the move-exception instruction
         * clear it for us.
         *
         * If the handler doesn't call move-exception, we don't want to
         * finish here with an exception still pending.
         */
        if (INST_INST(FETCH(0)) == OP_MOVE_EXCEPTION)
            dvmSetException(self, exception);

        dvmReleaseTrackedAlloc(exception, self);
        FINISH(0);
    }
GOTO_TARGET_END



    /*
     * General handling for invoke-{virtual,super,direct,static,interface},
     * including "quick" variants.
     *
     * Set "methodToCall" to the Method we're calling, and "methodCallRange"
     * depending on whether this is a "/range" instruction.
     *
     * For a range call:
     *  "vsrc1" holds the argument count (8 bits)
     *  "vdst" holds the first argument in the range
     * For a non-range call:
     *  "vsrc1" holds the argument count (4 bits) and the 5th argument index
     *  "vdst" holds four 4-bit register indices
     *
     * The caller must EXPORT_PC before jumping here, because any method
     * call can throw a stack overflow exception.
     */
GOTO_TARGET(invokeMethod, bool methodCallRange, const Method* _methodToCall,
    u2 count, u2 regs)
    {
        STUB_HACK(vsrc1 = count; vdst = regs; methodToCall = _methodToCall;);

        //printf("range=%d call=%p count=%d regs=0x%04x\n",
        //    methodCallRange, methodToCall, count, regs);
        //printf(" --> %s.%s %s\n", methodToCall->clazz->descriptor,
        //    methodToCall->name, methodToCall->shorty);

        u4* outs;
        int i;

        /*
         * Copy args.  This may corrupt vsrc1/vdst.
         */
        if (methodCallRange) {
            // could use memcpy or a "Duff's device"; most functions have
            // so few args it won't matter much
            assert(vsrc1 <= curMethod->outsSize);
            assert(vsrc1 == methodToCall->insSize);
            outs = OUTS_FROM_FP(fp, vsrc1);
            for (i = 0; i < vsrc1; i++)
                outs[i] = GET_REGISTER(vdst+i);
        } else {
            u4 count = vsrc1 >> 4;

            assert(count <= curMethod->outsSize);
            assert(count == methodToCall->insSize);
            assert(count <= 5);

            outs = OUTS_FROM_FP(fp, count);
#if 0
            if (count == 5) {
                outs[4] = GET_REGISTER(vsrc1 & 0x0f);
                count--;
            }
            for (i = 0; i < (int) count; i++) {
                outs[i] = GET_REGISTER(vdst & 0x0f);
                vdst >>= 4;
            }
#else
            // This version executes fewer instructions but is larger
            // overall.  Seems to be a teensy bit faster.
            assert((vdst >> 16) == 0);  // 16 bits -or- high 16 bits clear
            switch (count) {
            case 5:
                outs[4] = GET_REGISTER(vsrc1 & 0x0f);
            case 4:
                outs[3] = GET_REGISTER(vdst >> 12);
            case 3:
                outs[2] = GET_REGISTER((vdst & 0x0f00) >> 8);
            case 2:
                outs[1] = GET_REGISTER((vdst & 0x00f0) >> 4);
            case 1:
                outs[0] = GET_REGISTER(vdst & 0x0f);
            default:
                ;
            }
#endif
        }
    }

    /*
     * (This was originally a "goto" target; I've kept it separate from the
     * stuff above in case we want to refactor things again.)
     *
     * At this point, we have the arguments stored in the "outs" area of
     * the current method's stack frame, and the method to call in
     * "methodToCall".  Push a new stack frame.
     */
    {
        StackSaveArea* newSaveArea;
        u4* newFp;

        ILOGV("> %s%s.%s %s",
            dvmIsNativeMethod(methodToCall) ? "(NATIVE) " : "",
            methodToCall->clazz->descriptor, methodToCall->name,
            methodToCall->shorty);

        newFp = (u4*) SAVEAREA_FROM_FP(fp) - methodToCall->registersSize;
        newSaveArea = SAVEAREA_FROM_FP(newFp);

        /* verify that we have enough space */
        if (true) {
            u1* bottom;
            bottom = (u1*) newSaveArea - methodToCall->outsSize * sizeof(u4);
            if (bottom < self->interpStackEnd) {
                /* stack overflow */
                ALOGV("Stack overflow on method call (start=%p end=%p newBot=%p(%d) size=%d '%s')",
                    self->interpStackStart, self->interpStackEnd, bottom,
                    (u1*) fp - bottom, self->interpStackSize,
                    methodToCall->name);
                dvmHandleStackOverflow(self, methodToCall);
                assert(dvmCheckException(self));
                GOTO_exceptionThrown();
            }
            //ALOGD("+++ fp=%p newFp=%p newSave=%p bottom=%p",
            //    fp, newFp, newSaveArea, bottom);
        }

#ifdef LOG_INSTR
        if (methodToCall->registersSize > methodToCall->insSize) {
            /*
             * This makes valgrind quiet when we print registers that
             * haven't been initialized.  Turn it off when the debug
             * messages are disabled -- we want valgrind to report any
             * used-before-initialized issues.
             */
            memset(newFp, 0xcc,
                (methodToCall->registersSize - methodToCall->insSize) * 4);
        }
#endif

#ifdef EASY_GDB
        newSaveArea->prevSave = SAVEAREA_FROM_FP(fp);
#endif
        newSaveArea->prevFrame = fp;
        newSaveArea->savedPc = pc;
#if defined(WITH_JIT) && defined(MTERP_STUB)
        newSaveArea->returnAddr = 0;
#endif
        newSaveArea->method = methodToCall;

        if (self->interpBreak.ctl.subMode != 0) {
            /*
             * We mark ENTER here for both native and non-native
             * calls.  For native calls, we'll mark EXIT on return.
             * For non-native calls, EXIT is marked in the RETURN op.
             */
            PC_TO_SELF();
            dvmReportInvoke(self, methodToCall);
        }

        if (!dvmIsNativeMethod(methodToCall)) {
            /*
             * "Call" interpreted code.  Reposition the PC, update the
             * frame pointer and other local state, and continue.
             */
            curMethod = methodToCall;
            self->interpSave.method = curMethod;
            methodClassDex = curMethod->clazz->pDvmDex;
            pc = methodToCall->insns;
            fp = newFp;
            self->interpSave.curFrame = fp;
#ifdef EASY_GDB
            debugSaveArea = SAVEAREA_FROM_FP(newFp);
#endif
            self->debugIsMethodEntry = true;        // profiling, debugging
            ILOGD("> pc <-- %s.%s %s", curMethod->clazz->descriptor,
                curMethod->name, curMethod->shorty);
            DUMP_REGS(curMethod, fp, true);         // show input args
            FINISH(0);                              // jump to method start
        } else {
            /* set this up for JNI locals, even if not a JNI native */
            newSaveArea->xtra.localRefCookie = self->jniLocalRefTable.segmentState.all;

            self->interpSave.curFrame = newFp;

            DUMP_REGS(methodToCall, newFp, true);   // show input args

            if (self->interpBreak.ctl.subMode != 0) {
                dvmReportPreNativeInvoke(methodToCall, self, newSaveArea->prevFrame);
            }

            ILOGD("> native <-- %s.%s %s", methodToCall->clazz->descriptor,
                  methodToCall->name, methodToCall->shorty);

            /*
             * Jump through native call bridge.  Because we leave no
             * space for locals on native calls, "newFp" points directly
             * to the method arguments.
             */
            (*methodToCall->nativeFunc)(newFp, &retval, methodToCall, self);

            if (self->interpBreak.ctl.subMode != 0) {
                dvmReportPostNativeInvoke(methodToCall, self, newSaveArea->prevFrame);
            }

            /* pop frame off */
            dvmPopJniLocals(self, newSaveArea);
            self->interpSave.curFrame = newSaveArea->prevFrame;
            fp = newSaveArea->prevFrame;

            /*
             * If the native code threw an exception, or interpreted code
             * invoked by the native call threw one and nobody has cleared
             * it, jump to our local exception handling.
             */
            if (dvmCheckException(self)) {
                ALOGV("Exception thrown by/below native code");
                GOTO_exceptionThrown();
            }

            ILOGD("> retval=0x%llx (leaving native)", retval.j);
            ILOGD("> (return from native %s.%s to %s.%s %s)",
                methodToCall->clazz->descriptor, methodToCall->name,
                curMethod->clazz->descriptor, curMethod->name,
                curMethod->shorty);

            //u2 invokeInstr = INST_INST(FETCH(0));
            if (true /*invokeInstr >= OP_INVOKE_VIRTUAL &&
                invokeInstr <= OP_INVOKE_INTERFACE*/)
            {
                FINISH(3);
            } else {
                //ALOGE("Unknown invoke instr %02x at %d",
                //    invokeInstr, (int) (pc - curMethod->insns));
                assert(false);
            }
        }
    }
    assert(false);      // should not get here
GOTO_TARGET_END

/* File: cstubs/enddefs.cpp */

/* undefine "magic" name remapping */
#undef retval
#undef pc
#undef fp
#undef curMethod
#undef methodClassDex
#undef self
#undef debugTrackedRefStart

