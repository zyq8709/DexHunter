/*
 * This file was generated automatically by gen-template.py for 'mips'.
 *
 * --> DO NOT EDIT <--
 */

/* File: mips/header.S */
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

#if defined(WITH_JIT)

/*
 * This is a #include, not a %include, because we want the C pre-processor
 * to expand the macros into assembler assignment statements.
 */
#include "../../../mterp/common/asm-constants.h"
#include "../../../mterp/common/mips-defines.h"
#include "../../../mterp/common/jit-config.h"
#include <asm/regdef.h>
#include <asm/fpregdef.h>

#ifdef	__mips_hard_float
#define		HARD_FLOAT
#else
#define		SOFT_FLOAT
#endif

/* MIPS definitions and declarations

   reg	nick		purpose
   s0	rPC		interpreted program counter, used for fetching instructions
   s1	rFP		interpreted frame pointer, used for accessing locals and args
   s2	rSELF		pointer to thread
   s3	rIBASE		interpreted instruction base pointer, used for computed goto
   s4	rINST		first 16-bit code unit of current instruction
*/

/* register offsets */
#define r_ZERO      0
#define r_AT        1
#define r_V0        2
#define r_V1        3
#define r_A0        4
#define r_A1        5
#define r_A2        6
#define r_A3        7
#define r_T0        8
#define r_T1        9
#define r_T2        10
#define r_T3        11
#define r_T4        12
#define r_T5        13
#define r_T6        14
#define r_T7        15
#define r_S0        16
#define r_S1        17
#define r_S2        18
#define r_S3        19
#define r_S4        20
#define r_S5        21
#define r_S6        22
#define r_S7        23
#define r_T8        24
#define r_T9        25
#define r_K0        26
#define r_K1        27
#define r_GP        28
#define r_SP        29
#define r_FP        30
#define r_RA        31
#define r_F0        32
#define r_F1        33
#define r_F2        34
#define r_F3        35
#define r_F4        36
#define r_F5        37
#define r_F6        38
#define r_F7        39
#define r_F8        40
#define r_F9        41
#define r_F10       42
#define r_F11       43
#define r_F12       44
#define r_F13       45
#define r_F14       46
#define r_F15       47
#define r_F16       48
#define r_F17       49
#define r_F18       50
#define r_F19       51
#define r_F20       52
#define r_F21       53
#define r_F22       54
#define r_F23       55
#define r_F24       56
#define r_F25       57
#define r_F26       58
#define r_F27       59
#define r_F28       60
#define r_F29       61
#define r_F30       62
#define r_F31       63

/* single-purpose registers, given names for clarity */
#define rPC	s0
#define rFP	s1
#define rSELF	s2
#define rIBASE	s3
#define rINST	s4
#define rOBJ	s5
#define rBIX	s6
#define rTEMP	s7

/* The long arguments sent to function calls in Big-endian mode should be register
swapped when sent to functions in little endian mode. In other words long variable
sent as a0(MSW), a1(LSW) for a function call in LE mode should be sent as a1, a0 in
Big Endian mode */

#ifdef HAVE_LITTLE_ENDIAN
#define rARG0     a0
#define rARG1     a1
#define rARG2     a2
#define rARG3     a3
#define rRESULT0  v0
#define rRESULT1  v1
#else
#define rARG0     a1
#define rARG1     a0
#define rARG2     a3
#define rARG3     a2
#define rRESULT0  v1
#define rRESULT1  v0
#endif


/* save/restore the PC and/or FP from the thread struct */
#define LOAD_PC_FROM_SELF()	lw	rPC, offThread_pc(rSELF)
#define SAVE_PC_TO_SELF()	sw	rPC, offThread_pc(rSELF)
#define LOAD_FP_FROM_SELF()	lw	rFP, offThread_curFrame(rSELF)
#define SAVE_FP_TO_SELF()	sw	rFP, offThread_curFrame(rSELF)

#define EXPORT_PC() \
	sw	rPC, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)

#define SAVEAREA_FROM_FP(rd, _fpreg) \
	subu	rd, _fpreg, sizeofStackSaveArea

#define FETCH_INST()			lhu	rINST, (rPC)

#define FETCH_ADVANCE_INST(_count)	lhu     rINST, (_count*2)(rPC); \
					addu	rPC, rPC, (_count * 2)

#define FETCH_ADVANCE_INST_RB(rd)	addu	rPC, rPC, rd;	\
					lhu     rINST, (rPC)

#define FETCH(rd, _count)		lhu	rd, (_count * 2)(rPC)
#define FETCH_S(rd, _count)		lh	rd, (_count * 2)(rPC)

#ifdef HAVE_LITTLE_ENDIAN

#define FETCH_B(rd, _count)            lbu     rd, (_count * 2)(rPC)
#define FETCH_C(rd, _count)            lbu     rd, (_count * 2 + 1)(rPC)

#else

#define FETCH_B(rd, _count)            lbu     rd, (_count * 2 + 1)(rPC)
#define FETCH_C(rd, _count)            lbu     rd, (_count * 2)(rPC)

#endif

#define GET_INST_OPCODE(rd)		and	rd, rINST, 0xFF

#define GOTO_OPCODE(rd)			sll  rd, rd, -1000;	\
					addu rd, rIBASE, rd;	\
					jr  rd


#define LOAD(rd, rbase)			lw  rd, 0(rbase)
#define LOAD_F(rd, rbase)		l.s rd, (rbase)
#define STORE(rd, rbase)		sw  rd, 0(rbase)
#define STORE_F(rd, rbase)		s.s rd, (rbase)

#define GET_VREG(rd, rix)		LOAD_eas2(rd,rFP,rix)

#define GET_VREG_F(rd, rix)		EAS2(AT, rFP, rix);		\
					.set noat;  l.s rd, (AT); .set at

#define SET_VREG(rd, rix)		STORE_eas2(rd, rFP, rix)

#define SET_VREG_GOTO(rd, rix, dst)	.set noreorder;		\
					sll  dst, dst, -1000;	\
					addu dst, rIBASE, dst;			\
					sll  t8, rix, 2;	\
					addu t8, t8, rFP;	\
					jr  dst;		\
					sw  rd, 0(t8);		\
					.set reorder

#define SET_VREG_F(rd, rix)		EAS2(AT, rFP, rix);		\
					.set noat;  s.s	rd, (AT); .set at


#define GET_OPA(rd)			srl     rd, rINST, 8
#ifndef		MIPS32R2
#define GET_OPA4(rd)			GET_OPA(rd);  and  rd, 0xf
#else
#define GET_OPA4(rd)			ext	rd, rd, 8, 4
#endif
#define GET_OPB(rd)			srl     rd, rINST, 12

#define LOAD_rSELF_OFF(rd,off)		lw    rd, offThread_##off##(rSELF)

#define LOAD_rSELF_method(rd)		LOAD_rSELF_OFF(rd, method)
#define LOAD_rSELF_methodClassDex(rd)	LOAD_rSELF_OFF(rd, methodClassDex)
#define LOAD_rSELF_interpStackEnd(rd)	LOAD_rSELF_OFF(rd, interpStackEnd)
#define LOAD_rSELF_retval(rd)		LOAD_rSELF_OFF(rd, retval)
#define LOAD_rSELF_pActiveProfilers(rd)	LOAD_rSELF_OFF(rd, pActiveProfilers)
#define LOAD_rSELF_bailPtr(rd)		LOAD_rSELF_OFF(rd, bailPtr)

#define GET_JIT_PROF_TABLE(rd)		LOAD_rSELF_OFF(rd,pJitProfTable)
#define GET_JIT_THRESHOLD(rd)		LOAD_rSELF_OFF(rd,jitThreshold)

/*
 * Form an Effective Address rd = rbase + roff<<n;
 * Uses reg AT
 */
#define EASN(rd,rbase,roff,rshift)	.set noat;		\
					sll  AT, roff, rshift;	\
					addu rd, rbase, AT;	\
					.set at

#define EAS1(rd,rbase,roff)		EASN(rd,rbase,roff,1)
#define EAS2(rd,rbase,roff)		EASN(rd,rbase,roff,2)
#define EAS3(rd,rbase,roff)		EASN(rd,rbase,roff,3)
#define EAS4(rd,rbase,roff)		EASN(rd,rbase,roff,4)

/*
 * Form an Effective Shift Right rd = rbase + roff>>n;
 * Uses reg AT
 */
#define ESRN(rd,rbase,roff,rshift)	.set noat;		\
					srl  AT, roff, rshift;	\
					addu rd, rbase, AT;	\
					.set at

#define LOAD_eas2(rd,rbase,roff)	EAS2(AT, rbase, roff);  \
					.set noat;  lw  rd, 0(AT); .set at

#define STORE_eas2(rd,rbase,roff)	EAS2(AT, rbase, roff);  \
					.set noat;  sw  rd, 0(AT); .set at

#define LOAD_RB_OFF(rd,rbase,off)	lw	rd, off(rbase)
#define LOADu2_RB_OFF(rd,rbase,off)	lhu	rd, off(rbase)
#define STORE_RB_OFF(rd,rbase,off)	sw	rd, off(rbase)

#ifdef HAVE_LITTLE_ENDIAN

#define STORE64_off(rlo,rhi,rbase,off)	        sw	rlo, off(rbase);	\
					        sw	rhi, (off+4)(rbase)
#define LOAD64_off(rlo,rhi,rbase,off)	        lw	rlo, off(rbase);	\
					        lw	rhi, (off+4)(rbase)

#define STORE64_off_F(rlo,rhi,rbase,off)	s.s	rlo, off(rbase);	\
						s.s	rhi, (off+4)(rbase)
#define LOAD64_off_F(rlo,rhi,rbase,off)		l.s	rlo, off(rbase);	\
						l.s	rhi, (off+4)(rbase)
#else

#define STORE64_off(rlo,rhi,rbase,off)	        sw	rlo, (off+4)(rbase);	\
					        sw	rhi, (off)(rbase)
#define LOAD64_off(rlo,rhi,rbase,off)	        lw	rlo, (off+4)(rbase);	\
					        lw	rhi, (off)(rbase)
#define STORE64_off_F(rlo,rhi,rbase,off)	s.s	rlo, (off+4)(rbase);	\
						s.s	rhi, (off)(rbase)
#define LOAD64_off_F(rlo,rhi,rbase,off)		l.s	rlo, (off+4)(rbase);	\
						l.s	rhi, (off)(rbase)
#endif

#define STORE64(rlo,rhi,rbase)		STORE64_off(rlo,rhi,rbase,0)
#define LOAD64(rlo,rhi,rbase)		LOAD64_off(rlo,rhi,rbase,0)

#define STORE64_F(rlo,rhi,rbase)	STORE64_off_F(rlo,rhi,rbase,0)
#define LOAD64_F(rlo,rhi,rbase)		LOAD64_off_F(rlo,rhi,rbase,0)

#define STORE64_lo(rd,rbase)		sw	rd, 0(rbase)
#define STORE64_hi(rd,rbase)		sw	rd, 4(rbase)


#define LOAD_offThread_exception(rd,rbase)		LOAD_RB_OFF(rd,rbase,offThread_exception)
#define LOAD_base_offArrayObject_length(rd,rbase)	LOAD_RB_OFF(rd,rbase,offArrayObject_length)
#define LOAD_base_offClassObject_accessFlags(rd,rbase)	LOAD_RB_OFF(rd,rbase,offClassObject_accessFlags)
#define LOAD_base_offClassObject_descriptor(rd,rbase)   LOAD_RB_OFF(rd,rbase,offClassObject_descriptor)
#define LOAD_base_offClassObject_super(rd,rbase)	LOAD_RB_OFF(rd,rbase,offClassObject_super)

#define LOAD_base_offClassObject_vtable(rd,rbase)	LOAD_RB_OFF(rd,rbase,offClassObject_vtable)
#define LOAD_base_offClassObject_vtableCount(rd,rbase)	LOAD_RB_OFF(rd,rbase,offClassObject_vtableCount)
#define LOAD_base_offDvmDex_pResClasses(rd,rbase)	LOAD_RB_OFF(rd,rbase,offDvmDex_pResClasses)
#define LOAD_base_offDvmDex_pResFields(rd,rbase)	LOAD_RB_OFF(rd,rbase,offDvmDex_pResFields)

#define LOAD_base_offDvmDex_pResMethods(rd,rbase)	LOAD_RB_OFF(rd,rbase,offDvmDex_pResMethods)
#define LOAD_base_offDvmDex_pResStrings(rd,rbase)	LOAD_RB_OFF(rd,rbase,offDvmDex_pResStrings)
#define LOAD_base_offInstField_byteOffset(rd,rbase)	LOAD_RB_OFF(rd,rbase,offInstField_byteOffset)
#define LOAD_base_offStaticField_value(rd,rbase)	LOAD_RB_OFF(rd,rbase,offStaticField_value)
#define LOAD_base_offMethod_clazz(rd,rbase)		LOAD_RB_OFF(rd,rbase,offMethod_clazz)

#define LOAD_base_offMethod_name(rd,rbase)		LOAD_RB_OFF(rd,rbase,offMethod_name)
#define LOAD_base_offObject_clazz(rd,rbase)		LOAD_RB_OFF(rd,rbase,offObject_clazz)

#define LOADu2_offMethod_methodIndex(rd,rbase)		LOADu2_RB_OFF(rd,rbase,offMethod_methodIndex)


#define STORE_offThread_exception(rd,rbase)		STORE_RB_OFF(rd,rbase,offThread_exception)


#define	STACK_STORE(rd,off)	sw   rd, off(sp)
#define	STACK_LOAD(rd,off)	lw   rd, off(sp)
#define CREATE_STACK(n)	 	subu sp, sp, n
#define DELETE_STACK(n)	 	addu sp, sp, n

#define SAVE_RA(offset)	 	STACK_STORE(ra, offset)
#define LOAD_RA(offset)	 	STACK_LOAD(ra, offset)

#define LOAD_ADDR(dest,addr)	la   dest, addr
#define LOAD_IMM(dest, imm)	li   dest, imm
#define MOVE_REG(dest,src)	move dest, src
#define	RETURN			jr   ra
#define	STACK_SIZE		128

#define STACK_OFFSET_ARG04	16
#define STACK_OFFSET_GP		84
#define STACK_OFFSET_rFP	112

/* This directive will make sure all subsequent jal restore gp at a known offset */
        .cprestore STACK_OFFSET_GP

#define JAL(func)		move rTEMP, ra;				\
				jal  func;				\
				move ra, rTEMP

#define JALR(reg)		move rTEMP, ra;				\
				jalr ra, reg;				\
				move ra, rTEMP

#define BAL(n)			bal  n

#define	STACK_STORE_RA()  	CREATE_STACK(STACK_SIZE);		\
				STACK_STORE(gp, STACK_OFFSET_GP);	\
				STACK_STORE(ra, 124)

#define	STACK_STORE_S0()  	STACK_STORE_RA();			\
				STACK_STORE(s0, 116)

#define	STACK_STORE_S0S1()  	STACK_STORE_S0();			\
				STACK_STORE(s1, STACK_OFFSET_rFP)

#define	STACK_LOAD_RA()		STACK_LOAD(ra, 124);			\
				STACK_LOAD(gp, STACK_OFFSET_GP);	\
				DELETE_STACK(STACK_SIZE)

#define	STACK_LOAD_S0()  	STACK_LOAD(s0, 116);			\
				STACK_LOAD_RA()

#define	STACK_LOAD_S0S1()  	STACK_LOAD(s1, STACK_OFFSET_rFP);	\
				STACK_LOAD_S0()

#define STACK_STORE_FULL()	CREATE_STACK(STACK_SIZE);	\
				STACK_STORE(ra, 124);		\
				STACK_STORE(fp, 120);		\
				STACK_STORE(s0, 116);		\
				STACK_STORE(s1, STACK_OFFSET_rFP);	\
				STACK_STORE(s2, 108);		\
				STACK_STORE(s3, 104);		\
				STACK_STORE(s4, 100);		\
				STACK_STORE(s5, 96);		\
				STACK_STORE(s6, 92);		\
				STACK_STORE(s7, 88);

#define STACK_LOAD_FULL()	STACK_LOAD(gp, STACK_OFFSET_GP);	\
				STACK_LOAD(s7, 88);	\
				STACK_LOAD(s6, 92);	\
				STACK_LOAD(s5, 96);	\
				STACK_LOAD(s4, 100);	\
				STACK_LOAD(s3, 104);	\
				STACK_LOAD(s2, 108);	\
				STACK_LOAD(s1, STACK_OFFSET_rFP);	\
				STACK_LOAD(s0, 116);	\
				STACK_LOAD(fp, 120);	\
				STACK_LOAD(ra, 124);	\
				DELETE_STACK(STACK_SIZE)

/*
 * first 8 words are reserved for function calls
 * Maximum offset is STACK_OFFSET_SCRMX-STACK_OFFSET_SCR
 */
#define STACK_OFFSET_SCR   32
#define SCRATCH_STORE(r,off) \
    STACK_STORE(r, STACK_OFFSET_SCR+off);
#define SCRATCH_LOAD(r,off) \
    STACK_LOAD(r, STACK_OFFSET_SCR+off);

/* File: mips/platform.S */
/*
 * ===========================================================================
 *  CPU-version-specific defines and utility
 * ===========================================================================
 */



    .global dvmCompilerTemplateStart
    .type   dvmCompilerTemplateStart, %function
    .section .data.rel.ro

dvmCompilerTemplateStart:

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_CMP_LONG
dvmCompiler_TEMPLATE_CMP_LONG:
/* File: mips/TEMPLATE_CMP_LONG.S */
    /*
     * Compare two 64-bit values
     *    x = y     return  0
     *    x < y     return -1
     *    x > y     return  1
     *
     * I think I can improve on the ARM code by the following observation
     *    slt   t0,  x.hi, y.hi;        # (x.hi < y.hi) ? 1:0
     *    sgt   t1,  x.hi, y.hi;        # (y.hi > x.hi) ? 1:0
     *    subu  v0, t0, t1              # v0= -1:1:0 for [ < > = ]
     *
     * This code assumes the register pair ordering will depend on endianess (a1:a0 or a0:a1).
     *    a1:a0 => vBB
     *    a3:a2 => vCC
     */
    /* cmp-long vAA, vBB, vCC */
    slt    t0, rARG1, rARG3             # compare hi
    sgt    t1, rARG1, rARG3
    subu   v0, t1, t0                   # v0<- (-1,1,0)
    bnez   v0, .LTEMPLATE_CMP_LONG_finish
                                        # at this point x.hi==y.hi
    sltu   t0, rARG0, rARG2             # compare lo
    sgtu   t1, rARG0, rARG2
    subu   v0, t1, t0                   # v0<- (-1,1,0) for [< > =]
.LTEMPLATE_CMP_LONG_finish:
    RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_RETURN
dvmCompiler_TEMPLATE_RETURN:
/* File: mips/TEMPLATE_RETURN.S */
    /*
     * Unwind a frame from the Dalvik stack for compiled OP_RETURN_XXX.
     * If the stored value in returnAddr
     * is non-zero, the caller is compiled by the JIT thus return to the
     * address in the code cache following the invoke instruction. Otherwise
     * return to the special dvmJitToInterpNoChain entry point.
     */
#if defined(TEMPLATE_INLINE_PROFILING)
    # preserve a0-a2 and ra
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(ra, 12)

    # a0=rSELF
    move    a0, rSELF
    la      t9, dvmFastMethodTraceExit
    JALR(t9)
    lw      gp, STACK_OFFSET_GP(sp)

    # restore a0-a2 and ra
    SCRATCH_LOAD(ra, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)
#endif
    SAVEAREA_FROM_FP(a0, rFP)           # a0<- saveArea (old)
    lw      t0, offStackSaveArea_prevFrame(a0)     # t0<- saveArea->prevFrame
    lbu     t1, offThread_breakFlags(rSELF)        # t1<- breakFlags
    lw      rPC, offStackSaveArea_savedPc(a0)      # rPC<- saveArea->savedPc
#if !defined(WITH_SELF_VERIFICATION)
    lw      t2,  offStackSaveArea_returnAddr(a0)   # t2<- chaining cell ret
#else
    move    t2, zero                               # disable chaining
#endif
    lw      a2, offStackSaveArea_method - sizeofStackSaveArea(t0)
                                                   # a2<- method we're returning to
#if !defined(WITH_SELF_VERIFICATION)
    beq     a2, zero, 1f                           # bail to interpreter
#else
    bne     a2, zero, 2f
    JALR(ra)                                       # punt to interpreter and compare state
    # DOUG: assume this does not return ???
2:
#endif
    la      t4, .LdvmJitToInterpNoChainNoProfile   # defined in footer.S
    lw      a1, (t4)
    move    rFP, t0                                # publish new FP
    beq     a2, zero, 4f
    lw      t0, offMethod_clazz(a2)                # t0<- method->clazz
4:

    sw      a2, offThread_method(rSELF)            # self->method = newSave->method
    lw      a0, offClassObject_pDvmDex(t0)         # a0<- method->clazz->pDvmDex
    sw      rFP, offThread_curFrame(rSELF)         # self->curFrame = fp
    add     rPC, rPC, 3*2                          # publish new rPC
    sw      a0, offThread_methodClassDex(rSELF)
    movn    t2, zero, t1                           # check the breadFlags and
                                                   # clear the chaining cell address
    sw      t2, offThread_inJitCodeCache(rSELF)    # in code cache or not
    beq     t2, zero, 3f                           # chaining cell exists?
    JALR(t2)                                       # jump to the chaining cell
    # DOUG: assume this does not return ???
3:
#if defined(WITH_JIT_TUNING)
    li      a0, kCallsiteInterpreted
#endif
    j       a1                                     # callsite is interpreted
1:
    sw      zero, offThread_inJitCodeCache(rSELF)  # reset inJitCodeCache
    SAVE_PC_TO_SELF()                              # SAVE_PC_FP_TO_SELF()
    SAVE_FP_TO_SELF()
    la      t4, .LdvmMterpStdBail                  # defined in footer.S
    lw      a2, (t4)
    move    a0, rSELF                              # Expecting rSELF in a0
    JALR(a2)                                       # exit the interpreter
    # DOUG: assume this does not return ???

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_NO_OPT
dvmCompiler_TEMPLATE_INVOKE_METHOD_NO_OPT:
/* File: mips/TEMPLATE_INVOKE_METHOD_NO_OPT.S */
    /*
     * For polymorphic callsites - setup the Dalvik frame and load Dalvik PC
     * into rPC then jump to dvmJitToInterpNoChain to dispatch the
     * runtime-resolved callee.
     */
    # a0 = methodToCall, a1 = returnCell, rPC = dalvikCallsite
    lh     t7, offMethod_registersSize(a0)        # t7<- methodToCall->regsSize
    lh     a2, offMethod_outsSize(a0)             # a2<- methodToCall->outsSize
    lw     t9, offThread_interpStackEnd(rSELF)    # t9<- interpStackEnd
    lbu    t8, offThread_breakFlags(rSELF)        # t8<- breakFlags
    move   a3, a1                                 # a3<- returnCell
    SAVEAREA_FROM_FP(a1, rFP)                     # a1<- stack save area
    sll    t6, t7, 2                              # multiply regsSize by 4 (4 bytes per reg)
    sub    a1, a1, t6                             # a1<- newFp(old savearea-regsSize)
    SAVEAREA_FROM_FP(t0, a1)                      # t0<- stack save area
    sll    t6, a2, 2                              # multiply outsSize by 4 (4 bytes per reg)
    sub    t0, t0, t6                             # t0<- bottom (newsave-outsSize)
    bgeu   t0, t9, 1f                             # bottom < interpStackEnd?
    RETURN                                        # return to raise stack overflow excep.

1:
    # a1 = newFP, a0 = methodToCall, a3 = returnCell, rPC = dalvikCallsite
    lw     t9, offMethod_clazz(a0)                # t9<- methodToCall->clazz
    lw     t0, offMethod_accessFlags(a0)          # t0<- methodToCall->accessFlags
    sw     rPC, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)
    sw     rPC, (offStackSaveArea_savedPc - sizeofStackSaveArea)(a1)
    lw     rPC, offMethod_insns(a0)               # rPC<- methodToCall->insns

    # set up newSaveArea
    sw     rFP, (offStackSaveArea_prevFrame - sizeofStackSaveArea)(a1)
    sw     a3, (offStackSaveArea_returnAddr - sizeofStackSaveArea)(a1)
    sw     a0, (offStackSaveArea_method - sizeofStackSaveArea)(a1)
    beqz   t8, 2f                                 # breakFlags != 0
    RETURN                                        # bail to the interpreter

2:
    and    t6, t0, ACC_NATIVE
    beqz   t6, 3f
#if !defined(WITH_SELF_VERIFICATION)
    j      .LinvokeNative
#else
    RETURN                                        # bail to the interpreter
#endif

3:
    # continue executing the next instruction through the interpreter
    la     t0, .LdvmJitToInterpTraceSelectNoChain # defined in footer.S
    lw     rTEMP, (t0)
    lw     a3, offClassObject_pDvmDex(t9)         # a3<- method->clazz->pDvmDex

    # Update "thread" values for the new method
    sw     a0, offThread_method(rSELF)            # self->method = methodToCall
    sw     a3, offThread_methodClassDex(rSELF)    # self->methodClassDex = ...
    move   rFP, a1                                # fp = newFp
    sw     rFP, offThread_curFrame(rSELF)         # self->curFrame = newFp
#if defined(TEMPLATE_INLINE_PROFILING)
    # preserve rTEMP,a1-a3
    SCRATCH_STORE(rTEMP, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(a3, 12)

    # a0=methodToCall, a1=rSELF
    move   a1, rSELF
    la     t9, dvmFastMethodTraceEnter
    JALR(t9)
    lw     gp, STACK_OFFSET_GP(sp)

    # restore rTEMP,a1-a3
    SCRATCH_LOAD(a3, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(rTEMP, 0)
#endif

    # Start executing the callee
#if defined(WITH_JIT_TUNING)
    li     a0, kInlineCacheMiss
#endif
    jr     rTEMP                                  # dvmJitToInterpTraceSelectNoChain

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_CHAIN
dvmCompiler_TEMPLATE_INVOKE_METHOD_CHAIN:
/* File: mips/TEMPLATE_INVOKE_METHOD_CHAIN.S */
    /*
     * For monomorphic callsite, setup the Dalvik frame and return to the
     * Thumb code through the link register to transfer control to the callee
     * method through a dedicated chaining cell.
     */
    # a0 = methodToCall, a1 = returnCell, rPC = dalvikCallsite
    # methodToCall is guaranteed to be non-native
.LinvokeChain:
    lh     t7, offMethod_registersSize(a0)        # t7<- methodToCall->regsSize
    lh     a2, offMethod_outsSize(a0)             # a2<- methodToCall->outsSize
    lw     t9, offThread_interpStackEnd(rSELF)    # t9<- interpStackEnd
    lbu    t8, offThread_breakFlags(rSELF)        # t8<- breakFlags
    move   a3, a1                                 # a3<- returnCell
    SAVEAREA_FROM_FP(a1, rFP)                     # a1<- stack save area
    sll    t6, t7, 2                              # multiply regsSize by 4 (4 bytes per reg)
    sub    a1, a1, t6                             # a1<- newFp(old savearea-regsSize)
    SAVEAREA_FROM_FP(t0, a1)                      # t0<- stack save area
    add    t2, ra, 8                              # setup the punt-to-interp address
                                                  # 8 bytes skips branch and delay slot
    sll    t6, a2, 2                              # multiply outsSize by 4 (4 bytes per reg)
    sub    t0, t0, t6                             # t0<- bottom (newsave-outsSize)
    bgeu   t0, t9, 1f                             # bottom < interpStackEnd?
    jr     t2                                     # return to raise stack overflow excep.

1:
    # a1 = newFP, a0 = methodToCall, a3 = returnCell, rPC = dalvikCallsite
    lw     t9, offMethod_clazz(a0)                # t9<- methodToCall->clazz
    sw     rPC, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)
    sw     rPC, (offStackSaveArea_savedPc - sizeofStackSaveArea)(a1)
    lw     rPC, offMethod_insns(a0)               # rPC<- methodToCall->insns

    # set up newSaveArea
    sw     rFP, (offStackSaveArea_prevFrame - sizeofStackSaveArea)(a1)
    sw     a3, (offStackSaveArea_returnAddr - sizeofStackSaveArea)(a1)
    sw     a0, (offStackSaveArea_method - sizeofStackSaveArea)(a1)
    beqz   t8, 2f                                 # breakFlags != 0
    jr     t2                                     # bail to the interpreter

2:
    lw     a3, offClassObject_pDvmDex(t9)         # a3<- methodToCall->clazz->pDvmDex

    # Update "thread" values for the new method
    sw     a0, offThread_method(rSELF)            # self->method = methodToCall
    sw     a3, offThread_methodClassDex(rSELF)    # self->methodClassDex = ...
    move   rFP, a1                                # fp = newFp
    sw     rFP, offThread_curFrame(rSELF)         # self->curFrame = newFp
#if defined(TEMPLATE_INLINE_PROFILING)
    # preserve a0-a2 and ra
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(ra, 12)

    move   a1, rSELF
    # a0=methodToCall, a1=rSELF
    la     t9, dvmFastMethodTraceEnter
    jalr   t9
    lw     gp, STACK_OFFSET_GP(sp)

    # restore a0-a2 and ra
    SCRATCH_LOAD(ra, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)
#endif
    RETURN                                        # return to the callee-chaining cell

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN
dvmCompiler_TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN:
/* File: mips/TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN.S */
    /*
     * For polymorphic callsite, check whether the cached class pointer matches
     * the current one. If so setup the Dalvik frame and return to the
     * Thumb code through the link register to transfer control to the callee
     * method through a dedicated chaining cell.
     *
     * The predicted chaining cell is declared in ArmLIR.h with the
     * following layout:
     *
     *  typedef struct PredictedChainingCell {
     *      u4 branch;
     *      u4 delay_slot;
     *      const ClassObject *clazz;
     *      const Method *method;
     *      u4 counter;
     *  } PredictedChainingCell;
     *
     * Upon returning to the callsite:
     *    - lr   : to branch to the chaining cell
     *    - lr+8 : to punt to the interpreter
     *    - lr+16: to fully resolve the callee and may rechain.
     *             a3 <- class
     */
    # a0 = this, a1 = returnCell, a2 = predictedChainCell, rPC = dalvikCallsite
    lw      a3, offObject_clazz(a0)     # a3 <- this->class
    lw      rIBASE, 8(a2)                   # t0 <- predictedChainCell->clazz
    lw      a0, 12(a2)                  # a0 <- predictedChainCell->method
    lw      t1, offThread_icRechainCount(rSELF)    # t1 <- shared rechainCount

#if defined(WITH_JIT_TUNING)
    la      rINST, .LdvmICHitCount
    #add     t2, t2, 1
    bne    a3, rIBASE, 1f
    nop
    lw      t2, 0(rINST)
    add     t2, t2, 1
    sw      t2, 0(rINST)
1:
    #add     t2, t2, 1
#endif
    beq     a3, rIBASE, .LinvokeChain       # branch if predicted chain is valid
    lw      rINST, offClassObject_vtable(a3)     # rINST <- this->class->vtable
    beqz    rIBASE, 2f                      # initialized class or not
    sub     a1, t1, 1                   # count--
    sw      a1, offThread_icRechainCount(rSELF)   # write back to InterpState
    b       3f
2:
    move    a1, zero
3:
    add     ra, ra, 16                  # return to fully-resolve landing pad
    /*
     * a1 <- count
     * a2 <- &predictedChainCell
     * a3 <- this->class
     * rPC <- dPC
     * rINST <- this->class->vtable
     */
    RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_NATIVE
dvmCompiler_TEMPLATE_INVOKE_METHOD_NATIVE:
/* File: mips/TEMPLATE_INVOKE_METHOD_NATIVE.S */
    # a0 = methodToCall, a1 = returnCell, rPC = dalvikCallsite
    lh     t7, offMethod_registersSize(a0)        # t7<- methodToCall->regsSize
    lw     t9, offThread_interpStackEnd(rSELF)    # t9<- interpStackEnd
    lbu    t8, offThread_breakFlags(rSELF)        # t8<- breakFlags
    move   a3, a1                                 # a3<- returnCell
    SAVEAREA_FROM_FP(a1, rFP)                     # a1<- stack save area
    sll    t6, t7, 2                              # multiply regsSize by 4 (4 bytes per reg)
    sub    a1, a1, t6                             # a1<- newFp(old savearea-regsSize)
    SAVEAREA_FROM_FP(t0, a1)                      # t0<- stack save area
    bgeu   t0, t9, 1f                             # bottom < interpStackEnd?
    RETURN                                        # return to raise stack overflow excep.

1:
    # a1 = newFP, a0 = methodToCall, a3 = returnCell, rPC = dalvikCallsite
    sw     rPC, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)
    sw     rPC, (offStackSaveArea_savedPc - sizeofStackSaveArea)(a1)
    lw     rPC, offMethod_insns(a0)               # rPC<- methodToCall->insns

    # set up newSaveArea
    sw     rFP, (offStackSaveArea_prevFrame - sizeofStackSaveArea)(a1)
    sw     a3, (offStackSaveArea_returnAddr - sizeofStackSaveArea)(a1)
    sw     a0, (offStackSaveArea_method - sizeofStackSaveArea)(a1)
    lw     rTEMP, offMethod_nativeFunc(a0)        # t9<- method->nativeFunc
#if !defined(WITH_SELF_VERIFICATION)
    beqz   t8, 2f                                 # breakFlags != 0
    RETURN                                        # bail to the interpreter
2:
#else
    RETURN                                        # bail to the interpreter unconditionally
#endif

    # go ahead and transfer control to the native code
    lw     t6, offThread_jniLocal_topCookie(rSELF)  # t6<- thread->localRef->...
    sw     a1, offThread_curFrame(rSELF)          # self->curFrame = newFp
    sw     zero, offThread_inJitCodeCache(rSELF)  # not in the jit code cache
    sw     t6, (offStackSaveArea_localRefCookie - sizeofStackSaveArea)(a1)
                                                  # newFp->localRefCookie=top
    SAVEAREA_FROM_FP(rBIX, a1)                    # rBIX<- new stack save area
    move   a2, a0                                 # a2<- methodToCall
    move   a0, a1                                 # a0<- newFp
    add    a1, rSELF, offThread_retval            # a1<- &retval
    move   a3, rSELF                              # a3<- self
#if defined(TEMPLATE_INLINE_PROFILING)
    # a2: methodToCall
    # preserve rTEMP,a0-a3
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(a3, 12)
    SCRATCH_STORE(rTEMP, 16)

    move   a0, a2
    move   a1, rSELF
    # a0=JNIMethod, a1=rSELF
    la      t9, dvmFastMethodTraceEnter
    JALR(t9)                                      # off to the native code
    lw     gp, STACK_OFFSET_GP(sp)

    # restore rTEMP,a0-a3
    SCRATCH_LOAD(rTEMP, 16)
    SCRATCH_LOAD(a3, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)

    move   rOBJ, a2                               # save a2
#endif
    move   t9, rTEMP
    JALR(t9)                                   # off to the native code
    lw     gp, STACK_OFFSET_GP(sp)

#if defined(TEMPLATE_INLINE_PROFILING)
    move   a0, rOBJ
    move   a1, rSELF
    # a0=JNIMethod, a1=rSELF
    la      t9, dvmFastNativeMethodTraceExit
    JALR(t9)
    lw     gp, STACK_OFFSET_GP(sp)
#endif

    # native return; rBIX=newSaveArea
    # equivalent to dvmPopJniLocals
    lw     a2, offStackSaveArea_returnAddr(rBIX)     # a2 = chaining cell ret addr
    lw     a0, offStackSaveArea_localRefCookie(rBIX) # a0<- saved->top
    lw     a1, offThread_exception(rSELF)            # check for exception
    sw     rFP, offThread_curFrame(rSELF)            # self->curFrame = fp
    sw     a0, offThread_jniLocal_topCookie(rSELF)   # new top <- old top
    lw     a0, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)

    # a0 = dalvikCallsitePC
    bnez   a1, .LhandleException                     # handle exception if any

    sw     a2, offThread_inJitCodeCache(rSELF)       # set the mode properly
    beqz   a2, 3f
    jr     a2                                        # go if return chaining cell still exist

3:
    # continue executing the next instruction through the interpreter
    la     a1, .LdvmJitToInterpTraceSelectNoChain    # defined in footer.S
    lw     a1, (a1)
    add    rPC, a0, 3*2                              # reconstruct new rPC (advance 3 dalvik instr)

#if defined(WITH_JIT_TUNING)
    li     a0, kCallsiteInterpreted
#endif
    jr     a1

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_MUL_LONG
dvmCompiler_TEMPLATE_MUL_LONG:
/* File: mips/TEMPLATE_MUL_LONG.S */
    /*
     * Signed 64-bit integer multiply.
     *
     * For JIT: op1 in a0/a1, op2 in a2/a3, return in v0/v1
     *
     * Consider WXxYZ (a1a0 x a3a2) with a long multiply:
     *
     *         a1   a0
     *   x     a3   a2
     *   -------------
     *       a2a1 a2a0
     *       a3a0
     *  a3a1 (<= unused)
     *  ---------------
     *         v1   v0
     *
     */
    /* mul-long vAA, vBB, vCC */
    mul     rRESULT1,rARG3,rARG0              #  v1= a3a0
    multu   rARG2,rARG0
    mfhi    t1
    mflo    rRESULT0                          #  v0= a2a0
    mul     t0,rARG2,rARG1                    #  t0= a2a1
    addu    rRESULT1,rRESULT1,t1              #  v1= a3a0 + hi(a2a0)
    addu    rRESULT1,rRESULT1,t0              #  v1= a3a0 + hi(a2a0) + a2a1;
    RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_SHL_LONG
dvmCompiler_TEMPLATE_SHL_LONG:
/* File: mips/TEMPLATE_SHL_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     */
    /* shl-long vAA:vBB(rARG1:rARG0), vCC(a2) - result in (rRESULT1:rRESULT0) */
    sll     rRESULT0, rARG0, a2		#  rlo<- alo << (shift&31)
    not     rRESULT1, a2		#  rhi<- 31-shift  (shift is 5b)
    srl     rARG0, 1
    srl     rARG0, rRESULT1		#  alo<- alo >> (32-(shift&31))
    sll     rRESULT1, rARG1, a2		#  rhi<- ahi << (shift&31)
    or      rRESULT1, rARG0		#  rhi<- rhi | alo
    andi    a2, 0x20			#  shift< shift & 0x20
    movn    rRESULT1, rRESULT0, a2	#  rhi<- rlo (if shift&0x20)
    movn    rRESULT0, zero, a2		#  rlo<- 0  (if shift&0x20)
    RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_SHR_LONG
dvmCompiler_TEMPLATE_SHR_LONG:
/* File: mips/TEMPLATE_SHR_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     */
    /* shr-long vAA:vBB(rARG1:rARG0), vCC(a2) - result in (rRESULT1:rRESULT0) */
    sra     rRESULT1, rARG1, a2		#  rhi<- ahi >> (shift&31)
    srl     rRESULT0, rARG0, a2		#  rlo<- alo >> (shift&31)
    sra     a3, rARG1, 31		#  a3<- sign(ah)
    not     rARG0, a2			#  alo<- 31-shift (shift is 5b)
    sll     rARG1, 1
    sll     rARG1, rARG0		#  ahi<- ahi << (32-(shift&31))
    or      rRESULT0, rARG1		#  rlo<- rlo | ahi
    andi    a2, 0x20			#  shift & 0x20
    movn    rRESULT0, rRESULT1, a2	#  rlo<- rhi (if shift&0x20)
    movn    rRESULT1, a3, a2		#  rhi<- sign(ahi) (if shift&0x20)
    RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_USHR_LONG
dvmCompiler_TEMPLATE_USHR_LONG:
/* File: mips/TEMPLATE_USHR_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     */
    /* ushr-long vAA:vBB(rARG1:rARG0), vCC(a2) - result in (rRESULT1:rRESULT0) */
    srl     rRESULT1, rARG1, a2		#  rhi<- ahi >> (shift&31)
    srl     rRESULT0, rARG0, a2		#  rlo<- alo >> (shift&31)
    not     rARG0, a2			#  alo<- 31-n  (shift is 5b)
    sll     rARG1, 1
    sll     rARG1, rARG0		#  ahi<- ahi << (32-(shift&31))
    or      rRESULT0, rARG1		#  rlo<- rlo | ahi
    andi    a2, 0x20			#  shift & 0x20
    movn    rRESULT0, rRESULT1, a2	#  rlo<- rhi (if shift&0x20)
    movn    rRESULT1, zero, a2		#  rhi<- 0 (if shift&0x20)
    RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_ADD_FLOAT_VFP
dvmCompiler_TEMPLATE_ADD_FLOAT_VFP:
/* File: mips/TEMPLATE_ADD_FLOAT_VFP.S */
/* File: mips/fbinop.S */
    /*
     * Generic 32-bit binary float operation. a0 = a1 op a2.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = op1 address
     *     a2 = op2 address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     *
     */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    LOAD(a0, a1)                        # a0<- vBB
    LOAD(a1, a2)                        # a1<- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
                               # optional op
    JAL(__addsf3)                              # v0 = result
    STORE(v0, rOBJ)                     # vAA <- v0
#else
    LOAD_F(fa0, a1)                     # fa0<- vBB
    LOAD_F(fa1, a2)                     # fa1<- vCC
    .if 0
    # is second operand zero?
    li.s        ft0, 0
    c.eq.s      fcc0, ft0, fa1          # condition bit and comparision with 0
    bc1t        fcc0, common_errDivideByZero
    .endif
                               # optional op
    add.s fv0, fa0, fa1                            # fv0 = result
    STORE_F(fv0, rOBJ)                  # vAA <- fv0
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_SUB_FLOAT_VFP
dvmCompiler_TEMPLATE_SUB_FLOAT_VFP:
/* File: mips/TEMPLATE_SUB_FLOAT_VFP.S */
/* File: mips/fbinop.S */
    /*
     * Generic 32-bit binary float operation. a0 = a1 op a2.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = op1 address
     *     a2 = op2 address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     *
     */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    LOAD(a0, a1)                        # a0<- vBB
    LOAD(a1, a2)                        # a1<- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
                               # optional op
    JAL(__subsf3)                              # v0 = result
    STORE(v0, rOBJ)                     # vAA <- v0
#else
    LOAD_F(fa0, a1)                     # fa0<- vBB
    LOAD_F(fa1, a2)                     # fa1<- vCC
    .if 0
    # is second operand zero?
    li.s        ft0, 0
    c.eq.s      fcc0, ft0, fa1          # condition bit and comparision with 0
    bc1t        fcc0, common_errDivideByZero
    .endif
                               # optional op
    sub.s fv0, fa0, fa1                            # fv0 = result
    STORE_F(fv0, rOBJ)                  # vAA <- fv0
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_MUL_FLOAT_VFP
dvmCompiler_TEMPLATE_MUL_FLOAT_VFP:
/* File: mips/TEMPLATE_MUL_FLOAT_VFP.S */
/* File: mips/fbinop.S */
    /*
     * Generic 32-bit binary float operation. a0 = a1 op a2.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = op1 address
     *     a2 = op2 address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     *
     */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    LOAD(a0, a1)                        # a0<- vBB
    LOAD(a1, a2)                        # a1<- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
                               # optional op
    JAL(__mulsf3)                              # v0 = result
    STORE(v0, rOBJ)                     # vAA <- v0
#else
    LOAD_F(fa0, a1)                     # fa0<- vBB
    LOAD_F(fa1, a2)                     # fa1<- vCC
    .if 0
    # is second operand zero?
    li.s        ft0, 0
    c.eq.s      fcc0, ft0, fa1          # condition bit and comparision with 0
    bc1t        fcc0, common_errDivideByZero
    .endif
                               # optional op
    mul.s fv0, fa0, fa1                            # fv0 = result
    STORE_F(fv0, rOBJ)                  # vAA <- fv0
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_DIV_FLOAT_VFP
dvmCompiler_TEMPLATE_DIV_FLOAT_VFP:
/* File: mips/TEMPLATE_DIV_FLOAT_VFP.S */
/* File: mips/fbinop.S */
    /*
     * Generic 32-bit binary float operation. a0 = a1 op a2.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = op1 address
     *     a2 = op2 address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     *
     */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    LOAD(a0, a1)                        # a0<- vBB
    LOAD(a1, a2)                        # a1<- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
                               # optional op
    JAL(__divsf3)                              # v0 = result
    STORE(v0, rOBJ)                     # vAA <- v0
#else
    LOAD_F(fa0, a1)                     # fa0<- vBB
    LOAD_F(fa1, a2)                     # fa1<- vCC
    .if 0
    # is second operand zero?
    li.s        ft0, 0
    c.eq.s      fcc0, ft0, fa1          # condition bit and comparision with 0
    bc1t        fcc0, common_errDivideByZero
    .endif
                               # optional op
    div.s fv0, fa0, fa1                            # fv0 = result
    STORE_F(fv0, rOBJ)                  # vAA <- fv0
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_ADD_DOUBLE_VFP
dvmCompiler_TEMPLATE_ADD_DOUBLE_VFP:
/* File: mips/TEMPLATE_ADD_DOUBLE_VFP.S */
/* File: mips/fbinopWide.S */
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
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = op1 address
     *     a2 = op2 address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    move t0, a1                         # save a1
    move t1, a2                         # save a2
    LOAD64(rARG0, rARG1, t0)            # a0/a1<- vBB/vBB+1
    LOAD64(rARG2, rARG3, t1)            # a2/a3<- vCC/vCC+1
    .if 0
    or          t0, rARG2, rARG3        # second arg (a2-a3) is zero?
    beqz        t0, common_errDivideByZero
    .endif
                               # optional op
    JAL(__adddf3)                              # result<- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    LOAD64_F(fa0, fa0f, a1)
    LOAD64_F(fa1, fa1f, a2)
    .if 0
    li.d        ft0, 0
    c.eq.d      fcc0, fa1, ft0
    bc1t        fcc0, common_errDivideByZero
    .endif
                               # optional op
    add.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_SUB_DOUBLE_VFP
dvmCompiler_TEMPLATE_SUB_DOUBLE_VFP:
/* File: mips/TEMPLATE_SUB_DOUBLE_VFP.S */
/* File: mips/fbinopWide.S */
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
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = op1 address
     *     a2 = op2 address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    move t0, a1                         # save a1
    move t1, a2                         # save a2
    LOAD64(rARG0, rARG1, t0)            # a0/a1<- vBB/vBB+1
    LOAD64(rARG2, rARG3, t1)            # a2/a3<- vCC/vCC+1
    .if 0
    or          t0, rARG2, rARG3        # second arg (a2-a3) is zero?
    beqz        t0, common_errDivideByZero
    .endif
                               # optional op
    JAL(__subdf3)                              # result<- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    LOAD64_F(fa0, fa0f, a1)
    LOAD64_F(fa1, fa1f, a2)
    .if 0
    li.d        ft0, 0
    c.eq.d      fcc0, fa1, ft0
    bc1t        fcc0, common_errDivideByZero
    .endif
                               # optional op
    sub.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_MUL_DOUBLE_VFP
dvmCompiler_TEMPLATE_MUL_DOUBLE_VFP:
/* File: mips/TEMPLATE_MUL_DOUBLE_VFP.S */
/* File: mips/fbinopWide.S */
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
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = op1 address
     *     a2 = op2 address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    move t0, a1                         # save a1
    move t1, a2                         # save a2
    LOAD64(rARG0, rARG1, t0)            # a0/a1<- vBB/vBB+1
    LOAD64(rARG2, rARG3, t1)            # a2/a3<- vCC/vCC+1
    .if 0
    or          t0, rARG2, rARG3        # second arg (a2-a3) is zero?
    beqz        t0, common_errDivideByZero
    .endif
                               # optional op
    JAL(__muldf3)                              # result<- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    LOAD64_F(fa0, fa0f, a1)
    LOAD64_F(fa1, fa1f, a2)
    .if 0
    li.d        ft0, 0
    c.eq.d      fcc0, fa1, ft0
    bc1t        fcc0, common_errDivideByZero
    .endif
                               # optional op
    mul.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_DIV_DOUBLE_VFP
dvmCompiler_TEMPLATE_DIV_DOUBLE_VFP:
/* File: mips/TEMPLATE_DIV_DOUBLE_VFP.S */
/* File: mips/fbinopWide.S */
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
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = op1 address
     *     a2 = op2 address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    move t0, a1                         # save a1
    move t1, a2                         # save a2
    LOAD64(rARG0, rARG1, t0)            # a0/a1<- vBB/vBB+1
    LOAD64(rARG2, rARG3, t1)            # a2/a3<- vCC/vCC+1
    .if 0
    or          t0, rARG2, rARG3        # second arg (a2-a3) is zero?
    beqz        t0, common_errDivideByZero
    .endif
                               # optional op
    JAL(__divdf3)                              # result<- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    LOAD64_F(fa0, fa0f, a1)
    LOAD64_F(fa1, fa1f, a2)
    .if 0
    li.d        ft0, 0
    c.eq.d      fcc0, fa1, ft0
    bc1t        fcc0, common_errDivideByZero
    .endif
                               # optional op
    div.d fv0, fa0, fa1
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_DOUBLE_TO_FLOAT_VFP
dvmCompiler_TEMPLATE_DOUBLE_TO_FLOAT_VFP:
/* File: mips/TEMPLATE_DOUBLE_TO_FLOAT_VFP.S */
/* File: mips/funopNarrower.S */
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
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = src dalvik register address
     *
     */
    move rINST, a0                      # save a0
#ifdef  SOFT_FLOAT
    move t0, a1                         # save a1
    LOAD64(rARG0, rARG1, t0)            # a0/a1<- vB/vB+1
                               # optional op
    JAL(__truncdfsf2)                              # v0<- op, a0-a3 changed
.LTEMPLATE_DOUBLE_TO_FLOAT_VFP_set_vreg:
    STORE(v0, rINST)                    # vA<- v0
#else
    LOAD64_F(fa0, fa0f, a1)
                               # optional op
    cvt.s.d  fv0,fa0                            # fv0 = result
.LTEMPLATE_DOUBLE_TO_FLOAT_VFP_set_vreg_f:
    STORE_F(fv0, rINST)                 # vA<- fv0
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_DOUBLE_TO_INT_VFP
dvmCompiler_TEMPLATE_DOUBLE_TO_INT_VFP:
/* File: mips/TEMPLATE_DOUBLE_TO_INT_VFP.S */
/* File: mips/funopNarrower.S */
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
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = src dalvik register address
     *
     */
    move rINST, a0                      # save a0
#ifdef  SOFT_FLOAT
    move t0, a1                         # save a1
    LOAD64(rARG0, rARG1, t0)            # a0/a1<- vB/vB+1
                               # optional op
    b    d2i_doconv                              # v0<- op, a0-a3 changed
.LTEMPLATE_DOUBLE_TO_INT_VFP_set_vreg:
    STORE(v0, rINST)                    # vA<- v0
#else
    LOAD64_F(fa0, fa0f, a1)
                               # optional op
    b    d2i_doconv                            # fv0 = result
.LTEMPLATE_DOUBLE_TO_INT_VFP_set_vreg_f:
    STORE_F(fv0, rINST)                 # vA<- fv0
#endif
    RETURN


/*
 * Convert the double in a0/a1 to an int in a0.
 *
 * We have to clip values to int min/max per the specification.  The
 * expected common case is a "reasonable" value that converts directly
 * to modest integer.  The EABI convert function isn't doing this for us.
 * Use rBIX / rOBJ as global to hold arguments (they are not bound to a global var)
 */

d2i_doconv:
#ifdef SOFT_FLOAT
    la          t0, .LDOUBLE_TO_INT_max
    LOAD64(rARG2, rARG3, t0)
    move        rBIX, rARG0                       # save a0
    move        rOBJ, rARG1                       #  and a1
    JAL(__gedf2)                               # is arg >= maxint?

    move        t0, v0
    li          v0, ~0x80000000                # return maxint (7fffffff)
    bgez        t0, .LTEMPLATE_DOUBLE_TO_INT_VFP_set_vreg       # nonzero == yes

    move        rARG0, rBIX                       # recover arg
    move        rARG1, rOBJ
    la          t0, .LDOUBLE_TO_INT_min
    LOAD64(rARG2, rARG3, t0)
    JAL(__ledf2)                               # is arg <= minint?

    move        t0, v0
    li          v0, 0x80000000                 # return minint (80000000)
    blez        t0, .LTEMPLATE_DOUBLE_TO_INT_VFP_set_vreg       # nonzero == yes

    move        rARG0, rBIX                  # recover arg
    move        rARG1, rOBJ
    move        rARG2, rBIX                  # compare against self
    move        rARG3, rOBJ
    JAL(__nedf2)                        # is arg == self?

    move        t0, v0                  # zero == no
    li          v0, 0
    bnez        t0, .LTEMPLATE_DOUBLE_TO_INT_VFP_set_vreg        # return zero for NaN

    move        rARG0, rBIX                  # recover arg
    move        rARG1, rOBJ
    JAL(__fixdfsi)                      # convert double to int
    b           .LTEMPLATE_DOUBLE_TO_INT_VFP_set_vreg
#else
    la          t0, .LDOUBLE_TO_INT_max
    LOAD64_F(fa1, fa1f, t0)
    c.ole.d     fcc0, fa1, fa0
    l.s         fv0, .LDOUBLE_TO_INT_maxret
    bc1t        .LTEMPLATE_DOUBLE_TO_INT_VFP_set_vreg_f

    la          t0, .LDOUBLE_TO_INT_min
    LOAD64_F(fa1, fa1f, t0)
    c.ole.d     fcc0, fa0, fa1
    l.s         fv0, .LDOUBLE_TO_INT_minret
    bc1t        .LTEMPLATE_DOUBLE_TO_INT_VFP_set_vreg_f

    mov.d       fa1, fa0
    c.un.d      fcc0, fa0, fa1
    li.s        fv0, 0
    bc1t        .LTEMPLATE_DOUBLE_TO_INT_VFP_set_vreg_f

    trunc.w.d   fv0, fa0
    b           .LTEMPLATE_DOUBLE_TO_INT_VFP_set_vreg_f
#endif


.LDOUBLE_TO_INT_max:
    .dword   0x41dfffffffc00000
.LDOUBLE_TO_INT_min:
    .dword   0xc1e0000000000000                  # minint, as a double (high word)
.LDOUBLE_TO_INT_maxret:
    .word   0x7fffffff
.LDOUBLE_TO_INT_minret:
    .word   0x80000000

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_FLOAT_TO_DOUBLE_VFP
dvmCompiler_TEMPLATE_FLOAT_TO_DOUBLE_VFP:
/* File: mips/TEMPLATE_FLOAT_TO_DOUBLE_VFP.S */
/* File: mips/funopWider.S */
    /*
     * Generic 32bit-to-64bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "d0 = op s0".
     *
     * For: int-to-double, float-to-double
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = src dalvik register address
     */
    /* unop vA, vB */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    LOAD(a0, a1)                        # a0<- vB
                               # optional op
    JAL(__extendsfdf2)                              # result<- op, a0-a3 changed

.LTEMPLATE_FLOAT_TO_DOUBLE_VFP_set_vreg:
    STORE64(rRESULT0, rRESULT1, rOBJ)   # vA/vA+1<- v0/v1
#else
    LOAD_F(fa0, a1)                     # fa0<- vB
                               # optional op
    cvt.d.s fv0, fa0

.LTEMPLATE_FLOAT_TO_DOUBLE_VFP_set_vreg:
    STORE64_F(fv0, fv0f, rOBJ)                          # vA/vA+1<- fv0/fv0f
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_FLOAT_TO_INT_VFP
dvmCompiler_TEMPLATE_FLOAT_TO_INT_VFP:
/* File: mips/TEMPLATE_FLOAT_TO_INT_VFP.S */
/* File: mips/funop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-float, float-to-int
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = src dalvik register address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     *
     */
    move rOBJ, a0                       # save a0
#ifdef SOFT_FLOAT
    LOAD(a0, a1)                        # a0<- vBB
                               # optional op
    b    f2i_doconv                              # v0<- op, a0-a3 changed
.LTEMPLATE_FLOAT_TO_INT_VFP_set_vreg:
    STORE(v0, rOBJ)                     # vAA<- v0
#else
    LOAD_F(fa0, a1)                     # fa0<- vBB
                               # optional op
    b        f2i_doconv                            # fv0 = result
.LTEMPLATE_FLOAT_TO_INT_VFP_set_vreg_f:
    STORE_F(fv0, rOBJ)                  # vAA <- fv0
#endif
    RETURN


/*
 * Not an entry point as it is used only once !!
 */
f2i_doconv:
#ifdef SOFT_FLOAT
        li      a1, 0x4f000000  # (float)maxint
        move    rBIX, a0
        JAL(__gesf2)            # is arg >= maxint?
        move    t0, v0
        li      v0, ~0x80000000 # return maxint (7fffffff)
        bgez    t0, .LTEMPLATE_FLOAT_TO_INT_VFP_set_vreg

        move    a0, rBIX                # recover arg
        li      a1, 0xcf000000  # (float)minint
        JAL(__lesf2)

        move    t0, v0
        li      v0, 0x80000000  # return minint (80000000)
        blez    t0, .LTEMPLATE_FLOAT_TO_INT_VFP_set_vreg
        move    a0, rBIX
        move    a1, rBIX
        JAL(__nesf2)

        move    t0, v0
        li      v0, 0           # return zero for NaN
        bnez    t0, .LTEMPLATE_FLOAT_TO_INT_VFP_set_vreg

        move    a0, rBIX
        JAL(__fixsfsi)
        b .LTEMPLATE_FLOAT_TO_INT_VFP_set_vreg
#else
        l.s             fa1, .LFLOAT_TO_INT_max
        c.ole.s         fcc0, fa1, fa0
        l.s             fv0, .LFLOAT_TO_INT_ret_max
        bc1t            .LTEMPLATE_FLOAT_TO_INT_VFP_set_vreg_f

        l.s             fa1, .LFLOAT_TO_INT_min
        c.ole.s         fcc0, fa0, fa1
        l.s             fv0, .LFLOAT_TO_INT_ret_min
        bc1t            .LTEMPLATE_FLOAT_TO_INT_VFP_set_vreg_f

        mov.s           fa1, fa0
        c.un.s          fcc0, fa0, fa1
        li.s            fv0, 0
        bc1t            .LTEMPLATE_FLOAT_TO_INT_VFP_set_vreg_f

        trunc.w.s       fv0, fa0
        b .LTEMPLATE_FLOAT_TO_INT_VFP_set_vreg_f
#endif

.LFLOAT_TO_INT_max:
        .word   0x4f000000
.LFLOAT_TO_INT_min:
        .word   0xcf000000
.LFLOAT_TO_INT_ret_max:
        .word   0x7fffffff
.LFLOAT_TO_INT_ret_min:
        .word   0x80000000


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INT_TO_DOUBLE_VFP
dvmCompiler_TEMPLATE_INT_TO_DOUBLE_VFP:
/* File: mips/TEMPLATE_INT_TO_DOUBLE_VFP.S */
/* File: mips/funopWider.S */
    /*
     * Generic 32bit-to-64bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "d0 = op s0".
     *
     * For: int-to-double, float-to-double
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = src dalvik register address
     */
    /* unop vA, vB */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    LOAD(a0, a1)                        # a0<- vB
                               # optional op
    JAL(__floatsidf)                              # result<- op, a0-a3 changed

.LTEMPLATE_INT_TO_DOUBLE_VFP_set_vreg:
    STORE64(rRESULT0, rRESULT1, rOBJ)   # vA/vA+1<- v0/v1
#else
    LOAD_F(fa0, a1)                     # fa0<- vB
                               # optional op
    cvt.d.w    fv0, fa0

.LTEMPLATE_INT_TO_DOUBLE_VFP_set_vreg:
    STORE64_F(fv0, fv0f, rOBJ)                          # vA/vA+1<- fv0/fv0f
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INT_TO_FLOAT_VFP
dvmCompiler_TEMPLATE_INT_TO_FLOAT_VFP:
/* File: mips/TEMPLATE_INT_TO_FLOAT_VFP.S */
/* File: mips/funop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-float, float-to-int
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = src dalvik register address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     *
     */
    move rOBJ, a0                       # save a0
#ifdef SOFT_FLOAT
    LOAD(a0, a1)                        # a0<- vBB
                               # optional op
    JAL(__floatsisf)                              # v0<- op, a0-a3 changed
.LTEMPLATE_INT_TO_FLOAT_VFP_set_vreg:
    STORE(v0, rOBJ)                     # vAA<- v0
#else
    LOAD_F(fa0, a1)                     # fa0<- vBB
                               # optional op
    cvt.s.w fv0, fa0                            # fv0 = result
.LTEMPLATE_INT_TO_FLOAT_VFP_set_vreg_f:
    STORE_F(fv0, rOBJ)                  # vAA <- fv0
#endif
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_CMPG_DOUBLE_VFP
dvmCompiler_TEMPLATE_CMPG_DOUBLE_VFP:
/* File: mips/TEMPLATE_CMPG_DOUBLE_VFP.S */
/* File: mips/TEMPLATE_CMPL_DOUBLE_VFP.S */
    /*
     * Compare two double precision floating-point values.  Puts 0, 1, or -1 into the
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
     * On entry:
     *    a0 = &op1 [vBB]
     *    a1 = &op2 [vCC]
     *
     * for: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */

    /* "clasic" form */
#ifdef  SOFT_FLOAT
    move rOBJ, a0                       # save a0
    move rBIX, a1                       # save a1
    LOAD64(rARG0, rARG1, rOBJ)          # a0/a1<- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)          # a2/a3<- vCC/vCC+1
    JAL(__eqdf2)                        # v0<- (vBB == vCC)
    li       rTEMP, 0                   # vAA<- 0
    beqz     v0, TEMPLATE_CMPG_DOUBLE_VFP_finish
    LOAD64(rARG0, rARG1, rOBJ)          # a0/a1<- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)          # a2/a3<- vCC/vCC+1
    JAL(__ltdf2)                        # a0<- (vBB < vCC)
    li       rTEMP, -1                  # vAA<- -1
    bltz     v0, TEMPLATE_CMPG_DOUBLE_VFP_finish
    LOAD64(rARG0, rARG1, rOBJ)          # a0/a1<- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)          # a2/a3<- vCC/vCC+1
    JAL(__gtdf2)                        # v0<- (vBB > vCC)
    li      rTEMP, 1                    # vAA<- 1
    bgtz    v0, TEMPLATE_CMPG_DOUBLE_VFP_finish
#else
    LOAD64_F(fs0, fs0f, a0)             # fs0<- vBB
    LOAD64_F(fs1, fs1f, a1)             # fs1<- vCC
    c.olt.d     fcc0, fs0, fs1          # Is fs0 < fs1
    li          rTEMP, -1
    bc1t        fcc0, TEMPLATE_CMPG_DOUBLE_VFP_finish
    c.olt.d     fcc0, fs1, fs0
    li          rTEMP, 1
    bc1t        fcc0, TEMPLATE_CMPG_DOUBLE_VFP_finish
    c.eq.d      fcc0, fs0, fs1
    li          rTEMP, 0
    bc1t        fcc0, TEMPLATE_CMPG_DOUBLE_VFP_finish
#endif

    li            rTEMP, 1

TEMPLATE_CMPG_DOUBLE_VFP_finish:
    move     v0, rTEMP                  # v0<- vAA
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_CMPL_DOUBLE_VFP
dvmCompiler_TEMPLATE_CMPL_DOUBLE_VFP:
/* File: mips/TEMPLATE_CMPL_DOUBLE_VFP.S */
    /*
     * Compare two double precision floating-point values.  Puts 0, 1, or -1 into the
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
     * On entry:
     *    a0 = &op1 [vBB]
     *    a1 = &op2 [vCC]
     *
     * for: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */

    /* "clasic" form */
#ifdef  SOFT_FLOAT
    move rOBJ, a0                       # save a0
    move rBIX, a1                       # save a1
    LOAD64(rARG0, rARG1, rOBJ)          # a0/a1<- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)          # a2/a3<- vCC/vCC+1
    JAL(__eqdf2)                        # v0<- (vBB == vCC)
    li       rTEMP, 0                   # vAA<- 0
    beqz     v0, TEMPLATE_CMPL_DOUBLE_VFP_finish
    LOAD64(rARG0, rARG1, rOBJ)          # a0/a1<- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)          # a2/a3<- vCC/vCC+1
    JAL(__ltdf2)                        # a0<- (vBB < vCC)
    li       rTEMP, -1                  # vAA<- -1
    bltz     v0, TEMPLATE_CMPL_DOUBLE_VFP_finish
    LOAD64(rARG0, rARG1, rOBJ)          # a0/a1<- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)          # a2/a3<- vCC/vCC+1
    JAL(__gtdf2)                        # v0<- (vBB > vCC)
    li      rTEMP, 1                    # vAA<- 1
    bgtz    v0, TEMPLATE_CMPL_DOUBLE_VFP_finish
#else
    LOAD64_F(fs0, fs0f, a0)             # fs0<- vBB
    LOAD64_F(fs1, fs1f, a1)             # fs1<- vCC
    c.olt.d     fcc0, fs0, fs1          # Is fs0 < fs1
    li          rTEMP, -1
    bc1t        fcc0, TEMPLATE_CMPL_DOUBLE_VFP_finish
    c.olt.d     fcc0, fs1, fs0
    li          rTEMP, 1
    bc1t        fcc0, TEMPLATE_CMPL_DOUBLE_VFP_finish
    c.eq.d      fcc0, fs0, fs1
    li          rTEMP, 0
    bc1t        fcc0, TEMPLATE_CMPL_DOUBLE_VFP_finish
#endif

    li     rTEMP, -1

TEMPLATE_CMPL_DOUBLE_VFP_finish:
    move     v0, rTEMP                  # v0<- vAA
    RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_CMPG_FLOAT_VFP
dvmCompiler_TEMPLATE_CMPG_FLOAT_VFP:
/* File: mips/TEMPLATE_CMPG_FLOAT_VFP.S */
/* File: mips/TEMPLATE_CMPL_FLOAT_VFP.S */
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
     * On entry:
     *    a0 = &op1 [vBB]
     *    a1 = &op2 [vCC]
     *
     * for: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */

    /* "clasic" form */
#ifdef  SOFT_FLOAT
    LOAD(rOBJ, a0)                      # rOBJ<- vBB
    LOAD(rBIX, a1)                      # rBIX<- vCC
    move     a0, rOBJ                   # a0<- vBB
    move     a1, rBIX                   # a1<- vCC
    JAL(__eqsf2)                        # v0<- (vBB == vCC)
    li       rTEMP, 0                   # vAA<- 0
    beqz     v0, TEMPLATE_CMPG_FLOAT_VFP_finish
    move     a0, rOBJ                   # a0<- vBB
    move     a1, rBIX                   # a1<- vCC
    JAL(__ltsf2)                        # a0<- (vBB < vCC)
    li       rTEMP, -1                  # vAA<- -1
    bltz     v0, TEMPLATE_CMPG_FLOAT_VFP_finish
    move     a0, rOBJ                   # a0<- vBB
    move     a1, rBIX                   # a1<- vCC
    JAL(__gtsf2)                        # v0<- (vBB > vCC)
    li      rTEMP, 1                    # vAA<- 1
    bgtz    v0, TEMPLATE_CMPG_FLOAT_VFP_finish
#else
    LOAD_F(fs0, a0)                     # fs0<- vBB
    LOAD_F(fs1, a1)                     # fs1<- vCC
    c.olt.s     fcc0, fs0, fs1          #Is fs0 < fs1
    li          rTEMP, -1
    bc1t        fcc0, TEMPLATE_CMPG_FLOAT_VFP_finish
    c.olt.s     fcc0, fs1, fs0
    li          rTEMP, 1
    bc1t        fcc0, TEMPLATE_CMPG_FLOAT_VFP_finish
    c.eq.s      fcc0, fs0, fs1
    li          rTEMP, 0
    bc1t        fcc0, TEMPLATE_CMPG_FLOAT_VFP_finish
#endif

    li     rTEMP, 1

TEMPLATE_CMPG_FLOAT_VFP_finish:
    move     v0, rTEMP                  # v0<- vAA
    RETURN


/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_CMPL_FLOAT_VFP
dvmCompiler_TEMPLATE_CMPL_FLOAT_VFP:
/* File: mips/TEMPLATE_CMPL_FLOAT_VFP.S */
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
     * On entry:
     *    a0 = &op1 [vBB]
     *    a1 = &op2 [vCC]
     *
     * for: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */

    /* "clasic" form */
#ifdef  SOFT_FLOAT
    LOAD(rOBJ, a0)                      # rOBJ<- vBB
    LOAD(rBIX, a1)                      # rBIX<- vCC
    move     a0, rOBJ                   # a0<- vBB
    move     a1, rBIX                   # a1<- vCC
    JAL(__eqsf2)                        # v0<- (vBB == vCC)
    li       rTEMP, 0                   # vAA<- 0
    beqz     v0, TEMPLATE_CMPL_FLOAT_VFP_finish
    move     a0, rOBJ                   # a0<- vBB
    move     a1, rBIX                   # a1<- vCC
    JAL(__ltsf2)                        # a0<- (vBB < vCC)
    li       rTEMP, -1                  # vAA<- -1
    bltz     v0, TEMPLATE_CMPL_FLOAT_VFP_finish
    move     a0, rOBJ                   # a0<- vBB
    move     a1, rBIX                   # a1<- vCC
    JAL(__gtsf2)                        # v0<- (vBB > vCC)
    li      rTEMP, 1                    # vAA<- 1
    bgtz    v0, TEMPLATE_CMPL_FLOAT_VFP_finish
#else
    LOAD_F(fs0, a0)                     # fs0<- vBB
    LOAD_F(fs1, a1)                     # fs1<- vCC
    c.olt.s     fcc0, fs0, fs1          #Is fs0 < fs1
    li          rTEMP, -1
    bc1t        fcc0, TEMPLATE_CMPL_FLOAT_VFP_finish
    c.olt.s     fcc0, fs1, fs0
    li          rTEMP, 1
    bc1t        fcc0, TEMPLATE_CMPL_FLOAT_VFP_finish
    c.eq.s      fcc0, fs0, fs1
    li          rTEMP, 0
    bc1t        fcc0, TEMPLATE_CMPL_FLOAT_VFP_finish
#endif

    li     rTEMP, -1

TEMPLATE_CMPL_FLOAT_VFP_finish:
    move     v0, rTEMP                  # v0<- vAA
    RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_SQRT_DOUBLE_VFP
dvmCompiler_TEMPLATE_SQRT_DOUBLE_VFP:
/* File: mips/TEMPLATE_SQRT_DOUBLE_VFP.S */

    /*
     * 64-bit floating point sqrt operation.
     * If the result is a NaN, bail out to library code to do
     * the right thing.
     *
     * On entry:
     *     a2 src addr of op1
     * On exit:
     *     v0,v1/fv0 = res
     */
#ifdef  SOFT_FLOAT
    LOAD64(rARG0, rARG1, a2)        # a0/a1<- vBB/vBB+1
#else
    LOAD64_F(fa0, fa0f, a2)         # fa0/fa0f<- vBB/vBB+1
    sqrt.d	fv0, fa0
    c.eq.d	fv0, fv0
    bc1t	1f
#endif
    JAL(sqrt)
1:
    RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_THROW_EXCEPTION_COMMON
dvmCompiler_TEMPLATE_THROW_EXCEPTION_COMMON:
/* File: mips/TEMPLATE_THROW_EXCEPTION_COMMON.S */
    /*
     * Throw an exception from JIT'ed code.
     * On entry:
     *    a0    Dalvik PC that raises the exception
     */
    j      .LhandleException

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_MEM_OP_DECODE
dvmCompiler_TEMPLATE_MEM_OP_DECODE:
/* File: mips/TEMPLATE_MEM_OP_DECODE.S */
#if defined(WITH_SELF_VERIFICATION)
    /*
     * This handler encapsulates heap memory ops for selfVerification mode.
     *
     * The call to the handler is inserted prior to a heap memory operation.
     * This handler then calls a function to decode the memory op, and process
     * it accordingly. Afterwards, the handler changes the return address to
     * skip the memory op so it never gets executed.
     */
#ifdef HARD_FLOAT
    /* push f0-f31 onto stack */
    sw      f0, fr0*-4(sp)              # push f0
    sw      f1, fr1*-4(sp)              # push f1
    sw      f2, fr2*-4(sp)              # push f2
    sw      f3, fr3*-4(sp)              # push f3
    sw      f4, fr4*-4(sp)              # push f4
    sw      f5, fr5*-4(sp)              # push f5
    sw      f6, fr6*-4(sp)              # push f6
    sw      f7, fr7*-4(sp)              # push f7
    sw      f8, fr8*-4(sp)              # push f8
    sw      f9, fr9*-4(sp)              # push f9
    sw      f10, fr10*-4(sp)            # push f10
    sw      f11, fr11*-4(sp)            # push f11
    sw      f12, fr12*-4(sp)            # push f12
    sw      f13, fr13*-4(sp)            # push f13
    sw      f14, fr14*-4(sp)            # push f14
    sw      f15, fr15*-4(sp)            # push f15
    sw      f16, fr16*-4(sp)            # push f16
    sw      f17, fr17*-4(sp)            # push f17
    sw      f18, fr18*-4(sp)            # push f18
    sw      f19, fr19*-4(sp)            # push f19
    sw      f20, fr20*-4(sp)            # push f20
    sw      f21, fr21*-4(sp)            # push f21
    sw      f22, fr22*-4(sp)            # push f22
    sw      f23, fr23*-4(sp)            # push f23
    sw      f24, fr24*-4(sp)            # push f24
    sw      f25, fr25*-4(sp)            # push f25
    sw      f26, fr26*-4(sp)            # push f26
    sw      f27, fr27*-4(sp)            # push f27
    sw      f28, fr28*-4(sp)            # push f28
    sw      f29, fr29*-4(sp)            # push f29
    sw      f30, fr30*-4(sp)            # push f30
    sw      f31, fr31*-4(sp)            # push f31

    sub     sp, (32-0)*4                # adjust stack pointer
#endif

    /* push gp registers (except zero, gp, sp, and fp) */
    .set noat
    sw      AT, r_AT*-4(sp)             # push at
    .set at
    sw      v0, r_V0*-4(sp)             # push v0
    sw      v1, r_V1*-4(sp)             # push v1
    sw      a0, r_A0*-4(sp)             # push a0
    sw      a1, r_A1*-4(sp)             # push a1
    sw      a2, r_A2*-4(sp)             # push a2
    sw      a3, r_A3*-4(sp)             # push a3
    sw      t0, r_T0*-4(sp)             # push t0
    sw      t1, r_T1*-4(sp)             # push t1
    sw      t2, r_T2*-4(sp)             # push t2
    sw      t3, r_T3*-4(sp)             # push t3
    sw      t4, r_T4*-4(sp)             # push t4
    sw      t5, r_T5*-4(sp)             # push t5
    sw      t6, r_T6*-4(sp)             # push t6
    sw      t7, r_T7*-4(sp)             # push t7
    sw      s0, r_S0*-4(sp)             # push s0
    sw      s1, r_S1*-4(sp)             # push s1
    sw      s2, r_S2*-4(sp)             # push s2
    sw      s3, r_S3*-4(sp)             # push s3
    sw      s4, r_S4*-4(sp)             # push s4
    sw      s5, r_S5*-4(sp)             # push s5
    sw      s6, r_S6*-4(sp)             # push s6
    sw      s7, r_S7*-4(sp)             # push s7
    sw      t8, r_T8*-4(sp)             # push t8
    sw      t9, r_T9*-4(sp)             # push t9
    sw      k0, r_K0*-4(sp)             # push k0
    sw      k1, r_K1*-4(sp)             # push k1
    sw      ra, r_RA*-4(sp)             # push RA

    # Note: even if we don't save all 32 registers, we still need to
    #       adjust SP by 32 registers due to the way we are storing
    #       the registers on the stack.
    sub     sp, (32-0)*4                # adjust stack pointer

    la     a2, .LdvmSelfVerificationMemOpDecode  # defined in footer.S
    lw     a2, (a2)
    move   a0, ra                       # a0<- link register
    move   a1, sp                       # a1<- stack pointer
    JALR(a2)

    /* pop gp registers (except zero, gp, sp, and fp) */
    # Note: even if we don't save all 32 registers, we still need to
    #       adjust SP by 32 registers due to the way we are storing
    #       the registers on the stack.
    add     sp, (32-0)*4                # adjust stack pointer
    .set noat
    lw      AT, r_AT*-4(sp)             # pop at
    .set at
    lw      v0, r_V0*-4(sp)             # pop v0
    lw      v1, r_V1*-4(sp)             # pop v1
    lw      a0, r_A0*-4(sp)             # pop a0
    lw      a1, r_A1*-4(sp)             # pop a1
    lw      a2, r_A2*-4(sp)             # pop a2
    lw      a3, r_A3*-4(sp)             # pop a3
    lw      t0, r_T0*-4(sp)             # pop t0
    lw      t1, r_T1*-4(sp)             # pop t1
    lw      t2, r_T2*-4(sp)             # pop t2
    lw      t3, r_T3*-4(sp)             # pop t3
    lw      t4, r_T4*-4(sp)             # pop t4
    lw      t5, r_T5*-4(sp)             # pop t5
    lw      t6, r_T6*-4(sp)             # pop t6
    lw      t7, r_T7*-4(sp)             # pop t7
    lw      s0, r_S0*-4(sp)             # pop s0
    lw      s1, r_S1*-4(sp)             # pop s1
    lw      s2, r_S2*-4(sp)             # pop s2
    lw      s3, r_S3*-4(sp)             # pop s3
    lw      s4, r_S4*-4(sp)             # pop s4
    lw      s5, r_S5*-4(sp)             # pop s5
    lw      s6, r_S6*-4(sp)             # pop s6
    lw      s7, r_S7*-4(sp)             # pop s7
    lw      t8, r_T8*-4(sp)             # pop t8
    lw      t9, r_T9*-4(sp)             # pop t9
    lw      k0, r_K0*-4(sp)             # pop k0
    lw      k1, r_K1*-4(sp)             # pop k1
    lw      ra, r_RA*-4(sp)             # pop RA

#ifdef HARD_FLOAT
    /* pop f0-f31 from stack */
    add     sp, (32-0)*4                # adjust stack pointer
    lw      f0, fr0*-4(sp)              # pop f0
    lw      f1, fr1*-4(sp)              # pop f1
    lw      f2, fr2*-4(sp)              # pop f2
    lw      f3, fr3*-4(sp)              # pop f3
    lw      f4, fr4*-4(sp)              # pop f4
    lw      f5, fr5*-4(sp)              # pop f5
    lw      f6, fr6*-4(sp)              # pop f6
    lw      f7, fr7*-4(sp)              # pop f7
    lw      f8, fr8*-4(sp)              # pop f8
    lw      f9, fr9*-4(sp)              # pop f9
    lw      f10, fr10*-4(sp)            # pop f10
    lw      f11, fr11*-4(sp)            # pop f11
    lw      f12, fr12*-4(sp)            # pop f12
    lw      f13, fr13*-4(sp)            # pop f13
    lw      f14, fr14*-4(sp)            # pop f14
    lw      f15, fr15*-4(sp)            # pop f15
    lw      f16, fr16*-4(sp)            # pop f16
    lw      f17, fr17*-4(sp)            # pop f17
    lw      f18, fr18*-4(sp)            # pop f18
    lw      f19, fr19*-4(sp)            # pop f19
    lw      f20, fr20*-4(sp)            # pop f20
    lw      f21, fr21*-4(sp)            # pop f21
    lw      f22, fr22*-4(sp)            # pop f22
    lw      f23, fr23*-4(sp)            # pop f23
    lw      f24, fr24*-4(sp)            # pop f24
    lw      f25, fr25*-4(sp)            # pop f25
    lw      f26, fr26*-4(sp)            # pop f26
    lw      f27, fr27*-4(sp)            # pop f27
    lw      f28, fr28*-4(sp)            # pop f28
    lw      f29, fr29*-4(sp)            # pop f29
    lw      f30, fr30*-4(sp)            # pop f30
    lw      f31, fr31*-4(sp)            # pop f31
#endif

    RETURN
#endif

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_STRING_COMPARETO
dvmCompiler_TEMPLATE_STRING_COMPARETO:
/* File: mips/TEMPLATE_STRING_COMPARETO.S */
    /*
     * String's compareTo.
     *
     * Requires a0/a1 to have been previously checked for null.  Will
     * return negative if this's string is < comp, 0 if they are the
     * same and positive if >.
     *
     * IMPORTANT NOTE:
     *
     * This code relies on hard-coded offsets for string objects, and must be
     * kept in sync with definitions in UtfString.h.  See asm-constants.h
     *
     * On entry:
     *    a0:   this object pointer
     *    a1:   comp object pointer
     *
     */

     subu  v0, a0, a1                # Same?
     bnez  v0, 1f
     RETURN
1:
     lw    t0, STRING_FIELDOFF_OFFSET(a0)
     lw    t1, STRING_FIELDOFF_OFFSET(a1)
     lw    t2, STRING_FIELDOFF_COUNT(a0)
     lw    a2, STRING_FIELDOFF_COUNT(a1)
     lw    a0, STRING_FIELDOFF_VALUE(a0)
     lw    a1, STRING_FIELDOFF_VALUE(a1)

    /*
     * At this point, we have this/comp:
     *    offset: t0/t1
     *    count:  t2/a2
     *    value:  a0/a1
     * We're going to compute
     *    a3 <- countDiff
     *    a2 <- minCount
     */
     subu  a3, t2, a2                # a3<- countDiff
     sleu  t7, t2, a2
     movn  a2, t2, t7                # a2<- minCount

     /*
      * Note: data pointers point to first element.
      */
     addu  a0, 16                    # point to contents[0]
     addu  a1, 16                    # point to contents[0]

     /* Now, build pointers to the string data */
     sll   t7, t0, 1                 # multiply offset by 2
     addu  a0, a0, t7
     sll   t7, t1, 1                 # multiply offset by 2
     addu  a1, a1, t7

     /*
      * At this point we have:
      *   a0: *this string data
      *   a1: *comp string data
      *   a2: iteration count for comparison
      *   a3: value to return if the first part of the string is equal
      *   v0: reserved for result
      *   t0-t5 available for loading string data
      */

     subu  a2, 2
     bltz  a2, do_remainder2

     /*
      * Unroll the first two checks so we can quickly catch early mismatch
      * on long strings (but preserve incoming alignment)
      */
     lhu   t0, 0(a0)
     lhu   t1, 0(a1)
     subu  v0, t0, t1
     beqz  v0, 1f
     RETURN
1:
     lhu   t2, 2(a0)
     lhu   t3, 2(a1)
     subu  v0, t2, t3
     beqz  v0, 2f
     RETURN
2:
     addu  a0, 4                     # offset to contents[2]
     addu  a1, 4                     # offset to contents[2]
     li    t7, 28
     bgt   a2, t7, do_memcmp16
     subu  a2, 3
     bltz  a2, do_remainder

loopback_triple:
     lhu   t0, 0(a0)
     lhu   t1, 0(a1)
     subu  v0, t0, t1
     beqz  v0, 1f
     RETURN
1:
     lhu   t2, 2(a0)
     lhu   t3, 2(a1)
     subu  v0, t2, t3
     beqz  v0, 2f
     RETURN
2:
     lhu   t4, 4(a0)
     lhu   t5, 4(a1)
     subu  v0, t4, t5
     beqz  v0, 3f
     RETURN
3:
     addu  a0, 6                     # offset to contents[i+3]
     addu  a1, 6                     # offset to contents[i+3]
     subu  a2, 3
     bgez  a2, loopback_triple

do_remainder:
     addu  a2, 3
     beqz  a2, returnDiff

loopback_single:
     lhu   t0, 0(a0)
     lhu   t1, 0(a1)
     subu  v0, t0, t1
     bnez  v0, 1f
     addu  a0, 2                     # offset to contents[i+1]
     addu  a1, 2                     # offset to contents[i+1]
     subu  a2, 1
     bnez  a2, loopback_single

returnDiff:
     move  v0, a3
1:
     RETURN

do_remainder2:
     addu  a2, 2
     bnez  a2, loopback_single
     move  v0, a3
     RETURN

    /* Long string case */
do_memcmp16:
     move  rOBJ, a3                  # save return value if strings are equal
     JAL(__memcmp16)
     seq   t0, v0, zero
     movn  v0, rOBJ, t0              # overwrite return value if strings are equal
     RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_STRING_INDEXOF
dvmCompiler_TEMPLATE_STRING_INDEXOF:
/* File: mips/TEMPLATE_STRING_INDEXOF.S */
    /*
     * String's indexOf.
     *
     * Requires a0 to have been previously checked for null.  Will
     * return index of match of a1 in v0.
     *
     * IMPORTANT NOTE:
     *
     * This code relies on hard-coded offsets for string objects, and must be
     * kept in sync wth definitions in UtfString.h  See asm-constants.h
     *
     * On entry:
     *    a0:   string object pointer
     *    a1:   char to match
     *    a2:   Starting offset in string data
     */

     lw    t0, STRING_FIELDOFF_OFFSET(a0)
     lw    t1, STRING_FIELDOFF_COUNT(a0)
     lw    v0, STRING_FIELDOFF_VALUE(a0)

    /*
     * At this point, we have:
     *    v0: object pointer
     *    a1: char to match
     *    a2: starting offset
     *    t0: offset
     *    t1: string length
     */

    /* Point to first element */
     addu  v0, 16                    # point to contents[0]

    /* Build pointer to start of string data */
     sll   t7, t0, 1                 # multiply offset by 2
     addu  v0, v0, t7

    /* Save a copy of starting data in v1 */
     move  v1, v0

    /* Clamp start to [0..count] */
     slt   t7, a2, zero
     movn  a2, zero, t7
     sgt   t7, a2, t1
     movn  a2, t1, t7

    /* Build pointer to start of data to compare */
     sll   t7, a2, 1                # multiply offset by 2
     addu  v0, v0, t7

    /* Compute iteration count */
     subu  a3, t1, a2

    /*
     * At this point we have:
     *   v0: start of data to test
     *   a1: char to compare
     *   a3: iteration count
     *   v1: original start of string
     *   t0-t7 available for loading string data
     */
     subu  a3, 4
     bltz  a3, indexof_remainder

indexof_loop4:
     lhu   t0, 0(v0)
     beq   t0, a1, match_0
     lhu   t0, 2(v0)
     beq   t0, a1, match_1
     lhu   t0, 4(v0)
     beq   t0, a1, match_2
     lhu   t0, 6(v0)
     beq   t0, a1, match_3
     addu  v0, 8                     # offset to contents[i+4]
     subu  a3, 4
     bgez  a3, indexof_loop4

indexof_remainder:
     addu  a3, 4
     beqz  a3, indexof_nomatch

indexof_loop1:
     lhu   t0, 0(v0)
     beq   t0, a1, match_0
     addu  v0, 2                     # offset to contents[i+1]
     subu  a3, 1
     bnez  a3, indexof_loop1

indexof_nomatch:
     li    v0, -1
     RETURN

match_0:
     subu  v0, v1
     sra   v0, v0, 1                 # divide by 2
     RETURN
match_1:
     addu  v0, 2
     subu  v0, v1
     sra   v0, v0, 1                 # divide by 2
     RETURN
match_2:
     addu  v0, 4
     subu  v0, v1
     sra   v0, v0, 1                 # divide by 2
     RETURN
match_3:
     addu  v0, 6
     subu  v0, v1
     sra   v0, v0, 1                 # divide by 2
     RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INTERPRET
dvmCompiler_TEMPLATE_INTERPRET:
/* File: mips/TEMPLATE_INTERPRET.S */
    /*
     * This handler transfers control to the interpeter without performing
     * any lookups.  It may be called either as part of a normal chaining
     * operation, or from the transition code in header.S.  We distinquish
     * the two cases by looking at the link register.  If called from a
     * translation chain, it will point to the chaining Dalvik PC.
     * On entry:
     *    ra - if NULL:
     *        a1 - the Dalvik PC to begin interpretation.
     *    else
     *        [ra] contains Dalvik PC to begin interpretation
     *    rSELF - pointer to thread
     *    rFP - Dalvik frame pointer
     */
    la      t0, dvmJitToInterpPunt
    move    a0, a1
    beq     ra, zero, 1f
    lw      a0, 0(ra)
1:
    jr      t0
    # doesn't return

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_MONITOR_ENTER
dvmCompiler_TEMPLATE_MONITOR_ENTER:
/* File: mips/TEMPLATE_MONITOR_ENTER.S */
    /*
     * Call out to the runtime to lock an object.  Because this thread
     * may have been suspended in THREAD_MONITOR state and the Jit's
     * translation cache subsequently cleared, we cannot return directly.
     * Instead, unconditionally transition to the interpreter to resume.
     *
     * On entry:
     *    a0 - self pointer
     *    a1 - the object (which has already been null-checked by the caller
     *    rPC - the Dalvik PC of the following instruction.
     */
    la     a2, .LdvmLockObject
    lw     t9, (a2)
    sw     zero, offThread_inJitCodeCache(a0)   # record that we're not returning
    JALR(t9)                                    # dvmLockObject(self, obj)
    lw     gp, STACK_OFFSET_GP(sp)

    la     a2, .LdvmJitToInterpNoChain
    lw     a2, (a2)

    # Bail to interpreter - no chain [note - rPC still contains dPC]
#if defined(WITH_JIT_TUNING)
    li      a0, kHeavyweightMonitor
#endif
    jr      a2

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_MONITOR_ENTER_DEBUG
dvmCompiler_TEMPLATE_MONITOR_ENTER_DEBUG:
/* File: mips/TEMPLATE_MONITOR_ENTER_DEBUG.S */
    /*
     * To support deadlock prediction, this version of MONITOR_ENTER
     * will always call the heavyweight dvmLockObject, check for an
     * exception and then bail out to the interpreter.
     *
     * On entry:
     *    a0 - self pointer
     *    a1 - the object (which has already been null-checked by the caller
     *    rPC - the Dalvik PC of the following instruction.
     *
     */
    la     a2, .LdvmLockObject
    lw     t9, (a2)
    sw     zero, offThread_inJitCodeCache(a0)   # record that we're not returning
    JALR(t9)                                    # dvmLockObject(self, obj)
    lw     gp, STACK_OFFSET_GP(sp)

    # test for exception
    lw     a1, offThread_exception(rSELF)
    beqz   a1, 1f
    sub    a0, rPC, 2                           # roll dPC back to this monitor instruction
    j      .LhandleException
1:
    # Bail to interpreter - no chain [note - rPC still contains dPC]
#if defined(WITH_JIT_TUNING)
    li     a0, kHeavyweightMonitor
#endif
    la     a2, .LdvmJitToInterpNoChain
    lw     a2, (a2)
    jr     a2

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_RESTORE_STATE
dvmCompiler_TEMPLATE_RESTORE_STATE:
/* File: mips/TEMPLATE_RESTORE_STATE.S */
    /*
     * This handler restores state following a selfVerification memory access.
     * On entry:
     *    a0 - offset from rSELF to the 1st element of the coreRegs save array.
     * Note: the following registers are not restored
     *       zero, AT, gp, sp, fp, ra
     */

    add     a0, a0, rSELF               # pointer to heapArgSpace.coreRegs[0]
#if 0
    lw      zero, r_ZERO*4(a0)          # restore zero
#endif
    .set noat
    lw      AT, r_AT*4(a0)              # restore at
    .set at
    lw      v0, r_V0*4(a0)              # restore v0
    lw      v1, r_V1*4(a0)              # restore v1

    lw      a1, r_A1*4(a0)              # restore a1
    lw      a2, r_A2*4(a0)              # restore a2
    lw      a3, r_A3*4(a0)              # restore a3

    lw      t0, r_T0*4(a0)              # restore t0
    lw      t1, r_T1*4(a0)              # restore t1
    lw      t2, r_T2*4(a0)              # restore t2
    lw      t3, r_T3*4(a0)              # restore t3
    lw      t4, r_T4*4(a0)              # restore t4
    lw      t5, r_T5*4(a0)              # restore t5
    lw      t6, r_T6*4(a0)              # restore t6
    lw      t7, r_T7*4(a0)              # restore t7

    lw      s0, r_S0*4(a0)              # restore s0
    lw      s1, r_S1*4(a0)              # restore s1
    lw      s2, r_S2*4(a0)              # restore s2
    lw      s3, r_S3*4(a0)              # restore s3
    lw      s4, r_S4*4(a0)              # restore s4
    lw      s5, r_S5*4(a0)              # restore s5
    lw      s6, r_S6*4(a0)              # restore s6
    lw      s7, r_S7*4(a0)              # restore s7

    lw      t8, r_T8*4(a0)              # restore t8
    lw      t9, r_T9*4(a0)              # restore t9

    lw      k0, r_K0*4(a0)              # restore k0
    lw      k1, r_K1*4(a0)              # restore k1

#if 0
    lw      gp, r_GP*4(a0)              # restore gp
    lw      sp, r_SP*4(a0)              # restore sp
    lw      fp, r_FP*4(a0)              # restore fp
    lw      ra, r_RA*4(a0)              # restore ra
#endif

/* #ifdef HARD_FLOAT */
#if 0
    lw      f0, fr0*4(a0)               # restore f0
    lw      f1, fr1*4(a0)               # restore f1
    lw      f2, fr2*4(a0)               # restore f2
    lw      f3, fr3*4(a0)               # restore f3
    lw      f4, fr4*4(a0)               # restore f4
    lw      f5, fr5*4(a0)               # restore f5
    lw      f6, fr6*4(a0)               # restore f6
    lw      f7, fr7*4(a0)               # restore f7
    lw      f8, fr8*4(a0)               # restore f8
    lw      f9, fr9*4(a0)               # restore f9
    lw      f10, fr10*4(a0)             # restore f10
    lw      f11, fr11*4(a0)             # restore f11
    lw      f12, fr12*4(a0)             # restore f12
    lw      f13, fr13*4(a0)             # restore f13
    lw      f14, fr14*4(a0)             # restore f14
    lw      f15, fr15*4(a0)             # restore f15
    lw      f16, fr16*4(a0)             # restore f16
    lw      f17, fr17*4(a0)             # restore f17
    lw      f18, fr18*4(a0)             # restore f18
    lw      f19, fr19*4(a0)             # restore f19
    lw      f20, fr20*4(a0)             # restore f20
    lw      f21, fr21*4(a0)             # restore f21
    lw      f22, fr22*4(a0)             # restore f22
    lw      f23, fr23*4(a0)             # restore f23
    lw      f24, fr24*4(a0)             # restore f24
    lw      f25, fr25*4(a0)             # restore f25
    lw      f26, fr26*4(a0)             # restore f26
    lw      f27, fr27*4(a0)             # restore f27
    lw      f28, fr28*4(a0)             # restore f28
    lw      f29, fr29*4(a0)             # restore f29
    lw      f30, fr30*4(a0)             # restore f30
    lw      f31, fr31*4(a0)             # restore f31
#endif

    lw      a0, r_A1*4(a0)              # restore a0
    RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_SAVE_STATE
dvmCompiler_TEMPLATE_SAVE_STATE:
/* File: mips/TEMPLATE_SAVE_STATE.S */
    /*
     * This handler performs a register save for selfVerification mode.
     * On entry:
     *    Top of stack + 4: a1 value to save
     *    Top of stack + 0: a0 value to save
     *    a0 - offset from rSELF to the beginning of the heapArgSpace record
     *    a1 - the value of regMap
     *
     * The handler must save regMap, r0-r31, f0-f31 if FPU, and then return with
     * r0-r31 with their original values (note that this means a0 and a1 must take
     * the values on the stack - not the ones in those registers on entry.
     * Finally, the two registers previously pushed must be popped.
     * Note: the following registers are not saved
     *       zero, AT, gp, sp, fp, ra
     */
    add     a0, a0, rSELF               # pointer to heapArgSpace
    sw      a1, 0(a0)                   # save regMap
    add     a0, a0, 4                   # pointer to coreRegs
#if 0
    sw      zero, r_ZERO*4(a0)          # save zero
#endif
    .set noat
    sw      AT, r_AT*4(a0)              # save at
    .set at
    sw      v0, r_V0*4(a0)              # save v0
    sw      v1, r_V1*4(a0)              # save v1

    lw      a1, 0(sp)                   # recover a0 value
    sw      a1, r_A0*4(a0)              # save a0
    lw      a1, 4(sp)                   # recover a1 value
    sw      a1, r_A1*4(a0)              # save a1
    sw      a2, r_A2*4(a0)              # save a2
    sw      a3, r_A3*4(a0)              # save a3

    sw      t0, r_T0*4(a0)              # save t0
    sw      t1, r_T1*4(a0)              # save t1
    sw      t2, r_T2*4(a0)              # save t2
    sw      t3, r_T3*4(a0)              # save t3
    sw      t4, r_T4*4(a0)              # save t4
    sw      t5, r_T5*4(a0)              # save t5
    sw      t6, r_T6*4(a0)              # save t6
    sw      t7, r_T7*4(a0)              # save t7

    sw      s0, r_S0*4(a0)              # save s0
    sw      s1, r_S1*4(a0)              # save s1
    sw      s2, r_S2*4(a0)              # save s2
    sw      s3, r_S3*4(a0)              # save s3
    sw      s4, r_S4*4(a0)              # save s4
    sw      s5, r_S5*4(a0)              # save s5
    sw      s6, r_S6*4(a0)              # save s6
    sw      s7, r_S7*4(a0)              # save s7

    sw      t8, r_T8*4(a0)              # save t8
    sw      t9, r_T9*4(a0)              # save t9

    sw      k0, r_K0*4(a0)              # save k0
    sw      k1, r_K1*4(a0)              # save k1

#if 0
    sw      gp, r_GP*4(a0)              # save gp
    sw      sp, r_SP*4(a0)              # save sp (need to adjust??? )
    sw      fp, r_FP*4(a0)              # save fp
    sw      ra, r_RA*4(a0)              # save ra
#endif

/* #ifdef HARD_FLOAT */
#if 0
    sw      f0, fr0*4(a0)               # save f0
    sw      f1, fr1*4(a0)               # save f1
    sw      f2, fr2*4(a0)               # save f2
    sw      f3, fr3*4(a0)               # save f3
    sw      f4, fr4*4(a0)               # save f4
    sw      f5, fr5*4(a0)               # save f5
    sw      f6, fr6*4(a0)               # save f6
    sw      f7, fr7*4(a0)               # save f7
    sw      f8, fr8*4(a0)               # save f8
    sw      f9, fr9*4(a0)               # save f9
    sw      f10, fr10*4(a0)             # save f10
    sw      f11, fr11*4(a0)             # save f11
    sw      f12, fr12*4(a0)             # save f12
    sw      f13, fr13*4(a0)             # save f13
    sw      f14, fr14*4(a0)             # save f14
    sw      f15, fr15*4(a0)             # save f15
    sw      f16, fr16*4(a0)             # save f16
    sw      f17, fr17*4(a0)             # save f17
    sw      f18, fr18*4(a0)             # save f18
    sw      f19, fr19*4(a0)             # save f19
    sw      f20, fr20*4(a0)             # save f20
    sw      f21, fr21*4(a0)             # save f21
    sw      f22, fr22*4(a0)             # save f22
    sw      f23, fr23*4(a0)             # save f23
    sw      f24, fr24*4(a0)             # save f24
    sw      f25, fr25*4(a0)             # save f25
    sw      f26, fr26*4(a0)             # save f26
    sw      f27, fr27*4(a0)             # save f27
    sw      f28, fr28*4(a0)             # save f28
    sw      f29, fr29*4(a0)             # save f29
    sw      f30, fr30*4(a0)             # save f30
    sw      f31, fr31*4(a0)             # save f31
#endif

    lw      a1, 0(sp)                   # recover a0 value
    lw      a1, 4(sp)                   # recover a1 value
    sub     sp, sp, 8                   # adjust stack ptr
    RETURN

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_PERIODIC_PROFILING
dvmCompiler_TEMPLATE_PERIODIC_PROFILING:
/* File: mips/TEMPLATE_PERIODIC_PROFILING.S */
    /*
     * Increment profile counter for this trace, and decrement
     * sample counter.  If sample counter goes below zero, turn
     * off profiling.
     *
     * On entry
     * (ra-16) is address of pointer to counter.  Note: the counter
     *    actually exists 16 bytes before the return target for mips.
     *     - 4 bytes for prof count addr.
     *     - 4 bytes for chain cell offset (2bytes 32 bit aligned).
     *     - 4 bytes for call TEMPLATE_PERIODIC_PROFILING.
     *     - 4 bytes for call delay slot.
     */
     lw     a0, -16(ra)
     lw     a1, offThread_pProfileCountdown(rSELF)
     lw     a2, 0(a0)                   # get counter
     lw     a3, 0(a1)                   # get countdown timer
     addu   a2, 1
     sub    a3, 1                       # FIXME - bug in ARM code???
     bltz   a3, .LTEMPLATE_PERIODIC_PROFILING_disable_profiling
     sw     a2, 0(a0)
     sw     a3, 0(a1)
     RETURN
.LTEMPLATE_PERIODIC_PROFILING_disable_profiling:
     la     a0, dvmJitTraceProfilingOff
     JALR(a0)
     # The ra register is preserved by the JALR macro.
     jr     ra

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_RETURN_PROF
dvmCompiler_TEMPLATE_RETURN_PROF:
/* File: mips/TEMPLATE_RETURN_PROF.S */
#define TEMPLATE_INLINE_PROFILING
/* File: mips/TEMPLATE_RETURN.S */
    /*
     * Unwind a frame from the Dalvik stack for compiled OP_RETURN_XXX.
     * If the stored value in returnAddr
     * is non-zero, the caller is compiled by the JIT thus return to the
     * address in the code cache following the invoke instruction. Otherwise
     * return to the special dvmJitToInterpNoChain entry point.
     */
#if defined(TEMPLATE_INLINE_PROFILING)
    # preserve a0-a2 and ra
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(ra, 12)

    # a0=rSELF
    move    a0, rSELF
    la      t9, dvmFastMethodTraceExit
    JALR(t9)
    lw      gp, STACK_OFFSET_GP(sp)

    # restore a0-a2 and ra
    SCRATCH_LOAD(ra, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)
#endif
    SAVEAREA_FROM_FP(a0, rFP)           # a0<- saveArea (old)
    lw      t0, offStackSaveArea_prevFrame(a0)     # t0<- saveArea->prevFrame
    lbu     t1, offThread_breakFlags(rSELF)        # t1<- breakFlags
    lw      rPC, offStackSaveArea_savedPc(a0)      # rPC<- saveArea->savedPc
#if !defined(WITH_SELF_VERIFICATION)
    lw      t2,  offStackSaveArea_returnAddr(a0)   # t2<- chaining cell ret
#else
    move    t2, zero                               # disable chaining
#endif
    lw      a2, offStackSaveArea_method - sizeofStackSaveArea(t0)
                                                   # a2<- method we're returning to
#if !defined(WITH_SELF_VERIFICATION)
    beq     a2, zero, 1f                           # bail to interpreter
#else
    bne     a2, zero, 2f
    JALR(ra)                                       # punt to interpreter and compare state
    # DOUG: assume this does not return ???
2:
#endif
    la      t4, .LdvmJitToInterpNoChainNoProfile   # defined in footer.S
    lw      a1, (t4)
    move    rFP, t0                                # publish new FP
    beq     a2, zero, 4f
    lw      t0, offMethod_clazz(a2)                # t0<- method->clazz
4:

    sw      a2, offThread_method(rSELF)            # self->method = newSave->method
    lw      a0, offClassObject_pDvmDex(t0)         # a0<- method->clazz->pDvmDex
    sw      rFP, offThread_curFrame(rSELF)         # self->curFrame = fp
    add     rPC, rPC, 3*2                          # publish new rPC
    sw      a0, offThread_methodClassDex(rSELF)
    movn    t2, zero, t1                           # check the breadFlags and
                                                   # clear the chaining cell address
    sw      t2, offThread_inJitCodeCache(rSELF)    # in code cache or not
    beq     t2, zero, 3f                           # chaining cell exists?
    JALR(t2)                                       # jump to the chaining cell
    # DOUG: assume this does not return ???
3:
#if defined(WITH_JIT_TUNING)
    li      a0, kCallsiteInterpreted
#endif
    j       a1                                     # callsite is interpreted
1:
    sw      zero, offThread_inJitCodeCache(rSELF)  # reset inJitCodeCache
    SAVE_PC_TO_SELF()                              # SAVE_PC_FP_TO_SELF()
    SAVE_FP_TO_SELF()
    la      t4, .LdvmMterpStdBail                  # defined in footer.S
    lw      a2, (t4)
    move    a0, rSELF                              # Expecting rSELF in a0
    JALR(a2)                                       # exit the interpreter
    # DOUG: assume this does not return ???

#undef TEMPLATE_INLINE_PROFILING

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_NO_OPT_PROF
dvmCompiler_TEMPLATE_INVOKE_METHOD_NO_OPT_PROF:
/* File: mips/TEMPLATE_INVOKE_METHOD_NO_OPT_PROF.S */
#define TEMPLATE_INLINE_PROFILING
/* File: mips/TEMPLATE_INVOKE_METHOD_NO_OPT.S */
    /*
     * For polymorphic callsites - setup the Dalvik frame and load Dalvik PC
     * into rPC then jump to dvmJitToInterpNoChain to dispatch the
     * runtime-resolved callee.
     */
    # a0 = methodToCall, a1 = returnCell, rPC = dalvikCallsite
    lh     t7, offMethod_registersSize(a0)        # t7<- methodToCall->regsSize
    lh     a2, offMethod_outsSize(a0)             # a2<- methodToCall->outsSize
    lw     t9, offThread_interpStackEnd(rSELF)    # t9<- interpStackEnd
    lbu    t8, offThread_breakFlags(rSELF)        # t8<- breakFlags
    move   a3, a1                                 # a3<- returnCell
    SAVEAREA_FROM_FP(a1, rFP)                     # a1<- stack save area
    sll    t6, t7, 2                              # multiply regsSize by 4 (4 bytes per reg)
    sub    a1, a1, t6                             # a1<- newFp(old savearea-regsSize)
    SAVEAREA_FROM_FP(t0, a1)                      # t0<- stack save area
    sll    t6, a2, 2                              # multiply outsSize by 4 (4 bytes per reg)
    sub    t0, t0, t6                             # t0<- bottom (newsave-outsSize)
    bgeu   t0, t9, 1f                             # bottom < interpStackEnd?
    RETURN                                        # return to raise stack overflow excep.

1:
    # a1 = newFP, a0 = methodToCall, a3 = returnCell, rPC = dalvikCallsite
    lw     t9, offMethod_clazz(a0)                # t9<- methodToCall->clazz
    lw     t0, offMethod_accessFlags(a0)          # t0<- methodToCall->accessFlags
    sw     rPC, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)
    sw     rPC, (offStackSaveArea_savedPc - sizeofStackSaveArea)(a1)
    lw     rPC, offMethod_insns(a0)               # rPC<- methodToCall->insns

    # set up newSaveArea
    sw     rFP, (offStackSaveArea_prevFrame - sizeofStackSaveArea)(a1)
    sw     a3, (offStackSaveArea_returnAddr - sizeofStackSaveArea)(a1)
    sw     a0, (offStackSaveArea_method - sizeofStackSaveArea)(a1)
    beqz   t8, 2f                                 # breakFlags != 0
    RETURN                                        # bail to the interpreter

2:
    and    t6, t0, ACC_NATIVE
    beqz   t6, 3f
#if !defined(WITH_SELF_VERIFICATION)
    j      .LinvokeNative
#else
    RETURN                                        # bail to the interpreter
#endif

3:
    # continue executing the next instruction through the interpreter
    la     t0, .LdvmJitToInterpTraceSelectNoChain # defined in footer.S
    lw     rTEMP, (t0)
    lw     a3, offClassObject_pDvmDex(t9)         # a3<- method->clazz->pDvmDex

    # Update "thread" values for the new method
    sw     a0, offThread_method(rSELF)            # self->method = methodToCall
    sw     a3, offThread_methodClassDex(rSELF)    # self->methodClassDex = ...
    move   rFP, a1                                # fp = newFp
    sw     rFP, offThread_curFrame(rSELF)         # self->curFrame = newFp
#if defined(TEMPLATE_INLINE_PROFILING)
    # preserve rTEMP,a1-a3
    SCRATCH_STORE(rTEMP, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(a3, 12)

    # a0=methodToCall, a1=rSELF
    move   a1, rSELF
    la     t9, dvmFastMethodTraceEnter
    JALR(t9)
    lw     gp, STACK_OFFSET_GP(sp)

    # restore rTEMP,a1-a3
    SCRATCH_LOAD(a3, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(rTEMP, 0)
#endif

    # Start executing the callee
#if defined(WITH_JIT_TUNING)
    li     a0, kInlineCacheMiss
#endif
    jr     rTEMP                                  # dvmJitToInterpTraceSelectNoChain

#undef TEMPLATE_INLINE_PROFILING

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_CHAIN_PROF
dvmCompiler_TEMPLATE_INVOKE_METHOD_CHAIN_PROF:
/* File: mips/TEMPLATE_INVOKE_METHOD_CHAIN_PROF.S */
#define TEMPLATE_INLINE_PROFILING
/* File: mips/TEMPLATE_INVOKE_METHOD_CHAIN.S */
    /*
     * For monomorphic callsite, setup the Dalvik frame and return to the
     * Thumb code through the link register to transfer control to the callee
     * method through a dedicated chaining cell.
     */
    # a0 = methodToCall, a1 = returnCell, rPC = dalvikCallsite
    # methodToCall is guaranteed to be non-native
.LinvokeChainProf:
    lh     t7, offMethod_registersSize(a0)        # t7<- methodToCall->regsSize
    lh     a2, offMethod_outsSize(a0)             # a2<- methodToCall->outsSize
    lw     t9, offThread_interpStackEnd(rSELF)    # t9<- interpStackEnd
    lbu    t8, offThread_breakFlags(rSELF)        # t8<- breakFlags
    move   a3, a1                                 # a3<- returnCell
    SAVEAREA_FROM_FP(a1, rFP)                     # a1<- stack save area
    sll    t6, t7, 2                              # multiply regsSize by 4 (4 bytes per reg)
    sub    a1, a1, t6                             # a1<- newFp(old savearea-regsSize)
    SAVEAREA_FROM_FP(t0, a1)                      # t0<- stack save area
    add    t2, ra, 8                              # setup the punt-to-interp address
                                                  # 8 bytes skips branch and delay slot
    sll    t6, a2, 2                              # multiply outsSize by 4 (4 bytes per reg)
    sub    t0, t0, t6                             # t0<- bottom (newsave-outsSize)
    bgeu   t0, t9, 1f                             # bottom < interpStackEnd?
    jr     t2                                     # return to raise stack overflow excep.

1:
    # a1 = newFP, a0 = methodToCall, a3 = returnCell, rPC = dalvikCallsite
    lw     t9, offMethod_clazz(a0)                # t9<- methodToCall->clazz
    sw     rPC, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)
    sw     rPC, (offStackSaveArea_savedPc - sizeofStackSaveArea)(a1)
    lw     rPC, offMethod_insns(a0)               # rPC<- methodToCall->insns

    # set up newSaveArea
    sw     rFP, (offStackSaveArea_prevFrame - sizeofStackSaveArea)(a1)
    sw     a3, (offStackSaveArea_returnAddr - sizeofStackSaveArea)(a1)
    sw     a0, (offStackSaveArea_method - sizeofStackSaveArea)(a1)
    beqz   t8, 2f                                 # breakFlags != 0
    jr     t2                                     # bail to the interpreter

2:
    lw     a3, offClassObject_pDvmDex(t9)         # a3<- methodToCall->clazz->pDvmDex

    # Update "thread" values for the new method
    sw     a0, offThread_method(rSELF)            # self->method = methodToCall
    sw     a3, offThread_methodClassDex(rSELF)    # self->methodClassDex = ...
    move   rFP, a1                                # fp = newFp
    sw     rFP, offThread_curFrame(rSELF)         # self->curFrame = newFp
#if defined(TEMPLATE_INLINE_PROFILING)
    # preserve a0-a2 and ra
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(ra, 12)

    move   a1, rSELF
    # a0=methodToCall, a1=rSELF
    la     t9, dvmFastMethodTraceEnter
    jalr   t9
    lw     gp, STACK_OFFSET_GP(sp)

    # restore a0-a2 and ra
    SCRATCH_LOAD(ra, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)
#endif
    RETURN                                        # return to the callee-chaining cell

#undef TEMPLATE_INLINE_PROFILING

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN_PROF
dvmCompiler_TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN_PROF:
/* File: mips/TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN_PROF.S */
#define TEMPLATE_INLINE_PROFILING
/* File: mips/TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN.S */
    /*
     * For polymorphic callsite, check whether the cached class pointer matches
     * the current one. If so setup the Dalvik frame and return to the
     * Thumb code through the link register to transfer control to the callee
     * method through a dedicated chaining cell.
     *
     * The predicted chaining cell is declared in ArmLIR.h with the
     * following layout:
     *
     *  typedef struct PredictedChainingCell {
     *      u4 branch;
     *      u4 delay_slot;
     *      const ClassObject *clazz;
     *      const Method *method;
     *      u4 counter;
     *  } PredictedChainingCell;
     *
     * Upon returning to the callsite:
     *    - lr   : to branch to the chaining cell
     *    - lr+8 : to punt to the interpreter
     *    - lr+16: to fully resolve the callee and may rechain.
     *             a3 <- class
     */
    # a0 = this, a1 = returnCell, a2 = predictedChainCell, rPC = dalvikCallsite
    lw      a3, offObject_clazz(a0)     # a3 <- this->class
    lw      rIBASE, 8(a2)                   # t0 <- predictedChainCell->clazz
    lw      a0, 12(a2)                  # a0 <- predictedChainCell->method
    lw      t1, offThread_icRechainCount(rSELF)    # t1 <- shared rechainCount

#if defined(WITH_JIT_TUNING)
    la      rINST, .LdvmICHitCount
    #add     t2, t2, 1
    bne    a3, rIBASE, 1f
    nop
    lw      t2, 0(rINST)
    add     t2, t2, 1
    sw      t2, 0(rINST)
1:
    #add     t2, t2, 1
#endif
    beq     a3, rIBASE, .LinvokeChainProf       # branch if predicted chain is valid
    lw      rINST, offClassObject_vtable(a3)     # rINST <- this->class->vtable
    beqz    rIBASE, 2f                      # initialized class or not
    sub     a1, t1, 1                   # count--
    sw      a1, offThread_icRechainCount(rSELF)   # write back to InterpState
    b       3f
2:
    move    a1, zero
3:
    add     ra, ra, 16                  # return to fully-resolve landing pad
    /*
     * a1 <- count
     * a2 <- &predictedChainCell
     * a3 <- this->class
     * rPC <- dPC
     * rINST <- this->class->vtable
     */
    RETURN

#undef TEMPLATE_INLINE_PROFILING

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_NATIVE_PROF
dvmCompiler_TEMPLATE_INVOKE_METHOD_NATIVE_PROF:
/* File: mips/TEMPLATE_INVOKE_METHOD_NATIVE_PROF.S */
#define TEMPLATE_INLINE_PROFILING
/* File: mips/TEMPLATE_INVOKE_METHOD_NATIVE.S */
    # a0 = methodToCall, a1 = returnCell, rPC = dalvikCallsite
    lh     t7, offMethod_registersSize(a0)        # t7<- methodToCall->regsSize
    lw     t9, offThread_interpStackEnd(rSELF)    # t9<- interpStackEnd
    lbu    t8, offThread_breakFlags(rSELF)        # t8<- breakFlags
    move   a3, a1                                 # a3<- returnCell
    SAVEAREA_FROM_FP(a1, rFP)                     # a1<- stack save area
    sll    t6, t7, 2                              # multiply regsSize by 4 (4 bytes per reg)
    sub    a1, a1, t6                             # a1<- newFp(old savearea-regsSize)
    SAVEAREA_FROM_FP(t0, a1)                      # t0<- stack save area
    bgeu   t0, t9, 1f                             # bottom < interpStackEnd?
    RETURN                                        # return to raise stack overflow excep.

1:
    # a1 = newFP, a0 = methodToCall, a3 = returnCell, rPC = dalvikCallsite
    sw     rPC, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)
    sw     rPC, (offStackSaveArea_savedPc - sizeofStackSaveArea)(a1)
    lw     rPC, offMethod_insns(a0)               # rPC<- methodToCall->insns

    # set up newSaveArea
    sw     rFP, (offStackSaveArea_prevFrame - sizeofStackSaveArea)(a1)
    sw     a3, (offStackSaveArea_returnAddr - sizeofStackSaveArea)(a1)
    sw     a0, (offStackSaveArea_method - sizeofStackSaveArea)(a1)
    lw     rTEMP, offMethod_nativeFunc(a0)        # t9<- method->nativeFunc
#if !defined(WITH_SELF_VERIFICATION)
    beqz   t8, 2f                                 # breakFlags != 0
    RETURN                                        # bail to the interpreter
2:
#else
    RETURN                                        # bail to the interpreter unconditionally
#endif

    # go ahead and transfer control to the native code
    lw     t6, offThread_jniLocal_topCookie(rSELF)  # t6<- thread->localRef->...
    sw     a1, offThread_curFrame(rSELF)          # self->curFrame = newFp
    sw     zero, offThread_inJitCodeCache(rSELF)  # not in the jit code cache
    sw     t6, (offStackSaveArea_localRefCookie - sizeofStackSaveArea)(a1)
                                                  # newFp->localRefCookie=top
    SAVEAREA_FROM_FP(rBIX, a1)                    # rBIX<- new stack save area
    move   a2, a0                                 # a2<- methodToCall
    move   a0, a1                                 # a0<- newFp
    add    a1, rSELF, offThread_retval            # a1<- &retval
    move   a3, rSELF                              # a3<- self
#if defined(TEMPLATE_INLINE_PROFILING)
    # a2: methodToCall
    # preserve rTEMP,a0-a3
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(a3, 12)
    SCRATCH_STORE(rTEMP, 16)

    move   a0, a2
    move   a1, rSELF
    # a0=JNIMethod, a1=rSELF
    la      t9, dvmFastMethodTraceEnter
    JALR(t9)                                      # off to the native code
    lw     gp, STACK_OFFSET_GP(sp)

    # restore rTEMP,a0-a3
    SCRATCH_LOAD(rTEMP, 16)
    SCRATCH_LOAD(a3, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)

    move   rOBJ, a2                               # save a2
#endif
    move   t9, rTEMP
    JALR(t9)                                   # off to the native code
    lw     gp, STACK_OFFSET_GP(sp)

#if defined(TEMPLATE_INLINE_PROFILING)
    move   a0, rOBJ
    move   a1, rSELF
    # a0=JNIMethod, a1=rSELF
    la      t9, dvmFastNativeMethodTraceExit
    JALR(t9)
    lw     gp, STACK_OFFSET_GP(sp)
#endif

    # native return; rBIX=newSaveArea
    # equivalent to dvmPopJniLocals
    lw     a2, offStackSaveArea_returnAddr(rBIX)     # a2 = chaining cell ret addr
    lw     a0, offStackSaveArea_localRefCookie(rBIX) # a0<- saved->top
    lw     a1, offThread_exception(rSELF)            # check for exception
    sw     rFP, offThread_curFrame(rSELF)            # self->curFrame = fp
    sw     a0, offThread_jniLocal_topCookie(rSELF)   # new top <- old top
    lw     a0, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)

    # a0 = dalvikCallsitePC
    bnez   a1, .LhandleException                     # handle exception if any

    sw     a2, offThread_inJitCodeCache(rSELF)       # set the mode properly
    beqz   a2, 3f
    jr     a2                                        # go if return chaining cell still exist

3:
    # continue executing the next instruction through the interpreter
    la     a1, .LdvmJitToInterpTraceSelectNoChain    # defined in footer.S
    lw     a1, (a1)
    add    rPC, a0, 3*2                              # reconstruct new rPC (advance 3 dalvik instr)

#if defined(WITH_JIT_TUNING)
    li     a0, kCallsiteInterpreted
#endif
    jr     a1

#undef TEMPLATE_INLINE_PROFILING

    .size   dvmCompilerTemplateStart, .-dvmCompilerTemplateStart
/* File: mips/footer.S */
/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .section .data.rel.ro
    .align  4
.LinvokeNative:
    # Prep for the native call
    # a1 = newFP, a0 = methodToCall
    lw     t9, offThread_jniLocal_topCookie(rSELF)  # t9<- thread->localRef->...
    sw     zero, offThread_inJitCodeCache(rSELF)    # not in jit code cache
    sw     a1, offThread_curFrame(rSELF)            # self->curFrame = newFp
    sw     t9, (offStackSaveArea_localRefCookie - sizeofStackSaveArea)(a1)
                                                 # newFp->localRefCookie=top
    lhu     ra, offThread_subMode(rSELF)
    SAVEAREA_FROM_FP(rBIX, a1)                   # rBIX<- new stack save area

    move    a2, a0                               # a2<- methodToCall
    move    a0, a1                               # a0<- newFp
    add     a1, rSELF, offThread_retval          # a1<- &retval
    move    a3, rSELF                            # a3<- self
    andi    ra, kSubModeMethodTrace
    beqz    ra, 121f
    # a2: methodToCall
    # preserve a0-a3
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(a3, 12)

    move    a0, a2
    move    a1, rSELF
    la      t9, dvmFastMethodTraceEnter
    JALR(t9)
    lw      gp, STACK_OFFSET_GP(sp)

    # restore a0-a3
    SCRATCH_LOAD(a3, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)

    lw      t9, offMethod_nativeFunc(a2)
    JALR(t9)                                      # call methodToCall->nativeFunc
    lw      gp, STACK_OFFSET_GP(sp)

    # restore a2 again
    SCRATCH_LOAD(a2, 8)

    move    a0, a2
    move    a1, rSELF
    la      t9, dvmFastNativeMethodTraceExit
    JALR(t9)
    lw      gp, STACK_OFFSET_GP(sp)
    b       212f

121:
    lw      t9, offMethod_nativeFunc(a2)
    JALR(t9)                                     # call methodToCall->nativeFunc
    lw      gp, STACK_OFFSET_GP(sp)

212:
    # native return; rBIX=newSaveArea
    # equivalent to dvmPopJniLocals
    lw     a2, offStackSaveArea_returnAddr(rBIX)     # a2 = chaining cell ret addr
    lw     a0, offStackSaveArea_localRefCookie(rBIX) # a0<- saved->top
    lw     a1, offThread_exception(rSELF)            # check for exception
    sw     rFP, offThread_curFrame(rSELF)            # self->curFrame = fp
    sw     a0, offThread_jniLocal_topCookie(rSELF)   # new top <- old top
    lw     a0, offStackSaveArea_savedPc(rBIX)        # reload rPC

    # a0 = dalvikCallsitePC
    bnez   a1, .LhandleException                     # handle exception if any

    sw     a2, offThread_inJitCodeCache(rSELF)       # set the mode properly
    beqz   a2, 3f
    jr     a2                                        # go if return chaining cell still exist

3:
    # continue executing the next instruction through the interpreter
    la     a1, .LdvmJitToInterpTraceSelectNoChain    # defined in footer.S
    lw     a1, (a1)
    add    rPC, a0, 3*2                              # reconstruct new rPC

#if defined(WITH_JIT_TUNING)
    li     a0, kCallsiteInterpreted
#endif
    jr     a1


/*
 * On entry:
 * a0  Faulting Dalvik PC
 */
.LhandleException:
#if defined(WITH_SELF_VERIFICATION)
    la     t0, .LdeadFood
    lw     t0, (t0)                  # should not see this under self-verification mode
    jr     t0
.LdeadFood:
    .word   0xdeadf00d
#endif
    sw     zero, offThread_inJitCodeCache(rSELF)  # in interpreter land
    la     a1, .LdvmMterpCommonExceptionThrown  # PIC way of getting &func
    lw     a1, (a1)
    la     rIBASE, .LdvmAsmInstructionStart     # PIC way of getting &func
    lw     rIBASE, (rIBASE)
    move   rPC, a0                              # reload the faulting Dalvid address
    jr     a1                                   # branch to dvmMterpCommonExeceptionThrown

    .align  4
.LdvmAsmInstructionStart:
    .word   dvmAsmInstructionStart
.LdvmJitToInterpNoChainNoProfile:
    .word   dvmJitToInterpNoChainNoProfile
.LdvmJitToInterpTraceSelectNoChain:
    .word   dvmJitToInterpTraceSelectNoChain
.LdvmJitToInterpNoChain:
    .word   dvmJitToInterpNoChain
.LdvmMterpStdBail:
    .word   dvmMterpStdBail
.LdvmMterpCommonExceptionThrown:
    .word   dvmMterpCommonExceptionThrown
.LdvmLockObject:
    .word   dvmLockObject
#if defined(WITH_JIT_TUNING)
.LdvmICHitCount:
    .word   gDvmICHitCount
#endif
#if defined(WITH_SELF_VERIFICATION)
.LdvmSelfVerificationMemOpDecode:
    .word   dvmSelfVerificationMemOpDecode
#endif

    .global dvmCompilerTemplateEnd
dvmCompilerTemplateEnd:

#endif /* WITH_JIT */

