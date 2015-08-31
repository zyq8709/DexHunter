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

#define GOTO_OPCODE(rd)			sll  rd, rd, ${handler_size_bits};	\
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
					sll  dst, dst, ${handler_size_bits};	\
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
