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

#define GOTO_OPCODE(rd) sll rd, rd, ${handler_size_bits}; \
    addu      rd, rIBASE, rd; \
    jr        rd

#define GOTO_OPCODE_BASE(_base, rd)  sll rd, rd, ${handler_size_bits}; \
    addu      rd, _base, rd; \
    jr        rd

#define GET_VREG(rd, rix) LOAD_eas2(rd, rFP, rix)

#define GET_VREG_F(rd, rix) EAS2(AT, rFP, rix); \
    .set noat; l.s rd, (AT); .set at

#define SET_VREG(rd, rix) STORE_eas2(rd, rFP, rix)

#define SET_VREG_GOTO(rd, rix, dst) .set noreorder; \
    sll       dst, dst, ${handler_size_bits}; \
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
