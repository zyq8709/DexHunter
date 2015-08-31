/*
 * This file was generated automatically by gen-mterp.py for 'mips'.
 *
 * --> DO NOT EDIT <--
 */

/* File: mips/header.S */
#include "../common/asm-constants.h"
#include "../common/mips-defines.h"
#include <asm/regdef.h>
#include <asm/fpregdef.h>

#ifdef __mips_hard_float
#define HARD_FLOAT
#else
#define SOFT_FLOAT
#endif

#if (__mips==32) && (__mips_isa_rev>=2)
#define MIPS32R2
#endif

/* MIPS definitions and declarations

   reg	nick		purpose
   s0	rPC		interpreted program counter, used for fetching instructions
   s1	rFP		interpreted frame pointer, used for accessing locals and args
   s2	rSELF		self (Thread) pointer
   s3	rIBASE		interpreted instruction base pointer, used for computed goto
   s4	rINST		first 16-bit code unit of current instruction
*/


/* single-purpose registers, given names for clarity */
#define rPC s0
#define rFP s1
#define rSELF s2
#define rIBASE s3
#define rINST s4
#define rOBJ s5
#define rBIX s6
#define rTEMP s7

/* The long arguments sent to function calls in Big-endian mode should be register
swapped when sent to functions in little endian mode. In other words long variable
sent as a0(MSW), a1(LSW) for a function call in LE mode should be sent as a1, a0 in
Big Endian mode */

#ifdef HAVE_LITTLE_ENDIAN
#define rARG0 a0
#define rARG1 a1
#define rARG2 a2
#define rARG3 a3
#define rRESULT0 v0
#define rRESULT1 v1
#else
#define rARG0 a1
#define rARG1 a0
#define rARG2 a3
#define rARG3 a2
#define rRESULT0 v1
#define rRESULT1 v0
#endif


/* save/restore the PC and/or FP from the glue struct */
#define LOAD_PC_FROM_SELF() lw rPC, offThread_pc(rSELF)
#define SAVE_PC_TO_SELF() sw rPC, offThread_pc(rSELF)
#define LOAD_FP_FROM_SELF() lw rFP, offThread_curFrame(rSELF)
#define SAVE_FP_TO_SELF() sw rFP, offThread_curFrame(rSELF)
#define LOAD_PC_FP_FROM_SELF() \
	LOAD_PC_FROM_SELF();   \
	LOAD_FP_FROM_SELF()
#define SAVE_PC_FP_TO_SELF()   \
	SAVE_PC_TO_SELF();     \
	SAVE_FP_TO_SELF()

#define EXPORT_PC() \
    sw        rPC, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)

#define SAVEAREA_FROM_FP(rd, _fpreg) \
    subu      rd, _fpreg, sizeofStackSaveArea

#define FETCH_INST() lhu rINST, (rPC)

#define FETCH_ADVANCE_INST(_count) lhu rINST, ((_count)*2)(rPC); \
    addu      rPC, rPC, ((_count) * 2)

#define PREFETCH_ADVANCE_INST(_dreg, _sreg, _count) \
    lhu       _dreg, ((_count)*2)(_sreg) ;            \
    addu      _sreg, _sreg, (_count)*2

#define FETCH_ADVANCE_INST_RB(rd) addu rPC, rPC, rd; \
    lhu       rINST, (rPC)

#define FETCH(rd, _count) lhu rd, ((_count) * 2)(rPC)
#define FETCH_S(rd, _count) lh rd, ((_count) * 2)(rPC)

#ifdef HAVE_LITTLE_ENDIAN

#define FETCH_B(rd, _count) lbu rd, ((_count) * 2)(rPC)
#define FETCH_C(rd, _count) lbu rd, ((_count) * 2 + 1)(rPC)

#else

#define FETCH_B(rd, _count) lbu rd, ((_count) * 2 + 1)(rPC)
#define FETCH_C(rd, _count) lbu rd, ((_count) * 2)(rPC)

#endif

#define GET_INST_OPCODE(rd) and rd, rINST, 0xFF

/*
 * Put the prefetched instruction's opcode field into the specified register.
 */

#define GET_PREFETCHED_OPCODE(dreg, sreg)   andi     dreg, sreg, 255

#define GOTO_OPCODE(rd) sll rd, rd, 7; \
    addu      rd, rIBASE, rd; \
    jr        rd

#define GOTO_OPCODE_BASE(_base, rd)  sll rd, rd, 7; \
    addu      rd, _base, rd; \
    jr        rd

#define GET_VREG(rd, rix) LOAD_eas2(rd, rFP, rix)

#define GET_VREG_F(rd, rix) EAS2(AT, rFP, rix); \
    .set noat; l.s rd, (AT); .set at

#define SET_VREG(rd, rix) STORE_eas2(rd, rFP, rix)

#define SET_VREG_GOTO(rd, rix, dst) .set noreorder; \
    sll       dst, dst, 7; \
    addu      dst, rIBASE, dst; \
    sll       t8, rix, 2; \
    addu      t8, t8, rFP; \
    jr        dst; \
    sw        rd, 0(t8); \
    .set reorder

#define SET_VREG_F(rd, rix) EAS2(AT, rFP, rix); \
    .set noat; s.s rd, (AT); .set at


#define GET_OPA(rd) srl rd, rINST, 8
#ifndef MIPS32R2
#define GET_OPA4(rd) GET_OPA(rd); and rd, 0xf
#else
#define GET_OPA4(rd) ext rd, rINST, 8, 4
#endif
#define GET_OPB(rd) srl rd, rINST, 12

#define LOAD_rSELF_OFF(rd, off) lw rd, offThread_##off## (rSELF)

#define LOAD_rSELF_method(rd) LOAD_rSELF_OFF(rd, method)
#define LOAD_rSELF_methodClassDex(rd) LOAD_rSELF_OFF(rd, methodClassDex)
#define LOAD_rSELF_interpStackEnd(rd) LOAD_rSELF_OFF(rd, interpStackEnd)
#define LOAD_rSELF_retval(rd) LOAD_rSELF_OFF(rd, retval)
#define LOAD_rSELF_pActiveProfilers(rd) LOAD_rSELF_OFF(rd, pActiveProfilers)
#define LOAD_rSELF_bailPtr(rd) LOAD_rSELF_OFF(rd, bailPtr)
#define LOAD_rSELF_SelfSuspendCount(rd) LOAD_rSELF_OFF(rd, SelfSuspendCount)


/*
 * Form an Effective Address rd = rbase + roff<<n;
 * Uses reg AT
 */
#define EASN(rd, rbase, roff, rshift) .set noat; \
    sll       AT, roff, rshift; \
    addu      rd, rbase, AT; \
    .set at

#define EAS1(rd, rbase, roff) EASN(rd, rbase, roff, 1)
#define EAS2(rd, rbase, roff) EASN(rd, rbase, roff, 2)
#define EAS3(rd, rbase, roff) EASN(rd, rbase, roff, 3)
#define EAS4(rd, rbase, roff) EASN(rd, rbase, roff, 4)

/*
 * Form an Effective Shift Right rd = rbase + roff>>n;
 * Uses reg AT
 */
#define ESRN(rd, rbase, roff, rshift) .set noat; \
    srl       AT, roff, rshift; \
    addu      rd, rbase, AT; \
    .set at

#define LOAD_eas2(rd, rbase, roff) EAS2(AT, rbase, roff); \
    .set noat; lw rd, 0(AT); .set at

#define STORE_eas2(rd, rbase, roff) EAS2(AT, rbase, roff); \
    .set noat; sw rd, 0(AT); .set at

#define LOAD_RB_OFF(rd, rbase, off) lw rd, off(rbase)
#define LOADu2_RB_OFF(rd, rbase, off) lhu rd, off(rbase)
#define STORE_RB_OFF(rd, rbase, off) sw rd, off(rbase)

#ifdef HAVE_LITTLE_ENDIAN

#define STORE64_off(rlo, rhi, rbase, off) sw rlo, off(rbase); \
    sw        rhi, (off+4)(rbase)
#define LOAD64_off(rlo, rhi, rbase, off) lw rlo, off(rbase); \
    lw        rhi, (off+4)(rbase)

#define vSTORE64_off(rlo, rhi, rbase, off) sw rlo, off(rbase); \
    sw        rhi, (off+4)(rbase)
#define vLOAD64_off(rlo, rhi, rbase, off) lw rlo, off(rbase); \
    lw        rhi, (off+4)(rbase)

#define STORE64_off_F(rlo, rhi, rbase, off) s.s rlo, off(rbase); \
    s.s       rhi, (off+4)(rbase)
#define LOAD64_off_F(rlo, rhi, rbase, off) l.s rlo, off(rbase); \
    l.s       rhi, (off+4)(rbase)
#else

#define STORE64_off(rlo, rhi, rbase, off) sw rlo, (off+4)(rbase); \
    sw        rhi, (off)(rbase)
#define LOAD64_off(rlo, rhi, rbase, off) lw rlo, (off+4)(rbase); \
    lw        rhi, (off)(rbase)
#define vSTORE64_off(rlo, rhi, rbase, off) sw rlo, (off+4)(rbase); \
    sw        rhi, (off)(rbase)
#define vLOAD64_off(rlo, rhi, rbase, off) lw rlo, (off+4)(rbase); \
    lw        rhi, (off)(rbase)
#define STORE64_off_F(rlo, rhi, rbase, off) s.s rlo, (off+4)(rbase); \
    s.s       rhi, (off)(rbase)
#define LOAD64_off_F(rlo, rhi, rbase, off) l.s rlo, (off+4)(rbase); \
    l.s       rhi, (off)(rbase)
#endif

#define STORE64(rlo, rhi, rbase) STORE64_off(rlo, rhi, rbase, 0)
#define LOAD64(rlo, rhi, rbase) LOAD64_off(rlo, rhi, rbase, 0)

#define vSTORE64(rlo, rhi, rbase) vSTORE64_off(rlo, rhi, rbase, 0)
#define vLOAD64(rlo, rhi, rbase) vLOAD64_off(rlo, rhi, rbase, 0)

#define STORE64_F(rlo, rhi, rbase) STORE64_off_F(rlo, rhi, rbase, 0)
#define LOAD64_F(rlo, rhi, rbase) LOAD64_off_F(rlo, rhi, rbase, 0)

#define STORE64_lo(rd, rbase) sw rd, 0(rbase)
#define STORE64_hi(rd, rbase) sw rd, 4(rbase)


#define LOAD_offThread_exception(rd, rbase) LOAD_RB_OFF(rd, rbase, offThread_exception)
#define LOAD_base_offArrayObject_length(rd, rbase) LOAD_RB_OFF(rd, rbase, offArrayObject_length)
#define LOAD_base_offClassObject_accessFlags(rd, rbase) LOAD_RB_OFF(rd, rbase, offClassObject_accessFlags)
#define LOAD_base_offClassObject_descriptor(rd, rbase) LOAD_RB_OFF(rd, rbase, offClassObject_descriptor)
#define LOAD_base_offClassObject_super(rd, rbase) LOAD_RB_OFF(rd, rbase, offClassObject_super)

#define LOAD_base_offClassObject_vtable(rd, rbase) LOAD_RB_OFF(rd, rbase, offClassObject_vtable)
#define LOAD_base_offClassObject_vtableCount(rd, rbase) LOAD_RB_OFF(rd, rbase, offClassObject_vtableCount)
#define LOAD_base_offDvmDex_pResClasses(rd, rbase) LOAD_RB_OFF(rd, rbase, offDvmDex_pResClasses)
#define LOAD_base_offDvmDex_pResFields(rd, rbase) LOAD_RB_OFF(rd, rbase, offDvmDex_pResFields)

#define LOAD_base_offDvmDex_pResMethods(rd, rbase) LOAD_RB_OFF(rd, rbase, offDvmDex_pResMethods)
#define LOAD_base_offDvmDex_pResStrings(rd, rbase) LOAD_RB_OFF(rd, rbase, offDvmDex_pResStrings)
#define LOAD_base_offInstField_byteOffset(rd, rbase) LOAD_RB_OFF(rd, rbase, offInstField_byteOffset)
#define LOAD_base_offStaticField_value(rd, rbase) LOAD_RB_OFF(rd, rbase, offStaticField_value)
#define LOAD_base_offMethod_clazz(rd, rbase) LOAD_RB_OFF(rd, rbase, offMethod_clazz)

#define LOAD_base_offMethod_name(rd, rbase) LOAD_RB_OFF(rd, rbase, offMethod_name)
#define LOAD_base_offObject_clazz(rd, rbase) LOAD_RB_OFF(rd, rbase, offObject_clazz)

#define LOADu2_offMethod_methodIndex(rd, rbase) LOADu2_RB_OFF(rd, rbase, offMethod_methodIndex)


#define STORE_offThread_exception(rd, rbase) STORE_RB_OFF(rd, rbase, offThread_exception)


#define STACK_STORE(rd, off) sw rd, off(sp)
#define STACK_LOAD(rd, off) lw rd, off(sp)
#define CREATE_STACK(n) subu sp, sp, n
#define DELETE_STACK(n) addu sp, sp, n

#define SAVE_RA(offset) STACK_STORE(ra, offset)
#define LOAD_RA(offset) STACK_LOAD(ra, offset)

#define LOAD_ADDR(dest, addr) la dest, addr
#define LOAD_IMM(dest, imm) li dest, imm
#define MOVE_REG(dest, src) move dest, src
#define RETURN jr ra
#define STACK_SIZE 128

#define STACK_OFFSET_ARG04 16
#define STACK_OFFSET_ARG05 20
#define STACK_OFFSET_ARG06 24
#define STACK_OFFSET_ARG07 28
#define STACK_OFFSET_SCR   32
#define STACK_OFFSET_SCRMX 80
#define STACK_OFFSET_GP    84
#define STACK_OFFSET_rFP   112

#define JAL(n) jal n
#define BAL(n) bal n

#define STACK_STORE_RA() CREATE_STACK(STACK_SIZE); \
    STACK_STORE(gp, STACK_OFFSET_GP); \
    STACK_STORE(ra, 124)

#define STACK_STORE_S0() STACK_STORE_RA(); \
    STACK_STORE(s0, 116)

#define STACK_STORE_S0S1() STACK_STORE_S0(); \
    STACK_STORE(s1, STACK_OFFSET_rFP)

#define STACK_LOAD_RA() STACK_LOAD(ra, 124); \
    STACK_LOAD(gp, STACK_OFFSET_GP); \
    DELETE_STACK(STACK_SIZE)

#define STACK_LOAD_S0() STACK_LOAD(s0, 116); \
    STACK_LOAD_RA()

#define STACK_LOAD_S0S1() STACK_LOAD(s1, STACK_OFFSET_rFP); \
    STACK_LOAD_S0()

#define STACK_STORE_FULL() CREATE_STACK(STACK_SIZE); \
    STACK_STORE(ra, 124); \
    STACK_STORE(fp, 120); \
    STACK_STORE(s0, 116); \
    STACK_STORE(s1, STACK_OFFSET_rFP); \
    STACK_STORE(s2, 108); \
    STACK_STORE(s3, 104); \
    STACK_STORE(s4, 100); \
    STACK_STORE(s5, 96); \
    STACK_STORE(s6, 92); \
    STACK_STORE(s7, 88);

#define STACK_LOAD_FULL() STACK_LOAD(gp, STACK_OFFSET_GP); \
    STACK_LOAD(s7, 88); \
    STACK_LOAD(s6, 92); \
    STACK_LOAD(s5, 96); \
    STACK_LOAD(s4, 100); \
    STACK_LOAD(s3, 104); \
    STACK_LOAD(s2, 108); \
    STACK_LOAD(s1, STACK_OFFSET_rFP); \
    STACK_LOAD(s0, 116); \
    STACK_LOAD(fp, 120); \
    STACK_LOAD(ra, 124); \
    DELETE_STACK(STACK_SIZE)

/*
 * first 8 words are reserved for function calls
 * Maximum offset is STACK_OFFSET_SCRMX-STACK_OFFSET_SCR
 */
#define SCRATCH_STORE(r,off) \
    STACK_STORE(r, STACK_OFFSET_SCR+off);
#define SCRATCH_LOAD(r,off) \
    STACK_LOAD(r, STACK_OFFSET_SCR+off);

#if defined(WITH_JIT)
#include "../common/jit-config.h"
#endif

/* File: mips/platform.S */
/*
 * ===========================================================================
 *  CPU-version-specific defines
 * ===========================================================================
 */

#if !defined(ANDROID_SMP)
# error "Must define ANDROID_SMP"
#endif

/*
 * Macro for data memory barrier.
 */
.macro SMP_DMB
#if ANDROID_SMP != 0
    sync
#else
    /* not SMP */
#endif
.endm

/*
 * Macro for data memory barrier (store/store variant).
 */
.macro  SMP_DMB_ST
#if ANDROID_SMP != 0
    // FIXME: Is this really needed?
    sync
#else
    /* not SMP */
#endif
.endm

/* File: mips/entry.S */

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
 * Interpreter entry point.
 */

#define ASSIST_DEBUGGER 1

    .text
    .align 2
    .global dvmMterpStdRun
    .ent dvmMterpStdRun
    .frame sp, STACK_SIZE, ra
/*
 * On entry:
 *  r0  Thread* self
 *
 * The return comes via a call to dvmMterpStdBail().
 */

dvmMterpStdRun:
    .set noreorder
    .cpload t9
    .set reorder
/* Save to the stack. Frame size = STACK_SIZE */
    STACK_STORE_FULL()
/* This directive will make sure all subsequent jal restore gp at a known offset */
    .cprestore STACK_OFFSET_GP

    addu      fp, sp, STACK_SIZE           #  Move Frame Pointer to the base of frame
    /* save stack pointer, add magic word for debuggerd */
    sw        sp, offThread_bailPtr(a0)      # Save SP

    /* set up "named" registers, figure out entry point */
    move      rSELF, a0                    #  set rSELF
    LOAD_PC_FROM_SELF()
    LOAD_FP_FROM_SELF()
    lw        rIBASE, offThread_curHandlerTable(rSELF)

#if defined(WITH_JIT)
.LentryInstr:
    /* Entry is always a possible trace start */
    lw        a0, offThread_pJitProfTable(rSELF)
    FETCH_INST()                           #  load rINST from rPC
    sw        zero, offThread_inJitCodeCache(rSELF)
#if !defined(WITH_SELF_VERIFICATION)
    bnez      a0, common_updateProfile     # profiling is enabled
#else
    lw       a2, offThread_shadowSpace(rSELF) # to find out the jit exit state
    beqz     a0, 1f                        # profiling is disabled
    lw       a3, offShadowSpace_jitExitState(a2) # jit exit state
    li	     t0, kSVSTraceSelect
    bne      a3, t0, 2f
    li       a2, kJitTSelectRequestHot     # ask for trace selection
    b        common_selectTrace            # go build the trace
2:
    li       a4, kSVSNoProfile
    beq      a3, a4, 1f                    # don't profile the next instruction?
    b        common_updateProfile          # collect profiles
#endif
1:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#else
    /* start executing the instruction at rPC */
    FETCH_INST()                           #  load rINST from rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#endif

.Lbad_arg:
    la        a0, .LstrBadEntryPoint
    #a1 holds value of entryPoint
    JAL(printf)
    JAL(dvmAbort)

    .end dvmMterpStdRun

    .global dvmMterpStdBail
    .ent dvmMterpStdBail

/* Restore the stack pointer and all the registers stored at sp from the save
 * point established  on entry. Return to whoever called dvmMterpStdRun.
 *
 * On entry:
 *   a0    Thread* self
 */
dvmMterpStdBail:
    lw        sp, offThread_bailPtr(a0)      #  Restore sp
    STACK_LOAD_FULL()
    jr        ra

    .end dvmMterpStdBail


    .global dvmAsmInstructionStart
    .type   dvmAsmInstructionStart, %function
dvmAsmInstructionStart = .L_OP_NOP
    .text

/* ------------------------------ */
    .balign 128
.L_OP_NOP: /* 0x00 */
/* File: mips/OP_NOP.S */
    FETCH_ADVANCE_INST(1)                  #  advance to next instr, load rINST
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)                        #  execute it

#ifdef ASSIST_DEBUGGER
    /* insert fake function header to help gdb find the stack frame */
    .type dalvik_inst, @function
dalvik_inst:
    .ent dalvik_inst
    .end dalvik_inst
#endif


/* ------------------------------ */
    .balign 128
.L_OP_MOVE: /* 0x01 */
/* File: mips/OP_MOVE.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    GET_OPB(a1)                            #  a1 <- B from 15:12
    GET_OPA4(a0)                           #  a0 <- A from 11:8
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[B]
    GET_INST_OPCODE(t0)                    #  t0 <- opcode from rINST
    SET_VREG_GOTO(a2, a0, t0)              #  fp[A] <- a2


/* ------------------------------ */
    .balign 128
.L_OP_MOVE_FROM16: /* 0x02 */
/* File: mips/OP_MOVE_FROM16.S */
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    FETCH(a1, 1)                           #  a1 <- BBBB
    GET_OPA(a0)                            #  a0 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[BBBB]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, a0, t0)              #  fp[AA] <- a2


/* ------------------------------ */
    .balign 128
.L_OP_MOVE_16: /* 0x03 */
/* File: mips/OP_MOVE_16.S */
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    FETCH(a1, 2)                           #  a1 <- BBBB
    FETCH(a0, 1)                           #  a0 <- AAAA
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[BBBB]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, a0, t0)              #  fp[AAAA] <- a2 and jump


/* ------------------------------ */
    .balign 128
.L_OP_MOVE_WIDE: /* 0x04 */
/* File: mips/OP_MOVE_WIDE.S */
    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    GET_OPA4(a2)                           #  a2 <- A(+)
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[A]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[B]
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a2)                    #  fp[A] <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_MOVE_WIDE_FROM16: /* 0x05 */
/* File: mips/OP_MOVE_WIDE_FROM16.S */
    /* move-wide/from16 vAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    FETCH(a3, 1)                           #  a3 <- BBBB
    GET_OPA(a2)                            #  a2 <- AA
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BBBB]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[AA]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[BBBB]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a2)                    #  fp[AA] <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_MOVE_WIDE_16: /* 0x06 */
/* File: mips/OP_MOVE_WIDE_16.S */
    /* move-wide/16 vAAAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    FETCH(a3, 2)                           #  a3 <- BBBB
    FETCH(a2, 1)                           #  a2 <- AAAA
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BBBB]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[AAAA]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[BBBB]
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a2)                    #  fp[AAAA] <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_MOVE_OBJECT: /* 0x07 */
/* File: mips/OP_MOVE_OBJECT.S */
/* File: mips/OP_MOVE.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    GET_OPB(a1)                            #  a1 <- B from 15:12
    GET_OPA4(a0)                           #  a0 <- A from 11:8
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[B]
    GET_INST_OPCODE(t0)                    #  t0 <- opcode from rINST
    SET_VREG_GOTO(a2, a0, t0)              #  fp[A] <- a2



/* ------------------------------ */
    .balign 128
.L_OP_MOVE_OBJECT_FROM16: /* 0x08 */
/* File: mips/OP_MOVE_OBJECT_FROM16.S */
/* File: mips/OP_MOVE_FROM16.S */
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    FETCH(a1, 1)                           #  a1 <- BBBB
    GET_OPA(a0)                            #  a0 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[BBBB]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, a0, t0)              #  fp[AA] <- a2



/* ------------------------------ */
    .balign 128
.L_OP_MOVE_OBJECT_16: /* 0x09 */
/* File: mips/OP_MOVE_OBJECT_16.S */
/* File: mips/OP_MOVE_16.S */
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    FETCH(a1, 2)                           #  a1 <- BBBB
    FETCH(a0, 1)                           #  a0 <- AAAA
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[BBBB]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, a0, t0)              #  fp[AAAA] <- a2 and jump



/* ------------------------------ */
    .balign 128
.L_OP_MOVE_RESULT: /* 0x0a */
/* File: mips/OP_MOVE_RESULT.S */
    /* for: move-result, move-result-object */
    /* op vAA */
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    LOAD_rSELF_retval(a0)                  #  a0 <- self->retval.i
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a2, t0)              #  fp[AA] <- a0


/* ------------------------------ */
    .balign 128
.L_OP_MOVE_RESULT_WIDE: /* 0x0b */
/* File: mips/OP_MOVE_RESULT_WIDE.S */
    /* move-result-wide vAA */
    GET_OPA(a2)                            #  a2 <- AA
    addu      a3, rSELF, offThread_retval  #  a3 <- &self->retval
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[AA]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- retval.j
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a2)                    #  fp[AA] <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_MOVE_RESULT_OBJECT: /* 0x0c */
/* File: mips/OP_MOVE_RESULT_OBJECT.S */
/* File: mips/OP_MOVE_RESULT.S */
    /* for: move-result, move-result-object */
    /* op vAA */
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    LOAD_rSELF_retval(a0)                  #  a0 <- self->retval.i
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a2, t0)              #  fp[AA] <- a0



/* ------------------------------ */
    .balign 128
.L_OP_MOVE_EXCEPTION: /* 0x0d */
/* File: mips/OP_MOVE_EXCEPTION.S */
    /* move-exception vAA */
    GET_OPA(a2)                            #  a2 <- AA
    LOAD_offThread_exception(a3, rSELF)    #  a3 <- dvmGetException bypass
    li        a1, 0                        #  a1 <- 0
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    SET_VREG(a3, a2)                       #  fp[AA] <- exception obj
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE_offThread_exception(a1, rSELF)   #  dvmClearException bypass
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_RETURN_VOID: /* 0x0e */
/* File: mips/OP_RETURN_VOID.S */
    b         common_returnFromMethod


/* ------------------------------ */
    .balign 128
.L_OP_RETURN: /* 0x0f */
/* File: mips/OP_RETURN.S */
    /*
     * Return a 32-bit value.  Copies the return value into the "thread"
     * structure, then jumps to the return handler.
     *
     * for: return, return-object
     */
    /* op vAA */
    GET_OPA(a2)                            #  a2 <- AA
    GET_VREG(a0, a2)                       #  a0 <- vAA
    sw        a0, offThread_retval(rSELF)  #  retval.i <- vAA
    b         common_returnFromMethod


/* ------------------------------ */
    .balign 128
.L_OP_RETURN_WIDE: /* 0x10 */
/* File: mips/OP_RETURN_WIDE.S */
    /*
     * Return a 64-bit value.  Copies the return value into the "thread"
     * structure, then jumps to the return handler.
     */
    /* return-wide vAA */
    GET_OPA(a2)                            #  a2 <- AA
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[AA]
    addu      a3, rSELF, offThread_retval  #  a3 <- &self->retval
    LOAD64(a0, a1, a2)                     #  a0/a1 <- vAA/vAA+1
    STORE64(a0, a1, a3)                    #  retval <- a0/a1
    b         common_returnFromMethod


/* ------------------------------ */
    .balign 128
.L_OP_RETURN_OBJECT: /* 0x11 */
/* File: mips/OP_RETURN_OBJECT.S */
/* File: mips/OP_RETURN.S */
    /*
     * Return a 32-bit value.  Copies the return value into the "thread"
     * structure, then jumps to the return handler.
     *
     * for: return, return-object
     */
    /* op vAA */
    GET_OPA(a2)                            #  a2 <- AA
    GET_VREG(a0, a2)                       #  a0 <- vAA
    sw        a0, offThread_retval(rSELF)  #  retval.i <- vAA
    b         common_returnFromMethod



/* ------------------------------ */
    .balign 128
.L_OP_CONST_4: /* 0x12 */
/* File: mips/OP_CONST_4.S */
    # const/4 vA,                          /* +B */
    sll       a1, rINST, 16                #  a1 <- Bxxx0000
    GET_OPA(a0)                            #  a0 <- A+
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    sra       a1, a1, 28                   #  a1 <- sssssssB (sign-extended)
    and       a0, a0, 15
    GET_INST_OPCODE(t0)                    #  ip <- opcode from rINST
    SET_VREG_GOTO(a1, a0, t0)              #  fp[A] <- a1


/* ------------------------------ */
    .balign 128
.L_OP_CONST_16: /* 0x13 */
/* File: mips/OP_CONST_16.S */
    # const/16 vAA,                        /* +BBBB */
    FETCH_S(a0, 1)                         #  a0 <- ssssBBBB (sign-extended)
    GET_OPA(a3)                            #  a3 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_OP_CONST: /* 0x14 */
/* File: mips/OP_CONST.S */
    # const vAA,                           /* +BBBBbbbb */
    GET_OPA(a3)                            #  a3 <- AA
    FETCH(a0, 1)                           #  a0 <- bbbb (low)
    FETCH(a1, 2)                           #  a1 <- BBBB (high)
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    sll       a1, a1, 16
    or        a0, a1, a0                   #  a0 <- BBBBbbbb
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_OP_CONST_HIGH16: /* 0x15 */
/* File: mips/OP_CONST_HIGH16.S */
    # const/high16 vAA,                    /* +BBBB0000 */
    FETCH(a0, 1)                           #  a0 <- 0000BBBB (zero-extended)
    GET_OPA(a3)                            #  a3 <- AA
    sll       a0, a0, 16                   #  a0 <- BBBB0000
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_OP_CONST_WIDE_16: /* 0x16 */
/* File: mips/OP_CONST_WIDE_16.S */
    # const-wide/16 vAA,                   /* +BBBB */
    FETCH_S(a0, 1)                         #  a0 <- ssssBBBB (sign-extended)
    GET_OPA(a3)                            #  a3 <- AA
    sra       a1, a0, 31                   #  a1 <- ssssssss
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a3)                    #  vAA <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_CONST_WIDE_32: /* 0x17 */
/* File: mips/OP_CONST_WIDE_32.S */
    # const-wide/32 vAA,                   /* +BBBBbbbb */
    FETCH(a0, 1)                           #  a0 <- 0000bbbb (low)
    GET_OPA(a3)                            #  a3 <- AA
    FETCH_S(a2, 2)                         #  a2 <- ssssBBBB (high)
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    sll       a2, a2, 16
    or        a0, a0, a2                   #  a0 <- BBBBbbbb
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[AA]
    sra       a1, a0, 31                   #  a1 <- ssssssss
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a3)                    #  vAA <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_CONST_WIDE: /* 0x18 */
/* File: mips/OP_CONST_WIDE.S */
    # const-wide vAA,                      /* +HHHHhhhhBBBBbbbb */
    FETCH(a0, 1)                           #  a0 <- bbbb (low)
    FETCH(a1, 2)                           #  a1 <- BBBB (low middle)
    FETCH(a2, 3)                           #  a2 <- hhhh (high middle)
    sll       a1, 16 #
    or        a0, a1                       #  a0 <- BBBBbbbb (low word)
    FETCH(a3, 4)                           #  a3 <- HHHH (high)
    GET_OPA(t1)                            #  t1 <- AA
    sll       a3, 16
    or        a1, a3, a2                   #  a1 <- HHHHhhhh (high word)
    FETCH_ADVANCE_INST(5)                  #  advance rPC, load rINST
    EAS2(t1, rFP, t1)                      #  t1 <- &fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, t1)                    #  vAA <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_CONST_WIDE_HIGH16: /* 0x19 */
/* File: mips/OP_CONST_WIDE_HIGH16.S */
    # const-wide/high16 vAA,               /* +BBBB000000000000 */
    FETCH(a1, 1)                           #  a1 <- 0000BBBB (zero-extended)
    GET_OPA(a3)                            #  a3 <- AA
    li        a0, 0                        #  a0 <- 00000000
    sll       a1, 16                       #  a1 <- BBBB0000
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a3)                    #  vAA <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_CONST_STRING: /* 0x1a */
/* File: mips/OP_CONST_STRING.S */
    # const/string vAA, String             /* BBBB */
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- self->methodClassDex
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    LOAD_base_offDvmDex_pResStrings(a2, a2) #  a2 <- dvmDex->pResStrings
    LOAD_eas2(v0, a2, a1)                  #  v0 <- pResStrings[BBBB]
    # not yet resolved?
    bnez      v0, .LOP_CONST_STRING_resolve
    /*
     * Continuation if the String has not yet been resolved.
     *  a1:   BBBB (String ref)
     *  rOBJ: target register
     */
    EXPORT_PC()
    LOAD_rSELF_method(a0)                  #  a0 <- self->method
    LOAD_base_offMethod_clazz(a0, a0)      #  a0 <- method->clazz
    JAL(dvmResolveString)                  #  v0 <- String reference
    # failed?
    beqz      v0, common_exceptionThrown   #  yup, handle the exception

.LOP_CONST_STRING_resolve:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(v0, rOBJ, t0)            #  vAA <- v0






/* ------------------------------ */
    .balign 128
.L_OP_CONST_STRING_JUMBO: /* 0x1b */
/* File: mips/OP_CONST_STRING_JUMBO.S */
    # const/string vAA, String             /* BBBBBBBB */
    FETCH(a0, 1)                           #  a0 <- bbbb (low)
    FETCH(a1, 2)                           #  a1 <- BBBB (high)
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- self->methodClassDex
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    LOAD_base_offDvmDex_pResStrings(a2, a2) #  a2 <- dvmDex->pResStrings
    sll       a1, a1, 16
    or        a1, a1, a0                   #  a1 <- BBBBbbbb
    LOAD_eas2(v0, a2, a1)                  #  v0 <- pResStrings[BBBB]
    bnez      v0, .LOP_CONST_STRING_JUMBO_resolve

    /*
     * Continuation if the String has not yet been resolved.
     *  a1: BBBBBBBB (String ref)
     *  rOBJ: target register
     */
    EXPORT_PC()
    LOAD_rSELF_method(a0)                  #  a0 <- self->method
    LOAD_base_offMethod_clazz(a0, a0)      #  a0 <- method->clazz
    JAL(dvmResolveString)                  #  v0 <- String reference
    # failed?
    beqz      v0, common_exceptionThrown   #  yup, handle the exception

.LOP_CONST_STRING_JUMBO_resolve:
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(v0, rOBJ, t1)            #  vAA <- v0


/* ------------------------------ */
    .balign 128
.L_OP_CONST_CLASS: /* 0x1c */
/* File: mips/OP_CONST_CLASS.S */
    # const/class vAA, Class               /* BBBB */
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- self->methodClassDex
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    LOAD_base_offDvmDex_pResClasses(a2, a2) #  a2 <- dvmDex->pResClasses
    LOAD_eas2(v0, a2, a1)                  #  v0 <- pResClasses[BBBB]

    bnez      v0, .LOP_CONST_CLASS_resolve      #  v0!=0 => resolved-ok
    /*
     * Continuation if the Class has not yet been resolved.
     *  a1: BBBB (Class ref)
     *  rOBJ: target register
     */
    EXPORT_PC()
    LOAD_rSELF_method(a0)                  #  a0 <- self->method
    li        a2, 1                        #  a2 <- true
    LOAD_base_offMethod_clazz(a0, a0)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- Class reference
    # failed==0?
    beqz      v0, common_exceptionThrown   #  yup, handle the exception

.LOP_CONST_CLASS_resolve:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(v0, rOBJ, t0)            #  vAA <- v0



/* ------------------------------ */
    .balign 128
.L_OP_MONITOR_ENTER: /* 0x1d */
/* File: mips/OP_MONITOR_ENTER.S */
    /*
     * Synchronize on an object.
     */
    /* monitor-enter vAA */
    GET_OPA(a2)                            #  a2 <- AA
    GET_VREG(a1, a2)                       #  a1 <- vAA (object)
    move      a0, rSELF                    #  a0 <- self
    EXPORT_PC()                            #  export PC so we can grab stack trace
    # null object?
    beqz      a1, common_errNullObject     #  null object, throw an exception
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    JAL(dvmLockObject)                     #  call(self, obj)
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_MONITOR_EXIT: /* 0x1e */
/* File: mips/OP_MONITOR_EXIT.S */
    /*
     * Unlock an object.
     *
     * Exceptions that occur when unlocking a monitor need to appear as
     * if they happened at the following instruction.  See the Dalvik
     * instruction spec.
     */
    /* monitor-exit vAA */
    GET_OPA(a2)                            #  a2 <- AA
    EXPORT_PC()                            #  before fetch: export the PC
    GET_VREG(a1, a2)                       #  a1 <- vAA (object)
    # null object?
    beqz      a1, 1f
    move      a0, rSELF                    #  a0 <- self
    JAL(dvmUnlockObject)                   #  v0 <- success for unlock(self, obj)
    # failed?
    FETCH_ADVANCE_INST(1)                  #  before throw: advance rPC, load rINST
    beqz      v0, common_exceptionThrown   #  yes, exception is pending
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
1:
    FETCH_ADVANCE_INST(1)                  #  before throw: advance rPC, load rINST
    b         common_errNullObject

/* ------------------------------ */
    .balign 128
.L_OP_CHECK_CAST: /* 0x1f */
/* File: mips/OP_CHECK_CAST.S */
    /*
     * Check to see if a cast from one class to another is allowed.
     */
    # check-cast vAA, class                /* BBBB */
    GET_OPA(a3)                            #  a3 <- AA
    FETCH(a2, 1)                           #  a2 <- BBBB
    GET_VREG(rOBJ, a3)                     #  rOBJ <- object
    LOAD_rSELF_methodClassDex(a0)          #  a0 <- pDvmDex
    LOAD_base_offDvmDex_pResClasses(a0, a0) #  a0 <- pDvmDex->pResClasses
    # is object null?
    beqz      rOBJ, .LOP_CHECK_CAST_okay       #  null obj, cast always succeeds
    LOAD_eas2(a1, a0, a2)                  #  a1 <- resolved class
    LOAD_base_offObject_clazz(a0, rOBJ)    #  a0 <- obj->clazz
    # have we resolved this before?
    beqz      a1, .LOP_CHECK_CAST_resolve      #  not resolved, do it now
.LOP_CHECK_CAST_resolved:
    # same class (trivial success)?
    bne       a0, a1, .LOP_CHECK_CAST_fullcheck #  no, do full check
.LOP_CHECK_CAST_okay:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

    /*
     * Trivial test failed, need to perform full check.  This is common.
     *  a0 holds obj->clazz
     *  a1 holds class resolved from BBBB
     *  rOBJ holds object
     */
.LOP_CHECK_CAST_fullcheck:
    move      rBIX,a1                      #  avoid ClassObject getting clobbered
    JAL(dvmInstanceofNonTrivial)           #  v0 <- boolean result
    # failed?
    bnez      v0, .LOP_CHECK_CAST_okay         #  no, success
    b         .LOP_CHECK_CAST_castfailure

/* ------------------------------ */
    .balign 128
.L_OP_INSTANCE_OF: /* 0x20 */
/* File: mips/OP_INSTANCE_OF.S */
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    # instance-of vA, vB, class            /* CCCC */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB (object)
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- pDvmDex
    # is object null?
    beqz      a0, .LOP_INSTANCE_OF_store        #  null obj, not an instance, store a0
    FETCH(a3, 1)                           #  a3 <- CCCC
    LOAD_base_offDvmDex_pResClasses(a2, a2) #  a2 <- pDvmDex->pResClasses
    LOAD_eas2(a1, a2, a3)                  #  a1 <- resolved class
    LOAD_base_offObject_clazz(a0, a0)      #  a0 <- obj->clazz
    # have we resolved this before?
    beqz      a1, .LOP_INSTANCE_OF_resolve      #  not resolved, do it now
.LOP_INSTANCE_OF_resolved:                   #  a0=obj->clazz, a1=resolved class
    # same class (trivial success)?
    beq       a0, a1, .LOP_INSTANCE_OF_trivial  #  yes, trivial finish
    b         .LOP_INSTANCE_OF_fullcheck        #  no, do full check

    /*
     * Trivial test succeeded, save and bail.
     *  rOBJ holds A
     */
.LOP_INSTANCE_OF_trivial:
    li        a0, 1                        #  indicate success
    # fall thru
    /*
     * a0   holds boolean result
     * rOBJ holds A
     */
.LOP_INSTANCE_OF_store:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    SET_VREG(a0, rOBJ)                     #  vA <- a0
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_ARRAY_LENGTH: /* 0x21 */
/* File: mips/OP_ARRAY_LENGTH.S */
    /*
     * Return the length of an array.
     */
    GET_OPB(a1)                            #  a1 <- B
    GET_OPA4(a2)                           #  a2 <- A+
    GET_VREG(a0, a1)                       #  a0 <- vB (object ref)
    # is object null?
    beqz      a0, common_errNullObject     #  yup, fail
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- array length
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a3, a2, t0)              #  vA <- length


/* ------------------------------ */
    .balign 128
.L_OP_NEW_INSTANCE: /* 0x22 */
/* File: mips/OP_NEW_INSTANCE.S */
    /*
     * Create a new instance of a class.
     */
    # new-instance vAA, class              /* BBBB */
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResClasses(a3, a3) #  a3 <- pDvmDex->pResClasses
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved class
#if defined(WITH_JIT)
    EAS2(rBIX, a3, a1)                     #  rBIX <- &resolved_class
#endif
    EXPORT_PC()                            #  req'd for init, resolve, alloc
    # already resolved?
    beqz      a0, .LOP_NEW_INSTANCE_resolve      #  no, resolve it now
.LOP_NEW_INSTANCE_resolved:                      #  a0=class
    lbu       a1, offClassObject_status(a0) #  a1 <- ClassStatus enum
    # has class been initialized?
    li        t0, CLASS_INITIALIZED
    move      rOBJ, a0                     #  save a0
    bne       a1, t0, .LOP_NEW_INSTANCE_needinit #  no, init class now

.LOP_NEW_INSTANCE_initialized:                   #  a0=class
    LOAD_base_offClassObject_accessFlags(a3, a0) #  a3 <- clazz->accessFlags
    li        a1, ALLOC_DONT_TRACK         #  flags for alloc call
    # a0=class
    JAL(dvmAllocObject)                    #  v0 <- new object
    GET_OPA(a3)                            #  a3 <- AA
#if defined(WITH_JIT)
    /*
     * The JIT needs the class to be fully resolved before it can
     * include this instruction in a trace.
     */
    lhu       a1, offThread_subMode(rSELF)
    beqz      v0, common_exceptionThrown   #  yes, handle the exception
    and       a1, kSubModeJitTraceBuild    #  under construction?
    bnez      a1, .LOP_NEW_INSTANCE_jitCheck
#else
    # failed?
    beqz      v0, common_exceptionThrown   #  yes, handle the exception
#endif
    b         .LOP_NEW_INSTANCE_continue


/* ------------------------------ */
    .balign 128
.L_OP_NEW_ARRAY: /* 0x23 */
/* File: mips/OP_NEW_ARRAY.S */
    /*
     * Allocate an array of objects, specified with the array class
     * and a count.
     *
     * The verifier guarantees that this is an array class, so we don't
     * check for it here.
     */
    /* new-array vA, vB, class@CCCC */
    GET_OPB(a0)                            #  a0 <- B
    FETCH(a2, 1)                           #  a2 <- CCCC
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    GET_VREG(a1, a0)                       #  a1 <- vB (array length)
    LOAD_base_offDvmDex_pResClasses(a3, a3) #  a3 <- pDvmDex->pResClasses
    LOAD_eas2(a0, a3, a2)                  #  a0 <- resolved class
    # check length
    bltz      a1, common_errNegativeArraySize #  negative length, bail - len in a1
    EXPORT_PC()                            #  req'd for resolve, alloc
    # already resolved?
    beqz      a0, .LOP_NEW_ARRAY_resolve

    /*
     * Finish allocation.
     *
     *  a0 holds class
     *  a1 holds array length
     */
.LOP_NEW_ARRAY_finish:
    li        a2, ALLOC_DONT_TRACK         #  don't track in local refs table
    JAL(dvmAllocArrayByClass)              #  v0 <- call(clazz, length, flags)
    GET_OPA4(a2)                           #  a2 <- A+
    # failed?
    beqz      v0, common_exceptionThrown   #  yes, handle the exception
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(v0, a2)                       #  vA <- v0
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_OP_FILLED_NEW_ARRAY: /* 0x24 */
/* File: mips/OP_FILLED_NEW_ARRAY.S */
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op {vCCCC..v(CCCC+AA-1)}, type       /* BBBB */
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResClasses(a3, a3) #  a3 <- pDvmDex->pResClasses
    EXPORT_PC()                            #  need for resolve and alloc
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved class
    GET_OPA(rOBJ)                          #  rOBJ <- AA or BA
    # already resolved?
    bnez      a0, .LOP_FILLED_NEW_ARRAY_continue     #  yes, continue on
    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    li        a2, 0                        #  a2 <- false
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- call(clazz, ref)
    move      a0, v0
    # got null?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    b         .LOP_FILLED_NEW_ARRAY_continue

/* ------------------------------ */
    .balign 128
.L_OP_FILLED_NEW_ARRAY_RANGE: /* 0x25 */
/* File: mips/OP_FILLED_NEW_ARRAY_RANGE.S */
/* File: mips/OP_FILLED_NEW_ARRAY.S */
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op {vCCCC..v(CCCC+AA-1)}, type       /* BBBB */
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResClasses(a3, a3) #  a3 <- pDvmDex->pResClasses
    EXPORT_PC()                            #  need for resolve and alloc
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved class
    GET_OPA(rOBJ)                          #  rOBJ <- AA or BA
    # already resolved?
    bnez      a0, .LOP_FILLED_NEW_ARRAY_RANGE_continue     #  yes, continue on
    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    li        a2, 0                        #  a2 <- false
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- call(clazz, ref)
    move      a0, v0
    # got null?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    b         .LOP_FILLED_NEW_ARRAY_RANGE_continue


/* ------------------------------ */
    .balign 128
.L_OP_FILL_ARRAY_DATA: /* 0x26 */
/* File: mips/OP_FILL_ARRAY_DATA.S */
    /* fill-array-data vAA, +BBBBBBBB */
    FETCH(a0, 1)                           #  a0 <- bbbb (lo)
    FETCH(a1, 2)                           #  a1 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    sll       a1, a1, 16                   #  a1 <- BBBBbbbb
    or        a1, a0, a1                   #  a1 <- BBBBbbbb
    GET_VREG(a0, a3)                       #  a0 <- vAA (array object)
    EAS1(a1, rPC, a1)                      #  a1 <- PC + BBBBbbbb*2 (array data off.)
    EXPORT_PC()
    JAL(dvmInterpHandleFillArrayData)      #  fill the array with predefined data
    # 0 means an exception is thrown
    beqz      v0, common_exceptionThrown   #  has exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_OP_THROW: /* 0x27 */
/* File: mips/OP_THROW.S */
    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    GET_OPA(a2)                            #  a2 <- AA
    GET_VREG(a1, a2)                       #  a1 <- vAA (exception object)
    EXPORT_PC()                            #  exception handler can throw
    # null object?
    beqz      a1, common_errNullObject     #  yes, throw an NPE instead
    # bypass dvmSetException, just store it
    STORE_offThread_exception(a1, rSELF)   #  thread->exception <- obj
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_GOTO: /* 0x28 */
/* File: mips/OP_GOTO.S */
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    sll       a0, rINST, 16                #  a0 <- AAxx0000
    sra       a1, a0, 24                   #  a1 <- ssssssAA (sign-extended)
    addu      a2, a1, a1                   #  a2 <- byte offset
    /* If backwards branch refresh rBASE */
    bgez      a1, 1f
    lw        rIBASE, offThread_curHandlerTable(rSELF) #  refresh handler base
1:
    FETCH_ADVANCE_INST_RB(a2)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bltz      a1, common_testUpdateProfile #  (a0) check for trace hotness
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_OP_GOTO_16: /* 0x29 */
/* File: mips/OP_GOTO_16.S */
    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
    FETCH_S(a0, 1)                         #  a0 <- ssssAAAA (sign-extended)
    addu      a1, a0, a0                   #  a1 <- byte offset, flags set
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    bgez      a1, 1f
    lw        rIBASE, offThread_curHandlerTable(rSELF) #  refresh handler base
1:
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bltz      a1, common_testUpdateProfile #  (a0) hot trace head?
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_OP_GOTO_32: /* 0x2a */
/* File: mips/OP_GOTO_32.S */
    /*
     * Unconditional branch, 32-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     *
     * Unlike most opcodes, this one is allowed to branch to itself, so
     * our "backward branch" test must be "<=0" instead of "<0".
     */
    /* goto/32 +AAAAAAAA */
    FETCH(a0, 1)                           #  a0 <- aaaa (lo)
    FETCH(a1, 2)                           #  a1 <- AAAA (hi)
    sll       a1, a1, 16
    or        a0, a0, a1                   #  a0 <- AAAAaaaa
    addu      a1, a0, a0                   #  a1 <- byte offset
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgtz      a1, 1f
    lw        rIBASE, offThread_curHandlerTable(rSELF) #  refresh handler base
1:
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    blez      a1, common_testUpdateProfile # (a0) hot trace head?
#else
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    bgtz      a0, 2f
    lw        rIBASE, offThread_curHandlerTable(rSELF) #  refresh handler base
2:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_OP_PACKED_SWITCH: /* 0x2b */
/* File: mips/OP_PACKED_SWITCH.S */
    /*
     * Handle a packed-switch or sparse-switch instruction.  In both cases
     * we decode it and hand it off to a helper function.
     *
     * We don't really expect backward branches in a switch statement, but
     * they're perfectly legal, so we check for them here.
     *
     * When the JIT is present, all targets are considered treated as
     * a potential trace heads regardless of branch direction.
     *
     * for: packed-switch, sparse-switch
     */
    /* op vAA, +BBBB */
    FETCH(a0, 1)                           #  a0 <- bbbb (lo)
    FETCH(a1, 2)                           #  a1 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    sll       t0, a1, 16
    or        a0, a0, t0                   #  a0 <- BBBBbbbb
    GET_VREG(a1, a3)                       #  a1 <- vAA
    EAS1(a0, rPC, a0)                      #  a0 <- PC + BBBBbbbb*2
    JAL(dvmInterpHandlePackedSwitch)                             #  a0 <- code-unit branch offset
    addu      a1, v0, v0                   #  a1 <- byte offset
    bgtz      a1, 1f
    lw        rIBASE, offThread_curHandlerTable(rSELF) #  refresh handler base
1:
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bnez      a0, common_updateProfile
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_OP_SPARSE_SWITCH: /* 0x2c */
/* File: mips/OP_SPARSE_SWITCH.S */
/* File: mips/OP_PACKED_SWITCH.S */
    /*
     * Handle a packed-switch or sparse-switch instruction.  In both cases
     * we decode it and hand it off to a helper function.
     *
     * We don't really expect backward branches in a switch statement, but
     * they're perfectly legal, so we check for them here.
     *
     * When the JIT is present, all targets are considered treated as
     * a potential trace heads regardless of branch direction.
     *
     * for: packed-switch, sparse-switch
     */
    /* op vAA, +BBBB */
    FETCH(a0, 1)                           #  a0 <- bbbb (lo)
    FETCH(a1, 2)                           #  a1 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    sll       t0, a1, 16
    or        a0, a0, t0                   #  a0 <- BBBBbbbb
    GET_VREG(a1, a3)                       #  a1 <- vAA
    EAS1(a0, rPC, a0)                      #  a0 <- PC + BBBBbbbb*2
    JAL(dvmInterpHandleSparseSwitch)                             #  a0 <- code-unit branch offset
    addu      a1, v0, v0                   #  a1 <- byte offset
    bgtz      a1, 1f
    lw        rIBASE, offThread_curHandlerTable(rSELF) #  refresh handler base
1:
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bnez      a0, common_updateProfile
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_CMPL_FLOAT: /* 0x2d */
/* File: mips/OP_CMPL_FLOAT.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * Provide a "naninst" instruction that puts 1 or -1 into a1 depending
     * on what value we'd like to return when one of the operands is NaN.
     *
     * The operation we're implementing is:
     *   if (x == y)
     *     return 0;
     *   else if (x < y)
     *     return -1;
     *   else if (x > y)
     *     return 1;
     *   else
     *     return {-1,1};  // one or both operands was NaN
     *
     * for: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */

    /* "clasic" form */
    FETCH(a0, 1)                           #  a0 <- CCBB
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8
#ifdef SOFT_FLOAT
    GET_VREG(rOBJ, a2)                     #  rOBJ <- vBB
    GET_VREG(rBIX, a3)                     #  rBIX <- vCC
    move      a0, rOBJ                     #  a0 <- vBB
    move      a1, rBIX                     #  a1 <- vCC
    JAL(__eqsf2)                           #  a0 <- (vBB == vCC)
    li        rTEMP, 0                     # set rTEMP to 0
    beqz      v0, OP_CMPL_FLOAT_finish
    move      a0, rOBJ                     #  a0 <- vBB
    move      a1, rBIX                     #  a1 <- vCC
    JAL(__ltsf2)                           #  a0 <- (vBB < vCC)
    li        rTEMP, -1
    bltz      v0, OP_CMPL_FLOAT_finish
    move      a0, rOBJ                     #  a0 <- vBB
    move      a1, rBIX                     #  a1 <- vCC
    b         OP_CMPL_FLOAT_continue
#else
    GET_VREG_F(ft0, a2)
    GET_VREG_F(ft1, a3)
    c.olt.s   fcc0, ft0, ft1               # Is ft0 < ft1
    li        rTEMP, -1
    bc1t      fcc0, OP_CMPL_FLOAT_finish
    c.olt.s   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, OP_CMPL_FLOAT_finish
    c.eq.s    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, OP_CMPL_FLOAT_finish
    b         OP_CMPL_FLOAT_nan

#endif


/* ------------------------------ */
    .balign 128
.L_OP_CMPG_FLOAT: /* 0x2e */
/* File: mips/OP_CMPG_FLOAT.S */
/* File: mips/OP_CMPL_FLOAT.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * Provide a "naninst" instruction that puts 1 or -1 into a1 depending
     * on what value we'd like to return when one of the operands is NaN.
     *
     * The operation we're implementing is:
     *   if (x == y)
     *     return 0;
     *   else if (x < y)
     *     return -1;
     *   else if (x > y)
     *     return 1;
     *   else
     *     return {-1,1};  // one or both operands was NaN
     *
     * for: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */

    /* "clasic" form */
    FETCH(a0, 1)                           #  a0 <- CCBB
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8
#ifdef SOFT_FLOAT
    GET_VREG(rOBJ, a2)                     #  rOBJ <- vBB
    GET_VREG(rBIX, a3)                     #  rBIX <- vCC
    move      a0, rOBJ                     #  a0 <- vBB
    move      a1, rBIX                     #  a1 <- vCC
    JAL(__eqsf2)                           #  a0 <- (vBB == vCC)
    li        rTEMP, 0                     # set rTEMP to 0
    beqz      v0, OP_CMPG_FLOAT_finish
    move      a0, rOBJ                     #  a0 <- vBB
    move      a1, rBIX                     #  a1 <- vCC
    JAL(__ltsf2)                           #  a0 <- (vBB < vCC)
    li        rTEMP, -1
    bltz      v0, OP_CMPG_FLOAT_finish
    move      a0, rOBJ                     #  a0 <- vBB
    move      a1, rBIX                     #  a1 <- vCC
    b         OP_CMPG_FLOAT_continue
#else
    GET_VREG_F(ft0, a2)
    GET_VREG_F(ft1, a3)
    c.olt.s   fcc0, ft0, ft1               # Is ft0 < ft1
    li        rTEMP, -1
    bc1t      fcc0, OP_CMPG_FLOAT_finish
    c.olt.s   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, OP_CMPG_FLOAT_finish
    c.eq.s    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, OP_CMPG_FLOAT_finish
    b         OP_CMPG_FLOAT_nan

#endif



/* ------------------------------ */
    .balign 128
.L_OP_CMPL_DOUBLE: /* 0x2f */
/* File: mips/OP_CMPL_DOUBLE.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * Provide a "naninst" instruction that puts 1 or -1 into a1 depending
     * on what value we'd like to return when one of the operands is NaN.
     *
     * See OP_CMPL_FLOAT for an explanation.
     *
     * For: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */

    FETCH(a0, 1)                           #  a0 <- CCBB
    and       rOBJ, a0, 255                #  s0 <- BB
    srl       rBIX, a0, 8                  #  t0 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  s0 <- &fp[BB]
    EAS2(rBIX, rFP, rBIX)                  #  t0 <- &fp[CC]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)             #  a2/a3 <- vCC/vCC+1
    JAL(__eqdf2)                           #  cmp <=: C clear if <, Z set if eq
    li        rTEMP, 0
    beqz      v0, OP_CMPL_DOUBLE_finish

    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)             #  a2/a3 <- vCC/vCC+1
    JAL(__ltdf2)
    li        rTEMP, -1
    bltz      v0, OP_CMPL_DOUBLE_finish
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vBB/vBB+1
    b         OP_CMPL_DOUBLE_continue
#else
    LOAD64_F(ft0, ft0f, rOBJ)
    LOAD64_F(ft1, ft1f, rBIX)
    c.olt.d   fcc0, ft0, ft1
    li        rTEMP, -1
    bc1t      fcc0, OP_CMPL_DOUBLE_finish
    c.olt.d   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, OP_CMPL_DOUBLE_finish
    c.eq.d    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, OP_CMPL_DOUBLE_finish
    b         OP_CMPL_DOUBLE_nan
#endif

/* ------------------------------ */
    .balign 128
.L_OP_CMPG_DOUBLE: /* 0x30 */
/* File: mips/OP_CMPG_DOUBLE.S */
/* File: mips/OP_CMPL_DOUBLE.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * Provide a "naninst" instruction that puts 1 or -1 into a1 depending
     * on what value we'd like to return when one of the operands is NaN.
     *
     * See OP_CMPL_FLOAT for an explanation.
     *
     * For: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */

    FETCH(a0, 1)                           #  a0 <- CCBB
    and       rOBJ, a0, 255                #  s0 <- BB
    srl       rBIX, a0, 8                  #  t0 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  s0 <- &fp[BB]
    EAS2(rBIX, rFP, rBIX)                  #  t0 <- &fp[CC]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)             #  a2/a3 <- vCC/vCC+1
    JAL(__eqdf2)                           #  cmp <=: C clear if <, Z set if eq
    li        rTEMP, 0
    beqz      v0, OP_CMPG_DOUBLE_finish

    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)             #  a2/a3 <- vCC/vCC+1
    JAL(__ltdf2)
    li        rTEMP, -1
    bltz      v0, OP_CMPG_DOUBLE_finish
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vBB/vBB+1
    b         OP_CMPG_DOUBLE_continue
#else
    LOAD64_F(ft0, ft0f, rOBJ)
    LOAD64_F(ft1, ft1f, rBIX)
    c.olt.d   fcc0, ft0, ft1
    li        rTEMP, -1
    bc1t      fcc0, OP_CMPG_DOUBLE_finish
    c.olt.d   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, OP_CMPG_DOUBLE_finish
    c.eq.d    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, OP_CMPG_DOUBLE_finish
    b         OP_CMPG_DOUBLE_nan
#endif


/* ------------------------------ */
    .balign 128
.L_OP_CMP_LONG: /* 0x31 */
/* File: mips/OP_CMP_LONG.S */
    /*
     * Compare two 64-bit values
     *    x = y     return  0
     *    x < y     return -1
     *    x > y     return  1
     *
     * I think I can improve on the ARM code by the following observation
     *    slt   t0,  x.hi, y.hi;	# (x.hi < y.hi) ? 1:0
     *    sgt   t1,  x.hi, y.hi;	# (y.hi > x.hi) ? 1:0
     *    subu  v0, t0, t1              # v0= -1:1:0 for [ < > = ]
     */
    /* cmp-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)                     #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, a3)                     #  a2/a3 <- vCC/vCC+1

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    slt       t0, a1, a3                   #  compare hi
    sgt       t1, a1, a3
    subu      v0, t1, t0                   #  v0 <- (-1, 1, 0)
    bnez      v0, .LOP_CMP_LONG_finish
    # at this point x.hi==y.hi
    sltu      t0, a0, a2                   #  compare lo
    sgtu      t1, a0, a2
    subu      v0, t1, t0                   #  v0 <- (-1, 1, 0) for [< > =]

.LOP_CMP_LONG_finish:
    SET_VREG(v0, rOBJ)                     #  vAA <- v0
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_EQ: /* 0x32 */
/* File: mips/OP_IF_EQ.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a2, a0)                       #  a2 <- vA
    bne a2, a3, 1f                  #  branch to 1 if comparison failed
    FETCH_S(a1, 1)                         #  a1<- branch offset, in code units
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a2, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a2)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a2, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
3:
    bnez      a0, common_updateProfile
#else
    bgez      a2, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_NE: /* 0x33 */
/* File: mips/OP_IF_NE.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a2, a0)                       #  a2 <- vA
    beq a2, a3, 1f                  #  branch to 1 if comparison failed
    FETCH_S(a1, 1)                         #  a1<- branch offset, in code units
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a2, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a2)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a2, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
3:
    bnez      a0, common_updateProfile
#else
    bgez      a2, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_LT: /* 0x34 */
/* File: mips/OP_IF_LT.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a2, a0)                       #  a2 <- vA
    bge a2, a3, 1f                  #  branch to 1 if comparison failed
    FETCH_S(a1, 1)                         #  a1<- branch offset, in code units
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a2, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a2)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a2, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
3:
    bnez      a0, common_updateProfile
#else
    bgez      a2, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_GE: /* 0x35 */
/* File: mips/OP_IF_GE.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a2, a0)                       #  a2 <- vA
    blt a2, a3, 1f                  #  branch to 1 if comparison failed
    FETCH_S(a1, 1)                         #  a1<- branch offset, in code units
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a2, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a2)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a2, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
3:
    bnez      a0, common_updateProfile
#else
    bgez      a2, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_GT: /* 0x36 */
/* File: mips/OP_IF_GT.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a2, a0)                       #  a2 <- vA
    ble a2, a3, 1f                  #  branch to 1 if comparison failed
    FETCH_S(a1, 1)                         #  a1<- branch offset, in code units
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a2, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a2)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a2, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
3:
    bnez      a0, common_updateProfile
#else
    bgez      a2, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_LE: /* 0x37 */
/* File: mips/OP_IF_LE.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a2, a0)                       #  a2 <- vA
    bgt a2, a3, 1f                  #  branch to 1 if comparison failed
    FETCH_S(a1, 1)                         #  a1<- branch offset, in code units
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a2, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a2)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a2, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
3:
    bnez      a0, common_updateProfile
#else
    bgez      a2, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_EQZ: /* 0x38 */
/* File: mips/OP_IF_EQZ.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a2, a0)                       #  a2 <- vAA
    FETCH_S(a1, 1)                         #  a1 <- branch offset, in code units
    bne a2, zero, 1f                #  branch to 1 if comparison failed
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a1, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a1, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh table base
3:
    bnez      a0, common_updateProfile     #  test for JIT off at target
#else
    bgez      a1, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rtable base
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_NEZ: /* 0x39 */
/* File: mips/OP_IF_NEZ.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a2, a0)                       #  a2 <- vAA
    FETCH_S(a1, 1)                         #  a1 <- branch offset, in code units
    beq a2, zero, 1f                #  branch to 1 if comparison failed
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a1, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a1, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh table base
3:
    bnez      a0, common_updateProfile     #  test for JIT off at target
#else
    bgez      a1, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rtable base
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_LTZ: /* 0x3a */
/* File: mips/OP_IF_LTZ.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a2, a0)                       #  a2 <- vAA
    FETCH_S(a1, 1)                         #  a1 <- branch offset, in code units
    bge a2, zero, 1f                #  branch to 1 if comparison failed
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a1, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a1, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh table base
3:
    bnez      a0, common_updateProfile     #  test for JIT off at target
#else
    bgez      a1, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rtable base
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_GEZ: /* 0x3b */
/* File: mips/OP_IF_GEZ.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a2, a0)                       #  a2 <- vAA
    FETCH_S(a1, 1)                         #  a1 <- branch offset, in code units
    blt a2, zero, 1f                #  branch to 1 if comparison failed
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a1, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a1, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh table base
3:
    bnez      a0, common_updateProfile     #  test for JIT off at target
#else
    bgez      a1, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rtable base
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_GTZ: /* 0x3c */
/* File: mips/OP_IF_GTZ.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a2, a0)                       #  a2 <- vAA
    FETCH_S(a1, 1)                         #  a1 <- branch offset, in code units
    ble a2, zero, 1f                #  branch to 1 if comparison failed
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a1, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a1, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh table base
3:
    bnez      a0, common_updateProfile     #  test for JIT off at target
#else
    bgez      a1, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rtable base
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IF_LEZ: /* 0x3d */
/* File: mips/OP_IF_LEZ.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a2, a0)                       #  a2 <- vAA
    FETCH_S(a1, 1)                         #  a1 <- branch offset, in code units
    bgt a2, zero, 1f                #  branch to 1 if comparison failed
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a1, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a1, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh table base
3:
    bnez      a0, common_updateProfile     #  test for JIT off at target
#else
    bgez      a1, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rtable base
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_UNUSED_3E: /* 0x3e */
/* File: mips/OP_UNUSED_3E.S */
/* File: mips/unused.S */
    BAL(common_abort)



/* ------------------------------ */
    .balign 128
.L_OP_UNUSED_3F: /* 0x3f */
/* File: mips/OP_UNUSED_3F.S */
/* File: mips/unused.S */
    BAL(common_abort)



/* ------------------------------ */
    .balign 128
.L_OP_UNUSED_40: /* 0x40 */
/* File: mips/OP_UNUSED_40.S */
/* File: mips/unused.S */
    BAL(common_abort)



/* ------------------------------ */
    .balign 128
.L_OP_UNUSED_41: /* 0x41 */
/* File: mips/OP_UNUSED_41.S */
/* File: mips/unused.S */
    BAL(common_abort)



/* ------------------------------ */
    .balign 128
.L_OP_UNUSED_42: /* 0x42 */
/* File: mips/OP_UNUSED_42.S */
/* File: mips/unused.S */
    BAL(common_abort)



/* ------------------------------ */
    .balign 128
.L_OP_UNUSED_43: /* 0x43 */
/* File: mips/OP_UNUSED_43.S */
/* File: mips/unused.S */
    BAL(common_abort)



/* ------------------------------ */
    .balign 128
.L_OP_AGET: /* 0x44 */
/* File: mips/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if 2
    EASN(a0, a0, a1, 2)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    lw a2, offArrayObject_contents(a0)  #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2


/* ------------------------------ */
    .balign 128
.L_OP_AGET_WIDE: /* 0x45 */
/* File: mips/OP_AGET_WIDE.S */
    /*
     * Array get, 64 bits.  vAA <- vBB[vCC].
     *
     * Arrays of long/double are 64-bit aligned.
     */
    /* aget-wide vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    EAS3(a0, a0, a1)                       #  a0 <- arrayObj + index*width
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail

.LOP_AGET_WIDE_finish:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64_off(a2, a3, a0, offArrayObject_contents)
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a2, a3, rOBJ)                  #  vAA/vAA+1 <- a2/a3
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_AGET_OBJECT: /* 0x46 */
/* File: mips/OP_AGET_OBJECT.S */
/* File: mips/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if 2
    EASN(a0, a0, a1, 2)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    lw a2, offArrayObject_contents(a0)  #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2



/* ------------------------------ */
    .balign 128
.L_OP_AGET_BOOLEAN: /* 0x47 */
/* File: mips/OP_AGET_BOOLEAN.S */
/* File: mips/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if 0
    EASN(a0, a0, a1, 0)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    lbu a2, offArrayObject_contents(a0)  #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2



/* ------------------------------ */
    .balign 128
.L_OP_AGET_BYTE: /* 0x48 */
/* File: mips/OP_AGET_BYTE.S */
/* File: mips/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if 0
    EASN(a0, a0, a1, 0)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    lb a2, offArrayObject_contents(a0)  #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2



/* ------------------------------ */
    .balign 128
.L_OP_AGET_CHAR: /* 0x49 */
/* File: mips/OP_AGET_CHAR.S */
/* File: mips/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if 1
    EASN(a0, a0, a1, 1)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    lhu a2, offArrayObject_contents(a0)  #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2



/* ------------------------------ */
    .balign 128
.L_OP_AGET_SHORT: /* 0x4a */
/* File: mips/OP_AGET_SHORT.S */
/* File: mips/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if 1
    EASN(a0, a0, a1, 1)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    lh a2, offArrayObject_contents(a0)  #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2



/* ------------------------------ */
    .balign 128
.L_OP_APUT: /* 0x4b */
/* File: mips/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if 2
    EASN(a0, a0, a1, 2)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    sw a2, offArrayObject_contents(a0) #  vBB[vCC] <- a2
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_APUT_WIDE: /* 0x4c */
/* File: mips/OP_APUT_WIDE.S */
    /*
     * Array put, 64 bits.  vBB[vCC] <- vAA.
     *
     * Arrays of long/double are 64-bit aligned, so it's okay to use STRD.
     */
    /* aput-wide vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t0)                            #  t0 <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    EAS3(a0, a0, a1)                       #  a0 <- arrayObj + index*width
    EAS2(rOBJ, rFP, t0)                    #  rOBJ <- &fp[AA]
    # compare unsigned index, length
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64(a2, a3, rOBJ)                   #  a2/a3 <- vAA/vAA+1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64_off(a2, a3, a0, offArrayObject_contents) #  a2/a3 <- vBB[vCC]
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_APUT_OBJECT: /* 0x4d */
/* File: mips/OP_APUT_OBJECT.S */
    /*
     * Store an object into an array.  vBB[vCC] <- vAA.
     *
     */
    /* op vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t1)                            #  t1 <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    GET_VREG(rINST, a2)                    #  rINST <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    GET_VREG(rBIX, t1)                     #  rBIX <- vAA
    # null array object?
    beqz      rINST, common_errNullObject  #  yes, bail

    LOAD_base_offArrayObject_length(a3, rINST) #  a3 <- arrayObj->length
    EAS2(rOBJ, rINST, a1)                  #  rOBJ <- arrayObj + index*width
    # compare unsigned index, length
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    /*
     * On entry:
     *  rINST = vBB (arrayObj)
     *  rBIX = vAA (obj)
     *  rOBJ = offset into array (vBB + vCC * width)
     */
    bnez      rBIX, .LOP_APUT_OBJECT_checks     #  yes, skip type checks
.LOP_APUT_OBJECT_finish:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    sw        rBIX, offArrayObject_contents(rOBJ) #  vBB[vCC] <- vAA
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_APUT_BOOLEAN: /* 0x4e */
/* File: mips/OP_APUT_BOOLEAN.S */
/* File: mips/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if 0
    EASN(a0, a0, a1, 0)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    sb a2, offArrayObject_contents(a0) #  vBB[vCC] <- a2
    GOTO_OPCODE(t0)                        #  jump to next instruction



/* ------------------------------ */
    .balign 128
.L_OP_APUT_BYTE: /* 0x4f */
/* File: mips/OP_APUT_BYTE.S */
/* File: mips/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if 0
    EASN(a0, a0, a1, 0)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    sb a2, offArrayObject_contents(a0) #  vBB[vCC] <- a2
    GOTO_OPCODE(t0)                        #  jump to next instruction



/* ------------------------------ */
    .balign 128
.L_OP_APUT_CHAR: /* 0x50 */
/* File: mips/OP_APUT_CHAR.S */
/* File: mips/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if 1
    EASN(a0, a0, a1, 1)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    sh a2, offArrayObject_contents(a0) #  vBB[vCC] <- a2
    GOTO_OPCODE(t0)                        #  jump to next instruction



/* ------------------------------ */
    .balign 128
.L_OP_APUT_SHORT: /* 0x51 */
/* File: mips/OP_APUT_SHORT.S */
/* File: mips/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if 1
    EASN(a0, a0, a1, 1)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    sh a2, offArrayObject_contents(a0) #  vBB[vCC] <- a2
    GOTO_OPCODE(t0)                        #  jump to next instruction



/* ------------------------------ */
    .balign 128
.L_OP_IGET: /* 0x52 */
/* File: mips/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IGET_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test results
    move      a0, v0
    bnez      v0, .LOP_IGET_finish
    b         common_exceptionThrown

/* ------------------------------ */
    .balign 128
.L_OP_IGET_WIDE: /* 0x53 */
/* File: mips/OP_IGET_WIDE.S */
    /*
     * Wide 32-bit instance field get.
     */
    # iget-wide vA, vB, field              /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IGET_WIDE_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test return code
    move      a0, v0
    bnez      v0, .LOP_IGET_WIDE_finish
    b         common_exceptionThrown

/* ------------------------------ */
    .balign 128
.L_OP_IGET_OBJECT: /* 0x54 */
/* File: mips/OP_IGET_OBJECT.S */
/* File: mips/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IGET_OBJECT_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test results
    move      a0, v0
    bnez      v0, .LOP_IGET_OBJECT_finish
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_IGET_BOOLEAN: /* 0x55 */
/* File: mips/OP_IGET_BOOLEAN.S */
/* File: mips/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IGET_BOOLEAN_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test results
    move      a0, v0
    bnez      v0, .LOP_IGET_BOOLEAN_finish
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_IGET_BYTE: /* 0x56 */
/* File: mips/OP_IGET_BYTE.S */
/* File: mips/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IGET_BYTE_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test results
    move      a0, v0
    bnez      v0, .LOP_IGET_BYTE_finish
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_IGET_CHAR: /* 0x57 */
/* File: mips/OP_IGET_CHAR.S */
/* File: mips/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IGET_CHAR_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test results
    move      a0, v0
    bnez      v0, .LOP_IGET_CHAR_finish
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_IGET_SHORT: /* 0x58 */
/* File: mips/OP_IGET_SHORT.S */
/* File: mips/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IGET_SHORT_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test results
    move      a0, v0
    bnez      v0, .LOP_IGET_SHORT_finish
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_IPUT: /* 0x59 */
/* File: mips/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IPUT_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # success?
    move      a0, v0
    bnez      v0, .LOP_IPUT_finish       #  yes, finish up
    b         common_exceptionThrown

/* ------------------------------ */
    .balign 128
.L_OP_IPUT_WIDE: /* 0x5a */
/* File: mips/OP_IPUT_WIDE.S */
    # iput-wide vA, vB, field              /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IPUT_WIDE_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # success?
    move      a0, v0
    bnez      v0, .LOP_IPUT_WIDE_finish       #  yes, finish up
    b         common_exceptionThrown

/* ------------------------------ */
    .balign 128
.L_OP_IPUT_OBJECT: /* 0x5b */
/* File: mips/OP_IPUT_OBJECT.S */
    /*
     * 32-bit instance field put.
     *
     * for: iput-object, iput-object-volatile
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IPUT_OBJECT_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # success?
    move      a0, v0
    bnez      v0, .LOP_IPUT_OBJECT_finish       #  yes, finish up
    b         common_exceptionThrown

/* ------------------------------ */
    .balign 128
.L_OP_IPUT_BOOLEAN: /* 0x5c */
/* File: mips/OP_IPUT_BOOLEAN.S */
/* File: mips/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IPUT_BOOLEAN_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # success?
    move      a0, v0
    bnez      v0, .LOP_IPUT_BOOLEAN_finish       #  yes, finish up
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_IPUT_BYTE: /* 0x5d */
/* File: mips/OP_IPUT_BYTE.S */
/* File: mips/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IPUT_BYTE_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # success?
    move      a0, v0
    bnez      v0, .LOP_IPUT_BYTE_finish       #  yes, finish up
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_IPUT_CHAR: /* 0x5e */
/* File: mips/OP_IPUT_CHAR.S */
/* File: mips/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IPUT_CHAR_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # success?
    move      a0, v0
    bnez      v0, .LOP_IPUT_CHAR_finish       #  yes, finish up
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_IPUT_SHORT: /* 0x5f */
/* File: mips/OP_IPUT_SHORT.S */
/* File: mips/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IPUT_SHORT_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # success?
    move      a0, v0
    bnez      v0, .LOP_IPUT_SHORT_finish       #  yes, finish up
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_SGET: /* 0x60 */
/* File: mips/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    # is resolved entry !null?
    bnez      a0, .LOP_SGET_finish

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    # success?
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SGET_finish            # resume

/* ------------------------------ */
    .balign 128
.L_OP_SGET_WIDE: /* 0x61 */
/* File: mips/OP_SGET_WIDE.S */
    /*
     * 64-bit SGET handler.
     */
    # sget-wide vAA, field                 /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    # is resolved entry null?
    bnez      a0, .LOP_SGET_WIDE_finish

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     *
     * Returns StaticField pointer in v0.
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    # success?
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif

    b        .LOP_SGET_WIDE_finish            # resume

/* ------------------------------ */
    .balign 128
.L_OP_SGET_OBJECT: /* 0x62 */
/* File: mips/OP_SGET_OBJECT.S */
/* File: mips/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    # is resolved entry !null?
    bnez      a0, .LOP_SGET_OBJECT_finish

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    # success?
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SGET_OBJECT_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_SGET_BOOLEAN: /* 0x63 */
/* File: mips/OP_SGET_BOOLEAN.S */
/* File: mips/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    # is resolved entry !null?
    bnez      a0, .LOP_SGET_BOOLEAN_finish

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    # success?
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SGET_BOOLEAN_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_SGET_BYTE: /* 0x64 */
/* File: mips/OP_SGET_BYTE.S */
/* File: mips/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    # is resolved entry !null?
    bnez      a0, .LOP_SGET_BYTE_finish

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    # success?
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SGET_BYTE_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_SGET_CHAR: /* 0x65 */
/* File: mips/OP_SGET_CHAR.S */
/* File: mips/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    # is resolved entry !null?
    bnez      a0, .LOP_SGET_CHAR_finish

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    # success?
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SGET_CHAR_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_SGET_SHORT: /* 0x66 */
/* File: mips/OP_SGET_SHORT.S */
/* File: mips/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    # is resolved entry !null?
    bnez      a0, .LOP_SGET_SHORT_finish

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    # success?
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SGET_SHORT_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_SPUT: /* 0x67 */
/* File: mips/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-object, sput-boolean, sput-byte, sput-char, sput-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    bnez      a0, .LOP_SPUT_finish       #  is resolved entry null?
    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() may throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    beqz      v0, common_exceptionThrown   #  success? no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SPUT_finish            # resume

/* ------------------------------ */
    .balign 128
.L_OP_SPUT_WIDE: /* 0x68 */
/* File: mips/OP_SPUT_WIDE.S */
    /*
     * 64-bit SPUT handler.
     */
    # sput-wide vAA, field                 /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    GET_OPA(t0)                            #  t0 <- AA
    LOAD_eas2(a2, rBIX, a1)                #  a2 <- resolved StaticField ptr
    EAS2(rOBJ, rFP, t0)                    #  rOBJ<- &fp[AA]
    # is resolved entry null?
    beqz      a2, .LOP_SPUT_WIDE_resolve      #  yes, do resolve
.LOP_SPUT_WIDE_finish:                        #  field ptr in a2, AA in rOBJ
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64(a0, a1, rOBJ)                   #  a0/a1 <- vAA/vAA+1
    GET_INST_OPCODE(rBIX)                  #  extract opcode from rINST
    .if 0
    addu    a2, offStaticField_value       #  a2<- pointer to data
    JAL(dvmQuasiAtomicSwap64Sync)          #  stores a0/a1 into addr a2
    .else
    STORE64_off(a0, a1, a2, offStaticField_value) #  field <- vAA/vAA+1
    .endif
    GOTO_OPCODE(rBIX)                      #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_OP_SPUT_OBJECT: /* 0x69 */
/* File: mips/OP_SPUT_OBJECT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput-object, sput-object-volatile
     */
    /* op vAA, field@BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    bnez      a0, .LOP_SPUT_OBJECT_finish       #  is resolved entry null?

    /* Continuation if the field has not yet been resolved.
     * a1:  BBBB field ref
     * rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() may throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    beqz      v0, common_exceptionThrown   #  success? no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b       .LOP_SPUT_OBJECT_finish             # resume


/* ------------------------------ */
    .balign 128
.L_OP_SPUT_BOOLEAN: /* 0x6a */
/* File: mips/OP_SPUT_BOOLEAN.S */
/* File: mips/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-object, sput-boolean, sput-byte, sput-char, sput-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    bnez      a0, .LOP_SPUT_BOOLEAN_finish       #  is resolved entry null?
    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() may throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    beqz      v0, common_exceptionThrown   #  success? no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SPUT_BOOLEAN_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_SPUT_BYTE: /* 0x6b */
/* File: mips/OP_SPUT_BYTE.S */
/* File: mips/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-object, sput-boolean, sput-byte, sput-char, sput-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    bnez      a0, .LOP_SPUT_BYTE_finish       #  is resolved entry null?
    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() may throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    beqz      v0, common_exceptionThrown   #  success? no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SPUT_BYTE_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_SPUT_CHAR: /* 0x6c */
/* File: mips/OP_SPUT_CHAR.S */
/* File: mips/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-object, sput-boolean, sput-byte, sput-char, sput-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    bnez      a0, .LOP_SPUT_CHAR_finish       #  is resolved entry null?
    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() may throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    beqz      v0, common_exceptionThrown   #  success? no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SPUT_CHAR_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_SPUT_SHORT: /* 0x6d */
/* File: mips/OP_SPUT_SHORT.S */
/* File: mips/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-object, sput-boolean, sput-byte, sput-char, sput-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    bnez      a0, .LOP_SPUT_SHORT_finish       #  is resolved entry null?
    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() may throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    beqz      v0, common_exceptionThrown   #  success? no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SPUT_SHORT_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_VIRTUAL: /* 0x6e */
/* File: mips/OP_INVOKE_VIRTUAL.S */
    /*
     * Handle a virtual method call.
     *
     * for: invoke-virtual, invoke-virtual/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op vAA, {vCCCC..v(CCCC+AA-1)}, meth  /* BBBB */
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResMethods(a3, a3) #  a3 <- pDvmDex->pResMethods
    FETCH(rBIX, 2)                         #  rBIX <- GFED or CCCC
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved baseMethod
    .if (!0)
    and       rBIX, rBIX, 15               #  rBIX <- D (or stays CCCC)
    .endif
    EXPORT_PC()                            #  must export for invoke
    # already resolved?
    bnez      a0, .LOP_INVOKE_VIRTUAL_continue     #  yes, continue on

    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    li        a2, METHOD_VIRTUAL           #  resolver method type
    JAL(dvmResolveMethod)                  #  v0 <- call(clazz, ref, flags)
    move      a0, v0
    # got null?
    bnez      v0, .LOP_INVOKE_VIRTUAL_continue     #  no, continue
    b         common_exceptionThrown       #  yes, handle exception

/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_SUPER: /* 0x6f */
/* File: mips/OP_INVOKE_SUPER.S */
    /*
     * Handle a "super" method call.
     *
     * for: invoke-super, invoke-super/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op vAA, {vCCCC..v(CCCC+AA-1)}, meth  /* BBBB */
    FETCH(t0, 2)                           #  t0 <- GFED or CCCC
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    .if (!0)
    and       t0, t0, 15                   #  t0 <- D (or stays CCCC)
    .endif
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResMethods(a3, a3) #  a3 <- pDvmDex->pResMethods
    GET_VREG(rOBJ, t0)                     #  rOBJ <- "this" ptr
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved baseMethod
    # null "this"?
    LOAD_rSELF_method(t1)                  #  t1 <- current method
    beqz      rOBJ, common_errNullObject   #  null "this", throw exception
    # cmp a0, 0; already resolved?
    LOAD_base_offMethod_clazz(rBIX, t1)    #  rBIX <- method->clazz
    EXPORT_PC()                            #  must export for invoke
    bnez      a0, .LOP_INVOKE_SUPER_continue     #  resolved, continue on

    move      a0, rBIX                     #  a0 <- method->clazz
    li        a2, METHOD_VIRTUAL           #  resolver method type
    JAL(dvmResolveMethod)                  #  v0 <- call(clazz, ref, flags)
    move      a0, v0
    # got null?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    b         .LOP_INVOKE_SUPER_continue

/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_DIRECT: /* 0x70 */
/* File: mips/OP_INVOKE_DIRECT.S */
    /*
     * Handle a direct method call.
     *
     * (We could defer the "is 'this' pointer null" test to the common
     * method invocation code, and use a flag to indicate that static
     * calls don't count.  If we do this as part of copying the arguments
     * out we could avoiding loading the first arg twice.)
     *
     * for: invoke-direct, invoke-direct/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op {vCCCC..v(CCCC+AA-1)}, meth       /* BBBB */
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResMethods(a3, a3) #  a3 <- pDvmDex->pResMethods
    FETCH(rBIX, 2)                         #  rBIX <- GFED or CCCC
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved methodToCall
    .if (!0)
    and       rBIX, rBIX, 15               #  rBIX <- D (or stays CCCC)
    .endif
    EXPORT_PC()                            #  must export for invoke
    GET_VREG(rOBJ, rBIX)                   #  rOBJ <- "this" ptr
    # already resolved?
    bnez      a0, 1f                       #  resolved, call the function

    lw        a3, offThread_method(rSELF)  #  a3 <- self->method
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    li        a2, METHOD_DIRECT            #  resolver method type
    JAL(dvmResolveMethod)                  #  v0 <- call(clazz, ref, flags)
    move      a0, v0
    # got null?
    beqz      v0, common_exceptionThrown   #  yes, handle exception

1:
    bnez      rOBJ, common_invokeMethodNoRange #  a0=method, rOBJ="this"
    b         common_errNullObject         #  yes, throw exception




/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_STATIC: /* 0x71 */
/* File: mips/OP_INVOKE_STATIC.S */
    /*
     * Handle a static method call.
     *
     * for: invoke-static, invoke-static/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op {vCCCC..v(CCCC+AA-1)}, meth       /* BBBB */
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResMethods(a3, a3) #  a3 <- pDvmDex->pResMethods
    li      rOBJ, 0                        #  null "this" in delay slot
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved methodToCall
#if defined(WITH_JIT)
    EAS2(rBIX, a3, a1)                     #  rBIX<- &resolved_metherToCall
#endif
    EXPORT_PC()                            #  must export for invoke
    # already resolved?
    bnez      a0, common_invokeMethodNoRange #  yes, continue on
    b         .LOP_INVOKE_STATIC_resolve

/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_INTERFACE: /* 0x72 */
/* File: mips/OP_INVOKE_INTERFACE.S */
    /*
     * Handle an interface method call.
     *
     * for: invoke-interface, invoke-interface/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(a2, 2)                           #  a2 <- FEDC or CCCC
    FETCH(a1, 1)                           #  a1 <- BBBB
    .if (!0)
    and       a2, 15                       #  a2 <- C (or stays CCCC)
    .endif
    EXPORT_PC()                            #  must export for invoke
    GET_VREG(rOBJ, a2)                     #  rOBJ <- first arg ("this")
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- methodClassDex
    LOAD_rSELF_method(a2)                  #  a2 <- method
    # null obj?
    beqz      rOBJ, common_errNullObject   #  yes, fail
    LOAD_base_offObject_clazz(a0, rOBJ)      #  a0 <- thisPtr->clazz
    JAL(dvmFindInterfaceMethodInCache)     #  v0 <- call(class, ref, method, dex)
    move      a0, v0
    # failed?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    b         common_invokeMethodNoRange #  (a0=method, rOBJ="this")

/* ------------------------------ */
    .balign 128
.L_OP_UNUSED_73: /* 0x73 */
/* File: mips/OP_UNUSED_73.S */
/* File: mips/unused.S */
    BAL(common_abort)



/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_VIRTUAL_RANGE: /* 0x74 */
/* File: mips/OP_INVOKE_VIRTUAL_RANGE.S */
/* File: mips/OP_INVOKE_VIRTUAL.S */
    /*
     * Handle a virtual method call.
     *
     * for: invoke-virtual, invoke-virtual/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op vAA, {vCCCC..v(CCCC+AA-1)}, meth  /* BBBB */
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResMethods(a3, a3) #  a3 <- pDvmDex->pResMethods
    FETCH(rBIX, 2)                         #  rBIX <- GFED or CCCC
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved baseMethod
    .if (!1)
    and       rBIX, rBIX, 15               #  rBIX <- D (or stays CCCC)
    .endif
    EXPORT_PC()                            #  must export for invoke
    # already resolved?
    bnez      a0, .LOP_INVOKE_VIRTUAL_RANGE_continue     #  yes, continue on

    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    li        a2, METHOD_VIRTUAL           #  resolver method type
    JAL(dvmResolveMethod)                  #  v0 <- call(clazz, ref, flags)
    move      a0, v0
    # got null?
    bnez      v0, .LOP_INVOKE_VIRTUAL_RANGE_continue     #  no, continue
    b         common_exceptionThrown       #  yes, handle exception


/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_SUPER_RANGE: /* 0x75 */
/* File: mips/OP_INVOKE_SUPER_RANGE.S */
/* File: mips/OP_INVOKE_SUPER.S */
    /*
     * Handle a "super" method call.
     *
     * for: invoke-super, invoke-super/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op vAA, {vCCCC..v(CCCC+AA-1)}, meth  /* BBBB */
    FETCH(t0, 2)                           #  t0 <- GFED or CCCC
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    .if (!1)
    and       t0, t0, 15                   #  t0 <- D (or stays CCCC)
    .endif
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResMethods(a3, a3) #  a3 <- pDvmDex->pResMethods
    GET_VREG(rOBJ, t0)                     #  rOBJ <- "this" ptr
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved baseMethod
    # null "this"?
    LOAD_rSELF_method(t1)                  #  t1 <- current method
    beqz      rOBJ, common_errNullObject   #  null "this", throw exception
    # cmp a0, 0; already resolved?
    LOAD_base_offMethod_clazz(rBIX, t1)    #  rBIX <- method->clazz
    EXPORT_PC()                            #  must export for invoke
    bnez      a0, .LOP_INVOKE_SUPER_RANGE_continue     #  resolved, continue on

    move      a0, rBIX                     #  a0 <- method->clazz
    li        a2, METHOD_VIRTUAL           #  resolver method type
    JAL(dvmResolveMethod)                  #  v0 <- call(clazz, ref, flags)
    move      a0, v0
    # got null?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    b         .LOP_INVOKE_SUPER_RANGE_continue


/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_DIRECT_RANGE: /* 0x76 */
/* File: mips/OP_INVOKE_DIRECT_RANGE.S */
/* File: mips/OP_INVOKE_DIRECT.S */
    /*
     * Handle a direct method call.
     *
     * (We could defer the "is 'this' pointer null" test to the common
     * method invocation code, and use a flag to indicate that static
     * calls don't count.  If we do this as part of copying the arguments
     * out we could avoiding loading the first arg twice.)
     *
     * for: invoke-direct, invoke-direct/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op {vCCCC..v(CCCC+AA-1)}, meth       /* BBBB */
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResMethods(a3, a3) #  a3 <- pDvmDex->pResMethods
    FETCH(rBIX, 2)                         #  rBIX <- GFED or CCCC
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved methodToCall
    .if (!1)
    and       rBIX, rBIX, 15               #  rBIX <- D (or stays CCCC)
    .endif
    EXPORT_PC()                            #  must export for invoke
    GET_VREG(rOBJ, rBIX)                   #  rOBJ <- "this" ptr
    # already resolved?
    bnez      a0, 1f                       #  resolved, call the function

    lw        a3, offThread_method(rSELF)  #  a3 <- self->method
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    li        a2, METHOD_DIRECT            #  resolver method type
    JAL(dvmResolveMethod)                  #  v0 <- call(clazz, ref, flags)
    move      a0, v0
    # got null?
    beqz      v0, common_exceptionThrown   #  yes, handle exception

1:
    bnez      rOBJ, common_invokeMethodRange #  a0=method, rOBJ="this"
    b         common_errNullObject         #  yes, throw exception





/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_STATIC_RANGE: /* 0x77 */
/* File: mips/OP_INVOKE_STATIC_RANGE.S */
/* File: mips/OP_INVOKE_STATIC.S */
    /*
     * Handle a static method call.
     *
     * for: invoke-static, invoke-static/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op {vCCCC..v(CCCC+AA-1)}, meth       /* BBBB */
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResMethods(a3, a3) #  a3 <- pDvmDex->pResMethods
    li      rOBJ, 0                        #  null "this" in delay slot
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved methodToCall
#if defined(WITH_JIT)
    EAS2(rBIX, a3, a1)                     #  rBIX<- &resolved_metherToCall
#endif
    EXPORT_PC()                            #  must export for invoke
    # already resolved?
    bnez      a0, common_invokeMethodRange #  yes, continue on
    b         .LOP_INVOKE_STATIC_RANGE_resolve


/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_INTERFACE_RANGE: /* 0x78 */
/* File: mips/OP_INVOKE_INTERFACE_RANGE.S */
/* File: mips/OP_INVOKE_INTERFACE.S */
    /*
     * Handle an interface method call.
     *
     * for: invoke-interface, invoke-interface/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(a2, 2)                           #  a2 <- FEDC or CCCC
    FETCH(a1, 1)                           #  a1 <- BBBB
    .if (!1)
    and       a2, 15                       #  a2 <- C (or stays CCCC)
    .endif
    EXPORT_PC()                            #  must export for invoke
    GET_VREG(rOBJ, a2)                     #  rOBJ <- first arg ("this")
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- methodClassDex
    LOAD_rSELF_method(a2)                  #  a2 <- method
    # null obj?
    beqz      rOBJ, common_errNullObject   #  yes, fail
    LOAD_base_offObject_clazz(a0, rOBJ)      #  a0 <- thisPtr->clazz
    JAL(dvmFindInterfaceMethodInCache)     #  v0 <- call(class, ref, method, dex)
    move      a0, v0
    # failed?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    b         common_invokeMethodRange #  (a0=method, rOBJ="this")


/* ------------------------------ */
    .balign 128
.L_OP_UNUSED_79: /* 0x79 */
/* File: mips/OP_UNUSED_79.S */
/* File: mips/unused.S */
    BAL(common_abort)



/* ------------------------------ */
    .balign 128
.L_OP_UNUSED_7A: /* 0x7a */
/* File: mips/OP_UNUSED_7A.S */
/* File: mips/unused.S */
    BAL(common_abort)



/* ------------------------------ */
    .balign 128
.L_OP_NEG_INT: /* 0x7b */
/* File: mips/OP_NEG_INT.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    negu a0, a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vAA <- result0
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_NOT_INT: /* 0x7c */
/* File: mips/OP_NOT_INT.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    not a0, a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vAA <- result0
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_NEG_LONG: /* 0x7d */
/* File: mips/OP_NEG_LONG.S */
/* File: mips/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0/a1".
     * This could be MIPS instruction or a function call.
     *
     * For: neg-long, not-long, neg-double, long-to-double, double-to-long
     */
    /* unop vA, vB */
    GET_OPA4(t1)                           #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    EAS2(rOBJ, rFP, t1)                    #  rOBJ <- &fp[A]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vAA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    negu v0, a0                              #  optional op
    negu v1, a1; sltu a0, zero, v0; subu v1, v1, a0                                 #  a0/a1 <- op, a2-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, rOBJ)      #  vAA <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-13 instructions */




/* ------------------------------ */
    .balign 128
.L_OP_NOT_LONG: /* 0x7e */
/* File: mips/OP_NOT_LONG.S */
/* File: mips/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0/a1".
     * This could be MIPS instruction or a function call.
     *
     * For: neg-long, not-long, neg-double, long-to-double, double-to-long
     */
    /* unop vA, vB */
    GET_OPA4(t1)                           #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    EAS2(rOBJ, rFP, t1)                    #  rOBJ <- &fp[A]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vAA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    not a0, a0                              #  optional op
    not a1, a1                                 #  a0/a1 <- op, a2-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, rOBJ)      #  vAA <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_NEG_FLOAT: /* 0x7f */
/* File: mips/OP_NEG_FLOAT.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    addu a0, a0, 0x80000000                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vAA <- result0
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_NEG_DOUBLE: /* 0x80 */
/* File: mips/OP_NEG_DOUBLE.S */
/* File: mips/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0/a1".
     * This could be MIPS instruction or a function call.
     *
     * For: neg-long, not-long, neg-double, long-to-double, double-to-long
     */
    /* unop vA, vB */
    GET_OPA4(t1)                           #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    EAS2(rOBJ, rFP, t1)                    #  rOBJ <- &fp[A]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vAA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    addu a1, a1, 0x80000000                                 #  a0/a1 <- op, a2-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, rOBJ)      #  vAA <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_INT_TO_LONG: /* 0x81 */
/* File: mips/OP_INT_TO_LONG.S */
/* File: mips/unopWider.S */
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op a0", where
     * "result" is a 64-bit quantity in a0/a1.
     *
     * For: int-to-long, int-to-double, float-to-long, float-to-double
     */
    /* unop vA, vB */
    GET_OPA4(t1)                           #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, a3)                       #  a0 <- vB
    EAS2(rOBJ, rFP, t1)                    #  rOBJ <- &fp[A]
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    sra a1, a0, 31                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, rOBJ)      #  vA/vA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-11 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_INT_TO_FLOAT: /* 0x82 */
/* File: mips/OP_INT_TO_FLOAT.S */
/* File: mips/unflop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-float, float-to-int
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  t0 <- A+
#ifdef SOFT_FLOAT
    GET_VREG(a0, a3)                       #  a0 <- vB
#else
    GET_VREG_F(fa0, a3)
#endif
                                  #  optional op
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
#ifdef SOFT_FLOAT
    JAL(__floatsisf)                                 #  a0 <- op, a0-a3 changed

.LOP_INT_TO_FLOAT_set_vreg:
    SET_VREG(v0, rOBJ)                     #  vAA <- result0
#else
    cvt.s.w fv0, fa0

.LOP_INT_TO_FLOAT_set_vreg_f:
    SET_VREG_F(fv0, rOBJ)
#endif
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    GOTO_OPCODE(t1)                        #  jump to next instruction
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_INT_TO_DOUBLE: /* 0x83 */
/* File: mips/OP_INT_TO_DOUBLE.S */
/* File: mips/unflopWider.S */
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op a0", where
     * "result" is a 64-bit quantity in a0/a1.
     *
     * For: int-to-double, float-to-long, float-to-double
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
#ifdef SOFT_FLOAT
    GET_VREG(a0, a3)                       #  a0 <- vB
#else
    GET_VREG_F(fa0, a3)
#endif
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__floatsidf)                                 #  result <- op, a0-a3 changed

.LOP_INT_TO_DOUBLE_set_vreg:
    STORE64(rRESULT0, rRESULT1, rOBJ)      #  vA/vA+1 <- a0/a1
#else
    cvt.d.w fv0, fa0

.LOP_INT_TO_DOUBLE_set_vreg:
    STORE64_F(fv0, fv0f, rOBJ)                             #  vA/vA+1 <- a0/a1
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-11 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_LONG_TO_INT: /* 0x84 */
/* File: mips/OP_LONG_TO_INT.S */
    GET_OPB(a1)                            #  a1 <- B from 15:12
    GET_OPA4(a0)                           #  a0 <- A from 11:8
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
#ifdef HAVE_BIG_ENDIAN
    addu      a1, a1, 1
#endif
    GET_VREG(a2, a1)                       #  a2 <- fp[B]
    GET_INST_OPCODE(t0)                    #  t0 <- opcode from rINST
    SET_VREG_GOTO(a2, a0, t0)              #  fp[A] <- a2

/* ------------------------------ */
    .balign 128
.L_OP_LONG_TO_FLOAT: /* 0x85 */
/* File: mips/OP_LONG_TO_FLOAT.S */
/* File: mips/unopNarrower.S */
    /*
     * Generic 64bit-to-32bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op a0/a1", where
     * "result" is a 32-bit quantity in a0.
     *
     * For: long-to-float, double-to-int, double-to-float
     * If hard floating point support is available, use fa0 as the parameter, except for
     * long-to-float opcode.
     * (This would work for long-to-int, but that instruction is actually
     * an exact match for OP_MOVE.)
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  t1 <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a3)               #  a0/a1 <- vB/vB+1
#else
    LOAD64(rARG0, rARG1, a3)
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__floatdisf)                                 #  a0 <- op, a0-a3 changed

.LOP_LONG_TO_FLOAT_set_vreg:
    SET_VREG(v0, rOBJ)                     #  vA <- result0
#else
    JAL(__floatdisf)

.LOP_LONG_TO_FLOAT_set_vreg_f:
    SET_VREG_F(fv0, rOBJ)                  #  vA <- result0
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-11 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_LONG_TO_DOUBLE: /* 0x86 */
/* File: mips/OP_LONG_TO_DOUBLE.S */
/* File: mips/unflopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0/a1".
     * This could be a MIPS instruction or a function call.
     *
     * long-to-double, double-to-long
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  t1 <- &fp[A]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a3)               #  a0/a1 <- vAA
#else
    LOAD64(rARG0, rARG1, a3)
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    JAL(__floatdidf)                                 #  a0/a1 <- op, a2-a3 changed

.LOP_LONG_TO_DOUBLE_set_vreg:
#ifdef SOFT_FLOAT
    STORE64(rRESULT0, rRESULT1, rOBJ)      #  vAA <- a0/a1
#else
    STORE64_F(fv0, fv0f, rOBJ)                             #  vAA <- a0/a1
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_FLOAT_TO_INT: /* 0x87 */
/* File: mips/OP_FLOAT_TO_INT.S */
/* File: mips/unflop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-float, float-to-int
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  t0 <- A+
#ifdef SOFT_FLOAT
    GET_VREG(a0, a3)                       #  a0 <- vB
#else
    GET_VREG_F(fa0, a3)
#endif
                                  #  optional op
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
#ifdef SOFT_FLOAT
    b f2i_doconv                                 #  a0 <- op, a0-a3 changed

.LOP_FLOAT_TO_INT_set_vreg:
    SET_VREG(v0, rOBJ)                     #  vAA <- result0
#else
    b f2i_doconv

.LOP_FLOAT_TO_INT_set_vreg_f:
    SET_VREG_F(fv0, rOBJ)
#endif
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    GOTO_OPCODE(t1)                        #  jump to next instruction
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_FLOAT_TO_LONG: /* 0x88 */
/* File: mips/OP_FLOAT_TO_LONG.S */
/* File: mips/unflopWider.S */
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op a0", where
     * "result" is a 64-bit quantity in a0/a1.
     *
     * For: int-to-double, float-to-long, float-to-double
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
#ifdef SOFT_FLOAT
    GET_VREG(a0, a3)                       #  a0 <- vB
#else
    GET_VREG_F(fa0, a3)
#endif
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    b f2l_doconv                                 #  result <- op, a0-a3 changed

.LOP_FLOAT_TO_LONG_set_vreg:
    STORE64(rRESULT0, rRESULT1, rOBJ)      #  vA/vA+1 <- a0/a1
#else
    b f2l_doconv

.LOP_FLOAT_TO_LONG_set_vreg:
    STORE64(rRESULT0, rRESULT1, rOBJ)                             #  vA/vA+1 <- a0/a1
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-11 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_FLOAT_TO_DOUBLE: /* 0x89 */
/* File: mips/OP_FLOAT_TO_DOUBLE.S */
/* File: mips/unflopWider.S */
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op a0", where
     * "result" is a 64-bit quantity in a0/a1.
     *
     * For: int-to-double, float-to-long, float-to-double
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
#ifdef SOFT_FLOAT
    GET_VREG(a0, a3)                       #  a0 <- vB
#else
    GET_VREG_F(fa0, a3)
#endif
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__extendsfdf2)                                 #  result <- op, a0-a3 changed

.LOP_FLOAT_TO_DOUBLE_set_vreg:
    STORE64(rRESULT0, rRESULT1, rOBJ)      #  vA/vA+1 <- a0/a1
#else
    cvt.d.s fv0, fa0

.LOP_FLOAT_TO_DOUBLE_set_vreg:
    STORE64_F(fv0, fv0f, rOBJ)                             #  vA/vA+1 <- a0/a1
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-11 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_DOUBLE_TO_INT: /* 0x8a */
/* File: mips/OP_DOUBLE_TO_INT.S */
/* File: mips/unopNarrower.S */
    /*
     * Generic 64bit-to-32bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op a0/a1", where
     * "result" is a 32-bit quantity in a0.
     *
     * For: long-to-float, double-to-int, double-to-float
     * If hard floating point support is available, use fa0 as the parameter, except for
     * long-to-float opcode.
     * (This would work for long-to-int, but that instruction is actually
     * an exact match for OP_MOVE.)
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  t1 <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a3)               #  a0/a1 <- vB/vB+1
#else
    LOAD64_F(fa0, fa0f, a3)
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    b d2i_doconv                                 #  a0 <- op, a0-a3 changed

.LOP_DOUBLE_TO_INT_set_vreg:
    SET_VREG(v0, rOBJ)                     #  vA <- result0
#else
    b d2i_doconv

.LOP_DOUBLE_TO_INT_set_vreg_f:
    SET_VREG_F(fv0, rOBJ)                  #  vA <- result0
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-11 instructions */

/*
 * Convert the double in a0/a1 to an int in a0.
 *
 * We have to clip values to int min/max per the specification.  The
 * expected common case is a "reasonable" value that converts directly
 * to modest integer.  The EABI convert function isn't doing this for us.
 * Use rBIX / rTEMP as global to hold arguments (they are not bound to a global var)
 */

/* ------------------------------ */
    .balign 128
.L_OP_DOUBLE_TO_LONG: /* 0x8b */
/* File: mips/OP_DOUBLE_TO_LONG.S */
/* File: mips/unflopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0/a1".
     * This could be a MIPS instruction or a function call.
     *
     * long-to-double, double-to-long
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  t1 <- &fp[A]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a3)               #  a0/a1 <- vAA
#else
    LOAD64_F(fa0, fa0f, a3)
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    b d2l_doconv                                 #  a0/a1 <- op, a2-a3 changed

.LOP_DOUBLE_TO_LONG_set_vreg:
#ifdef SOFT_FLOAT
    STORE64(rRESULT0, rRESULT1, rOBJ)      #  vAA <- a0/a1
#else
    STORE64(rRESULT0, rRESULT1, rOBJ)                             #  vAA <- a0/a1
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_DOUBLE_TO_FLOAT: /* 0x8c */
/* File: mips/OP_DOUBLE_TO_FLOAT.S */
/* File: mips/unopNarrower.S */
    /*
     * Generic 64bit-to-32bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op a0/a1", where
     * "result" is a 32-bit quantity in a0.
     *
     * For: long-to-float, double-to-int, double-to-float
     * If hard floating point support is available, use fa0 as the parameter, except for
     * long-to-float opcode.
     * (This would work for long-to-int, but that instruction is actually
     * an exact match for OP_MOVE.)
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  t1 <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a3)               #  a0/a1 <- vB/vB+1
#else
    LOAD64_F(fa0, fa0f, a3)
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__truncdfsf2)                                 #  a0 <- op, a0-a3 changed

.LOP_DOUBLE_TO_FLOAT_set_vreg:
    SET_VREG(v0, rOBJ)                     #  vA <- result0
#else
    cvt.s.d fv0, fa0

.LOP_DOUBLE_TO_FLOAT_set_vreg_f:
    SET_VREG_F(fv0, rOBJ)                  #  vA <- result0
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-11 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_INT_TO_BYTE: /* 0x8d */
/* File: mips/OP_INT_TO_BYTE.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    sll a0, a0, 24                              #  optional op
    sra a0, a0, 24                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vAA <- result0
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_INT_TO_CHAR: /* 0x8e */
/* File: mips/OP_INT_TO_CHAR.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    and a0, 0xffff                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vAA <- result0
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_INT_TO_SHORT: /* 0x8f */
/* File: mips/OP_INT_TO_SHORT.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    sll a0, 16                              #  optional op
    sra a0, 16                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vAA <- result0
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 128
.L_OP_ADD_INT: /* 0x90 */
/* File: mips/OP_ADD_INT.S */
/* File: mips/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    addu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SUB_INT: /* 0x91 */
/* File: mips/OP_SUB_INT.S */
/* File: mips/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    subu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_MUL_INT: /* 0x92 */
/* File: mips/OP_MUL_INT.S */
/* File: mips/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    mul a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_DIV_INT: /* 0x93 */
/* File: mips/OP_DIV_INT.S */
/* File: mips/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    div zero, a0, a1; mflo a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_REM_INT: /* 0x94 */
/* File: mips/OP_REM_INT.S */
/* File: mips/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    div zero, a0, a1; mfhi a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_AND_INT: /* 0x95 */
/* File: mips/OP_AND_INT.S */
/* File: mips/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    and a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_OR_INT: /* 0x96 */
/* File: mips/OP_OR_INT.S */
/* File: mips/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    or a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_XOR_INT: /* 0x97 */
/* File: mips/OP_XOR_INT.S */
/* File: mips/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    xor a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SHL_INT: /* 0x98 */
/* File: mips/OP_SHL_INT.S */
/* File: mips/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    and a1, a1, 31                              #  optional op
    sll a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SHR_INT: /* 0x99 */
/* File: mips/OP_SHR_INT.S */
/* File: mips/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    and a1, a1, 31                              #  optional op
    sra a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_USHR_INT: /* 0x9a */
/* File: mips/OP_USHR_INT.S */
/* File: mips/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    and a1, a1, 31                              #  optional op
    srl a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_ADD_LONG: /* 0x9b */
/* File: mips/OP_ADD_LONG.S */
/*
 *  The compiler generates the following sequence for
 *  [v1 v0] =  [a1 a0] + [a3 a2];
 *    addu v0,a2,a0
 *    addu a1,a3,a1
 *    sltu v1,v0,a2
 *    addu v1,v1,a1
 */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    addu v0, a2, a0                              #  optional op
    addu a1, a3, a1; sltu v1, v0, a2; addu v1, v1, a1                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, rOBJ)      #  vAA/vAA+1 <- v0/v1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SUB_LONG: /* 0x9c */
/* File: mips/OP_SUB_LONG.S */
/*
 * For little endian the code sequence looks as follows:
 *    subu    v0,a0,a2
 *    subu    v1,a1,a3
 *    sltu    a0,a0,v0
 *    subu    v1,v1,a0
 */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    subu v0, a0, a2                              #  optional op
    subu v1, a1, a3; sltu a0, a0, v0; subu v1, v1, a0                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, rOBJ)      #  vAA/vAA+1 <- v0/v1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */




/* ------------------------------ */
    .balign 128
.L_OP_MUL_LONG: /* 0x9d */
/* File: mips/OP_MUL_LONG.S */
    /*
     * Signed 64-bit integer multiply.
     *         a1   a0
     *   x     a3   a2
     *   -------------
     *       a2a1 a2a0
     *       a3a0
     *  a3a1 (<= unused)
     *  ---------------
     *         v1   v0
     */
    /* mul-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    and       t0, a0, 255                  #  a2 <- BB
    srl       t1, a0, 8                    #  a3 <- CC
    EAS2(t0, rFP, t0)                      #  t0 <- &fp[BB]
    LOAD64(a0, a1, t0)                     #  a0/a1 <- vBB/vBB+1

    EAS2(t1, rFP, t1)                      #  t0 <- &fp[CC]
    LOAD64(a2, a3, t1)                     #  a2/a3 <- vCC/vCC+1

    mul       v1, a3, a0                   #  v1= a3a0
    multu     a2, a0
    mfhi      t1
    mflo      v0                           #  v0= a2a0
    mul       t0, a2, a1                   #  t0= a2a1
    addu      v1, v1, t1                   #  v1+= hi(a2a0)
    addu      v1, v1, t0                   #  v1= a3a0 + a2a1;

    GET_OPA(a0)                            #  a0 <- AA
    EAS2(a0, rFP, a0)                      #  a0 <- &fp[A]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    b         .LOP_MUL_LONG_finish

/* ------------------------------ */
    .balign 128
.L_OP_DIV_LONG: /* 0x9e */
/* File: mips/OP_DIV_LONG.S */
#ifdef HAVE_LITTLE_ENDIAN
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 1
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__divdi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, rOBJ)      #  vAA/vAA+1 <- v0/v1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */


#else
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a1, a0, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a3, a2, t1)               #  a2/a3 <- vCC/vCC+1
    .if 1
    or        t0, a3, a2             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__divdi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v1, v0, rOBJ)      #  vAA/vAA+1 <- v1/v0
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */


#endif

/* ------------------------------ */
    .balign 128
.L_OP_REM_LONG: /* 0x9f */
/* File: mips/OP_REM_LONG.S */
/* ldivmod returns quotient in a0/a1 and remainder in a2/a3 */
#ifdef HAVE_LITTLE_ENDIAN
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 1
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__moddi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, rOBJ)      #  vAA/vAA+1 <- v0/v1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */


#else
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a1, a0, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a3, a2, t1)               #  a2/a3 <- vCC/vCC+1
    .if 1
    or        t0, a3, a2             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__moddi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v1, v0, rOBJ)      #  vAA/vAA+1 <- v1/v0
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */


#endif

/* ------------------------------ */
    .balign 128
.L_OP_AND_LONG: /* 0xa0 */
/* File: mips/OP_AND_LONG.S */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    and a0, a0, a2                              #  optional op
    and a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, rOBJ)      #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_OR_LONG: /* 0xa1 */
/* File: mips/OP_OR_LONG.S */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    or a0, a0, a2                              #  optional op
    or a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, rOBJ)      #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_XOR_LONG: /* 0xa2 */
/* File: mips/OP_XOR_LONG.S */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    xor a0, a0, a2                              #  optional op
    xor a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, rOBJ)      #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SHL_LONG: /* 0xa3 */
/* File: mips/OP_SHL_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shl-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t2)                            #  t2 <- AA
    and       a3, a0, 255                  #  a3 <- BB
    srl       a0, a0, 8                    #  a0 <- CC
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BB]
    GET_VREG(a2, a0)                       #  a2 <- vCC
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vBB/vBB+1

    EAS2(t2, rFP, t2)                      #  t2 <- &fp[AA]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    sll     v0, a0, a2                     #  rlo<- alo << (shift&31)
    not     v1, a2                         #  rhi<- 31-shift  (shift is 5b)
    srl     a0, 1
    srl     a0, v1                         #  alo<- alo >> (32-(shift&31))
    sll     v1, a1, a2                     #  rhi<- ahi << (shift&31)
    or      v1, a0                         #  rhi<- rhi | alo
    andi    a2, 0x20                       #  shift< shift & 0x20
    movn    v1, v0, a2                     #  rhi<- rlo (if shift&0x20)
    movn    v0, zero, a2                   #  rlo<- 0  (if shift&0x20)

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, t2)                    #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_SHR_LONG: /* 0xa4 */
/* File: mips/OP_SHR_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shr-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t3)                            #  t3 <- AA
    and       a3, a0, 255                  #  a3 <- BB
    srl       a0, a0, 8                    #  a0 <- CC
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BB]
    GET_VREG(a2, a0)                       #  a2 <- vCC
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vBB/vBB+1
    EAS2(t3, rFP, t3)                      #  t3 <- &fp[AA]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    sra     v1, a1, a2                     #  rhi<- ahi >> (shift&31)
    srl     v0, a0, a2                     #  rlo<- alo >> (shift&31)
    sra     a3, a1, 31                     #  a3<- sign(ah)
    not     a0, a2                         #  alo<- 31-shift (shift is 5b)
    sll     a1, 1
    sll     a1, a0                         #  ahi<- ahi << (32-(shift&31))
    or      v0, a1                         #  rlo<- rlo | ahi
    andi    a2, 0x20                       #  shift & 0x20
    movn    v0, v1, a2                     #  rlo<- rhi (if shift&0x20)
    movn    v1, a3, a2                     #  rhi<- sign(ahi) (if shift&0x20)

    STORE64(v0, v1, t3)                    #  vAA/VAA+1 <- v0/v0
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_USHR_LONG: /* 0xa5 */
/* File: mips/OP_USHR_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* ushr-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t0)                            #  t3 <- AA
    and       a3, a0, 255                  #  a3 <- BB
    srl       a0, a0, 8                    #  a0 <- CC
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BB]
    GET_VREG(a2, a0)                       #  a2 <- vCC
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vBB/vBB+1
    EAS2(rOBJ, rFP, t0)                    #  rOBJ <- &fp[AA]

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    srl       v1, a1, a2                   #  rhi<- ahi >> (shift&31)
    srl       v0, a0, a2                   #  rlo<- alo >> (shift&31)
    not       a0, a2                       #  alo<- 31-n  (shift is 5b)
    sll       a1, 1
    sll       a1, a0                       #  ahi<- ahi << (32-(shift&31))
    or        v0, a1                       #  rlo<- rlo | ahi
    andi      a2, 0x20                     #  shift & 0x20
    movn      v0, v1, a2                   #  rlo<- rhi (if shift&0x20)
    movn      v1, zero, a2                 #  rhi<- 0 (if shift&0x20)

    STORE64(v0, v1, rOBJ)                  #  vAA/vAA+1 <- v0/v1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_ADD_FLOAT: /* 0xa6 */
/* File: mips/OP_ADD_FLOAT.S */
/* File: mips/binflop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
#ifdef SOFT_FLOAT
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    .if 0
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1               #  condition bit and comparision with 0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__addsf3)                                 #  v0 = result
    SET_VREG(v0, rOBJ)                     #  vAA <- v0
#else
    add.s fv0, fa0, fa1                               #  f0 = result
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- fv0
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SUB_FLOAT: /* 0xa7 */
/* File: mips/OP_SUB_FLOAT.S */
/* File: mips/binflop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
#ifdef SOFT_FLOAT
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    .if 0
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1               #  condition bit and comparision with 0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__subsf3)                                 #  v0 = result
    SET_VREG(v0, rOBJ)                     #  vAA <- v0
#else
    sub.s fv0, fa0, fa1                               #  f0 = result
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- fv0
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_MUL_FLOAT: /* 0xa8 */
/* File: mips/OP_MUL_FLOAT.S */
/* File: mips/binflop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
#ifdef SOFT_FLOAT
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    .if 0
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1               #  condition bit and comparision with 0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__mulsf3)                                 #  v0 = result
    SET_VREG(v0, rOBJ)                     #  vAA <- v0
#else
    mul.s fv0, fa0, fa1                               #  f0 = result
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- fv0
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_DIV_FLOAT: /* 0xa9 */
/* File: mips/OP_DIV_FLOAT.S */
/* File: mips/binflop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
#ifdef SOFT_FLOAT
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    .if 0
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1               #  condition bit and comparision with 0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__divsf3)                                 #  v0 = result
    SET_VREG(v0, rOBJ)                     #  vAA <- v0
#else
    div.s fv0, fa0, fa1                               #  f0 = result
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- fv0
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_REM_FLOAT: /* 0xaa */
/* File: mips/OP_REM_FLOAT.S */
/* File: mips/binflop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
#ifdef SOFT_FLOAT
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    .if 0
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1               #  condition bit and comparision with 0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(fmodf)                                 #  v0 = result
    SET_VREG(v0, rOBJ)                     #  vAA <- v0
#else
    JAL(fmodf)                               #  f0 = result
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- fv0
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 11-14 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_ADD_DOUBLE: /* 0xab */
/* File: mips/OP_ADD_DOUBLE.S */
/* File: mips/binflopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  s5 <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)
    .if 0
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__adddf3)                                 #  result <- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    add.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SUB_DOUBLE: /* 0xac */
/* File: mips/OP_SUB_DOUBLE.S */
/* File: mips/binflopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  s5 <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)
    .if 0
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__subdf3)                                 #  result <- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    sub.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_MUL_DOUBLE: /* 0xad */
/* File: mips/OP_MUL_DOUBLE.S */
/* File: mips/binflopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  s5 <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)
    .if 0
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__muldf3)                                 #  result <- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    mul.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_DIV_DOUBLE: /* 0xae */
/* File: mips/OP_DIV_DOUBLE.S */
/* File: mips/binflopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  s5 <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)
    .if 0
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__divdf3)                                 #  result <- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    div.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_REM_DOUBLE: /* 0xaf */
/* File: mips/OP_REM_DOUBLE.S */
/* File: mips/binflopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  s5 <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)
    .if 0
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(fmod)                                 #  result <- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    JAL(fmod)
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_ADD_INT_2ADDR: /* 0xb0 */
/* File: mips/OP_ADD_INT_2ADDR.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    addu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SUB_INT_2ADDR: /* 0xb1 */
/* File: mips/OP_SUB_INT_2ADDR.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    subu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_MUL_INT_2ADDR: /* 0xb2 */
/* File: mips/OP_MUL_INT_2ADDR.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    mul a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_DIV_INT_2ADDR: /* 0xb3 */
/* File: mips/OP_DIV_INT_2ADDR.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    div zero, a0, a1; mflo a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_REM_INT_2ADDR: /* 0xb4 */
/* File: mips/OP_REM_INT_2ADDR.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    div zero, a0, a1; mfhi a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_AND_INT_2ADDR: /* 0xb5 */
/* File: mips/OP_AND_INT_2ADDR.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    and a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_OR_INT_2ADDR: /* 0xb6 */
/* File: mips/OP_OR_INT_2ADDR.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    or a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_XOR_INT_2ADDR: /* 0xb7 */
/* File: mips/OP_XOR_INT_2ADDR.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    xor a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SHL_INT_2ADDR: /* 0xb8 */
/* File: mips/OP_SHL_INT_2ADDR.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    and a1, a1, 31                              #  optional op
    sll a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SHR_INT_2ADDR: /* 0xb9 */
/* File: mips/OP_SHR_INT_2ADDR.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    and a1, a1, 31                              #  optional op
    sra a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_USHR_INT_2ADDR: /* 0xba */
/* File: mips/OP_USHR_INT_2ADDR.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    and a1, a1, 31                              #  optional op
    srl a0, a0, a1                                  #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_ADD_LONG_2ADDR: /* 0xbb */
/* File: mips/OP_ADD_LONG_2ADDR.S */
/*
 *See OP_ADD_LONG.S for details
 */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(a0, a1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    addu v0, a2, a0                              #  optional op
    addu a1, a3, a1; sltu v1, v0, a2; addu v1, v1, a1                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, rOBJ)      #  vAA/vAA+1 <- v0/v1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SUB_LONG_2ADDR: /* 0xbc */
/* File: mips/OP_SUB_LONG_2ADDR.S */
/*
 * See comments in OP_SUB_LONG.S
 */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(a0, a1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    subu v0, a0, a2                              #  optional op
    subu v1, a1, a3; sltu a0, a0, v0; subu v1, v1, a0                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, rOBJ)      #  vAA/vAA+1 <- v0/v1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_MUL_LONG_2ADDR: /* 0xbd */
/* File: mips/OP_MUL_LONG_2ADDR.S */
    /*
     * See comments in OP_MUL_LONG.S
     */
    /* mul-long/2addr vA, vB */
    GET_OPA4(t0)                           #  t0 <- A+

    EAS2(t0, rFP, t0)                      #  t0 <- &fp[A]
    LOAD64(a0, a1, t0)                     #  vAA.low / high

    GET_OPB(t1)                            #  t1 <- B
    EAS2(t1, rFP, t1)                      #  t1 <- &fp[B]
    LOAD64(a2, a3, t1)                     #  vBB.low / high

    mul       v1, a3, a0                   #  v1= a3a0
    multu     a2, a0
    mfhi      t1
    mflo      v0                           #  v0= a2a0
    mul       t2, a2, a1                   #  t2= a2a1
    addu      v1, v1, t1                   #  v1= a3a0 + hi(a2a0)
    addu      v1, v1, t2                   #  v1= v1 + a2a1;

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    # vAA <- v0 (low)
    STORE64(v0, v1, t0)                    #  vAA+1 <- v1 (high)
    GOTO_OPCODE(t1)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_DIV_LONG_2ADDR: /* 0xbe */
/* File: mips/OP_DIV_LONG_2ADDR.S */
#ifdef HAVE_LITTLE_ENDIAN
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(a0, a1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 1
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__divdi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, rOBJ)      #  vAA/vAA+1 <- v0/v1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */


#else
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    LOAD64(a3, a2, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(a1, a0, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 1
    or        t0, a3, a2             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__divdi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v1, v0, rOBJ)      #  vAA/vAA+1 <- v1/v0
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */


#endif

/* ------------------------------ */
    .balign 128
.L_OP_REM_LONG_2ADDR: /* 0xbf */
/* File: mips/OP_REM_LONG_2ADDR.S */
#ifdef HAVE_LITTLE_ENDIAN
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(a0, a1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 1
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__moddi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, rOBJ)      #  vAA/vAA+1 <- v0/v1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */


#else
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    LOAD64(a3, a2, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(a1, a0, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 1
    or        t0, a3, a2             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__moddi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v1, v0, rOBJ)      #  vAA/vAA+1 <- v1/v0
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */


#endif

/* ------------------------------ */
    .balign 128
.L_OP_AND_LONG_2ADDR: /* 0xc0 */
/* File: mips/OP_AND_LONG_2ADDR.S */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(a0, a1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    and a0, a0, a2                              #  optional op
    and a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, rOBJ)      #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_OR_LONG_2ADDR: /* 0xc1 */
/* File: mips/OP_OR_LONG_2ADDR.S */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(a0, a1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    or a0, a0, a2                              #  optional op
    or a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, rOBJ)      #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_XOR_LONG_2ADDR: /* 0xc2 */
/* File: mips/OP_XOR_LONG_2ADDR.S */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(a0, a1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    xor a0, a0, a2                              #  optional op
    xor a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, rOBJ)      #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SHL_LONG_2ADDR: /* 0xc3 */
/* File: mips/OP_SHL_LONG_2ADDR.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shl-long/2addr vA, vB */
    GET_OPA4(t2)                           #  t2 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(rOBJ, rFP, t2)                    #  rOBJ <- &fp[A]
    LOAD64(a0, a1, rOBJ)                   #  a0/a1 <- vAA/vAA+1

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    sll     v0, a0, a2                     #  rlo<- alo << (shift&31)
    not     v1, a2                         #  rhi<- 31-shift  (shift is 5b)
    srl     a0, 1
    srl     a0, v1                         #  alo<- alo >> (32-(shift&31))
    sll     v1, a1, a2                     #  rhi<- ahi << (shift&31)
    or      v1, a0                         #  rhi<- rhi | alo
    andi    a2, 0x20                       #  shift< shift & 0x20
    movn    v1, v0, a2                     #  rhi<- rlo (if shift&0x20)
    movn    v0, zero, a2                   #  rlo<- 0  (if shift&0x20)

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, rOBJ)                  #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_SHR_LONG_2ADDR: /* 0xc4 */
/* File: mips/OP_SHR_LONG_2ADDR.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shr-long/2addr vA, vB */
    GET_OPA4(t2)                           #  t2 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(t2, rFP, t2)                      #  t2 <- &fp[A]
    LOAD64(a0, a1, t2)                     #  a0/a1 <- vAA/vAA+1
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    sra     v1, a1, a2                     #  rhi<- ahi >> (shift&31)
    srl     v0, a0, a2                     #  rlo<- alo >> (shift&31)
    sra     a3, a1, 31                     #  a3<- sign(ah)
    not     a0, a2                         #  alo<- 31-shift (shift is 5b)
    sll     a1, 1
    sll     a1, a0                         #  ahi<- ahi << (32-(shift&31))
    or      v0, a1                         #  rlo<- rlo | ahi
    andi    a2, 0x20                       #  shift & 0x20
    movn    v0, v1, a2                     #  rlo<- rhi (if shift&0x20)
    movn    v1, a3, a2                     #  rhi<- sign(ahi) (if shift&0x20)

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, t2)                    #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_USHR_LONG_2ADDR: /* 0xc5 */
/* File: mips/OP_USHR_LONG_2ADDR.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* ushr-long/2addr vA, vB */
    GET_OPA4(t3)                           #  t3 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(t3, rFP, t3)                      #  t3 <- &fp[A]
    LOAD64(a0, a1, t3)                     #  a0/a1 <- vAA/vAA+1

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    srl       v1, a1, a2                   #  rhi<- ahi >> (shift&31)
    srl       v0, a0, a2                   #  rlo<- alo >> (shift&31)
    not       a0, a2                       #  alo<- 31-n  (shift is 5b)
    sll       a1, 1
    sll       a1, a0                       #  ahi<- ahi << (32-(shift&31))
    or        v0, a1                       #  rlo<- rlo | ahi
    andi      a2, 0x20                     #  shift & 0x20
    movn      v0, v1, a2                   #  rlo<- rhi (if shift&0x20)
    movn      v1, zero, a2                 #  rhi<- 0 (if shift&0x20)

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, t3)                    #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_ADD_FLOAT_2ADDR: /* 0xc6 */
/* File: mips/OP_ADD_FLOAT_2ADDR.S */
/* File: mips/binflop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" and
     * "instr_f" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     * div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
#ifdef SOFT_FLOAT
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    .if 0
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__addsf3)                                 #  result <- op, a0-a3 changed
    SET_VREG(v0, rOBJ)                     #  vAA <- result
#else
    add.s fv0, fa0, fa1
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- result
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SUB_FLOAT_2ADDR: /* 0xc7 */
/* File: mips/OP_SUB_FLOAT_2ADDR.S */
/* File: mips/binflop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" and
     * "instr_f" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     * div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
#ifdef SOFT_FLOAT
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    .if 0
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__subsf3)                                 #  result <- op, a0-a3 changed
    SET_VREG(v0, rOBJ)                     #  vAA <- result
#else
    sub.s fv0, fa0, fa1
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- result
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_MUL_FLOAT_2ADDR: /* 0xc8 */
/* File: mips/OP_MUL_FLOAT_2ADDR.S */
/* File: mips/binflop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" and
     * "instr_f" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     * div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
#ifdef SOFT_FLOAT
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    .if 0
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__mulsf3)                                 #  result <- op, a0-a3 changed
    SET_VREG(v0, rOBJ)                     #  vAA <- result
#else
    mul.s fv0, fa0, fa1
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- result
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_DIV_FLOAT_2ADDR: /* 0xc9 */
/* File: mips/OP_DIV_FLOAT_2ADDR.S */
/* File: mips/binflop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" and
     * "instr_f" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     * div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
#ifdef SOFT_FLOAT
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    .if 0
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__divsf3)                                 #  result <- op, a0-a3 changed
    SET_VREG(v0, rOBJ)                     #  vAA <- result
#else
    div.s fv0, fa0, fa1
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- result
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_REM_FLOAT_2ADDR: /* 0xca */
/* File: mips/OP_REM_FLOAT_2ADDR.S */
/* File: mips/binflop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" and
     * "instr_f" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     * div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
#ifdef SOFT_FLOAT
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    .if 0
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(fmodf)                                 #  result <- op, a0-a3 changed
    SET_VREG(v0, rOBJ)                     #  vAA <- result
#else
    JAL(fmodf)
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- result
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_ADD_DOUBLE_2ADDR: /* 0xcb */
/* File: mips/OP_ADD_DOUBLE_2ADDR.S */
/* File: mips/binflopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *  div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
#ifdef SOFT_FLOAT
    LOAD64(rARG2, rARG3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 0
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, rOBJ)
    LOAD64_F(fa1, fa1f, a1)
    .if 0
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__adddf3)                                 #  result <- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    add.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SUB_DOUBLE_2ADDR: /* 0xcc */
/* File: mips/OP_SUB_DOUBLE_2ADDR.S */
/* File: mips/binflopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *  div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
#ifdef SOFT_FLOAT
    LOAD64(rARG2, rARG3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 0
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, rOBJ)
    LOAD64_F(fa1, fa1f, a1)
    .if 0
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__subdf3)                                 #  result <- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    sub.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_MUL_DOUBLE_2ADDR: /* 0xcd */
/* File: mips/OP_MUL_DOUBLE_2ADDR.S */
/* File: mips/binflopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *  div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
#ifdef SOFT_FLOAT
    LOAD64(rARG2, rARG3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 0
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, rOBJ)
    LOAD64_F(fa1, fa1f, a1)
    .if 0
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__muldf3)                                 #  result <- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    mul.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_DIV_DOUBLE_2ADDR: /* 0xce */
/* File: mips/OP_DIV_DOUBLE_2ADDR.S */
/* File: mips/binflopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *  div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
#ifdef SOFT_FLOAT
    LOAD64(rARG2, rARG3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 0
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, rOBJ)
    LOAD64_F(fa1, fa1f, a1)
    .if 0
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(__divdf3)                                 #  result <- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    div.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_REM_DOUBLE_2ADDR: /* 0xcf */
/* File: mips/OP_REM_DOUBLE_2ADDR.S */
/* File: mips/binflopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *  div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
#ifdef SOFT_FLOAT
    LOAD64(rARG2, rARG3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if 0
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, rOBJ)
    LOAD64_F(fa1, fa1f, a1)
    .if 0
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
#ifdef SOFT_FLOAT
    JAL(fmod)                                 #  result <- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    JAL(fmod)
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 12-15 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_ADD_INT_LIT16: /* 0xd0 */
/* File: mips/OP_ADD_INT_LIT16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    # binop/lit16 vA, vB,                  /* +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA(rOBJ)                          #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    and       rOBJ, rOBJ, 15
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    addu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_RSUB_INT: /* 0xd1 */
/* File: mips/OP_RSUB_INT.S */
/* this op is "rsub-int", but can be thought of as "rsub-int/lit16" */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    # binop/lit16 vA, vB,                  /* +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA(rOBJ)                          #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    and       rOBJ, rOBJ, 15
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    subu a0, a1, a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_MUL_INT_LIT16: /* 0xd2 */
/* File: mips/OP_MUL_INT_LIT16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    # binop/lit16 vA, vB,                  /* +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA(rOBJ)                          #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    and       rOBJ, rOBJ, 15
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    mul a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_DIV_INT_LIT16: /* 0xd3 */
/* File: mips/OP_DIV_INT_LIT16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    # binop/lit16 vA, vB,                  /* +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA(rOBJ)                          #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    and       rOBJ, rOBJ, 15
    .if 1
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    div zero, a0, a1; mflo a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_REM_INT_LIT16: /* 0xd4 */
/* File: mips/OP_REM_INT_LIT16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    # binop/lit16 vA, vB,                  /* +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA(rOBJ)                          #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    and       rOBJ, rOBJ, 15
    .if 1
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    div zero, a0, a1; mfhi a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_AND_INT_LIT16: /* 0xd5 */
/* File: mips/OP_AND_INT_LIT16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    # binop/lit16 vA, vB,                  /* +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA(rOBJ)                          #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    and       rOBJ, rOBJ, 15
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    and a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_OR_INT_LIT16: /* 0xd6 */
/* File: mips/OP_OR_INT_LIT16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    # binop/lit16 vA, vB,                  /* +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA(rOBJ)                          #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    and       rOBJ, rOBJ, 15
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    or a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_XOR_INT_LIT16: /* 0xd7 */
/* File: mips/OP_XOR_INT_LIT16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    # binop/lit16 vA, vB,                  /* +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA(rOBJ)                          #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    and       rOBJ, rOBJ, 15
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    xor a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-13 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_ADD_INT_LIT8: /* 0xd8 */
/* File: mips/OP_ADD_INT_LIT8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    addu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-12 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_RSUB_INT_LIT8: /* 0xd9 */
/* File: mips/OP_RSUB_INT_LIT8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    subu a0, a1, a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-12 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_MUL_INT_LIT8: /* 0xda */
/* File: mips/OP_MUL_INT_LIT8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    mul a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-12 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_DIV_INT_LIT8: /* 0xdb */
/* File: mips/OP_DIV_INT_LIT8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    div zero, a0, a1; mflo a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-12 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_REM_INT_LIT8: /* 0xdc */
/* File: mips/OP_REM_INT_LIT8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    div zero, a0, a1; mfhi a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-12 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_AND_INT_LIT8: /* 0xdd */
/* File: mips/OP_AND_INT_LIT8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    and a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-12 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_OR_INT_LIT8: /* 0xde */
/* File: mips/OP_OR_INT_LIT8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    or a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-12 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_XOR_INT_LIT8: /* 0xdf */
/* File: mips/OP_XOR_INT_LIT8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    xor a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-12 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SHL_INT_LIT8: /* 0xe0 */
/* File: mips/OP_SHL_INT_LIT8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    and a1, a1, 31                              #  optional op
    sll a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-12 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_SHR_INT_LIT8: /* 0xe1 */
/* File: mips/OP_SHR_INT_LIT8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    and a1, a1, 31                              #  optional op
    sra a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-12 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_USHR_INT_LIT8: /* 0xe2 */
/* File: mips/OP_USHR_INT_LIT8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    and a1, a1, 31                              #  optional op
    srl a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0
    /* 10-12 instructions */



/* ------------------------------ */
    .balign 128
.L_OP_IGET_VOLATILE: /* 0xe3 */
/* File: mips/OP_IGET_VOLATILE.S */
/* File: mips/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IGET_VOLATILE_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test results
    move      a0, v0
    bnez      v0, .LOP_IGET_VOLATILE_finish
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_IPUT_VOLATILE: /* 0xe4 */
/* File: mips/OP_IPUT_VOLATILE.S */
/* File: mips/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IPUT_VOLATILE_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # success?
    move      a0, v0
    bnez      v0, .LOP_IPUT_VOLATILE_finish       #  yes, finish up
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_SGET_VOLATILE: /* 0xe5 */
/* File: mips/OP_SGET_VOLATILE.S */
/* File: mips/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    # is resolved entry !null?
    bnez      a0, .LOP_SGET_VOLATILE_finish

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    # success?
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SGET_VOLATILE_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_SPUT_VOLATILE: /* 0xe6 */
/* File: mips/OP_SPUT_VOLATILE.S */
/* File: mips/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-object, sput-boolean, sput-byte, sput-char, sput-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    bnez      a0, .LOP_SPUT_VOLATILE_finish       #  is resolved entry null?
    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() may throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    beqz      v0, common_exceptionThrown   #  success? no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SPUT_VOLATILE_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_IGET_OBJECT_VOLATILE: /* 0xe7 */
/* File: mips/OP_IGET_OBJECT_VOLATILE.S */
/* File: mips/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IGET_OBJECT_VOLATILE_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test results
    move      a0, v0
    bnez      v0, .LOP_IGET_OBJECT_VOLATILE_finish
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_IGET_WIDE_VOLATILE: /* 0xe8 */
/* File: mips/OP_IGET_WIDE_VOLATILE.S */
/* File: mips/OP_IGET_WIDE.S */
    /*
     * Wide 32-bit instance field get.
     */
    # iget-wide vA, vB, field              /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IGET_WIDE_VOLATILE_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test return code
    move      a0, v0
    bnez      v0, .LOP_IGET_WIDE_VOLATILE_finish
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_IPUT_WIDE_VOLATILE: /* 0xe9 */
/* File: mips/OP_IPUT_WIDE_VOLATILE.S */
/* File: mips/OP_IPUT_WIDE.S */
    # iput-wide vA, vB, field              /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IPUT_WIDE_VOLATILE_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # success?
    move      a0, v0
    bnez      v0, .LOP_IPUT_WIDE_VOLATILE_finish       #  yes, finish up
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_SGET_WIDE_VOLATILE: /* 0xea */
/* File: mips/OP_SGET_WIDE_VOLATILE.S */
/* File: mips/OP_SGET_WIDE.S */
    /*
     * 64-bit SGET handler.
     */
    # sget-wide vAA, field                 /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    # is resolved entry null?
    bnez      a0, .LOP_SGET_WIDE_VOLATILE_finish

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     *
     * Returns StaticField pointer in v0.
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    # success?
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif

    b        .LOP_SGET_WIDE_VOLATILE_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_SPUT_WIDE_VOLATILE: /* 0xeb */
/* File: mips/OP_SPUT_WIDE_VOLATILE.S */
/* File: mips/OP_SPUT_WIDE.S */
    /*
     * 64-bit SPUT handler.
     */
    # sput-wide vAA, field                 /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    GET_OPA(t0)                            #  t0 <- AA
    LOAD_eas2(a2, rBIX, a1)                #  a2 <- resolved StaticField ptr
    EAS2(rOBJ, rFP, t0)                    #  rOBJ<- &fp[AA]
    # is resolved entry null?
    beqz      a2, .LOP_SPUT_WIDE_VOLATILE_resolve      #  yes, do resolve
.LOP_SPUT_WIDE_VOLATILE_finish:                        #  field ptr in a2, AA in rOBJ
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64(a0, a1, rOBJ)                   #  a0/a1 <- vAA/vAA+1
    GET_INST_OPCODE(rBIX)                  #  extract opcode from rINST
    .if 1
    addu    a2, offStaticField_value       #  a2<- pointer to data
    JAL(dvmQuasiAtomicSwap64Sync)          #  stores a0/a1 into addr a2
    .else
    STORE64_off(a0, a1, a2, offStaticField_value) #  field <- vAA/vAA+1
    .endif
    GOTO_OPCODE(rBIX)                      #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_BREAKPOINT: /* 0xec */
    /* (stub) */
    SAVE_PC_TO_SELF()            # only need to export PC and FP
    SAVE_FP_TO_SELF()
    move        a0, rSELF        # self is first arg to function
    JAL(dvmMterp_OP_BREAKPOINT)      # call
    LOAD_PC_FROM_SELF()          # retrieve updated values
    LOAD_FP_FROM_SELF()
    FETCH_INST()                 # load next instruction from rPC
    GET_INST_OPCODE(t0)          # ...trim down to just the opcode
    GOTO_OPCODE(t0)              # ...and jump to the handler
/* ------------------------------ */
    .balign 128
.L_OP_THROW_VERIFICATION_ERROR: /* 0xed */
/* File: mips/OP_THROW_VERIFICATION_ERROR.S */
    /*
     * Handle a throw-verification-error instruction.  This throws an
     * exception for an error discovered during verification.  The
     * exception is indicated by AA, with some detail provided by BBBB.
     */
    /* op AA, ref@BBBB */

    LOAD_rSELF_method(a0)                  #  a0 <- self->method
    FETCH(a2, 1)                           #  a2 <- BBBB
    EXPORT_PC()                            #  export the PC
    GET_OPA(a1)                            #  a1 <- AA
    JAL(dvmThrowVerificationError)         #  always throws
    b         common_exceptionThrown       #  handle exception


/* ------------------------------ */
    .balign 128
.L_OP_EXECUTE_INLINE: /* 0xee */
/* File: mips/OP_EXECUTE_INLINE.S */
    /*
     * Execute a "native inline" instruction.
     *
     * We need to call an InlineOp4Func:
     *  bool (func)(u4 arg0, u4 arg1, u4 arg2, u4 arg3, JValue* pResult)
     *
     * The first four args are in a0-a3, pointer to return value storage
     * is on the stack.  The function's return value is a flag that tells
     * us if an exception was thrown.
     *
     * TUNING: could maintain two tables, pointer in Thread and
     * swap if profiler/debuggger active.
     */
    /* [opt] execute-inline vAA, {vC, vD, vE, vF}, inline@BBBB */
    lhu       a2, offThread_subMode(rSELF)
    FETCH(rBIX, 1)                         #  rBIX <- BBBB
    EXPORT_PC()                            #  can throw
    and       a2, kSubModeDebugProfile     #  Any going on?
    bnez      a2, .LOP_EXECUTE_INLINE_debugmode    #  yes - take slow path
.LOP_EXECUTE_INLINE_resume:
    addu      a1, rSELF, offThread_retval  #  a1 <- &self->retval
    GET_OPB(a0)                            #  a0 <- B
    # Stack should have 16/20 available
    sw        a1, STACK_OFFSET_ARG04(sp)   #  push &self->retval
    BAL(.LOP_EXECUTE_INLINE_continue)              #  make call; will return after
    lw        gp, STACK_OFFSET_GP(sp)      #  restore gp
    # test boolean result of inline
    beqz      v0, common_exceptionThrown   #  returned false, handle exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_OP_EXECUTE_INLINE_RANGE: /* 0xef */
/* File: mips/OP_EXECUTE_INLINE_RANGE.S */
    /*
     * Execute a "native inline" instruction, using "/range" semantics.
     * Same idea as execute-inline, but we get the args differently.
     *
     * We need to call an InlineOp4Func:
     *  bool (func)(u4 arg0, u4 arg1, u4 arg2, u4 arg3, JValue* pResult)
     *
     * The first four args are in a0-a3, pointer to return value storage
     * is on the stack.  The function's return value is a flag that tells
     * us if an exception was thrown.
     */
    /* [opt] execute-inline/range {vCCCC..v(CCCC+AA-1)}, inline@BBBB */
    lhu       a2, offThread_subMode(rSELF)
    FETCH(rBIX, 1)                       # rBIX<- BBBB
    EXPORT_PC()                          # can throw
    and       a2, kSubModeDebugProfile   # Any going on?
    bnez      a2, .LOP_EXECUTE_INLINE_RANGE_debugmode  # yes - take slow path
.LOP_EXECUTE_INLINE_RANGE_resume:
    addu      a1, rSELF, offThread_retval # a1<- &self->retval
    GET_OPA(a0)
    sw        a1, STACK_OFFSET_ARG04(sp)  # push &self->retval
    BAL(.LOP_EXECUTE_INLINE_RANGE_continue)             # make call; will return after
    lw        gp, STACK_OFFSET_GP(sp)     #  restore gp
    beqz      v0, common_exceptionThrown  # returned false, handle exception
    FETCH_ADVANCE_INST(3)                 # advance rPC, load rINST
    GET_INST_OPCODE(t0)                   # extract opcode from rINST
    GOTO_OPCODE(t0)                       # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_OBJECT_INIT_RANGE: /* 0xf0 */
/* File: mips/OP_INVOKE_OBJECT_INIT_RANGE.S */
    /*
     * Invoke Object.<init> on an object.  In practice we know that
     * Object's nullary constructor doesn't do anything, so we just
     * skip it unless a debugger is active.
     */
    FETCH(a1, 2)                  # a1<- CCCC
    GET_VREG(a0, a1)                    # a0<- "this" ptr
    # check for NULL
    beqz    a0, common_errNullObject    # export PC and throw NPE
    LOAD_base_offObject_clazz(a1, a0)   # a1<- obj->clazz
    LOAD_base_offClassObject_accessFlags(a2, a1) # a2<- clazz->accessFlags
    and     a2, CLASS_ISFINALIZABLE     # is this class finalizable?
    beqz    a2, .LOP_INVOKE_OBJECT_INIT_RANGE_finish      # no, go

.LOP_INVOKE_OBJECT_INIT_RANGE_setFinal:
    EXPORT_PC()                         # can throw
    JAL(dvmSetFinalizable)              # call dvmSetFinalizable(obj)
    LOAD_offThread_exception(a0, rSELF)	# a0<- self->exception
    # exception pending?
    bnez    a0, common_exceptionThrown  # yes, handle it

.LOP_INVOKE_OBJECT_INIT_RANGE_finish:
    lhu     a1, offThread_subMode(rSELF)
    and     a1, kSubModeDebuggerActive  # debugger active?
    bnez    a1, .LOP_INVOKE_OBJECT_INIT_RANGE_debugger    # Yes - skip optimization
    FETCH_ADVANCE_INST(2+1)       # advance to next instr, load rINST
    GET_INST_OPCODE(t0)                 # t0<- opcode from rINST
    GOTO_OPCODE(t0)                     # execute it


/* ------------------------------ */
    .balign 128
.L_OP_RETURN_VOID_BARRIER: /* 0xf1 */
/* File: mips/OP_RETURN_VOID_BARRIER.S */
    SMP_DMB
    b         common_returnFromMethod

/* ------------------------------ */
    .balign 128
.L_OP_IGET_QUICK: /* 0xf2 */
/* File: mips/OP_IGET_QUICK.S */
    /* For: iget-quick, iget-object-quick */
    # op vA, vB, offset                    /* CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1 #
    lw        a0, 0(t0)                    #  a0 <- obj.field (always 32 bits)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IGET_WIDE_QUICK: /* 0xf3 */
/* File: mips/OP_IGET_WIDE_QUICK.S */
    # iget-wide-quick vA, vB, offset       /* CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1                   #  t0 <- a3 + a1
    LOAD64(a0, a1, t0)                     #  a0 <- obj.field (64 bits, aligned)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    EAS2(a3, rFP, a2)
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a3)                    #  fp[A] <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IGET_OBJECT_QUICK: /* 0xf4 */
/* File: mips/OP_IGET_OBJECT_QUICK.S */
/* File: mips/OP_IGET_QUICK.S */
    /* For: iget-quick, iget-object-quick */
    # op vA, vB, offset                    /* CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1 #
    lw        a0, 0(t0)                    #  a0 <- obj.field (always 32 bits)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction



/* ------------------------------ */
    .balign 128
.L_OP_IPUT_QUICK: /* 0xf5 */
/* File: mips/OP_IPUT_QUICK.S */
    /* For: iput-quick, iput-object-quick */
    # op vA, vB, offset                    /* CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- fp[B], the object pointer
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    beqz      a3, common_errNullObject     #  object was null
    GET_VREG(a0, a2)                       #  a0 <- fp[A]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      t0, a3, a1
    sw        a0, 0(t0)                    #  obj.field (always 32 bits) <- a0
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IPUT_WIDE_QUICK: /* 0xf6 */
/* File: mips/OP_IPUT_WIDE_QUICK.S */
    # iput-wide-quick vA, vB, offset       /* CCCC */
    GET_OPA4(a0)                           #  a0 <- A(+)
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a2, a1)                       #  a2 <- fp[B], the object pointer
    EAS2(a3, rFP, a0)                      #  a3 <- &fp[A]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[A]
    # check object for null
    beqz      a2, common_errNullObject     #  object was null
    FETCH(a3, 1)                           #  a3 <- field byte offset
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      a2, a2, a3                   #  obj.field (64 bits, aligned) <- a0/a1
    STORE64(a0, a1, a2)                    #  obj.field (64 bits, aligned) <- a0/a1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_OP_IPUT_OBJECT_QUICK: /* 0xf7 */
/* File: mips/OP_IPUT_OBJECT_QUICK.S */
    /* For: iput-object-quick */
    # op vA, vB, offset                    /* CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- fp[B], the object pointer
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    beqz      a3, common_errNullObject     #  object was null
    GET_VREG(a0, a2)                       #  a0 <- fp[A]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      t0, a3, a1
    sw        a0, 0(t0)                    #  obj.field (always 32 bits) <- a0
    beqz      a0, 1f
    lw        a2, offThread_cardTable(rSELF) #  a2 <- card table base
    srl       t1, a3, GC_CARD_SHIFT
    addu      t2, a2, t1
    sb        a2, 0(t2)
1:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_VIRTUAL_QUICK: /* 0xf8 */
/* File: mips/OP_INVOKE_VIRTUAL_QUICK.S */
    /*
     * Handle an optimized virtual method call.
     *
     * for: [opt] invoke-virtual-quick, invoke-virtual-quick/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op vAA, {vCCCC..v(CCCC+AA-1)}, meth  /* BBBB */
    FETCH(a3, 2)                           #  a3 <- FEDC or CCCC
    FETCH(a1, 1)                           #  a1 <- BBBB
    .if (!0)
    and       a3, a3, 15                   #  a3 <- C (or stays CCCC)
    .endif
    GET_VREG(rOBJ, a3)                     #  rOBJ <- vC ("this" ptr)
    # is "this" null?
    beqz      rOBJ, common_errNullObject   #  null "this", throw exception
    LOAD_base_offObject_clazz(a2, rOBJ)    #  a2 <- thisPtr->clazz
    LOAD_base_offClassObject_vtable(a2, a2) #  a2 <- thisPtr->clazz->vtable
    EXPORT_PC()                            #  invoke must export
    LOAD_eas2(a0, a2, a1)                  #  a0 <- vtable[BBBB]
    b         common_invokeMethodNoRange #  (a0=method, r9="this")

/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_VIRTUAL_QUICK_RANGE: /* 0xf9 */
/* File: mips/OP_INVOKE_VIRTUAL_QUICK_RANGE.S */
/* File: mips/OP_INVOKE_VIRTUAL_QUICK.S */
    /*
     * Handle an optimized virtual method call.
     *
     * for: [opt] invoke-virtual-quick, invoke-virtual-quick/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op vAA, {vCCCC..v(CCCC+AA-1)}, meth  /* BBBB */
    FETCH(a3, 2)                           #  a3 <- FEDC or CCCC
    FETCH(a1, 1)                           #  a1 <- BBBB
    .if (!1)
    and       a3, a3, 15                   #  a3 <- C (or stays CCCC)
    .endif
    GET_VREG(rOBJ, a3)                     #  rOBJ <- vC ("this" ptr)
    # is "this" null?
    beqz      rOBJ, common_errNullObject   #  null "this", throw exception
    LOAD_base_offObject_clazz(a2, rOBJ)    #  a2 <- thisPtr->clazz
    LOAD_base_offClassObject_vtable(a2, a2) #  a2 <- thisPtr->clazz->vtable
    EXPORT_PC()                            #  invoke must export
    LOAD_eas2(a0, a2, a1)                  #  a0 <- vtable[BBBB]
    b         common_invokeMethodRange #  (a0=method, r9="this")


/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_SUPER_QUICK: /* 0xfa */
/* File: mips/OP_INVOKE_SUPER_QUICK.S */
    /*
     * Handle an optimized "super" method call.
     *
     * for: [opt] invoke-super-quick, invoke-super-quick/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op vAA, {vCCCC..v(CCCC+AA-1)}, meth  /* BBBB */
    FETCH(t0, 2)                           #  t0 <- GFED or CCCC
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    .if (!0)
    and       t0, t0, 15                   #  t0 <- D (or stays CCCC)
    .endif
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offMethod_clazz(a2, a2)      #  a2 <- method->clazz
    EXPORT_PC()                            #  must export for invoke
    LOAD_base_offClassObject_super(a2, a2) #  a2 <- method->clazz->super
    GET_VREG(rOBJ, t0)                     #  rOBJ <- "this"
    LOAD_base_offClassObject_vtable(a2, a2) #  a2 <- ...clazz->super->vtable
    # is "this" null ?
    LOAD_eas2(a0, a2, a1)                  #  a0 <- super->vtable[BBBB]
    beqz      rOBJ, common_errNullObject   #  "this" is null, throw exception
    b         common_invokeMethodNoRange #  (a0=method, rOBJ="this")


/* ------------------------------ */
    .balign 128
.L_OP_INVOKE_SUPER_QUICK_RANGE: /* 0xfb */
/* File: mips/OP_INVOKE_SUPER_QUICK_RANGE.S */
/* File: mips/OP_INVOKE_SUPER_QUICK.S */
    /*
     * Handle an optimized "super" method call.
     *
     * for: [opt] invoke-super-quick, invoke-super-quick/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op vAA, {vCCCC..v(CCCC+AA-1)}, meth  /* BBBB */
    FETCH(t0, 2)                           #  t0 <- GFED or CCCC
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    .if (!1)
    and       t0, t0, 15                   #  t0 <- D (or stays CCCC)
    .endif
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offMethod_clazz(a2, a2)      #  a2 <- method->clazz
    EXPORT_PC()                            #  must export for invoke
    LOAD_base_offClassObject_super(a2, a2) #  a2 <- method->clazz->super
    GET_VREG(rOBJ, t0)                     #  rOBJ <- "this"
    LOAD_base_offClassObject_vtable(a2, a2) #  a2 <- ...clazz->super->vtable
    # is "this" null ?
    LOAD_eas2(a0, a2, a1)                  #  a0 <- super->vtable[BBBB]
    beqz      rOBJ, common_errNullObject   #  "this" is null, throw exception
    b         common_invokeMethodRange #  (a0=method, rOBJ="this")



/* ------------------------------ */
    .balign 128
.L_OP_IPUT_OBJECT_VOLATILE: /* 0xfc */
/* File: mips/OP_IPUT_OBJECT_VOLATILE.S */
/* File: mips/OP_IPUT_OBJECT.S */
    /*
     * 32-bit instance field put.
     *
     * for: iput-object, iput-object-volatile
     */
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .LOP_IPUT_OBJECT_VOLATILE_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # success?
    move      a0, v0
    bnez      v0, .LOP_IPUT_OBJECT_VOLATILE_finish       #  yes, finish up
    b         common_exceptionThrown


/* ------------------------------ */
    .balign 128
.L_OP_SGET_OBJECT_VOLATILE: /* 0xfd */
/* File: mips/OP_SGET_OBJECT_VOLATILE.S */
/* File: mips/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    # op vAA, field                        /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    # is resolved entry !null?
    bnez      a0, .LOP_SGET_OBJECT_VOLATILE_finish

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    # success?
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b        .LOP_SGET_OBJECT_VOLATILE_finish            # resume


/* ------------------------------ */
    .balign 128
.L_OP_SPUT_OBJECT_VOLATILE: /* 0xfe */
/* File: mips/OP_SPUT_OBJECT_VOLATILE.S */
/* File: mips/OP_SPUT_OBJECT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput-object, sput-object-volatile
     */
    /* op vAA, field@BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    bnez      a0, .LOP_SPUT_OBJECT_VOLATILE_finish       #  is resolved entry null?

    /* Continuation if the field has not yet been resolved.
     * a1:  BBBB field ref
     * rBIX: dvmDex->pResFields
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() may throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    beqz      v0, common_exceptionThrown   #  success? no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    b       .LOP_SPUT_OBJECT_VOLATILE_finish             # resume



/* ------------------------------ */
    .balign 128
.L_OP_UNUSED_FF: /* 0xff */
/* File: mips/OP_UNUSED_FF.S */
/* File: mips/unused.S */
    BAL(common_abort)



    .balign 128
    .size   dvmAsmInstructionStart, .-dvmAsmInstructionStart
    .global dvmAsmInstructionEnd
dvmAsmInstructionEnd:

/*
 * ===========================================================================
 *  Sister implementations
 * ===========================================================================
 */
    .global dvmAsmSisterStart
    .type   dvmAsmSisterStart, %function
    .text
    .balign 4
dvmAsmSisterStart:

/* continuation for OP_CHECK_CAST */

.LOP_CHECK_CAST_castfailure:
    # A cast has failed. We need to throw a ClassCastException with the
    # class of the object that failed to be cast.
    EXPORT_PC()                            #  about to throw
    LOAD_base_offObject_clazz(a0, rOBJ)    #  a0 <- obj->clazz
    move      a1,rBIX                      #  r1<- desired class
    JAL(dvmThrowClassCastException)
    b         common_exceptionThrown

    /*
     * Resolution required.  This is the least-likely path.
     *
     *  a2   holds BBBB
     *  rOBJ holds object
     */
.LOP_CHECK_CAST_resolve:
    EXPORT_PC()                            #  resolve() could throw
    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    move      a1, a2                       #  a1 <- BBBB
    li        a2, 0                        #  a2 <- false
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- resolved ClassObject ptr
    # got null?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    move      a1, v0                       #  a1 <- class resolved from BBB
    LOAD_base_offObject_clazz(a0, rOBJ)    #  a0 <- obj->clazz
    b         .LOP_CHECK_CAST_resolved         #  pick up where we left off

/* continuation for OP_INSTANCE_OF */

    /*
     * Trivial test failed, need to perform full check.  This is common.
     *  a0   holds obj->clazz
     *  a1   holds class resolved from BBBB
     *  rOBJ holds A
     */
.LOP_INSTANCE_OF_fullcheck:
    JAL(dvmInstanceofNonTrivial)           #  v0 <- boolean result
    move      a0, v0                       #  fall through to OP_INSTANCE_OF_store
    b         .LOP_INSTANCE_OF_store

    /*
     * Resolution required.  This is the least-likely path.
     *
     *  a3   holds BBBB
     *  rOBJ holds A
     */
.LOP_INSTANCE_OF_resolve:
    EXPORT_PC()                            #  resolve() could throw
    LOAD_rSELF_method(a0)                  #  a0 <- self->method
    move      a1, a3                       #  a1 <- BBBB
    li        a2, 1                        #  a2 <- true
    LOAD_base_offMethod_clazz(a0, a0)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- resolved ClassObject ptr
    # got null?
    move      a1, v0                       #  a1 <- class resolved from BBB
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, a3)                       #  a0 <- vB (object)
    LOAD_base_offObject_clazz(a0, a0)      #  a0 <- obj->clazz
    b         .LOP_INSTANCE_OF_resolved         #  pick up where we left off


/* continuation for OP_NEW_INSTANCE */

.LOP_NEW_INSTANCE_continue:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(v0, a3)                       #  vAA <- v0
    GOTO_OPCODE(t0)                        #  jump to next instruction

#if defined(WITH_JIT)
    /*
     * Check to see if we need to stop the trace building early.
     * v0: new object
     * a3: vAA
     */
.LOP_NEW_INSTANCE_jitCheck:
    lw        a1, 0(rBIX)                  #  reload resolved class
    # okay?
    bnez      a1, .LOP_NEW_INSTANCE_continue     #  yes, finish
    move      rOBJ, v0                     #  preserve new object
    move      rBIX, a3                     #  preserve vAA
    move      a0, rSELF
    move      a1, rPC
    JAL(dvmJitEndTraceSelect)              #  (self, pc)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(rOBJ, rBIX)                   #  vAA <- new object
    GOTO_OPCODE(t0)                        #  jump to next instruction
#endif

    /*
     * Class initialization required.
     *
     *  a0 holds class object
     */
.LOP_NEW_INSTANCE_needinit:
    JAL(dvmInitClass)                      #  initialize class
    move      a0, rOBJ                     #  restore a0
    # check boolean result
    bnez      v0, .LOP_NEW_INSTANCE_initialized  #  success, continue
    b         common_exceptionThrown       #  failed, deal with init exception


    /*
     * Resolution required.  This is the least-likely path.
     *
     *  a1 holds BBBB
     */
.LOP_NEW_INSTANCE_resolve:
    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    li        a2, 0                        #  a2 <- false
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- resolved ClassObject ptr
    move      a0, v0
    # got null?
    bnez      v0, .LOP_NEW_INSTANCE_resolved     #  no, continue
    b         common_exceptionThrown       #  yes, handle exception

/* continuation for OP_NEW_ARRAY */

    /*
     * Resolve class.  (This is an uncommon case.)
     *
     *  a1 holds array length
     *  a2 holds class ref CCCC
     */
.LOP_NEW_ARRAY_resolve:
    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    move      rOBJ, a1                     #  rOBJ <- length (save)
    move      a1, a2                       #  a1 <- CCCC
    li        a2, 0                        #  a2 <- false
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- call(clazz, ref)
    move      a1, rOBJ                     #  a1 <- length (restore)
    # got null?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    move      a0, v0
    b         .LOP_NEW_ARRAY_finish           #  continue with OP_NEW_ARRAY_finish



/* continuation for OP_FILLED_NEW_ARRAY */

    /*
     * On entry:
     *  a0 holds array class
     *  rOBJ holds AA or BA
     */
.LOP_FILLED_NEW_ARRAY_continue:
    LOAD_base_offClassObject_descriptor(a3, a0) #  a3 <- arrayClass->descriptor
    li        a2, ALLOC_DONT_TRACK         #  a2 <- alloc flags
    lbu       rINST, 1(a3)                 #  rINST <- descriptor[1]
    .if 0
    move      a1, rOBJ                     #  a1 <- AA (length)
    .else
    srl       a1, rOBJ, 4                  #  rOBJ <- B (length)
    .endif
    seq       t0, rINST, 'I'               #  array of ints?
    seq       t1, rINST, 'L'               #  array of objects?
    or        t0, t1
    seq       t1, rINST, '['               #  array of arrays?
    or        t0, t1
    move      rBIX, a1                     #  save length in rBIX
    beqz      t0, .LOP_FILLED_NEW_ARRAY_notimpl      #  no, not handled yet
    JAL(dvmAllocArrayByClass)              #  v0 <- call(arClass, length, flags)
    # null return?
    beqz      v0, common_exceptionThrown   #  alloc failed, handle exception

    FETCH(a1, 2)                           #  a1 <- FEDC or CCCC
    sw        v0, offThread_retval(rSELF)  #  retval.l <- new array
    sw        rINST, (offThread_retval+4)(rSELF) #  retval.h <- type
    addu      a0, v0, offArrayObject_contents #  a0 <- newArray->contents
    subu      rBIX, rBIX, 1                #  length--, check for neg
    FETCH_ADVANCE_INST(3)                  #  advance to next instr, load rINST
    bltz      rBIX, 2f                     #  was zero, bail

    # copy values from registers into the array
    # a0=array, a1=CCCC/FEDC, t0=length (from AA or B), rOBJ=AA/BA
    move      t0, rBIX
    .if 0
    EAS2(a2, rFP, a1)                      #  a2 <- &fp[CCCC]
1:
    lw        a3, 0(a2)                    #  a3 <- *a2++
    addu      a2, 4
    subu      t0, t0, 1                    #  count--
    sw        a3, (a0)                     #  *contents++ = vX
    addu      a0, 4
    bgez      t0, 1b

    # continue at 2
    .else
    slt       t1, t0, 4                    #  length was initially 5?
    and       a2, rOBJ, 15                 #  a2 <- A
    bnez      t1, 1f                       #  <= 4 args, branch
    GET_VREG(a3, a2)                       #  a3 <- vA
    subu      t0, t0, 1                    #  count--
    sw        a3, 16(a0)                   #  contents[4] = vA
1:
    and       a2, a1, 15                   #  a2 <- F/E/D/C
    GET_VREG(a3, a2)                       #  a3 <- vF/vE/vD/vC
    srl       a1, a1, 4                    #  a1 <- next reg in low 4
    subu      t0, t0, 1                    #  count--
    sw        a3, 0(a0)                    #  *contents++ = vX
    addu      a0, a0, 4
    bgez      t0, 1b
    # continue at 2
    .endif

2:
    lw        a0, offThread_retval(rSELF)  #  a0 <- object
    lw        a1, (offThread_retval+4)(rSELF) #  a1 <- type
    seq       t1, a1, 'I'                  #  Is int array?
    bnez      t1, 3f
    lw        a2, offThread_cardTable(rSELF) #  a2 <- card table base
    srl       t3, a0, GC_CARD_SHIFT
    addu      t2, a2, t3
    sb        a2, (t2)
3:
    GET_INST_OPCODE(t0)                    #  ip <- opcode from rINST
    GOTO_OPCODE(t0)                        #  execute it


    /*
     * Throw an exception indicating that we have not implemented this
     * mode of filled-new-array.
     */
.LOP_FILLED_NEW_ARRAY_notimpl:
    la        a0, .LstrFilledNewArrayNotImpl
    JAL(dvmThrowInternalError)
    b         common_exceptionThrown

    /*
     * Ideally we'd only define this once, but depending on layout we can
     * exceed the range of the load above.
     */

/* continuation for OP_FILLED_NEW_ARRAY_RANGE */

    /*
     * On entry:
     *  a0 holds array class
     *  rOBJ holds AA or BA
     */
.LOP_FILLED_NEW_ARRAY_RANGE_continue:
    LOAD_base_offClassObject_descriptor(a3, a0) #  a3 <- arrayClass->descriptor
    li        a2, ALLOC_DONT_TRACK         #  a2 <- alloc flags
    lbu       rINST, 1(a3)                 #  rINST <- descriptor[1]
    .if 1
    move      a1, rOBJ                     #  a1 <- AA (length)
    .else
    srl       a1, rOBJ, 4                  #  rOBJ <- B (length)
    .endif
    seq       t0, rINST, 'I'               #  array of ints?
    seq       t1, rINST, 'L'               #  array of objects?
    or        t0, t1
    seq       t1, rINST, '['               #  array of arrays?
    or        t0, t1
    move      rBIX, a1                     #  save length in rBIX
    beqz      t0, .LOP_FILLED_NEW_ARRAY_RANGE_notimpl      #  no, not handled yet
    JAL(dvmAllocArrayByClass)              #  v0 <- call(arClass, length, flags)
    # null return?
    beqz      v0, common_exceptionThrown   #  alloc failed, handle exception

    FETCH(a1, 2)                           #  a1 <- FEDC or CCCC
    sw        v0, offThread_retval(rSELF)  #  retval.l <- new array
    sw        rINST, (offThread_retval+4)(rSELF) #  retval.h <- type
    addu      a0, v0, offArrayObject_contents #  a0 <- newArray->contents
    subu      rBIX, rBIX, 1                #  length--, check for neg
    FETCH_ADVANCE_INST(3)                  #  advance to next instr, load rINST
    bltz      rBIX, 2f                     #  was zero, bail

    # copy values from registers into the array
    # a0=array, a1=CCCC/FEDC, t0=length (from AA or B), rOBJ=AA/BA
    move      t0, rBIX
    .if 1
    EAS2(a2, rFP, a1)                      #  a2 <- &fp[CCCC]
1:
    lw        a3, 0(a2)                    #  a3 <- *a2++
    addu      a2, 4
    subu      t0, t0, 1                    #  count--
    sw        a3, (a0)                     #  *contents++ = vX
    addu      a0, 4
    bgez      t0, 1b

    # continue at 2
    .else
    slt       t1, t0, 4                    #  length was initially 5?
    and       a2, rOBJ, 15                 #  a2 <- A
    bnez      t1, 1f                       #  <= 4 args, branch
    GET_VREG(a3, a2)                       #  a3 <- vA
    subu      t0, t0, 1                    #  count--
    sw        a3, 16(a0)                   #  contents[4] = vA
1:
    and       a2, a1, 15                   #  a2 <- F/E/D/C
    GET_VREG(a3, a2)                       #  a3 <- vF/vE/vD/vC
    srl       a1, a1, 4                    #  a1 <- next reg in low 4
    subu      t0, t0, 1                    #  count--
    sw        a3, 0(a0)                    #  *contents++ = vX
    addu      a0, a0, 4
    bgez      t0, 1b
    # continue at 2
    .endif

2:
    lw        a0, offThread_retval(rSELF)  #  a0 <- object
    lw        a1, (offThread_retval+4)(rSELF) #  a1 <- type
    seq       t1, a1, 'I'                  #  Is int array?
    bnez      t1, 3f
    lw        a2, offThread_cardTable(rSELF) #  a2 <- card table base
    srl       t3, a0, GC_CARD_SHIFT
    addu      t2, a2, t3
    sb        a2, (t2)
3:
    GET_INST_OPCODE(t0)                    #  ip <- opcode from rINST
    GOTO_OPCODE(t0)                        #  execute it


    /*
     * Throw an exception indicating that we have not implemented this
     * mode of filled-new-array.
     */
.LOP_FILLED_NEW_ARRAY_RANGE_notimpl:
    la        a0, .LstrFilledNewArrayNotImpl
    JAL(dvmThrowInternalError)
    b         common_exceptionThrown

    /*
     * Ideally we'd only define this once, but depending on layout we can
     * exceed the range of the load above.
     */

/* continuation for OP_CMPL_FLOAT */

OP_CMPL_FLOAT_nan:
    li rTEMP, -1
    b         OP_CMPL_FLOAT_finish

#ifdef SOFT_FLOAT
OP_CMPL_FLOAT_continue:
    JAL(__gtsf2)                           #  v0 <- (vBB > vCC)
    li        rTEMP, 1                     #  rTEMP = 1 if v0 != 0
    bgtz      v0, OP_CMPL_FLOAT_finish
    b         OP_CMPL_FLOAT_nan
#endif

OP_CMPL_FLOAT_finish:
    GET_OPA(t0)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    SET_VREG(rTEMP, t0)                    #  vAA <- rTEMP
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)

/* continuation for OP_CMPG_FLOAT */

OP_CMPG_FLOAT_nan:
    li rTEMP, 1
    b         OP_CMPG_FLOAT_finish

#ifdef SOFT_FLOAT
OP_CMPG_FLOAT_continue:
    JAL(__gtsf2)                           #  v0 <- (vBB > vCC)
    li        rTEMP, 1                     #  rTEMP = 1 if v0 != 0
    bgtz      v0, OP_CMPG_FLOAT_finish
    b         OP_CMPG_FLOAT_nan
#endif

OP_CMPG_FLOAT_finish:
    GET_OPA(t0)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    SET_VREG(rTEMP, t0)                    #  vAA <- rTEMP
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)

/* continuation for OP_CMPL_DOUBLE */

OP_CMPL_DOUBLE_nan:
    li rTEMP, -1
    b         OP_CMPL_DOUBLE_finish

#ifdef SOFT_FLOAT
OP_CMPL_DOUBLE_continue:
    LOAD64(rARG2, rARG3, rBIX)             #  a2/a3 <- vCC/vCC+1
    JAL(__gtdf2)                           #  fallthru
    li        rTEMP, 1                     #  rTEMP = 1 if v0 != 0
    blez      v0, OP_CMPL_DOUBLE_nan            #  fall thru for finish
#endif

OP_CMPL_DOUBLE_finish:
    GET_OPA(rOBJ)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(rTEMP, rOBJ, t0)         #  vAA <- rTEMP

/* continuation for OP_CMPG_DOUBLE */

OP_CMPG_DOUBLE_nan:
    li rTEMP, 1
    b         OP_CMPG_DOUBLE_finish

#ifdef SOFT_FLOAT
OP_CMPG_DOUBLE_continue:
    LOAD64(rARG2, rARG3, rBIX)             #  a2/a3 <- vCC/vCC+1
    JAL(__gtdf2)                           #  fallthru
    li        rTEMP, 1                     #  rTEMP = 1 if v0 != 0
    blez      v0, OP_CMPG_DOUBLE_nan            #  fall thru for finish
#endif

OP_CMPG_DOUBLE_finish:
    GET_OPA(rOBJ)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(rTEMP, rOBJ, t0)         #  vAA <- rTEMP

/* continuation for OP_APUT_OBJECT */
.LOP_APUT_OBJECT_checks:
    LOAD_base_offObject_clazz(a0, rBIX)    #  a0 <- obj->clazz
    LOAD_base_offObject_clazz(a1, rINST)   #  a1 <- arrayObj->clazz
    JAL(dvmCanPutArrayElement)             #  test object type vs. array type
    beqz      v0, .LOP_APUT_OBJECT_throw        #  okay ?
    lw        a2, offThread_cardTable(rSELF)
    srl       t1, rINST, GC_CARD_SHIFT
    addu      t2, a2, t1
    sb        a2, (t2)
    b         .LOP_APUT_OBJECT_finish           #  yes, skip type checks
.LOP_APUT_OBJECT_throw:
    LOAD_base_offObject_clazz(a0, rBIX)    #  a0 <- obj->clazz
    LOAD_base_offObject_clazz(a1, rINST)   #  a1 <- arrayObj->clazz
    EXPORT_PC()
    JAL(dvmThrowArrayStoreExceptionIncompatibleElement)
    b         common_exceptionThrown

/* continuation for OP_IGET */

    /*
     * Currently:
     *  v0 holds resolved field
     *  rOBJ holds object (caller saved)
     */
.LOP_IGET_finish:
    #BAL(common_squeak0)
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    addu      a3, a3, rOBJ                 #  form address
    lw a0, (a3)                         #  a0 <- obj.field (8/16/32 bits)
         # noop                               #  acquiring load
    GET_OPA4(a2)                           #  a2 <- A+
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IGET_WIDE */

    /*
     * Currently:
     *  a0   holds resolved field
     *  rOBJ holds object
     */
.LOP_IGET_WIDE_finish:
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    beqz      rOBJ, common_errNullObject   #  object was null
    GET_OPA4(a2)                           #  a2 <- A+
    addu      rOBJ, rOBJ, a3               #  form address
    .if 0
    vLOAD64(a0, a1, rOBJ)                  #  a0/a1 <- obj.field (64-bit align ok)
    .else
    LOAD64(a0, a1, rOBJ)                   #  a0/a1 <- obj.field (64-bit align ok)
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    EAS2(a3, rFP, a2)                      #  a3 <- &fp[A]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a3)                    #  fp[A] <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IGET_OBJECT */

    /*
     * Currently:
     *  v0 holds resolved field
     *  rOBJ holds object (caller saved)
     */
.LOP_IGET_OBJECT_finish:
    #BAL(common_squeak0)
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    addu      a3, a3, rOBJ                 #  form address
    lw a0, (a3)                         #  a0 <- obj.field (8/16/32 bits)
         # noop                               #  acquiring load
    GET_OPA4(a2)                           #  a2 <- A+
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IGET_BOOLEAN */

    /*
     * Currently:
     *  v0 holds resolved field
     *  rOBJ holds object (caller saved)
     */
.LOP_IGET_BOOLEAN_finish:
    #BAL(common_squeak0)
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    addu      a3, a3, rOBJ                 #  form address
    lw a0, (a3)                         #  a0 <- obj.field (8/16/32 bits)
         # noop                               #  acquiring load
    GET_OPA4(a2)                           #  a2 <- A+
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IGET_BYTE */

    /*
     * Currently:
     *  v0 holds resolved field
     *  rOBJ holds object (caller saved)
     */
.LOP_IGET_BYTE_finish:
    #BAL(common_squeak0)
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    addu      a3, a3, rOBJ                 #  form address
    lw a0, (a3)                         #  a0 <- obj.field (8/16/32 bits)
         # noop                               #  acquiring load
    GET_OPA4(a2)                           #  a2 <- A+
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IGET_CHAR */

    /*
     * Currently:
     *  v0 holds resolved field
     *  rOBJ holds object (caller saved)
     */
.LOP_IGET_CHAR_finish:
    #BAL(common_squeak0)
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    addu      a3, a3, rOBJ                 #  form address
    lw a0, (a3)                         #  a0 <- obj.field (8/16/32 bits)
         # noop                               #  acquiring load
    GET_OPA4(a2)                           #  a2 <- A+
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IGET_SHORT */

    /*
     * Currently:
     *  v0 holds resolved field
     *  rOBJ holds object (caller saved)
     */
.LOP_IGET_SHORT_finish:
    #BAL(common_squeak0)
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    addu      a3, a3, rOBJ                 #  form address
    lw a0, (a3)                         #  a0 <- obj.field (8/16/32 bits)
         # noop                               #  acquiring load
    GET_OPA4(a2)                           #  a2 <- A+
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IPUT */

    /*
     * Currently:
     *  a0 holds resolved field
     *  rOBJ holds object
     */
.LOP_IPUT_finish:
    #BAL(common_squeak0)
    GET_OPA4(a1)                           #  a1 <- A+
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    GET_VREG(a0, a1)                       #  a0 <- fp[A]
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    addu      rOBJ, rOBJ, a3               #  form address
        #  noop                            #  releasing store
    sw a0, (rOBJ)                      #  obj.field (8/16/32 bits) <- a0
        #  noop
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IPUT_WIDE */

    /*
     * Currently:
     *  a0   holds resolved field
     *  rOBJ holds object
     */
.LOP_IPUT_WIDE_finish:
    GET_OPA4(a2)                           #  a2 <- A+
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[A]
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64(a0, a1, a2)                     #  a0/a1 <- fp[A]
    GET_INST_OPCODE(rBIX)                  #  extract opcode from rINST
    addu      a2, rOBJ, a3                 #  form address
    .if 0
    JAL(dvmQuasiAtomicSwap64Sync)          # stores r0/r1 into addr r2
#    STORE64(a0, a1, a2)                    #  obj.field (64 bits, aligned) <- a0 a1
    .else
    STORE64(a0, a1, a2)                    #  obj.field (64 bits, aligned) <- a0 a1
    .endif
    GOTO_OPCODE(rBIX)                      #  jump to next instruction


/* continuation for OP_IPUT_OBJECT */

    /*
     * Currently:
     *  a0 holds resolved field
     *  rOBJ holds object
     */
.LOP_IPUT_OBJECT_finish:
    #BAL(common_squeak0)
    GET_OPA4(a1)                           #  a1 <- A+
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    GET_VREG(a0, a1)                       #  a0 <- fp[A]
    lw        a2, offThread_cardTable(rSELF) #  a2 <- card table base
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    addu      t2, rOBJ, a3                 #  form address
        #  noop                            #  releasing store
    sw a0, (t2)                        #  obj.field (32 bits) <- a0
        #  noop
    beqz      a0, 1f                       #  stored a null reference?
    srl       t1, rOBJ, GC_CARD_SHIFT
    addu      t2, a2, t1
    sb        a2, (t2)                     #  mark card if not
1:
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IPUT_BOOLEAN */

    /*
     * Currently:
     *  a0 holds resolved field
     *  rOBJ holds object
     */
.LOP_IPUT_BOOLEAN_finish:
    #BAL(common_squeak0)
    GET_OPA4(a1)                           #  a1 <- A+
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    GET_VREG(a0, a1)                       #  a0 <- fp[A]
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    addu      rOBJ, rOBJ, a3               #  form address
        #  noop                            #  releasing store
    sw a0, (rOBJ)                      #  obj.field (8/16/32 bits) <- a0
        #  noop
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IPUT_BYTE */

    /*
     * Currently:
     *  a0 holds resolved field
     *  rOBJ holds object
     */
.LOP_IPUT_BYTE_finish:
    #BAL(common_squeak0)
    GET_OPA4(a1)                           #  a1 <- A+
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    GET_VREG(a0, a1)                       #  a0 <- fp[A]
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    addu      rOBJ, rOBJ, a3               #  form address
        #  noop                            #  releasing store
    sw a0, (rOBJ)                      #  obj.field (8/16/32 bits) <- a0
        #  noop
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IPUT_CHAR */

    /*
     * Currently:
     *  a0 holds resolved field
     *  rOBJ holds object
     */
.LOP_IPUT_CHAR_finish:
    #BAL(common_squeak0)
    GET_OPA4(a1)                           #  a1 <- A+
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    GET_VREG(a0, a1)                       #  a0 <- fp[A]
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    addu      rOBJ, rOBJ, a3               #  form address
        #  noop                            #  releasing store
    sw a0, (rOBJ)                      #  obj.field (8/16/32 bits) <- a0
        #  noop
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IPUT_SHORT */

    /*
     * Currently:
     *  a0 holds resolved field
     *  rOBJ holds object
     */
.LOP_IPUT_SHORT_finish:
    #BAL(common_squeak0)
    GET_OPA4(a1)                           #  a1 <- A+
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    GET_VREG(a0, a1)                       #  a0 <- fp[A]
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    addu      rOBJ, rOBJ, a3               #  form address
        #  noop                            #  releasing store
    sw a0, (rOBJ)                      #  obj.field (8/16/32 bits) <- a0
        #  noop
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_SGET */

.LOP_SGET_finish:
    LOAD_base_offStaticField_value(a1, a0) #  a1 <- field value
                      #  no-op                                #  acquiring load
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a1, a2, t0)              #  fp[AA] <- a1

/* continuation for OP_SGET_WIDE */

.LOP_SGET_WIDE_finish:
    GET_OPA(a1)                            #  a1 <- AA
    .if 0
    vLOAD64_off(a2, a3, a0, offStaticField_value) #  a2/a3 <- field value (aligned)
    .else
    LOAD64_off(a2, a3, a0, offStaticField_value) #  a2/a3 <- field value (aligned)
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[AA]
    STORE64(a2, a3, a1)                    #  vAA/vAA+1 <- a2/a3
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction



/* continuation for OP_SGET_OBJECT */

.LOP_SGET_OBJECT_finish:
    LOAD_base_offStaticField_value(a1, a0) #  a1 <- field value
                      #  no-op                                #  acquiring load
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a1, a2, t0)              #  fp[AA] <- a1

/* continuation for OP_SGET_BOOLEAN */

.LOP_SGET_BOOLEAN_finish:
    LOAD_base_offStaticField_value(a1, a0) #  a1 <- field value
                      #  no-op                                #  acquiring load
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a1, a2, t0)              #  fp[AA] <- a1

/* continuation for OP_SGET_BYTE */

.LOP_SGET_BYTE_finish:
    LOAD_base_offStaticField_value(a1, a0) #  a1 <- field value
                      #  no-op                                #  acquiring load
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a1, a2, t0)              #  fp[AA] <- a1

/* continuation for OP_SGET_CHAR */

.LOP_SGET_CHAR_finish:
    LOAD_base_offStaticField_value(a1, a0) #  a1 <- field value
                      #  no-op                                #  acquiring load
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a1, a2, t0)              #  fp[AA] <- a1

/* continuation for OP_SGET_SHORT */

.LOP_SGET_SHORT_finish:
    LOAD_base_offStaticField_value(a1, a0) #  a1 <- field value
                      #  no-op                                #  acquiring load
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a1, a2, t0)              #  fp[AA] <- a1

/* continuation for OP_SPUT */

.LOP_SPUT_finish:
    # field ptr in a0
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a1, a2)                       #  a1 <- fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    #  no-op                            #  releasing store
    sw        a1, offStaticField_value(a0) #  field <- vAA
    #  no-op
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* continuation for OP_SPUT_WIDE */

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rOBJ:  &fp[AA]
     *  rBIX: dvmDex->pResFields
     *
     * Returns StaticField pointer in a2.
     */
.LOP_SPUT_WIDE_resolve:
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    # success ?
    move      a0, v0
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    move      a2, v0
    b         .LOP_SPUT_WIDE_finish           # resume

/* continuation for OP_SPUT_OBJECT */
.LOP_SPUT_OBJECT_finish:                        #  field ptr in a0
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a1, a2)                       #  a1 <- fp[AA]
    lw        a2, offThread_cardTable(rSELF) #  a2 <- card table base
    lw        t1, offField_clazz(a0)       #  t1 <- field->clazz
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    #  no-op                            #  releasing store
    sw        a1, offStaticField_value(a0) #  field <- vAA
    #  no-op
    beqz      a1, 1f
    srl       t2, t1, GC_CARD_SHIFT
    addu      t3, a2, t2
    sb        a2, (t3)
1:
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* continuation for OP_SPUT_BOOLEAN */

.LOP_SPUT_BOOLEAN_finish:
    # field ptr in a0
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a1, a2)                       #  a1 <- fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    #  no-op                            #  releasing store
    sw        a1, offStaticField_value(a0) #  field <- vAA
    #  no-op
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* continuation for OP_SPUT_BYTE */

.LOP_SPUT_BYTE_finish:
    # field ptr in a0
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a1, a2)                       #  a1 <- fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    #  no-op                            #  releasing store
    sw        a1, offStaticField_value(a0) #  field <- vAA
    #  no-op
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* continuation for OP_SPUT_CHAR */

.LOP_SPUT_CHAR_finish:
    # field ptr in a0
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a1, a2)                       #  a1 <- fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    #  no-op                            #  releasing store
    sw        a1, offStaticField_value(a0) #  field <- vAA
    #  no-op
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* continuation for OP_SPUT_SHORT */

.LOP_SPUT_SHORT_finish:
    # field ptr in a0
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a1, a2)                       #  a1 <- fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    #  no-op                            #  releasing store
    sw        a1, offStaticField_value(a0) #  field <- vAA
    #  no-op
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* continuation for OP_INVOKE_VIRTUAL */

    /*
     * At this point:
     *  a0 = resolved base method
     *  rBIX= C or CCCC (index of first arg, which is the "this" ptr)
     */
.LOP_INVOKE_VIRTUAL_continue:
    GET_VREG(rOBJ, rBIX)                   #  rOBJ <- "this" ptr
    LOADu2_offMethod_methodIndex(a2, a0)   #  a2 <- baseMethod->methodIndex
    # is "this" null?
    beqz      rOBJ, common_errNullObject   #  null "this", throw exception
    LOAD_base_offObject_clazz(a3, rOBJ)    #  a3 <- thisPtr->clazz
    LOAD_base_offClassObject_vtable(a3, a3) #  a3 <- thisPtr->clazz->vtable
    LOAD_eas2(a0, a3, a2)                  #  a0 <- vtable[methodIndex]
    b         common_invokeMethodNoRange #  (a0=method, rOBJ="this")


/* continuation for OP_INVOKE_SUPER */

    /*
     * At this point:
     *  a0 = resolved base method
     *  rBIX = method->clazz
     */
.LOP_INVOKE_SUPER_continue:
    LOAD_base_offClassObject_super(a1, rBIX) #  a1 <- method->clazz->super
    LOADu2_offMethod_methodIndex(a2, a0)   #  a2 <- baseMethod->methodIndex
    LOAD_base_offClassObject_vtableCount(a3, a1) #  a3 <- super->vtableCount
    EXPORT_PC()                            #  must export for invoke
    # compare (methodIndex, vtableCount)
    bgeu      a2, a3, .LOP_INVOKE_SUPER_nsm      #  method not present in superclass
    LOAD_base_offClassObject_vtable(a1, a1) #  a1 <- ...clazz->super->vtable
    LOAD_eas2(a0, a1, a2)                  #  a0 <- vtable[methodIndex]
    b         common_invokeMethodNoRange #  continue on

    /*
     * Throw a NoSuchMethodError with the method name as the message.
     *  a0 = resolved base method
     */
.LOP_INVOKE_SUPER_nsm:
    LOAD_base_offMethod_name(a1, a0)       #  a1 <- method name
    b         common_errNoSuchMethod


/* continuation for OP_INVOKE_STATIC */

.LOP_INVOKE_STATIC_resolve:
    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    li        a2, METHOD_STATIC            #  resolver method type
    JAL(dvmResolveMethod)                  #  v0 <- call(clazz, ref, flags)
    move      a0, v0
#if defined(WITH_JIT)
    /*
     * Check to see if we're actively building a trace.  If so,
     * we need to keep this instruction out of it.
     * rBIX: &resolved_methodToCall
     */
    lhu       a2, offThread_subMode(rSELF)
    beqz      v0, common_exceptionThrown   #  null, handle exception
    and       a2, kSubModeJitTraceBuild    #  trace under construction?
    beqz      a2, common_invokeMethodNoRange #  no, (a0=method, rOBJ="this")
    lw        a1, 0(rBIX)                  #  reload resolved method
    # finished resloving?
    bnez      a1, common_invokeMethodNoRange #  yes, (a0=method, rOBJ="this")
    move      rBIX, a0                     #  preserve method
    move      a0, rSELF
    move      a1, rPC
    JAL(dvmJitEndTraceSelect)              #  (self, pc)
    move      a0, rBIX
    b         common_invokeMethodNoRange #  whew, finally!
#else
    # got null?
    bnez      v0, common_invokeMethodNoRange #  (a0=method, rOBJ="this")
    b         common_exceptionThrown       #  yes, handle exception
#endif

/* continuation for OP_INVOKE_VIRTUAL_RANGE */

    /*
     * At this point:
     *  a0 = resolved base method
     *  rBIX= C or CCCC (index of first arg, which is the "this" ptr)
     */
.LOP_INVOKE_VIRTUAL_RANGE_continue:
    GET_VREG(rOBJ, rBIX)                   #  rOBJ <- "this" ptr
    LOADu2_offMethod_methodIndex(a2, a0)   #  a2 <- baseMethod->methodIndex
    # is "this" null?
    beqz      rOBJ, common_errNullObject   #  null "this", throw exception
    LOAD_base_offObject_clazz(a3, rOBJ)    #  a3 <- thisPtr->clazz
    LOAD_base_offClassObject_vtable(a3, a3) #  a3 <- thisPtr->clazz->vtable
    LOAD_eas2(a0, a3, a2)                  #  a0 <- vtable[methodIndex]
    b         common_invokeMethodRange #  (a0=method, rOBJ="this")


/* continuation for OP_INVOKE_SUPER_RANGE */

    /*
     * At this point:
     *  a0 = resolved base method
     *  rBIX = method->clazz
     */
.LOP_INVOKE_SUPER_RANGE_continue:
    LOAD_base_offClassObject_super(a1, rBIX) #  a1 <- method->clazz->super
    LOADu2_offMethod_methodIndex(a2, a0)   #  a2 <- baseMethod->methodIndex
    LOAD_base_offClassObject_vtableCount(a3, a1) #  a3 <- super->vtableCount
    EXPORT_PC()                            #  must export for invoke
    # compare (methodIndex, vtableCount)
    bgeu      a2, a3, .LOP_INVOKE_SUPER_RANGE_nsm      #  method not present in superclass
    LOAD_base_offClassObject_vtable(a1, a1) #  a1 <- ...clazz->super->vtable
    LOAD_eas2(a0, a1, a2)                  #  a0 <- vtable[methodIndex]
    b         common_invokeMethodRange #  continue on

    /*
     * Throw a NoSuchMethodError with the method name as the message.
     *  a0 = resolved base method
     */
.LOP_INVOKE_SUPER_RANGE_nsm:
    LOAD_base_offMethod_name(a1, a0)       #  a1 <- method name
    b         common_errNoSuchMethod


/* continuation for OP_INVOKE_STATIC_RANGE */

.LOP_INVOKE_STATIC_RANGE_resolve:
    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    li        a2, METHOD_STATIC            #  resolver method type
    JAL(dvmResolveMethod)                  #  v0 <- call(clazz, ref, flags)
    move      a0, v0
#if defined(WITH_JIT)
    /*
     * Check to see if we're actively building a trace.  If so,
     * we need to keep this instruction out of it.
     * rBIX: &resolved_methodToCall
     */
    lhu       a2, offThread_subMode(rSELF)
    beqz      v0, common_exceptionThrown   #  null, handle exception
    and       a2, kSubModeJitTraceBuild    #  trace under construction?
    beqz      a2, common_invokeMethodRange #  no, (a0=method, rOBJ="this")
    lw        a1, 0(rBIX)                  #  reload resolved method
    # finished resloving?
    bnez      a1, common_invokeMethodRange #  yes, (a0=method, rOBJ="this")
    move      rBIX, a0                     #  preserve method
    move      a0, rSELF
    move      a1, rPC
    JAL(dvmJitEndTraceSelect)              #  (self, pc)
    move      a0, rBIX
    b         common_invokeMethodRange #  whew, finally!
#else
    # got null?
    bnez      v0, common_invokeMethodRange #  (a0=method, rOBJ="this")
    b         common_exceptionThrown       #  yes, handle exception
#endif

/* continuation for OP_FLOAT_TO_INT */

/*
 * Not an entry point as it is used only once !!
 */
f2i_doconv:
#ifdef SOFT_FLOAT
    li        a1, 0x4f000000               #  (float)maxint
    move      rBIX, a0
    JAL(__gesf2)                           #  is arg >= maxint?
    move      t0, v0
    li        v0, ~0x80000000              #  return maxint (7fffffff)
    bgez      t0, .LOP_FLOAT_TO_INT_set_vreg

    move      a0, rBIX                     #  recover arg
    li        a1, 0xcf000000               #  (float)minint
    JAL(__lesf2)

    move      t0, v0
    li        v0, 0x80000000               #  return minint (80000000)
    blez      t0, .LOP_FLOAT_TO_INT_set_vreg
    move      a0, rBIX
    move      a1, rBIX
    JAL(__nesf2)

    move      t0, v0
    li        v0, 0                        #  return zero for NaN
    bnez      t0, .LOP_FLOAT_TO_INT_set_vreg

    move      a0, rBIX
    JAL(__fixsfsi)
    b         .LOP_FLOAT_TO_INT_set_vreg
#else
    l.s       fa1, .LFLOAT_TO_INT_max
    c.ole.s   fcc0, fa1, fa0
    l.s       fv0, .LFLOAT_TO_INT_ret_max
    bc1t      .LOP_FLOAT_TO_INT_set_vreg_f

    l.s       fa1, .LFLOAT_TO_INT_min
    c.ole.s   fcc0, fa0, fa1
    l.s       fv0, .LFLOAT_TO_INT_ret_min
    bc1t      .LOP_FLOAT_TO_INT_set_vreg_f

    mov.s     fa1, fa0
    c.un.s    fcc0, fa0, fa1
    li.s      fv0, 0
    bc1t      .LOP_FLOAT_TO_INT_set_vreg_f

    trunc.w.s  fv0, fa0
    b         .LOP_FLOAT_TO_INT_set_vreg_f
#endif

.LFLOAT_TO_INT_max:
    .word 0x4f000000
.LFLOAT_TO_INT_min:
    .word 0xcf000000
.LFLOAT_TO_INT_ret_max:
    .word 0x7fffffff
.LFLOAT_TO_INT_ret_min:
    .word 0x80000000


/* continuation for OP_FLOAT_TO_LONG */

f2l_doconv:
#ifdef SOFT_FLOAT
    li        a1, 0x5f000000
    move      rBIX, a0
    JAL(__gesf2)

    move      t0, v0
    li        rRESULT0, ~0
    li        rRESULT1, ~0x80000000
    bgez      t0, .LOP_FLOAT_TO_LONG_set_vreg

    move      a0, rBIX
    li        a1, 0xdf000000
    JAL(__lesf2)

    move      t0, v0
    li        rRESULT0, 0
    li        rRESULT1, 0x80000000
    blez      t0, .LOP_FLOAT_TO_LONG_set_vreg

    move      a0, rBIX
    move      a1, rBIX
    JAL(__nesf2)

    move      t0, v0
    li        rRESULT0, 0
    li        rRESULT1, 0
    bnez      t0, .LOP_FLOAT_TO_LONG_set_vreg

    move      a0, rBIX
    JAL(__fixsfdi)

#else
    l.s       fa1, .LLONG_TO_max
    c.ole.s   fcc0, fa1, fa0
    li        rRESULT0, ~0
    li        rRESULT1, ~0x80000000
    bc1t      .LOP_FLOAT_TO_LONG_set_vreg

    l.s       fa1, .LLONG_TO_min
    c.ole.s   fcc0, fa0, fa1
    li        rRESULT0, 0
    li        rRESULT1, 0x80000000
    bc1t      .LOP_FLOAT_TO_LONG_set_vreg

    mov.s     fa1, fa0
    c.un.s    fcc0, fa0, fa1
    li        rRESULT0, 0
    li        rRESULT1, 0
    bc1t      .LOP_FLOAT_TO_LONG_set_vreg

    JAL(__fixsfdi)
#endif

    b         .LOP_FLOAT_TO_LONG_set_vreg

.LLONG_TO_max:
    .word 0x5f000000

.LLONG_TO_min:
    .word 0xdf000000

/* continuation for OP_DOUBLE_TO_INT */


d2i_doconv:
#ifdef SOFT_FLOAT
    la        t0, .LDOUBLE_TO_INT_max
    LOAD64(rARG2, rARG3, t0)
    move      rBIX, rARG0                  #  save a0
    move      rTEMP, rARG1                 #  and a1
    JAL(__gedf2)                           #  is arg >= maxint?

    move      t0, v0
    li        v0, ~0x80000000              #  return maxint (7fffffff)
    bgez      t0, .LOP_DOUBLE_TO_INT_set_vreg     #  nonzero == yes

    move      rARG0, rBIX                  #  recover arg
    move      rARG1, rTEMP
    la        t0, .LDOUBLE_TO_INT_min
    LOAD64(rARG2, rARG3, t0)
    JAL(__ledf2)                           #  is arg <= minint?

    move      t0, v0
    li        v0, 0x80000000               #  return minint (80000000)
    blez      t0, .LOP_DOUBLE_TO_INT_set_vreg     #  nonzero == yes

    move      rARG0, rBIX                  #  recover arg
    move      rARG1, rTEMP
    move      rARG2, rBIX                  #  compare against self
    move      rARG3, rTEMP
    JAL(__nedf2)                           #  is arg == self?

    move      t0, v0                       #  zero == no
    li        v0, 0
    bnez      t0, .LOP_DOUBLE_TO_INT_set_vreg     #  return zero for NaN

    move      rARG0, rBIX                  #  recover arg
    move      rARG1, rTEMP
    JAL(__fixdfsi)                         #  convert double to int
    b         .LOP_DOUBLE_TO_INT_set_vreg
#else
    la        t0, .LDOUBLE_TO_INT_max
    LOAD64_F(fa1, fa1f, t0)
    c.ole.d   fcc0, fa1, fa0
    l.s       fv0, .LDOUBLE_TO_INT_maxret
    bc1t      .LOP_DOUBLE_TO_INT_set_vreg_f

    la        t0, .LDOUBLE_TO_INT_min
    LOAD64_F(fa1, fa1f, t0)
    c.ole.d   fcc0, fa0, fa1
    l.s       fv0, .LDOUBLE_TO_INT_minret
    bc1t      .LOP_DOUBLE_TO_INT_set_vreg_f

    mov.d     fa1, fa0
    c.un.d    fcc0, fa0, fa1
    li.s      fv0, 0
    bc1t      .LOP_DOUBLE_TO_INT_set_vreg_f

    trunc.w.d  fv0, fa0
    b         .LOP_DOUBLE_TO_INT_set_vreg_f
#endif


.LDOUBLE_TO_INT_max:
    .dword 0x41dfffffffc00000
.LDOUBLE_TO_INT_min:
    .dword 0xc1e0000000000000              #  minint, as a double (high word)
.LDOUBLE_TO_INT_maxret:
    .word 0x7fffffff
.LDOUBLE_TO_INT_minret:
    .word 0x80000000

/* continuation for OP_DOUBLE_TO_LONG */

d2l_doconv:
#ifdef SOFT_FLOAT
    la        t0, .LDOUBLE_TO_LONG_max
    LOAD64(rARG2, rARG3, t0)
    move      rBIX, rARG0                  #  save a0
    move      rTEMP, rARG1                 #  and a1
    JAL(__gedf2)

    move      t1, v0
    la        t0, .LDOUBLE_TO_LONG_ret_max
    LOAD64(rRESULT0, rRESULT1, t0)
    bgez      t1, .LOP_DOUBLE_TO_LONG_set_vreg

    move      rARG0, rBIX
    move      rARG1, rTEMP
    la        t0, .LDOUBLE_TO_LONG_min
    LOAD64(rARG2, rARG3, t0)
    JAL(__ledf2)

    move      t1, v0
    la        t0, .LDOUBLE_TO_LONG_ret_min
    LOAD64(rRESULT0, rRESULT1, t0)
    blez      t1, .LOP_DOUBLE_TO_LONG_set_vreg

    move      rARG0, rBIX
    move      rARG1, rTEMP
    move      rARG2, rBIX
    move      rARG3, rTEMP
    JAL(__nedf2)

    move      t0, v0
    li        rRESULT0, 0
    li        rRESULT1, 0
    bnez      t0, .LOP_DOUBLE_TO_LONG_set_vreg

    move      rARG0, rBIX
    move      rARG1, rTEMP
    JAL(__fixdfdi)

#else
    la        t0, .LDOUBLE_TO_LONG_max
    LOAD64_F(fa1, fa1f, t0)
    c.ole.d   fcc0, fa1, fa0
    la        t0, .LDOUBLE_TO_LONG_ret_max
    LOAD64(rRESULT0, rRESULT1, t0)
    bc1t      .LOP_DOUBLE_TO_LONG_set_vreg

    la        t0, .LDOUBLE_TO_LONG_min
    LOAD64_F(fa1, fa1f, t0)
    c.ole.d   fcc0, fa0, fa1
    la        t0, .LDOUBLE_TO_LONG_ret_min
    LOAD64(rRESULT0, rRESULT1, t0)
    bc1t      .LOP_DOUBLE_TO_LONG_set_vreg

    mov.d     fa1, fa0
    c.un.d    fcc0, fa0, fa1
    li        rRESULT0, 0
    li        rRESULT1, 0
    bc1t      .LOP_DOUBLE_TO_LONG_set_vreg
    JAL(__fixdfdi)
#endif
    b         .LOP_DOUBLE_TO_LONG_set_vreg


.LDOUBLE_TO_LONG_max:
    .dword 0x43e0000000000000              #  maxlong, as a double (high word)
.LDOUBLE_TO_LONG_min:
    .dword 0xc3e0000000000000              #  minlong, as a double (high word)
.LDOUBLE_TO_LONG_ret_max:
    .dword 0x7fffffffffffffff
.LDOUBLE_TO_LONG_ret_min:
    .dword 0x8000000000000000

/* continuation for OP_MUL_LONG */

.LOP_MUL_LONG_finish:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, a0)                    #  vAA::vAA+1 <- v0(low) :: v1(high)
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IGET_VOLATILE */

    /*
     * Currently:
     *  v0 holds resolved field
     *  rOBJ holds object (caller saved)
     */
.LOP_IGET_VOLATILE_finish:
    #BAL(common_squeak0)
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    addu      a3, a3, rOBJ                 #  form address
    lw a0, (a3)                         #  a0 <- obj.field (8/16/32 bits)
    SMP_DMB                               #  acquiring load
    GET_OPA4(a2)                           #  a2 <- A+
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IPUT_VOLATILE */

    /*
     * Currently:
     *  a0 holds resolved field
     *  rOBJ holds object
     */
.LOP_IPUT_VOLATILE_finish:
    #BAL(common_squeak0)
    GET_OPA4(a1)                           #  a1 <- A+
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    GET_VREG(a0, a1)                       #  a0 <- fp[A]
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    addu      rOBJ, rOBJ, a3               #  form address
    SMP_DMB_ST                            #  releasing store
    sw a0, (rOBJ)                      #  obj.field (8/16/32 bits) <- a0
    SMP_DMB
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_SGET_VOLATILE */

.LOP_SGET_VOLATILE_finish:
    LOAD_base_offStaticField_value(a1, a0) #  a1 <- field value
    SMP_DMB                               #  acquiring load
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a1, a2, t0)              #  fp[AA] <- a1

/* continuation for OP_SPUT_VOLATILE */

.LOP_SPUT_VOLATILE_finish:
    # field ptr in a0
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a1, a2)                       #  a1 <- fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SMP_DMB_ST                            #  releasing store
    sw        a1, offStaticField_value(a0) #  field <- vAA
    SMP_DMB
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* continuation for OP_IGET_OBJECT_VOLATILE */

    /*
     * Currently:
     *  v0 holds resolved field
     *  rOBJ holds object (caller saved)
     */
.LOP_IGET_OBJECT_VOLATILE_finish:
    #BAL(common_squeak0)
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    addu      a3, a3, rOBJ                 #  form address
    lw a0, (a3)                         #  a0 <- obj.field (8/16/32 bits)
    SMP_DMB                               #  acquiring load
    GET_OPA4(a2)                           #  a2 <- A+
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IGET_WIDE_VOLATILE */

    /*
     * Currently:
     *  a0   holds resolved field
     *  rOBJ holds object
     */
.LOP_IGET_WIDE_VOLATILE_finish:
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    beqz      rOBJ, common_errNullObject   #  object was null
    GET_OPA4(a2)                           #  a2 <- A+
    addu      rOBJ, rOBJ, a3               #  form address
    .if 1
    vLOAD64(a0, a1, rOBJ)                  #  a0/a1 <- obj.field (64-bit align ok)
    .else
    LOAD64(a0, a1, rOBJ)                   #  a0/a1 <- obj.field (64-bit align ok)
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    EAS2(a3, rFP, a2)                      #  a3 <- &fp[A]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a3)                    #  fp[A] <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_IPUT_WIDE_VOLATILE */

    /*
     * Currently:
     *  a0   holds resolved field
     *  rOBJ holds object
     */
.LOP_IPUT_WIDE_VOLATILE_finish:
    GET_OPA4(a2)                           #  a2 <- A+
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[A]
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64(a0, a1, a2)                     #  a0/a1 <- fp[A]
    GET_INST_OPCODE(rBIX)                  #  extract opcode from rINST
    addu      a2, rOBJ, a3                 #  form address
    .if 1
    JAL(dvmQuasiAtomicSwap64Sync)          # stores r0/r1 into addr r2
#    STORE64(a0, a1, a2)                    #  obj.field (64 bits, aligned) <- a0 a1
    .else
    STORE64(a0, a1, a2)                    #  obj.field (64 bits, aligned) <- a0 a1
    .endif
    GOTO_OPCODE(rBIX)                      #  jump to next instruction


/* continuation for OP_SGET_WIDE_VOLATILE */

.LOP_SGET_WIDE_VOLATILE_finish:
    GET_OPA(a1)                            #  a1 <- AA
    .if 1
    vLOAD64_off(a2, a3, a0, offStaticField_value) #  a2/a3 <- field value (aligned)
    .else
    LOAD64_off(a2, a3, a0, offStaticField_value) #  a2/a3 <- field value (aligned)
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[AA]
    STORE64(a2, a3, a1)                    #  vAA/vAA+1 <- a2/a3
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction



/* continuation for OP_SPUT_WIDE_VOLATILE */

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rOBJ:  &fp[AA]
     *  rBIX: dvmDex->pResFields
     *
     * Returns StaticField pointer in a2.
     */
.LOP_SPUT_WIDE_VOLATILE_resolve:
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    # success ?
    move      a0, v0
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif
    move      a2, v0
    b         .LOP_SPUT_WIDE_VOLATILE_finish           # resume

/* continuation for OP_EXECUTE_INLINE */

    /*
     * Extract args, call function.
     *  a0 = #of args (0-4)
     *  rBIX = call index
     *
     * Other ideas:
     * - Use a jump table from the main piece to jump directly into the
     *   AND/LW pairs.  Costs a data load, saves a branch.
     * - Have five separate pieces that do the loading, so we can work the
     *   interleave a little better.  Increases code size.
     */
.LOP_EXECUTE_INLINE_continue:
    FETCH(rINST, 2)                        #  rINST <- FEDC
    beq       a0, 0, 0f
    beq       a0, 1, 1f
    beq       a0, 2, 2f
    beq       a0, 3, 3f
    beq       a0, 4, 4f
    JAL(common_abort)                      #  too many arguments

4:
    and       t0, rINST, 0xf000            #  isolate F
    ESRN(t1, rFP, t0, 10)
    lw        a3, 0(t1)                    #  a3 <- vF (shift right 12, left 2)
3:
    and       t0, rINST, 0x0f00            #  isolate E
    ESRN(t1, rFP, t0, 6)
    lw        a2, 0(t1)                    #  a2 <- vE
2:
    and       t0, rINST, 0x00f0            #  isolate D
    ESRN(t1, rFP, t0, 2)
    lw        a1, 0(t1)                    #  a1 <- vD
1:
    and       t0, rINST, 0x000f            #  isolate C
    EASN(t1, rFP, t0, 2)
    lw        a0, 0(t1)                    #  a0 <- vC
0:
    la        rINST, gDvmInlineOpsTable    #  table of InlineOperation
    EAS4(t1, rINST, rBIX)                  #  t1 <- rINST + rBIX<<4
    lw        t9, 0(t1)
    jr        t9                           #  sizeof=16, "func" is first entry
    # (not reached)

    /*
     * We're debugging or profiling.
     * rBIX: opIndex
     */
.LOP_EXECUTE_INLINE_debugmode:
    move      a0, rBIX
    JAL(dvmResolveInlineNative)
    beqz      v0, .LOP_EXECUTE_INLINE_resume       #  did it resolve? no, just move on
    move      rOBJ, v0                     #  remember method
    move      a0, v0
    move      a1, rSELF
    JAL(dvmFastMethodTraceEnter)           #  (method, self)
    addu      a1, rSELF, offThread_retval  #  a1<- &self->retval
    GET_OPB(a0)                            #  a0 <- B
    # Stack should have 16/20 available
    sw        a1, STACK_OFFSET_ARG04(sp)   #  push &self->retval
    BAL(.LOP_EXECUTE_INLINE_continue)              #  make call; will return after
    lw        gp, STACK_OFFSET_GP(sp)      #  restore gp
    move      rINST, v0                    #  save result of inline
    move      a0, rOBJ                     #  a0<- method
    move      a1, rSELF                    #  a1<- self
    JAL(dvmFastNativeMethodTraceExit)      #  (method, self)
    beqz      rINST, common_exceptionThrown   #  returned false, handle exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* continuation for OP_EXECUTE_INLINE_RANGE */

    /*
     * Extract args, call function.
     *  a0 = #of args (0-4)
     *  rBIX = call index
     *  ra = return addr, above  [DO NOT JAL out of here w/o preserving ra]
     */
.LOP_EXECUTE_INLINE_RANGE_continue:
    FETCH(rOBJ, 2)                       # rOBJ <- CCCC
    beq       a0, 0, 0f
    beq       a0, 1, 1f
    beq       a0, 2, 2f
    beq       a0, 3, 3f
    beq       a0, 4, 4f
    JAL(common_abort)                      #  too many arguments

4:
    add       t0, rOBJ, 3
    GET_VREG(a3, t0)
3:
    add       t0, rOBJ, 2
    GET_VREG(a2, t0)
2:
    add       t0, rOBJ, 1
    GET_VREG(a1, t0)
1:
    GET_VREG(a0, rOBJ)
0:
    la        rOBJ, gDvmInlineOpsTable      # table of InlineOperation
    EAS4(t1, rOBJ, rBIX)                    # t1 <- rINST + rBIX<<4
    lw        t9, 0(t1)
    jr        t9                            # sizeof=16, "func" is first entry
    # not reached

    /*
     * We're debugging or profiling.
     * rBIX: opIndex
     */
.LOP_EXECUTE_INLINE_RANGE_debugmode:
    move      a0, rBIX
    JAL(dvmResolveInlineNative)
    beqz      v0, .LOP_EXECUTE_INLINE_RANGE_resume       #  did it resolve? no, just move on
    move      rOBJ, v0                     #  remember method
    move      a0, v0
    move      a1, rSELF
    JAL(dvmFastMethodTraceEnter)           #  (method, self)
    addu      a1, rSELF, offThread_retval  #  a1<- &self->retval
    GET_OPA(a0)                            #  a0 <- A
    # Stack should have 16/20 available
    sw        a1, STACK_OFFSET_ARG04(sp)   #  push &self->retval
    move      rINST, rOBJ                  #  rINST<- method
    BAL(.LOP_EXECUTE_INLINE_RANGE_continue)              #  make call; will return after
    lw        gp, STACK_OFFSET_GP(sp)      #  restore gp
    move      rOBJ, v0                     #  save result of inline
    move      a0, rINST                    #  a0<- method
    move      a1, rSELF                    #  a1<- self
    JAL(dvmFastNativeMethodTraceExit)      #  (method, self)
    beqz      rOBJ, common_exceptionThrown #  returned false, handle exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* continuation for OP_INVOKE_OBJECT_INIT_RANGE */
    /*
     * A debugger is attached, so we need to go ahead and do
     * this.  For simplicity, we'll just jump directly to the
     * corresponding handler.  Note that we can't use
     * rIBASE here because it may be in single-step mode.
     * Load the primary table base directly.
     */
.LOP_INVOKE_OBJECT_INIT_RANGE_debugger:
    lw      a1, offThread_mainHandlerTable(rSELF)
    li      t0, OP_INVOKE_DIRECT_RANGE
    GOTO_OPCODE_BASE(a1, t0)            # execute it

/* continuation for OP_IPUT_OBJECT_VOLATILE */

    /*
     * Currently:
     *  a0 holds resolved field
     *  rOBJ holds object
     */
.LOP_IPUT_OBJECT_VOLATILE_finish:
    #BAL(common_squeak0)
    GET_OPA4(a1)                           #  a1 <- A+
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    GET_VREG(a0, a1)                       #  a0 <- fp[A]
    lw        a2, offThread_cardTable(rSELF) #  a2 <- card table base
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    addu      t2, rOBJ, a3                 #  form address
    SMP_DMB_ST                            #  releasing store
    sw a0, (t2)                        #  obj.field (32 bits) <- a0
    SMP_DMB
    beqz      a0, 1f                       #  stored a null reference?
    srl       t1, rOBJ, GC_CARD_SHIFT
    addu      t2, a2, t1
    sb        a2, (t2)                     #  mark card if not
1:
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* continuation for OP_SGET_OBJECT_VOLATILE */

.LOP_SGET_OBJECT_VOLATILE_finish:
    LOAD_base_offStaticField_value(a1, a0) #  a1 <- field value
    SMP_DMB                               #  acquiring load
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a1, a2, t0)              #  fp[AA] <- a1

/* continuation for OP_SPUT_OBJECT_VOLATILE */
.LOP_SPUT_OBJECT_VOLATILE_finish:                        #  field ptr in a0
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a1, a2)                       #  a1 <- fp[AA]
    lw        a2, offThread_cardTable(rSELF) #  a2 <- card table base
    lw        t1, offField_clazz(a0)       #  t1 <- field->clazz
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SMP_DMB_ST                            #  releasing store
    sw        a1, offStaticField_value(a0) #  field <- vAA
    SMP_DMB
    beqz      a1, 1f
    srl       t2, t1, GC_CARD_SHIFT
    addu      t3, a2, t2
    sb        a2, (t3)
1:
    GOTO_OPCODE(t0)                        #  jump to next instruction

    .size   dvmAsmSisterStart, .-dvmAsmSisterStart
    .global dvmAsmSisterEnd
dvmAsmSisterEnd:

/* File: mips/footer.S */
/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .text
    .align 2

#if defined(WITH_JIT)
#if defined(WITH_SELF_VERIFICATION)

/*
 * "longjmp" to a translation after single-stepping.  Before returning
 * to translation, must save state for self-verification.
 */
    .global dvmJitResumeTranslation             # (Thread* self, u4* dFP)
dvmJitResumeTranslation:
    move    rSELF, a0                           # restore self
    move    rPC, a1                             # restore Dalvik pc
    move    rFP, a2                             # restore Dalvik fp
    lw      rBIX, offThread_jitResumeNPC(rSELF)
    sw      zero, offThread_jitResumeNPC(rSELF) # reset resume address
    lw      sp, offThread_jitResumeNSP(rSELF)   # cut back native stack
    b       jitSVShadowRunStart                 # resume as if cache hit
                                                # expects resume addr in rBIX

    .global dvmJitToInterpPunt
dvmJitToInterpPunt:
    li        a2, kSVSPunt                 #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpSingleStep
dvmJitToInterpSingleStep:
    move      rPC, a0                      # set up dalvik pc
    EXPORT_PC()
    sw        ra, offThread_jitResumeNPC(rSELF)
    sw        a1, offThread_jitResumeDPC(rSELF)
    li        a2, kSVSSingleStep           #  a2 <- interpreter entry point
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpNoChainNoProfile
dvmJitToInterpNoChainNoProfile:
    move      a0, rPC                      #  pass our target PC
    li        a2, kSVSNoProfile            #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpTraceSelectNoChain
dvmJitToInterpTraceSelectNoChain:
    move      a0, rPC                      #  pass our target PC
    li        a2, kSVSTraceSelect          #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpTraceSelect
dvmJitToInterpTraceSelect:
    lw        a0, 0(ra)                   #  pass our target PC
    li        a2, kSVSTraceSelect          #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpBackwardBranch
dvmJitToInterpBackwardBranch:
    lw        a0, 0(ra)                   #  pass our target PC
    li        a2, kSVSBackwardBranch       #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpNormal
dvmJitToInterpNormal:
    lw        a0, 0(ra)                   #  pass our target PC
    li        a2, kSVSNormal               #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpNoChain
dvmJitToInterpNoChain:
    move      a0, rPC                      #  pass our target PC
    li        a2, kSVSNoChain              #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return
#else                                   /*  WITH_SELF_VERIFICATION */


/*
 * "longjmp" to a translation after single-stepping.
 */
    .global dvmJitResumeTranslation             # (Thread* self, u4* dFP)
dvmJitResumeTranslation:
    move    rSELF, a0                           # restore self
    move    rPC, a1                             # restore Dalvik pc
    move    rFP, a2                             # restore Dalvik fp
    lw      a0, offThread_jitResumeNPC(rSELF)
    sw      zero, offThread_jitResumeNPC(rSELF) # reset resume address
    lw      sp, offThread_jitResumeNSP(rSELF)   # cut back native stack
    jr      a0                                  # resume translation


/*
 * Return from the translation cache to the interpreter when the compiler is
 * having issues translating/executing a Dalvik instruction. We have to skip
 * the code cache lookup otherwise it is possible to indefinitely bouce
 * between the interpreter and the code cache if the instruction that fails
 * to be compiled happens to be at a trace start.
 */
    .global dvmJitToInterpPunt
dvmJitToInterpPunt:
    lw        gp, STACK_OFFSET_GP(sp)
    move      rPC, a0
#if defined(WITH_JIT_TUNING)
    move      a0, ra
    JAL(dvmBumpPunt)
#endif
    EXPORT_PC()
    sw        zero, offThread_inJitCodeCache(rSELF) # Back to the interp land
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)

/*
 * Return to the interpreter to handle a single instruction.
 * On entry:
 *    rPC <= Dalvik PC of instrucion to interpret
 *    a1 <= Dalvik PC of resume instruction
 *    ra <= resume point in translation
 */

    .global dvmJitToInterpSingleStep
dvmJitToInterpSingleStep:
    lw        gp, STACK_OFFSET_GP(sp)
    move      rPC, a0                       # set up dalvik pc
    EXPORT_PC()
    sw        ra, offThread_jitResumeNPC(rSELF)
    sw        sp, offThread_jitResumeNSP(rSELF)
    sw        a1, offThread_jitResumeDPC(rSELF)
    li        a1, 1
    sw        a1, offThread_singleStepCount(rSELF) # just step once
    move      a0, rSELF
    li        a1, kSubModeCountedStep
    JAL(dvmEnableSubMode)                   # (self, subMode)
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)
/*
 * Return from the translation cache and immediately request
 * a translation for the exit target.  Commonly used for callees.
 */
    .global dvmJitToInterpTraceSelectNoChain
dvmJitToInterpTraceSelectNoChain:
    lw        gp, STACK_OFFSET_GP(sp)
#if defined(WITH_JIT_TUNING)
    JAL(dvmBumpNoChain)
#endif
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)          # (pc, self)
    move      a0, v0
    sw        a0, offThread_inJitCodeCache(rSELF) # set the inJitCodeCache flag
    move      a1, rPC                      # arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET
    beqz      a0, 2f                       # 0 means translation does not exist
    jr        a0

/*
 * Return from the translation cache and immediately request
 * a translation for the exit target.  Commonly used following
 * invokes.
 */
    .global dvmJitToInterpTraceSelect
dvmJitToInterpTraceSelect:
    lw        gp, STACK_OFFSET_GP(sp)
    lw        rPC, (ra)                    #  get our target PC
    subu      rINST, ra, 8                 #  save start of chain branch
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)          # @ (pc, self)
    sw        v0, offThread_inJitCodeCache(rSELF) # set the inJitCodeCache flag
    beqz      v0, 2f
    move      a0, v0
    move      a1, rINST
    JAL(dvmJitChain)                       #  v0 <- dvmJitChain(codeAddr, chainAddr)
    move      a1, rPC                      #  arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET
    move      a0, v0
    beqz      a0, toInterpreter            #  didn't chain - resume with interpreter

    jr        a0                           #  continue native execution

/* No translation, so request one if profiling isn't disabled */
2:
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    lw        a0, offThread_pJitProfTable(rSELF)
    FETCH_INST()
    li        t0, kJitTSelectRequestHot
    movn      a2, t0, a0                   #  ask for trace selection
    bnez      a0, common_selectTrace
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)

/*
 * Return from the translation cache to the interpreter.
 * The return was done with a BLX from thumb mode, and
 * the following 32-bit word contains the target rPC value.
 * Note that lr (r14) will have its low-order bit set to denote
 * its thumb-mode origin.
 *
 * We'll need to stash our lr origin away, recover the new
 * target and then check to see if there is a translation available
 * for our new target.  If so, we do a translation chain and
 * go back to native execution.  Otherwise, it's back to the
 * interpreter (after treating this entry as a potential
 * trace start).
 */
    .global dvmJitToInterpNormal
dvmJitToInterpNormal:
    lw        gp, STACK_OFFSET_GP(sp)
    lw        rPC, (ra)                    #  get our target PC
    subu      rINST, ra, 8                 #  save start of chain branch
#if defined(WITH_JIT_TUNING)
    JAL(dvmBumpNormal)
#endif
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)           # @ (pc, self)
    move      a0, v0
    sw        a0, offThread_inJitCodeCache(rSELF) #  set the inJitCodeCache flag
    beqz      a0, toInterpreter            #  go if not, otherwise do chain
    move      a1, rINST
    JAL(dvmJitChain)                       #  v0 <- dvmJitChain(codeAddr, chainAddr)
    move      a1, rPC                      #  arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET
    move      a0, v0
    beqz      a0, toInterpreter            #  didn't chain - resume with interpreter

    jr        a0                           #  continue native execution

/*
 * Return from the translation cache to the interpreter to do method invocation.
 * Check if translation exists for the callee, but don't chain to it.
 */
    .global dvmJitToInterpNoChainNoProfile
dvmJitToInterpNoChainNoProfile:
#if defined(WITH_JIT_TUNING)
    JAL(dvmBumpNoChain)
#endif
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)          # (pc, self)
    move      a0, v0
    sw        a0, offThread_inJitCodeCache(rSELF) #  set the inJitCodeCache flag
    move      a1, rPC                      #  arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET
    beqz      a0, footer235

    jr        a0                           #  continue native execution if so
footer235:
    EXPORT_PC()
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/*
 * Return from the translation cache to the interpreter to do method invocation.
 * Check if translation exists for the callee, but don't chain to it.
 */

    .global dvmJitToInterpNoChain
dvmJitToInterpNoChain:
    lw        gp, STACK_OFFSET_GP(sp)
#if defined(WITH_JIT_TUNING)
    JAL(dvmBumpNoChain)
#endif
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)          # (pc, self)
    move      a0, v0
    sw        a0, offThread_inJitCodeCache(rSELF) #  set the inJitCodeCache flag
    move      a1, rPC                      #  arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET
    beqz      a0, 1f
    jr        a0                           #  continue native execution if so
1:
#endif                                  /*  WITH_SELF_VERIFICATION */

/*
 * No translation, restore interpreter regs and start interpreting.
 * rSELF & rFP were preserved in the translated code, and rPC has
 * already been restored by the time we get here.  We'll need to set
 * up rIBASE & rINST, and load the address of the JitTable into r0.
 */

toInterpreter:
    EXPORT_PC()
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    lw        a0, offThread_pJitProfTable(rSELF)
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    # NOTE: intended fallthrough

/*
 * Similar to common_updateProfile, but tests for null pJitProfTable
 * r0 holds pJifProfTAble, rINST is loaded, rPC is current and
 * rIBASE has been recently refreshed.
 */

common_testUpdateProfile:

    beqz      a0, 4f

/*
 * Common code to update potential trace start counter, and initiate
 * a trace-build if appropriate.
 * On entry here:
 *    r0    <= pJitProfTable (verified non-NULL)
 *    rPC   <= Dalvik PC
 *    rINST <= next instruction
 */
common_updateProfile:
    srl       a3, rPC, 12                  #  cheap, but fast hash function
    xor       a3, a3, rPC
    andi      a3, a3, JIT_PROF_SIZE-1      #  eliminate excess bits
    addu      t1, a0, a3
    lbu       a1, (t1)                     #  get counter
    GET_INST_OPCODE(t0)
    subu      a1, a1, 1                    #  decrement counter
    sb        a1, (t1)                     #  and store it
    beqz      a1, 1f
    GOTO_OPCODE(t0)                        #  if not threshold, fallthrough otherwise
1:
    /* Looks good, reset the counter */
    lw        a1, offThread_jitThreshold(rSELF)
    sb        a1, (t1)
    EXPORT_PC()
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)          # (pc, self)
    move      a0, v0
    sw        v0, offThread_inJitCodeCache(rSELF) #  set the inJitCodeCache flag
    move      a1, rPC                      #  arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET

#if !defined(WITH_SELF_VERIFICATION)
    li        t0, kJitTSelectRequest       #  ask for trace selection
    movz      a2, t0, a0
    beqz      a0, common_selectTrace
    jr        a0                           #  jump to the translation
#else

    bne       a0, zero, skip_ask_for_trace_selection
    li        a2, kJitTSelectRequest       #  ask for trace selection
    j         common_selectTrace

skip_ask_for_trace_selection:
    /*
     * At this point, we have a target translation.  However, if
     * that translation is actually the interpret-only pseudo-translation
     * we want to treat it the same as no translation.
     */
    move      rBIX, a0                     #  save target
    jal       dvmCompilerGetInterpretTemplate
    # special case?
    bne       v0, rBIX, jitSVShadowRunStart  #  set up self verification shadow space
    # Need to clear the inJitCodeCache flag
    sw        zero, offThread_inJitCodeCache(rSELF) #  back to the interp land
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)
    /* no return */
#endif

/*
 * On entry:
 *  r2 is jit state.
 */

common_selectTrace:
    lhu        a0, offThread_subMode(rSELF)
    andi       a0, (kSubModeJitTraceBuild | kSubModeJitSV)
    bnez       a0, 3f                      # already doing JIT work, continue
    sw         a2, offThread_jitState(rSELF)
    move       a0, rSELF

/*
 * Call out to validate trace-building request.  If successful,
 * rIBASE will be swapped to to send us into single-stepping trace
 * building mode, so we need to refresh before we continue.
 */

    EXPORT_PC()
    SAVE_PC_TO_SELF()
    SAVE_FP_TO_SELF()
    JAL(dvmJitCheckTraceRequest)
3:
    FETCH_INST()
    lw        rIBASE, offThread_curHandlerTable(rSELF)
4:
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)
    /* no return */
#endif

#if defined(WITH_SELF_VERIFICATION)

/*
 * Save PC and registers to shadow memory for self verification mode
 * before jumping to native translation.
 * On entry:
 *    rPC, rFP, rSELF: the values that they should contain
 *    r10: the address of the target translation.
 */
jitSVShadowRunStart:
    move      a0, rPC                      #  r0 <- program counter
    move      a1, rFP                      #  r1 <- frame pointer
    move      a2, rSELF                    #  r2 <- InterpState pointer
    move      a3, rBIX                     #  r3 <- target translation
    jal       dvmSelfVerificationSaveState #  save registers to shadow space
    lw        rFP, offShadowSpace_shadowFP(v0) #  rFP <- fp in shadow space
    jr        rBIX                         #  jump to the translation

/*
 * Restore PC, registers, and interpState to original values
 * before jumping back to the interpreter.
 */
jitSVShadowRunEnd:
    move      a1, rFP                      #  pass ending fp
    move      a3, rSELF                    #  pass self ptr for convenience
    jal       dvmSelfVerificationRestoreState #  restore pc and fp values
    LOAD_PC_FP_FROM_SELF()                 #  restore pc, fp
    lw        a1, offShadowSpace_svState(a0) #  get self verification state
    beq       a1, zero, 1f                 #  check for punt condition

    # Setup SV single-stepping
    move      a0, rSELF
    li        a1, kSubModeJitSV
    JAL(dvmEnableSubMode)                  # (self, subMode)
    li        a2, kJitSelfVerification     #  ask for self verification
    sw        a2, offThread_jitState(rSELF)
    # Intentional fallthrough

1:
    # exit to interpreter without check
    EXPORT_PC()
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)
#endif

/*
 * The equivalent of "goto bail", this calls through the "bail handler".
 * It will end this interpreter activation, and return to the caller
 * of dvmMterpStdRun.
 *
 * State registers will be saved to the "thread" area before bailing
 * debugging purposes
 */
    .ent common_gotoBail
common_gotoBail:
    SAVE_PC_FP_TO_SELF()                   # export state to "thread"
    move      a0, rSELF                    # a0 <- self ptr
    b         dvmMterpStdBail              # call(self, changeInterp)
    .end common_gotoBail

/*
 * The JIT's invoke method needs to remember the callsite class and
 * target pair.  Save them here so that they are available to
 * dvmCheckJit following the interpretation of this invoke.
 */
#if defined(WITH_JIT)
save_callsiteinfo:
    beqz    rOBJ, 1f
    lw      rOBJ, offObject_clazz(rOBJ)
1:
    sw      a0, offThread_methodToCall(rSELF)
    sw      rOBJ, offThread_callsiteClass(rSELF)
    jr      ra
#endif

/*
 * Common code for method invocation with range.
 *
 * On entry:
 *  a0 is "Method* methodToCall", the method we're trying to call
 */
common_invokeMethodRange:
.LinvokeNewRange:
#if defined(WITH_JIT)
    lhu      a1, offThread_subMode(rSELF)
    andi     a1, kSubModeJitTraceBuild
    beqz     a1, 1f
    JAL(save_callsiteinfo)
#endif
    # prepare to copy args to "outs" area of current frame
1:
    GET_OPA(a2)
    SAVEAREA_FROM_FP(rBIX, rFP)              #  rBIX <- stack save area
    beqz      a2, .LinvokeArgsDone
    FETCH(a1, 2)                           #  a1 <- CCCC
.LinvokeRangeArgs:
    # a0=methodToCall, a1=CCCC, a2=count, rBIX=outs
    # (very few methods have > 10 args; could unroll for common cases)
    EAS2(a3, rFP, a1)
    sll       t0, a2, 2
    subu      rBIX, rBIX, t0

1:
    lw        a1, 0(a3)
    addu      a3, a3, 4
    subu      a2, a2, 1
    sw        a1, 0(rBIX)
    addu      rBIX, 4
    bnez      a2, 1b
    b         .LinvokeArgsDone

/*
 * Common code for method invocation without range.
 *
 * On entry:
 *  a0 is "Method* methodToCall", "rOBJ is this"
 */
common_invokeMethodNoRange:
.LinvokeNewNoRange:
#if defined(WITH_JIT)
    lhu      a1, offThread_subMode(rSELF)
    andi     a1, kSubModeJitTraceBuild
    beqz     a1, 1f
    JAL(save_callsiteinfo)
#endif

    # prepare to copy args to "outs" area of current frame
1:
    GET_OPB(a2)
    SAVEAREA_FROM_FP(rBIX, rFP)
    beqz      a2, .LinvokeArgsDone
    FETCH(a1, 2)

    # a0=methodToCall, a1=GFED, a2=count,
.LinvokeNonRange:
    beq       a2, 0, 0f
    beq       a2, 1, 1f
    beq       a2, 2, 2f
    beq       a2, 3, 3f
    beq       a2, 4, 4f
    beq       a2, 5, 5f

5:
    and       t0, rINST, 0x0f00
    ESRN(t2, rFP, t0, 6)
    lw        a3, (t2)
    subu      rBIX, 4
    sw        a3, 0(rBIX)

4:
    and       t0, a1, 0xf000
    ESRN(t2, rFP, t0, 10)
    lw        a3, (t2)
    subu      rBIX, 4
    sw        a3, 0(rBIX)

3:
    and       t0, a1, 0x0f00
    ESRN(t2, rFP, t0, 6)
    lw        a3, (t2)
    subu      rBIX, 4
    sw        a3, 0(rBIX)

2:
    and       t0, a1, 0x00f0
    ESRN(t2, rFP, t0, 2)
    lw        a3, (t2)
    subu      rBIX, 4
    sw        a3, 0(rBIX)

1:
    and       t0, a1, 0x000f
    EASN(t2, rFP, t0, 2)
    lw        a3, (t2)
    subu      rBIX, 4
    sw        a3, 0(rBIX)

0:
    #fall through .LinvokeArgsDone


.LinvokeArgsDone:                          #  a0=methodToCall
    lhu       rOBJ, offMethod_registersSize(a0)
    lhu       a3, offMethod_outsSize(a0)
    lw        a2, offMethod_insns(a0)
    lw        rINST, offMethod_clazz(a0)
    # find space for the new stack frame, check for overflow
    SAVEAREA_FROM_FP(a1, rFP)              # a1 <- stack save area
    sll       t0, rOBJ, 2                    #  a1 <- newFp (old savearea - regsSize)
    subu      a1, a1, t0
    SAVEAREA_FROM_FP(rBIX, a1)
    lw        rOBJ, offThread_interpStackEnd(rSELF) #  t3 <- interpStackEnd
    sll       t2, a3, 2
    subu      t0, rBIX, t2
    lhu       ra, offThread_subMode(rSELF)
    lw        a3, offMethod_accessFlags(a0) #  a3 <- methodToCall->accessFlags
    bltu      t0, rOBJ, .LstackOverflow      #  yes, this frame will overflow stack


    # set up newSaveArea
#ifdef EASY_GDB
    SAVEAREA_FROM_FP(t0, rFP)
    sw        t0, offStackSaveArea_prevSave(rBIX)
#endif
    sw        rFP, (offStackSaveArea_prevFrame)(rBIX)
    sw        rPC, (offStackSaveArea_savedPc)(rBIX)
#if defined(WITH_JIT)
    sw        zero, (offStackSaveArea_returnAddr)(rBIX)
#endif
    sw        a0, (offStackSaveArea_method)(rBIX)
    # Profiling?
    bnez       ra, 2f
1:
    and       t2, a3, ACC_NATIVE
    bnez      t2, .LinvokeNative
    lhu       rOBJ, (a2)           # rOBJ -< load Inst from New PC
    lw        a3, offClassObject_pDvmDex(rINST)
    move      rPC, a2              # Publish new rPC
    # Update state values for the new method
    # a0=methodToCall, a1=newFp, a3=newMethodClass, rOBJ=newINST
    sw        a0, offThread_method(rSELF)
    sw        a3, offThread_methodClassDex(rSELF)
    li        a2, 1
    sw        a2, offThread_debugIsMethodEntry(rSELF)

#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    move      rFP, a1                    # fp = newFp
    GET_PREFETCHED_OPCODE(t0, rOBJ)      # extract prefetched opcode from rOBJ
    move      rINST, rOBJ                # publish new rINST
    sw        a1, offThread_curFrame(rSELF)
    bnez      a0, common_updateProfile
    GOTO_OPCODE(t0)
#else
    move      rFP, a1
    GET_PREFETCHED_OPCODE(t0, rOBJ)
    move      rINST, rOBJ
    sw        a1, offThread_curFrame(rSELF)
    GOTO_OPCODE(t0)
#endif

2:
    # Profiling - record method entry.  a0: methodToCall
    STACK_STORE(a0, 0)
    STACK_STORE(a1, 4)
    STACK_STORE(a2, 8)
    STACK_STORE(a3, 12)
    sw       rPC, offThread_pc(rSELF)          # update interpSave.pc
    move     a1, a0
    move     a0, rSELF
    JAL(dvmReportInvoke)
    STACK_LOAD(a3, 12)                         # restore a0-a3
    STACK_LOAD(a2, 8)
    STACK_LOAD(a1, 4)
    STACK_LOAD(a0, 0)
    b        1b
.LinvokeNative:
    # Prep for the native call
    # a0=methodToCall, a1=newFp, rBIX=newSaveArea
    lhu       ra, offThread_subMode(rSELF)
    lw        t3, offThread_jniLocal_topCookie(rSELF)
    sw        a1, offThread_curFrame(rSELF)
    sw        t3, offStackSaveArea_localRefCookie(rBIX) # newFp->localRefCookie=top
    move      a2, a0
    move      a0, a1
    addu      a1, rSELF, offThread_retval
    move      a3, rSELF
#ifdef ASSIST_DEBUGGER
    /* insert fake function header to help gdb find the stack frame */
    b         .Lskip
    .ent dalvik_mterp
dalvik_mterp:
    STACK_STORE_FULL()
.Lskip:
#endif
    bnez      ra, 11f                          # Any special SubModes active?
    lw        t9, offMethod_nativeFunc(a2)
    jalr      t9
    lw        gp, STACK_OFFSET_GP(sp)
7:
    # native return; rBIX=newSaveArea
    # equivalent to dvmPopJniLocals
    lw        a0, offStackSaveArea_localRefCookie(rBIX)
    lw        a1, offThread_exception(rSELF)
    sw        rFP, offThread_curFrame(rSELF)
    sw        a0, offThread_jniLocal_topCookie(rSELF)    # new top <- old top
    bnez      a1, common_exceptionThrown

    FETCH_ADVANCE_INST(3)
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)
11:
    # a0=newFp, a1=&retval, a2=methodToCall, a3=self, ra=subModes
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(a3, 12)
    move      a0, a2                    # a0 <- methodToCall
    move      a1, rSELF
    move      a2, rFP
    JAL(dvmReportPreNativeInvoke)       # (methodToCall, self, fp)
    SCRATCH_LOAD(a3, 12)                         # restore a0-a3
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)

    # Call the native method
    lw       t9, offMethod_nativeFunc(a2)      # t9<-methodToCall->nativeFunc
    jalr     t9
    lw       gp, STACK_OFFSET_GP(sp)

    # Restore the pre-call arguments
    SCRATCH_LOAD(a3, 12)                         # restore a0-a3
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)

    # Finish up any post-invoke subMode requirements
    move      a0, a2
    move      a1, rSELF
    move      a2, rFP
    JAL(dvmReportPostNativeInvoke)      # (methodToCall, self, fp)
    b         7b


.LstackOverflow:       # a0=methodToCall
    move      a1, a0                    #  a1 <- methodToCall
    move      a0, rSELF                 # a0 <- self
    JAL(dvmHandleStackOverflow)         #  dvmHandleStackOverflow(self, methodToCall)
    b         common_exceptionThrown
#ifdef ASSIST_DEBUGGER
    .end dalvik_mterp
#endif

    /*
     * Common code for method invocation, calling through "glue code".
     *
     * TODO: now that we have range and non-range invoke handlers, this
     *       needs to be split into two.  Maybe just create entry points
     *       that set r9 and jump here?
     *
     * On entry:
     *  r0 is "Method* methodToCall", the method we're trying to call
     *  r9 is "bool methodCallRange", indicating if this is a /range variant
     */

/*
 * Common code for handling a return instruction.
 *
 * This does not return.
 */
common_returnFromMethod:
.LreturnNew:
    lhu       t0, offThread_subMode(rSELF)
    SAVEAREA_FROM_FP(a0, rFP)
    lw        rOBJ, offStackSaveArea_savedPc(a0) # rOBJ = saveArea->savedPc
    bnez      t0, 19f
14:
    lw        rFP, offStackSaveArea_prevFrame(a0) # fp = saveArea->prevFrame
    lw        a2, (offStackSaveArea_method - sizeofStackSaveArea)(rFP)
                                               # a2<- method we're returning to
    # is this a break frame?
    beqz      a2, common_gotoBail              # break frame, bail out completely

    lw        rBIX, offMethod_clazz(a2)        # rBIX<- method->clazz
    lw        rIBASE, offThread_curHandlerTable(rSELF) # refresh rIBASE
    PREFETCH_ADVANCE_INST(rINST, rOBJ, 3)      # advance rOBJ, update new rINST
    sw        a2, offThread_method(rSELF)      # self->method = newSave->method
    lw        a1, offClassObject_pDvmDex(rBIX) # r1<- method->clazz->pDvmDex
    sw        rFP, offThread_curFrame(rSELF)   # curFrame = fp
#if defined(WITH_JIT)
    lw         rBIX, offStackSaveArea_returnAddr(a0)
    move       rPC, rOBJ                       # publish new rPC
    sw         a1, offThread_methodClassDex(rSELF)
    sw         rBIX, offThread_inJitCodeCache(rSELF) # may return to JIT'ed land
    beqz       rBIX, 15f                       # caller is compiled code
    move       t9, rBIX
    jalr       t9
    lw         gp, STACK_OFFSET_GP(sp)
15:
    GET_INST_OPCODE(t0)                        # extract opcode from rINST
    GOTO_OPCODE(t0)                            # jump to next instruction
#else
    GET_INST_OPCODE(t0)                        # extract opcode from rINST
    move       rPC, rOBJ                       # publish new rPC
    sw         a1, offThread_methodClassDex(rSELF)
    GOTO_OPCODE(t0)
#endif

19:
    # Handle special actions
    # On entry, a0: StackSaveArea
    lw         a1, offStackSaveArea_prevFrame(a0) # a1<- prevFP
    sw         rPC, offThread_pc(rSELF)        # update interpSave.pc
    sw         a1, offThread_curFrame(rSELF)   # update interpSave.curFrame
    move       a0, rSELF
    JAL(dvmReportReturn)
    SAVEAREA_FROM_FP(a0, rFP)                  # restore StackSaveArea
    b          14b

    .if 0
    /*
     * Return handling, calls through "glue code".
     */
.LreturnOld:
    SAVE_PC_FP_TO_SELF()                       # export state
    move       a0, rSELF                       # arg to function
    JAL(dvmMterp_returnFromMethod)
    b          common_resumeAfterGlueCall
    .endif

/*
 * Somebody has thrown an exception.  Handle it.
 *
 * If the exception processing code returns to us (instead of falling
 * out of the interpreter), continue with whatever the next instruction
 * now happens to be.
 *
 * This does not return.
 */
    .global dvmMterpCommonExceptionThrown
dvmMterpCommonExceptionThrown:
common_exceptionThrown:
.LexceptionNew:

    EXPORT_PC()
    move     a0, rSELF
    JAL(dvmCheckSuspendPending)
    lw       rOBJ, offThread_exception(rSELF)
    move     a1, rSELF
    move     a0, rOBJ
    JAL(dvmAddTrackedAlloc)
    lhu      a2, offThread_subMode(rSELF)
    sw       zero, offThread_exception(rSELF)

    # Special subMode?
    bnez     a2, 7f                     # any special subMode handling needed?
8:
    /* set up args and a local for "&fp" */
    sw       rFP, 20(sp)                 #  store rFP => tmp
    addu     t0, sp, 20                  #  compute &tmp
    sw       t0, STACK_OFFSET_ARG04(sp)  #  save it in arg4 as per ABI
    li       a3, 0                       #  a3 <- false
    lw       a1, offThread_method(rSELF)
    move     a0, rSELF
    lw       a1, offMethod_insns(a1)
    move     a2, rOBJ
    subu     a1, rPC, a1
    sra      a1, a1, 1

    /* call, r0 gets catchRelPc (a code-unit offset) */
    JAL(dvmFindCatchBlock)           # call(self, relPc, exc, scan?, &fp)
    lw        rFP, 20(sp)            # retrieve the updated rFP

    /* update frame pointer and check result from dvmFindCatchBlock */
    move      a0, v0
    bltz      v0, .LnotCaughtLocally

    /* fix earlier stack overflow if necessary; Preserve a0 */
    lbu       a1, offThread_stackOverflowed(rSELF)
    beqz      a1, 1f
    move      rBIX, a0
    move      a0, rSELF
    move      a1, rOBJ
    JAL(dvmCleanupStackOverflow)
    move      a0, rBIX

1:

/* adjust locals to match self->interpSave.curFrame and updated PC */
    SAVEAREA_FROM_FP(a1, rFP)           # a1<- new save area
    lw        a1, offStackSaveArea_method(a1)
    sw        a1, offThread_method(rSELF)
    lw        a2, offMethod_clazz(a1)
    lw        a3, offMethod_insns(a1)
    lw        a2, offClassObject_pDvmDex(a2)
    EAS1(rPC, a3, a0)
    sw        a2, offThread_methodClassDex(rSELF)

    /* release the tracked alloc on the exception */
    move      a0, rOBJ
    move      a1, rSELF
    JAL(dvmReleaseTrackedAlloc)

    /* restore the exception if the handler wants it */
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    GET_INST_OPCODE(t0)
    bne       t0, OP_MOVE_EXCEPTION, 2f
    sw        rOBJ, offThread_exception(rSELF)
2:
    GOTO_OPCODE(t0)

    # Manage debugger bookkeeping
7:
    sw        rPC, offThread_pc(rSELF)
    sw        rFP, offThread_curFrame(rSELF)
    move      a0, rSELF
    move      a1, rOBJ
    JAL(dvmReportExceptionThrow)
    b         8b

.LnotCaughtLocally:                     #  rOBJ = exception
    /* fix stack overflow if necessary */
    lbu       a1, offThread_stackOverflowed(rSELF)
    beqz      a1, 3f
    move      a0, rSELF
    move      a1, rOBJ
    JAL(dvmCleanupStackOverflow)           #  dvmCleanupStackOverflow(self, exception)

3:
    # may want to show "not caught locally" debug messages here
#if DVM_SHOW_EXCEPTION >= 2
    /* call __android_log_print(prio, tag, format, ...) */
    /* "Exception %s from %s:%d not caught locally" */
    lw        a0, offThread_method(rSELF)
    lw        a1, offMethod_insns(a0)
    subu      a1, rPC, a1
    sra       a1, a1, 1
    JAL(dvmLineNumFromPC)
    sw        v0, 20(sp)
    # dvmGetMethodSourceFile(method)
    lw        a0, offThread_method(rSELF)
    JAL(dvmGetMethodSourceFile)
    sw        v0, 16(sp)
    # exception->clazz->descriptor
    lw        a3, offObject_clazz(rOBJ)
    lw        a3, offClassObject_descriptor(a3)
    la        a2, .LstrExceptionNotCaughtLocally
    la        a1, .LstrLogTag
    li        a0, 3
    JAL(__android_log_print)
#endif
    sw        rOBJ, offThread_exception(rSELF)
    move      a0, rOBJ
    move      a1, rSELF
    JAL(dvmReleaseTrackedAlloc)
    b         common_gotoBail

    /*
     * Exception handling, calls through "glue code".
     */
    .if     0
.LexceptionOld:
    SAVE_PC_TO_SELF()                # export state
    SAVE_FP_TO_SELF()
    move     a0, rSELF               # arg to function
    JAL(dvmMterp_exceptionThrown)
    b       common_resumeAfterGlueCall
    .endif

#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including the current
     * instruction.
     *
     * On entry:
     *     rBIX: &dvmDex->pResFields[field]
     *     a0:  field pointer (must preserve)
     */
common_verifyField:
     lhu     a3, offThread_subMode(rSELF)
     andi    a3, kSubModeJitTraceBuild
     bnez    a3, 1f                 # Not building trace, continue
     jr      ra
1:
     lw      a1, (rBIX)
     beqz    a1, 2f                 # resolution complete ?
     jr      ra
2:
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(a3, 12)
    SCRATCH_STORE(ra, 16)
    move    a0, rSELF
    move    a1, rPC
    JAL(dvmJitEndTraceSelect)        #(self,pc) end trace before this inst)
    SCRATCH_LOAD(a0, 0)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a3, 12)
    SCRATCH_LOAD(ra, 16)
    jr      ra                       # return
#endif

/*
 * After returning from a "glued" function, pull out the updated
 * values and start executing at the next instruction.
 */
common_resumeAfterGlueCall:
    LOAD_PC_FP_FROM_SELF()           #  pull rPC and rFP out of thread
    lw      rIBASE, offThread_curHandlerTable(rSELF) # refresh
    FETCH_INST()                     #  load rINST from rPC
    GET_INST_OPCODE(t0)              #  extract opcode from rINST
    GOTO_OPCODE(t0)                  #  jump to next instruction

/*
 * Invalid array index. Note that our calling convention is strange; we use a1
 * and a3 because those just happen to be the registers all our callers are
 * using. We move a3 before calling the C function, but a1 happens to match.
 * a1: index
 * a3: size
 */
common_errArrayIndex:
    EXPORT_PC()
    move      a0, a3
    JAL(dvmThrowArrayIndexOutOfBoundsException)
    b         common_exceptionThrown

/*
 * Integer divide or mod by zero.
 */
common_errDivideByZero:
    EXPORT_PC()
    la     a0, .LstrDivideByZero
    JAL(dvmThrowArithmeticException)
    b       common_exceptionThrown

/*
 * Attempt to allocate an array with a negative size.
 * On entry: length in a1
 */
common_errNegativeArraySize:
    EXPORT_PC()
    move    a0, a1                                # arg0 <- len
    JAL(dvmThrowNegativeArraySizeException)    # (len)
    b       common_exceptionThrown

/*
 * Invocation of a non-existent method.
 * On entry: method name in a1
 */
common_errNoSuchMethod:
    EXPORT_PC()
    move     a0, a1
    JAL(dvmThrowNoSuchMethodError)
    b       common_exceptionThrown

/*
 * We encountered a null object when we weren't expecting one.  We
 * export the PC, throw a NullPointerException, and goto the exception
 * processing code.
 */
common_errNullObject:
    EXPORT_PC()
    li      a0, 0
    JAL(dvmThrowNullPointerException)
    b       common_exceptionThrown

/*
 * For debugging, cause an immediate fault. The source address will be in ra. Use a jal to jump here.
 */
common_abort:
    lw      zero,-4(zero)            #  generate SIGSEGV

/*
 * Spit out a "we were here", preserving all registers.
 */
    .macro SQUEAK num
common_squeak\num:
    STACK_STORE_RA();
    la        a0, .LstrSqueak
    LOAD_IMM(a1, \num);
    JAL(printf);
    STACK_LOAD_RA();
    RETURN;
    .endm

    SQUEAK 0
    SQUEAK 1
    SQUEAK 2
    SQUEAK 3
    SQUEAK 4
    SQUEAK 5

/*
 * Spit out the number in a0, preserving registers.
 */
common_printNum:
    STACK_STORE_RA()
    MOVE_REG(a1, a0)
    la        a0, .LstrSqueak
    JAL(printf)
    STACK_LOAD_RA()
    RETURN

/*
 * Print a newline, preserving registers.
 */
common_printNewline:
    STACK_STORE_RA()
    la        a0, .LstrNewline
    JAL(printf)
    STACK_LOAD_RA()
    RETURN

    /*
     * Print the 32-bit quantity in a0 as a hex value, preserving registers.
     */
common_printHex:
    STACK_STORE_RA()
    MOVE_REG(a1, a0)
    la        a0, .LstrPrintHex
    JAL(printf)
    STACK_LOAD_RA()
RETURN;

/*
 * Print the 64-bit quantity in a0-a1, preserving registers.
 */
common_printLong:
    STACK_STORE_RA()
    MOVE_REG(a3, a1)
    MOVE_REG(a2, a0)
    la        a0, .LstrPrintLong
    JAL(printf)
    STACK_LOAD_RA()
    RETURN;

/*
 * Print full method info.  Pass the Method* in a0.  Preserves regs.
 */
common_printMethod:
    STACK_STORE_RA()
    JAL(dvmMterpPrintMethod)
    STACK_LOAD_RA()
    RETURN

/*
 * Call a C helper function that dumps regs and possibly some
 * additional info.  Requires the C function to be compiled in.
 */
    .if 0
common_dumpRegs:
    STACK_STORE_RA()
    JAL(dvmMterpDumpMipsRegs)
    STACK_LOAD_RA()
    RETURN
    .endif

/*
 * Zero-terminated ASCII string data.
 */
    .data

.LstrBadEntryPoint:
    .asciiz "Bad entry point %d\n"
.LstrDivideByZero:
    .asciiz "divide by zero"
.LstrFilledNewArrayNotImpl:
    .asciiz "filled-new-array only implemented for 'int'"
.LstrLogTag:
    .asciiz  "mterp"
.LstrExceptionNotCaughtLocally:
    .asciiz  "Exception %s from %s:%d not caught locally\n"

.LstrNewline:
    .asciiz "\n"
.LstrSqueak:
    .asciiz "<%d>"
.LstrPrintHex:
    .asciiz "<0x%x>"
.LstrPrintLong:
    .asciiz "<%lld>"


    .global dvmAsmAltInstructionStart
    .type   dvmAsmAltInstructionStart, %function
    .text

dvmAsmAltInstructionStart = .L_ALT_OP_NOP
/* ------------------------------ */
    .balign 128
.L_ALT_OP_NOP: /* 0x00 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (0 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE: /* 0x01 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (1 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_FROM16: /* 0x02 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (2 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_16: /* 0x03 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (3 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_WIDE: /* 0x04 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (4 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_WIDE_FROM16: /* 0x05 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (5 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_WIDE_16: /* 0x06 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (6 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_OBJECT: /* 0x07 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (7 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_OBJECT_FROM16: /* 0x08 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (8 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_OBJECT_16: /* 0x09 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (9 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_RESULT: /* 0x0a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (10 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_RESULT_WIDE: /* 0x0b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (11 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_RESULT_OBJECT: /* 0x0c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (12 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MOVE_EXCEPTION: /* 0x0d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (13 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_RETURN_VOID: /* 0x0e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (14 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_RETURN: /* 0x0f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (15 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_RETURN_WIDE: /* 0x10 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (16 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_RETURN_OBJECT: /* 0x11 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (17 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CONST_4: /* 0x12 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (18 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CONST_16: /* 0x13 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (19 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CONST: /* 0x14 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (20 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CONST_HIGH16: /* 0x15 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (21 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CONST_WIDE_16: /* 0x16 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (22 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CONST_WIDE_32: /* 0x17 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (23 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CONST_WIDE: /* 0x18 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (24 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CONST_WIDE_HIGH16: /* 0x19 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (25 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CONST_STRING: /* 0x1a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (26 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CONST_STRING_JUMBO: /* 0x1b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (27 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CONST_CLASS: /* 0x1c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (28 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MONITOR_ENTER: /* 0x1d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (29 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MONITOR_EXIT: /* 0x1e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (30 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CHECK_CAST: /* 0x1f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (31 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INSTANCE_OF: /* 0x20 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (32 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_ARRAY_LENGTH: /* 0x21 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (33 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_NEW_INSTANCE: /* 0x22 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (34 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_NEW_ARRAY: /* 0x23 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (35 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_FILLED_NEW_ARRAY: /* 0x24 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (36 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_FILLED_NEW_ARRAY_RANGE: /* 0x25 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (37 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_FILL_ARRAY_DATA: /* 0x26 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (38 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_THROW: /* 0x27 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (39 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_GOTO: /* 0x28 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (40 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_GOTO_16: /* 0x29 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (41 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_GOTO_32: /* 0x2a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (42 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_PACKED_SWITCH: /* 0x2b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (43 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SPARSE_SWITCH: /* 0x2c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (44 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CMPL_FLOAT: /* 0x2d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (45 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CMPG_FLOAT: /* 0x2e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (46 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CMPL_DOUBLE: /* 0x2f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (47 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CMPG_DOUBLE: /* 0x30 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (48 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_CMP_LONG: /* 0x31 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (49 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_EQ: /* 0x32 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (50 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_NE: /* 0x33 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (51 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_LT: /* 0x34 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (52 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_GE: /* 0x35 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (53 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_GT: /* 0x36 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (54 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_LE: /* 0x37 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (55 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_EQZ: /* 0x38 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (56 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_NEZ: /* 0x39 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (57 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_LTZ: /* 0x3a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (58 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_GEZ: /* 0x3b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (59 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_GTZ: /* 0x3c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (60 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IF_LEZ: /* 0x3d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (61 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_UNUSED_3E: /* 0x3e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (62 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_UNUSED_3F: /* 0x3f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (63 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_UNUSED_40: /* 0x40 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (64 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_UNUSED_41: /* 0x41 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (65 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_UNUSED_42: /* 0x42 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (66 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_UNUSED_43: /* 0x43 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (67 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AGET: /* 0x44 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (68 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AGET_WIDE: /* 0x45 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (69 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AGET_OBJECT: /* 0x46 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (70 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AGET_BOOLEAN: /* 0x47 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (71 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AGET_BYTE: /* 0x48 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (72 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AGET_CHAR: /* 0x49 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (73 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AGET_SHORT: /* 0x4a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (74 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_APUT: /* 0x4b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (75 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_APUT_WIDE: /* 0x4c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (76 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_APUT_OBJECT: /* 0x4d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (77 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_APUT_BOOLEAN: /* 0x4e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (78 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_APUT_BYTE: /* 0x4f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (79 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_APUT_CHAR: /* 0x50 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (80 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_APUT_SHORT: /* 0x51 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (81 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET: /* 0x52 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (82 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_WIDE: /* 0x53 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (83 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_OBJECT: /* 0x54 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (84 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_BOOLEAN: /* 0x55 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (85 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_BYTE: /* 0x56 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (86 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_CHAR: /* 0x57 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (87 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_SHORT: /* 0x58 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (88 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT: /* 0x59 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (89 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_WIDE: /* 0x5a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (90 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_OBJECT: /* 0x5b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (91 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_BOOLEAN: /* 0x5c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (92 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_BYTE: /* 0x5d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (93 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_CHAR: /* 0x5e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (94 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_SHORT: /* 0x5f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (95 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SGET: /* 0x60 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (96 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SGET_WIDE: /* 0x61 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (97 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SGET_OBJECT: /* 0x62 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (98 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SGET_BOOLEAN: /* 0x63 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (99 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SGET_BYTE: /* 0x64 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (100 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SGET_CHAR: /* 0x65 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (101 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SGET_SHORT: /* 0x66 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (102 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SPUT: /* 0x67 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (103 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SPUT_WIDE: /* 0x68 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (104 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SPUT_OBJECT: /* 0x69 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (105 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SPUT_BOOLEAN: /* 0x6a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (106 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SPUT_BYTE: /* 0x6b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (107 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SPUT_CHAR: /* 0x6c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (108 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SPUT_SHORT: /* 0x6d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (109 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_VIRTUAL: /* 0x6e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (110 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_SUPER: /* 0x6f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (111 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_DIRECT: /* 0x70 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (112 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_STATIC: /* 0x71 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (113 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_INTERFACE: /* 0x72 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (114 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_UNUSED_73: /* 0x73 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (115 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_VIRTUAL_RANGE: /* 0x74 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (116 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_SUPER_RANGE: /* 0x75 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (117 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_DIRECT_RANGE: /* 0x76 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (118 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_STATIC_RANGE: /* 0x77 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (119 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_INTERFACE_RANGE: /* 0x78 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (120 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_UNUSED_79: /* 0x79 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (121 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_UNUSED_7A: /* 0x7a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (122 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_NEG_INT: /* 0x7b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (123 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_NOT_INT: /* 0x7c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (124 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_NEG_LONG: /* 0x7d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (125 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_NOT_LONG: /* 0x7e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (126 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_NEG_FLOAT: /* 0x7f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (127 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_NEG_DOUBLE: /* 0x80 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (128 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INT_TO_LONG: /* 0x81 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (129 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INT_TO_FLOAT: /* 0x82 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (130 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INT_TO_DOUBLE: /* 0x83 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (131 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_LONG_TO_INT: /* 0x84 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (132 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_LONG_TO_FLOAT: /* 0x85 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (133 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_LONG_TO_DOUBLE: /* 0x86 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (134 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_FLOAT_TO_INT: /* 0x87 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (135 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_FLOAT_TO_LONG: /* 0x88 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (136 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_FLOAT_TO_DOUBLE: /* 0x89 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (137 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DOUBLE_TO_INT: /* 0x8a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (138 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DOUBLE_TO_LONG: /* 0x8b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (139 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DOUBLE_TO_FLOAT: /* 0x8c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (140 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INT_TO_BYTE: /* 0x8d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (141 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INT_TO_CHAR: /* 0x8e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (142 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INT_TO_SHORT: /* 0x8f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (143 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_ADD_INT: /* 0x90 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (144 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SUB_INT: /* 0x91 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (145 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MUL_INT: /* 0x92 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (146 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DIV_INT: /* 0x93 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (147 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_REM_INT: /* 0x94 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (148 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AND_INT: /* 0x95 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (149 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_OR_INT: /* 0x96 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (150 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_XOR_INT: /* 0x97 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (151 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SHL_INT: /* 0x98 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (152 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SHR_INT: /* 0x99 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (153 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_USHR_INT: /* 0x9a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (154 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_ADD_LONG: /* 0x9b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (155 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SUB_LONG: /* 0x9c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (156 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MUL_LONG: /* 0x9d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (157 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DIV_LONG: /* 0x9e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (158 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_REM_LONG: /* 0x9f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (159 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AND_LONG: /* 0xa0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (160 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_OR_LONG: /* 0xa1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (161 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_XOR_LONG: /* 0xa2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (162 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SHL_LONG: /* 0xa3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (163 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SHR_LONG: /* 0xa4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (164 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_USHR_LONG: /* 0xa5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (165 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_ADD_FLOAT: /* 0xa6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (166 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SUB_FLOAT: /* 0xa7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (167 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MUL_FLOAT: /* 0xa8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (168 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DIV_FLOAT: /* 0xa9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (169 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_REM_FLOAT: /* 0xaa */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (170 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_ADD_DOUBLE: /* 0xab */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (171 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SUB_DOUBLE: /* 0xac */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (172 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MUL_DOUBLE: /* 0xad */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (173 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DIV_DOUBLE: /* 0xae */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (174 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_REM_DOUBLE: /* 0xaf */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (175 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_ADD_INT_2ADDR: /* 0xb0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (176 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SUB_INT_2ADDR: /* 0xb1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (177 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MUL_INT_2ADDR: /* 0xb2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (178 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DIV_INT_2ADDR: /* 0xb3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (179 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_REM_INT_2ADDR: /* 0xb4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (180 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AND_INT_2ADDR: /* 0xb5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (181 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_OR_INT_2ADDR: /* 0xb6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (182 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_XOR_INT_2ADDR: /* 0xb7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (183 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SHL_INT_2ADDR: /* 0xb8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (184 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SHR_INT_2ADDR: /* 0xb9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (185 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_USHR_INT_2ADDR: /* 0xba */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (186 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_ADD_LONG_2ADDR: /* 0xbb */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (187 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SUB_LONG_2ADDR: /* 0xbc */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (188 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MUL_LONG_2ADDR: /* 0xbd */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (189 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DIV_LONG_2ADDR: /* 0xbe */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (190 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_REM_LONG_2ADDR: /* 0xbf */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (191 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AND_LONG_2ADDR: /* 0xc0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (192 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_OR_LONG_2ADDR: /* 0xc1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (193 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_XOR_LONG_2ADDR: /* 0xc2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (194 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SHL_LONG_2ADDR: /* 0xc3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (195 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SHR_LONG_2ADDR: /* 0xc4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (196 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_USHR_LONG_2ADDR: /* 0xc5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (197 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_ADD_FLOAT_2ADDR: /* 0xc6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (198 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SUB_FLOAT_2ADDR: /* 0xc7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (199 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MUL_FLOAT_2ADDR: /* 0xc8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (200 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DIV_FLOAT_2ADDR: /* 0xc9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (201 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_REM_FLOAT_2ADDR: /* 0xca */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (202 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_ADD_DOUBLE_2ADDR: /* 0xcb */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (203 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SUB_DOUBLE_2ADDR: /* 0xcc */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (204 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MUL_DOUBLE_2ADDR: /* 0xcd */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (205 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DIV_DOUBLE_2ADDR: /* 0xce */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (206 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_REM_DOUBLE_2ADDR: /* 0xcf */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (207 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_ADD_INT_LIT16: /* 0xd0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (208 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_RSUB_INT: /* 0xd1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (209 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MUL_INT_LIT16: /* 0xd2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (210 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DIV_INT_LIT16: /* 0xd3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (211 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_REM_INT_LIT16: /* 0xd4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (212 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AND_INT_LIT16: /* 0xd5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (213 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_OR_INT_LIT16: /* 0xd6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (214 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_XOR_INT_LIT16: /* 0xd7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (215 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_ADD_INT_LIT8: /* 0xd8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (216 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_RSUB_INT_LIT8: /* 0xd9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (217 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_MUL_INT_LIT8: /* 0xda */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (218 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_DIV_INT_LIT8: /* 0xdb */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (219 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_REM_INT_LIT8: /* 0xdc */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (220 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_AND_INT_LIT8: /* 0xdd */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (221 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_OR_INT_LIT8: /* 0xde */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (222 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_XOR_INT_LIT8: /* 0xdf */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (223 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SHL_INT_LIT8: /* 0xe0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (224 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SHR_INT_LIT8: /* 0xe1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (225 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_USHR_INT_LIT8: /* 0xe2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (226 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_VOLATILE: /* 0xe3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (227 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_VOLATILE: /* 0xe4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (228 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SGET_VOLATILE: /* 0xe5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (229 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SPUT_VOLATILE: /* 0xe6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (230 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_OBJECT_VOLATILE: /* 0xe7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (231 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_WIDE_VOLATILE: /* 0xe8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (232 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_WIDE_VOLATILE: /* 0xe9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (233 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SGET_WIDE_VOLATILE: /* 0xea */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (234 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SPUT_WIDE_VOLATILE: /* 0xeb */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (235 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_BREAKPOINT: /* 0xec */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (236 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_THROW_VERIFICATION_ERROR: /* 0xed */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (237 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_EXECUTE_INLINE: /* 0xee */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (238 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_EXECUTE_INLINE_RANGE: /* 0xef */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (239 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_OBJECT_INIT_RANGE: /* 0xf0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (240 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_RETURN_VOID_BARRIER: /* 0xf1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (241 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_QUICK: /* 0xf2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (242 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_WIDE_QUICK: /* 0xf3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (243 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IGET_OBJECT_QUICK: /* 0xf4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (244 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_QUICK: /* 0xf5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (245 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_WIDE_QUICK: /* 0xf6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (246 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_OBJECT_QUICK: /* 0xf7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (247 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_VIRTUAL_QUICK: /* 0xf8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (248 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_VIRTUAL_QUICK_RANGE: /* 0xf9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (249 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_SUPER_QUICK: /* 0xfa */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (250 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_INVOKE_SUPER_QUICK_RANGE: /* 0xfb */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (251 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_IPUT_OBJECT_VOLATILE: /* 0xfc */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (252 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SGET_OBJECT_VOLATILE: /* 0xfd */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (253 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_SPUT_OBJECT_VOLATILE: /* 0xfe */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (254 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

/* ------------------------------ */
    .balign 128
.L_ALT_OP_UNUSED_FF: /* 0xff */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (255 * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX

    .balign 128
    .size   dvmAsmAltInstructionStart, .-dvmAsmAltInstructionStart
    .global dvmAsmAltInstructionEnd
dvmAsmAltInstructionEnd:
