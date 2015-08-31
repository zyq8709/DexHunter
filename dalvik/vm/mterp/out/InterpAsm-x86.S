/*
 * This file was generated automatically by gen-mterp.py for 'x86'.
 *
 * --> DO NOT EDIT <--
 */

/* File: x86/header.S */
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
 * 32-bit x86 definitions and declarations.
 */

/*
386 ABI general notes:

Caller save set:
   eax, edx, ecx, st(0)-st(7)
Callee save set:
   ebx, esi, edi, ebp
Return regs:
   32-bit in eax
   64-bit in edx:eax (low-order 32 in eax)
   fp on top of fp stack st(0)

Parameters passed on stack, pushed right-to-left.  On entry to target, first
parm is at 4(%esp).  Traditional entry code is:

functEntry:
    push    %ebp             # save old frame pointer
    mov     %ebp,%esp        # establish new frame pointer
    sub     FrameSize,%esp   # Allocate storage for spill, locals & outs

Once past the prologue, arguments are referenced at ((argno + 2)*4)(%ebp)

Stack must be 16-byte aligned to support SSE in native code.

If we're not doing variable stack allocation (alloca), the frame pointer can be
eliminated and all arg references adjusted to be esp relative.

Mterp notes:

Some key interpreter variables will be assigned to registers.  Note that each
will also have an associated spill location (mostly useful for those assigned
to callee save registers).

  nick     reg   purpose
  rPC      esi   interpreted program counter, used for fetching instructions
  rFP      edi   interpreted frame pointer, used for accessing locals and args
  rINSTw   bx    first 16-bit code of current instruction
  rINSTbl  bl    opcode portion of instruction word
  rINSTbh  bh    high byte of inst word, usually contains src/tgt reg names
  rIBASE   edx   base of instruction handler table

Notes:
   o High order 16 bits of ebx must be zero on entry to handler
   o rPC, rFP, rINSTw/rINSTbl valid on handler entry and exit
   o eax and ecx are scratch, rINSTw/ebx sometimes scratch

*/

#define rSELF    8(%ebp)
#define rPC      %esi
#define rFP      %edi
#define rINST    %ebx
#define rINSTw   %bx
#define rINSTbh  %bh
#define rINSTbl  %bl
#define rIBASE   %edx


/* Frame diagram while executing dvmMterpStdRun, high to low addresses */
#define IN_ARG0        (  8)
#define CALLER_RP      (  4)
#define PREV_FP        (  0)
/* Spill offsets relative to %ebp */
#define EDI_SPILL      ( -4)
#define ESI_SPILL      ( -8)
#define EBX_SPILL      (-12)
#define rPC_SPILL      (-16)
#define rFP_SPILL      (-20)
#define rINST_SPILL    (-24)
#define rIBASE_SPILL   (-28)
#define TMP_SPILL1     (-32)
#define TMP_SPILL2     (-36)
#define TMP_SPILL3     (-20)
#define LOCAL0_OFFSET  (-44)
#define LOCAL1_OFFSET  (-48)
#define LOCAL2_OFFSET  (-52)
/* Out Arg offsets, relative to %esp */
#define OUT_ARG4       ( 16)
#define OUT_ARG3       ( 12)
#define OUT_ARG2       (  8)
#define OUT_ARG1       (  4)
#define OUT_ARG0       (  0)  /* <- dvmMterpStdRun esp */
#if defined(WITH_JIT)
/* for spill region: increase size by 48 (to keep 16-byte alignment) */
/* 76 + 48 = 124 */
#define JIT_SPILL      (-56)
#define FRAME_SIZE     124
#else
#define FRAME_SIZE     76
#endif

#define SPILL(reg) movl reg##,reg##_SPILL(%ebp)
#define UNSPILL(reg) movl reg##_SPILL(%ebp),reg
#define SPILL_TMP1(reg) movl reg,TMP_SPILL1(%ebp)
#define UNSPILL_TMP1(reg) movl TMP_SPILL1(%ebp),reg
#define SPILL_TMP2(reg) movl reg,TMP_SPILL2(%ebp)
#define UNSPILL_TMP2(reg) movl TMP_SPILL2(%ebp),reg
#define SPILL_TMP3(reg) movl reg,TMP_SPILL3(%ebp)
#define UNSPILL_TMP3(reg) movl TMP_SPILL3(%ebp),reg

#if defined(WITH_JIT)
.macro GET_JIT_PROF_TABLE _self _reg
    movl    offThread_pJitProfTable(\_self),\_reg
.endm
.macro GET_JIT_THRESHOLD _self _reg
    movl    offThread_jitThreshold(\_self),\_reg
.endm
#endif

/* save/restore the PC and/or FP from the self struct */
.macro SAVE_PC_FP_TO_SELF _reg
    movl     rSELF,\_reg
    movl     rPC,offThread_pc(\_reg)
    movl     rFP,offThread_curFrame(\_reg)
.endm

.macro LOAD_PC_FP_FROM_SELF
    movl    rSELF,rFP
    movl    offThread_pc(rFP),rPC
    movl    offThread_curFrame(rFP),rFP
.endm

/* The interpreter assumes a properly aligned stack on entry, and
 * will preserve 16-byte alignment.
 */

/*
 * "export" the PC to the interpreted stack frame, f/b/o future exception
 * objects.  Must be done *before* something throws.
 *
 * In C this is "SAVEAREA_FROM_FP(fp)->xtra.currentPc = pc", i.e.
 * fp - sizeof(StackSaveArea) + offsetof(SaveArea, xtra.currentPc)
 *
 * It's okay to do this more than once.
 */
.macro EXPORT_PC
    movl     rPC, (-sizeofStackSaveArea + offStackSaveArea_currentPc)(rFP)
.endm

.macro GET_PC
    movl     (-sizeofStackSaveArea + offStackSaveArea_currentPc)(rFP), rPC
.endm

/*
 * Given a frame pointer, find the stack save area.
 *
 * In C this is "((StackSaveArea*)(_fp) -1)".
 */
.macro SAVEAREA_FROM_FP _reg
    leal    -sizeofStackSaveArea(rFP), \_reg
.endm

/*
 * Fetch the next instruction from rPC into rINSTw.  Does not advance rPC.
 */
.macro FETCH_INST
    movzwl    (rPC),rINST
.endm

/*
 * Fetch the opcode byte and zero-extend it into _reg.  Must be used
 * in conjunction with GOTO_NEXT_R
 */
.macro FETCH_INST_R _reg
    movzbl    (rPC),\_reg
.endm

/*
 * Fetch the opcode byte at _count words offset from rPC and zero-extend
 * it into _reg.  Must be used in conjunction with GOTO_NEXT_R
 */
.macro FETCH_INST_OPCODE _count _reg
    movzbl  \_count*2(rPC),\_reg
.endm

/*
 * Fetch the nth instruction word from rPC into rINSTw.  Does not advance
 * rPC, and _count is in words
 */
.macro FETCH_INST_WORD _count
    movzwl  \_count*2(rPC),rINST
.endm

/*
 * Fetch instruction word indexed (used for branching).
 * Index is in instruction word units.
 */
.macro FETCH_INST_INDEXED _reg
    movzwl  (rPC,\_reg,2),rINST
.endm

/*
 * Advance rPC by instruction count
 */
.macro ADVANCE_PC _count
    leal  2*\_count(rPC),rPC
.endm

/*
 * Advance rPC by branch offset in register
 */
.macro ADVANCE_PC_INDEXED _reg
    leal (rPC,\_reg,2),rPC
.endm

.macro GOTO_NEXT
     movzx   rINSTbl,%eax
     movzbl  rINSTbh,rINST
     jmp     *(rIBASE,%eax,4)
.endm

   /*
    * Version of GOTO_NEXT that assumes _reg preloaded with opcode.
    * Should be paired with FETCH_INST_R
    */
.macro GOTO_NEXT_R _reg
     movzbl  1(rPC),rINST
     jmp     *(rIBASE,\_reg,4)
.endm

/*
 * Get/set the 32-bit value from a Dalvik register.
 */
.macro GET_VREG_R _reg _vreg
    movl     (rFP,\_vreg,4),\_reg
.endm

.macro SET_VREG _reg _vreg
    movl     \_reg,(rFP,\_vreg,4)
.endm

.macro GET_VREG_WORD _reg _vreg _offset
    movl     4*(\_offset)(rFP,\_vreg,4),\_reg
.endm

.macro SET_VREG_WORD _reg _vreg _offset
    movl     \_reg,4*(\_offset)(rFP,\_vreg,4)
.endm

#define sReg0 LOCAL0_OFFSET(%ebp)
#define sReg1 LOCAL1_OFFSET(%ebp)
#define sReg2 LOCAL2_OFFSET(%ebp)

   /*
    * x86 JIT Helpers
    */

    .macro dumpSwitch _regData _regScratch1 _regScratch2
    .endm

   /*
    * Hard coded helper values.
    */

.balign 16

.LdoubNeg:
    .quad       0x8000000000000000

.L64bits:
    .quad       0xFFFFFFFFFFFFFFFF

.LshiftMask2:
    .quad       0x0000000000000000
.LshiftMask:
    .quad       0x000000000000003F

.Lvalue64:
    .quad       0x0000000000000040

.LvaluePosInfLong:
    .quad       0x7FFFFFFFFFFFFFFF

.LvalueNegInfLong:
    .quad       0x8000000000000000

.LvalueNanLong:
    .quad       0x0000000000000000

.LintMin:
.long   0x80000000

.LintMax:
.long   0x7FFFFFFF


/*
 * This is a #include, not a %include, because we want the C pre-processor
 * to expand the macros into assembler assignment statements.
 */
#include "../common/asm-constants.h"

#if defined(WITH_JIT)
#include "../common/jit-config.h"
#endif


    .global dvmAsmInstructionStartCode
    .type   dvmAsmInstructionStartCode, %function
dvmAsmInstructionStartCode = .L_OP_NOP
    .text

/* ------------------------------ */
.L_OP_NOP: /* 0x00 */
/* File: x86/OP_NOP.S */
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MOVE: /* 0x01 */
/* File: x86/OP_MOVE.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    movzbl rINSTbl,%eax          # eax<- BA
    andb   $0xf,%al             # eax<- A
    shrl   $4,rINST            # rINST<- B
    GET_VREG_R rINST rINST
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    SET_VREG rINST %eax           # fp[A]<-fp[B]
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MOVE_FROM16: /* 0x02 */
/* File: x86/OP_MOVE_FROM16.S */
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    movzx    rINSTbl,%eax              # eax <= AA
    movw     2(rPC),rINSTw             # rINSTw <= BBBB
    GET_VREG_R rINST rINST             # rINST- fp[BBBB]
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG rINST %eax                # fp[AA]<- ecx]
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MOVE_16: /* 0x03 */
/* File: x86/OP_MOVE_16.S */
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    movzwl    4(rPC),%ecx              # ecx<- BBBB
    movzwl    2(rPC),%eax              # eax<- AAAA
    GET_VREG_R  rINST %ecx
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    SET_VREG  rINST %eax
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MOVE_WIDE: /* 0x04 */
/* File: x86/OP_MOVE_WIDE.S */
    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    movzbl    rINSTbl,%ecx                # ecx <- BA
    sarl      $4,rINST                   # rINST<- B
    GET_VREG_WORD %eax rINST 0            # eax<- v[B+0]
    GET_VREG_WORD rINST rINST 1           # rINST<- v[B+1]
    andb      $0xf,%cl                   # ecx <- A
    SET_VREG_WORD rINST %ecx 1            # v[A+1]<- rINST
    SET_VREG_WORD %eax %ecx 0             # v[A+0]<- eax
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MOVE_WIDE_FROM16: /* 0x05 */
/* File: x86/OP_MOVE_WIDE_FROM16.S */
    /* move-wide/from16 vAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    movzwl    2(rPC),%ecx              # ecx<- BBBB
    movzbl    rINSTbl,%eax             # eax<- AAAA
    GET_VREG_WORD rINST %ecx 0         # rINST<- v[BBBB+0]
    GET_VREG_WORD %ecx %ecx 1          # ecx<- v[BBBB+1]
    SET_VREG_WORD rINST %eax 0         # v[AAAA+0]<- rINST
    SET_VREG_WORD %ecx %eax 1          # v[AAAA+1]<- eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MOVE_WIDE_16: /* 0x06 */
/* File: x86/OP_MOVE_WIDE_16.S */
    /* move-wide/16 vAAAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    movzwl    4(rPC),%ecx            # ecx<- BBBB
    movzwl    2(rPC),%eax            # eax<- AAAA
    GET_VREG_WORD rINST %ecx 0       # rINSTw_WORD<- v[BBBB+0]
    GET_VREG_WORD %ecx %ecx 1        # ecx<- v[BBBB+1]
    SET_VREG_WORD rINST %eax 0       # v[AAAA+0]<- rINST
    SET_VREG_WORD %ecx %eax 1        # v[AAAA+1]<- ecx
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MOVE_OBJECT: /* 0x07 */
/* File: x86/OP_MOVE_OBJECT.S */
/* File: x86/OP_MOVE.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    movzbl rINSTbl,%eax          # eax<- BA
    andb   $0xf,%al             # eax<- A
    shrl   $4,rINST            # rINST<- B
    GET_VREG_R rINST rINST
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    SET_VREG rINST %eax           # fp[A]<-fp[B]
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_MOVE_OBJECT_FROM16: /* 0x08 */
/* File: x86/OP_MOVE_OBJECT_FROM16.S */
/* File: x86/OP_MOVE_FROM16.S */
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    movzx    rINSTbl,%eax              # eax <= AA
    movw     2(rPC),rINSTw             # rINSTw <= BBBB
    GET_VREG_R rINST rINST             # rINST- fp[BBBB]
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG rINST %eax                # fp[AA]<- ecx]
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_MOVE_OBJECT_16: /* 0x09 */
/* File: x86/OP_MOVE_OBJECT_16.S */
/* File: x86/OP_MOVE_16.S */
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    movzwl    4(rPC),%ecx              # ecx<- BBBB
    movzwl    2(rPC),%eax              # eax<- AAAA
    GET_VREG_R  rINST %ecx
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    SET_VREG  rINST %eax
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_MOVE_RESULT: /* 0x0a */
/* File: x86/OP_MOVE_RESULT.S */
    /* for: move-result, move-result-object */
    /* op vAA */
    movl     rSELF,%eax                    # eax<- rSELF
    movl     offThread_retval(%eax),%eax   # eax<- self->retval.l
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    SET_VREG  %eax rINST                   # fp[AA]<- retval.l
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MOVE_RESULT_WIDE: /* 0x0b */
/* File: x86/OP_MOVE_RESULT_WIDE.S */
    /* move-result-wide vAA */
    movl    rSELF,%ecx
    movl    offThread_retval(%ecx),%eax
    movl    4+offThread_retval(%ecx),%ecx
    SET_VREG_WORD %eax rINST 0     # v[AA+0] <- eax
    SET_VREG_WORD %ecx rINST 1     # v[AA+1] <- ecx
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MOVE_RESULT_OBJECT: /* 0x0c */
/* File: x86/OP_MOVE_RESULT_OBJECT.S */
/* File: x86/OP_MOVE_RESULT.S */
    /* for: move-result, move-result-object */
    /* op vAA */
    movl     rSELF,%eax                    # eax<- rSELF
    movl     offThread_retval(%eax),%eax   # eax<- self->retval.l
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    SET_VREG  %eax rINST                   # fp[AA]<- retval.l
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_MOVE_EXCEPTION: /* 0x0d */
/* File: x86/OP_MOVE_EXCEPTION.S */
    /* move-exception vAA */
    movl    rSELF,%ecx
    movl    offThread_exception(%ecx),%eax # eax<- dvmGetException bypass
    SET_VREG %eax rINST                # fp[AA]<- exception object
    FETCH_INST_OPCODE 1 %eax
    ADVANCE_PC 1
    movl    $0,offThread_exception(%ecx) # dvmClearException bypass
    GOTO_NEXT_R %eax

/* ------------------------------ */
.L_OP_RETURN_VOID: /* 0x0e */
/* File: x86/OP_RETURN_VOID.S */
    jmp       common_returnFromMethod

/* ------------------------------ */
.L_OP_RETURN: /* 0x0f */
/* File: x86/OP_RETURN.S */
    /*
     * Return a 32-bit value.  Copies the return value into the "self"
     * structure, then jumps to the return handler.
     *
     * for: return, return-object
     */
    /* op vAA */
    movl    rSELF,%ecx
    GET_VREG_R %eax rINST               # eax<- vAA
    movl    %eax,offThread_retval(%ecx)   # retval.i <- AA
    jmp     common_returnFromMethod

/* ------------------------------ */
.L_OP_RETURN_WIDE: /* 0x10 */
/* File: x86/OP_RETURN_WIDE.S */
    /*
     * Return a 64-bit value.  Copies the return value into the "self"
     * structure, then jumps to the return handler.
     */
    /* return-wide vAA */
    movl    rSELF,%ecx
    GET_VREG_WORD %eax rINST 0       # eax<- v[AA+0]
    GET_VREG_WORD rINST rINST 1      # rINST<- v[AA+1]
    movl    %eax,offThread_retval(%ecx)
    movl    rINST,4+offThread_retval(%ecx)
    jmp     common_returnFromMethod

/* ------------------------------ */
.L_OP_RETURN_OBJECT: /* 0x11 */
/* File: x86/OP_RETURN_OBJECT.S */
/* File: x86/OP_RETURN.S */
    /*
     * Return a 32-bit value.  Copies the return value into the "self"
     * structure, then jumps to the return handler.
     *
     * for: return, return-object
     */
    /* op vAA */
    movl    rSELF,%ecx
    GET_VREG_R %eax rINST               # eax<- vAA
    movl    %eax,offThread_retval(%ecx)   # retval.i <- AA
    jmp     common_returnFromMethod


/* ------------------------------ */
.L_OP_CONST_4: /* 0x12 */
/* File: x86/OP_CONST_4.S */
    /* const/4 vA, #+B */
    movsx   rINSTbl,%eax              # eax<-ssssssBx
    movl    $0xf,rINST
    andl    %eax,rINST                # rINST<- A
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    sarl    $4,%eax
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_CONST_16: /* 0x13 */
/* File: x86/OP_CONST_16.S */
    /* const/16 vAA, #+BBBB */
    movswl  2(rPC),%ecx                # ecx<- ssssBBBB
    FETCH_INST_OPCODE 2 %eax
    ADVANCE_PC 2
    SET_VREG %ecx rINST                # vAA<- ssssBBBB
    GOTO_NEXT_R %eax

/* ------------------------------ */
.L_OP_CONST: /* 0x14 */
/* File: x86/OP_CONST.S */
    /* const vAA, #+BBBBbbbb */
    movl      2(rPC),%eax             # grab all 32 bits at once
    movl      rINST,rINST             # rINST<- AA
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    SET_VREG %eax rINST               # vAA<- eax
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_CONST_HIGH16: /* 0x15 */
/* File: x86/OP_CONST_HIGH16.S */
    /* const/high16 vAA, #+BBBB0000 */
    movzwl     2(rPC),%eax                # eax<- 0000BBBB
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    sall       $16,%eax                  # eax<- BBBB0000
    SET_VREG %eax rINST                   # vAA<- eax
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_CONST_WIDE_16: /* 0x16 */
/* File: x86/OP_CONST_WIDE_16.S */
    /* const-wide/16 vAA, #+BBBB */
    movswl    2(rPC),%eax               # eax<- ssssBBBB
    SPILL(rIBASE)                       # preserve rIBASE (cltd trashes it)
    cltd                                # rIBASE:eax<- ssssssssssssBBBB
    SET_VREG_WORD rIBASE rINST 1        # store msw
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)                     # restore rIBASE
    SET_VREG_WORD %eax rINST 0          # store lsw
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_CONST_WIDE_32: /* 0x17 */
/* File: x86/OP_CONST_WIDE_32.S */
    /* const-wide/32 vAA, #+BBBBbbbb */
    movl     2(rPC),%eax                # eax<- BBBBbbbb
    SPILL(rIBASE)                       # save rIBASE (cltd trashes it)
    cltd                                # rIBASE:eax<- ssssssssssssBBBB
    SET_VREG_WORD rIBASE rINST,1        # store msw
    FETCH_INST_OPCODE 3 %ecx
    UNSPILL(rIBASE)                     # restore rIBASE
    SET_VREG_WORD %eax rINST 0          # store lsw
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_CONST_WIDE: /* 0x18 */
/* File: x86/OP_CONST_WIDE.S */
    /* const-wide vAA, #+HHHHhhhhBBBBbbbb */
    movl      2(rPC),%eax         # eax<- lsw
    movzbl    rINSTbl,%ecx        # ecx<- AA
    movl      6(rPC),rINST        # rINST<- msw
    leal      (rFP,%ecx,4),%ecx   # dst addr
    movl      rINST,4(%ecx)
    movl      %eax,(%ecx)
    FETCH_INST_OPCODE 5 %ecx
    ADVANCE_PC 5
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_CONST_WIDE_HIGH16: /* 0x19 */
/* File: x86/OP_CONST_WIDE_HIGH16.S */
    /* const-wide/high16 vAA, #+BBBB000000000000 */
    movzwl     2(rPC),%eax                # eax<- 0000BBBB
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    sall       $16,%eax                  # eax<- BBBB0000
    SET_VREG_WORD %eax rINST 1            # v[AA+1]<- eax
    xorl       %eax,%eax
    SET_VREG_WORD %eax rINST 0            # v[AA+0]<- eax
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_CONST_STRING: /* 0x1a */
/* File: x86/OP_CONST_STRING.S */

    /* const/string vAA, String@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax              # eax<- BBBB
    movl      offThread_methodClassDex(%ecx),%ecx# ecx<- self->methodClassDex
    movl      offDvmDex_pResStrings(%ecx),%ecx # ecx<- dvmDex->pResStrings
    movl      (%ecx,%eax,4),%eax       # eax<- rResString[BBBB]
    FETCH_INST_OPCODE 2 %ecx
    testl     %eax,%eax                # resolved yet?
    je        .LOP_CONST_STRING_resolve
    SET_VREG  %eax rINST               # vAA<- rResString[BBBB]
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* This is the less common path, so we'll redo some work
   here rather than force spills on the common path */
.LOP_CONST_STRING_resolve:
    movl     rSELF,%eax
    EXPORT_PC
    movl     offThread_method(%eax),%eax # eax<- self->method
    movzwl   2(rPC),%ecx               # ecx<- BBBB
    movl     offMethod_clazz(%eax),%eax
    movl     %ecx,OUT_ARG1(%esp)
    movl     %eax,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveString          # go resolve
    UNSPILL(rIBASE)
    testl    %eax,%eax                 # failed?
    je       common_exceptionThrown
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG %eax rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_CONST_STRING_JUMBO: /* 0x1b */
/* File: x86/OP_CONST_STRING_JUMBO.S */

    /* const/string vAA, String@BBBBBBBB */
    movl      rSELF,%ecx
    movl      2(rPC),%eax              # eax<- BBBBBBBB
    movl      offThread_methodClassDex(%ecx),%ecx# ecx<- self->methodClassDex
    movl      offDvmDex_pResStrings(%ecx),%ecx # ecx<- dvmDex->pResStrings
    movl      (%ecx,%eax,4),%eax       # eax<- rResString[BBBB]
    FETCH_INST_OPCODE 3 %ecx
    testl     %eax,%eax                # resolved yet?
    je        .LOP_CONST_STRING_JUMBO_resolve
    SET_VREG  %eax rINST               # vAA<- rResString[BBBB]
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx

/* This is the less common path, so we'll redo some work
   here rather than force spills on the common path */
.LOP_CONST_STRING_JUMBO_resolve:
    movl     rSELF,%eax
    EXPORT_PC
    movl     offThread_method(%eax),%eax # eax<- self->method
    movl     2(rPC),%ecx               # ecx<- BBBBBBBB
    movl     offMethod_clazz(%eax),%eax
    movl     %ecx,OUT_ARG1(%esp)
    movl     %eax,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveString          # go resolve
    UNSPILL(rIBASE)
    testl    %eax,%eax                 # failed?
    je       common_exceptionThrown
    FETCH_INST_OPCODE 3 %ecx
    SET_VREG %eax rINST
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_CONST_CLASS: /* 0x1c */
/* File: x86/OP_CONST_CLASS.S */

    /* const/class vAA, Class@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax              # eax<- BBBB
    movl      offThread_methodClassDex(%ecx),%ecx# ecx<- self->methodClassDex
    movl      offDvmDex_pResClasses(%ecx),%ecx # ecx<- dvmDex->pResClasses
    movl      (%ecx,%eax,4),%eax       # eax<- rResClasses[BBBB]
    testl     %eax,%eax                # resolved yet?
    je        .LOP_CONST_CLASS_resolve
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG  %eax rINST               # vAA<- rResClasses[BBBB]
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* This is the less common path, so we'll redo some work
   here rather than force spills on the common path */
.LOP_CONST_CLASS_resolve:
    movl     rSELF,%eax
    EXPORT_PC
    movl     offThread_method(%eax),%eax # eax<- self->method
    movl     $1,OUT_ARG2(%esp)        # true
    movzwl   2(rPC),%ecx               # ecx<- BBBB
    movl     offMethod_clazz(%eax),%eax
    movl     %ecx,OUT_ARG1(%esp)
    movl     %eax,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveClass           # go resolve
    UNSPILL(rIBASE)
    testl    %eax,%eax                 # failed?
    je       common_exceptionThrown
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG %eax rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MONITOR_ENTER: /* 0x1d */
/* File: x86/OP_MONITOR_ENTER.S */
    /*
     * Synchronize on an object.
     */
    /* monitor-enter vAA */
    movl    rSELF,%ecx
    GET_VREG_R %eax rINST               # eax<- vAA
    FETCH_INST_WORD 1
    testl   %eax,%eax                   # null object?
    EXPORT_PC                           # need for precise GC
    je     common_errNullObject
    movl    %ecx,OUT_ARG0(%esp)
    movl    %eax,OUT_ARG1(%esp)
    SPILL(rIBASE)
    call    dvmLockObject               # dvmLockObject(self,object)
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MONITOR_EXIT: /* 0x1e */
/* File: x86/OP_MONITOR_EXIT.S */
    /*
     * Unlock an object.
     *
     * Exceptions that occur when unlocking a monitor need to appear as
     * if they happened at the following instruction.  See the Dalvik
     * instruction spec.
     */
    /* monitor-exit vAA */
    GET_VREG_R %eax rINST
    movl    rSELF,%ecx
    EXPORT_PC
    testl   %eax,%eax                   # null object?
    je      .LOP_MONITOR_EXIT_errNullObject   # go if so
    movl    %eax,OUT_ARG1(%esp)
    movl    %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call    dvmUnlockObject             # unlock(self,obj)
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 1 %ecx
    testl   %eax,%eax                   # success?
    ADVANCE_PC 1
    je      common_exceptionThrown      # no, exception pending
    GOTO_NEXT_R %ecx
.LOP_MONITOR_EXIT_errNullObject:
    ADVANCE_PC 1                        # advance before throw
    jmp     common_errNullObject

/* ------------------------------ */
.L_OP_CHECK_CAST: /* 0x1f */
/* File: x86/OP_CHECK_CAST.S */
    /*
     * Check to see if a cast from one class to another is allowed.
     */
    /* check-cast vAA, class@BBBB */
    movl      rSELF,%ecx
    GET_VREG_R  rINST,rINST             # rINST<- vAA (object)
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offThread_methodClassDex(%ecx),%ecx # ecx<- pDvmDex
    testl     rINST,rINST               # is oject null?
    movl      offDvmDex_pResClasses(%ecx),%ecx # ecx<- pDvmDex->pResClasses
    je        .LOP_CHECK_CAST_okay          # null obj, cast always succeeds
    movl      (%ecx,%eax,4),%eax        # eax<- resolved class
    movl      offObject_clazz(rINST),%ecx # ecx<- obj->clazz
    testl     %eax,%eax                 # have we resolved this before?
    je        .LOP_CHECK_CAST_resolve       # no, go do it now
.LOP_CHECK_CAST_resolved:
    cmpl      %eax,%ecx                 # same class (trivial success)?
    jne       .LOP_CHECK_CAST_fullcheck     # no, do full check
.LOP_CHECK_CAST_okay:
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

    /*
     * Trivial test failed, need to perform full check.  This is common.
     *  ecx holds obj->clazz
     *  eax holds class resolved from BBBB
     *  rINST holds object
     */
.LOP_CHECK_CAST_fullcheck:
    movl    %eax,sReg0                 # we'll need the desired class on failure
    movl    %eax,OUT_ARG1(%esp)
    movl    %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call    dvmInstanceofNonTrivial    # eax<- boolean result
    UNSPILL(rIBASE)
    testl   %eax,%eax                  # failed?
    jne     .LOP_CHECK_CAST_okay           # no, success

    # A cast has failed.  We need to throw a ClassCastException.
    EXPORT_PC
    movl    offObject_clazz(rINST),%eax
    movl    %eax,OUT_ARG0(%esp)                 # arg0<- obj->clazz
    movl    sReg0,%ecx
    movl    %ecx,OUT_ARG1(%esp)                 # arg1<- desired class
    call    dvmThrowClassCastException
    jmp     common_exceptionThrown

    /*
     * Resolution required.  This is the least-likely path, and we're
     * going to have to recreate some data.
     *
     *  rINST holds object
     */
.LOP_CHECK_CAST_resolve:
    movl    rSELF,%ecx
    EXPORT_PC
    movzwl  2(rPC),%eax                # eax<- BBBB
    movl    offThread_method(%ecx),%ecx  # ecx<- self->method
    movl    %eax,OUT_ARG1(%esp)        # arg1<- BBBB
    movl    offMethod_clazz(%ecx),%ecx # ecx<- metho->clazz
    movl    $0,OUT_ARG2(%esp)         # arg2<- false
    movl    %ecx,OUT_ARG0(%esp)        # arg0<- method->clazz
    SPILL(rIBASE)
    call    dvmResolveClass            # eax<- resolved ClassObject ptr
    UNSPILL(rIBASE)
    testl   %eax,%eax                  # got null?
    je      common_exceptionThrown     # yes, handle exception
    movl    offObject_clazz(rINST),%ecx  # ecx<- obj->clazz
    jmp     .LOP_CHECK_CAST_resolved       # pick up where we left off

/* ------------------------------ */
.L_OP_INSTANCE_OF: /* 0x20 */
/* File: x86/OP_INSTANCE_OF.S */
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    /* instance-of vA, vB, class@CCCC */
    movl    rINST,%eax                  # eax<- BA
    sarl    $4,%eax                    # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB (obj)
    movl    rSELF,%ecx
    testl   %eax,%eax                   # object null?
    movl    offThread_methodClassDex(%ecx),%ecx  # ecx<- pDvmDex
    SPILL(rIBASE)                       # preserve rIBASE
    je      .LOP_INSTANCE_OF_store           # null obj, not instance, store it
    movzwl  2(rPC),rIBASE               # rIBASE<- CCCC
    movl    offDvmDex_pResClasses(%ecx),%ecx # ecx<- pDvmDex->pResClasses
    movl    (%ecx,rIBASE,4),%ecx        # ecx<- resolved class
    movl    offObject_clazz(%eax),%eax  # eax<- obj->clazz
    testl   %ecx,%ecx                   # have we resolved this before?
    je      .LOP_INSTANCE_OF_resolve         # not resolved, do it now
.LOP_INSTANCE_OF_resolved:  # eax<- obj->clazz, ecx<- resolved class
    cmpl    %eax,%ecx                   # same class (trivial success)?
    je      .LOP_INSTANCE_OF_trivial         # yes, trivial finish
    /*
     * Trivial test failed, need to perform full check.  This is common.
     *  eax holds obj->clazz
     *  ecx holds class resolved from BBBB
     *  rINST has BA
     */
    movl    %eax,OUT_ARG0(%esp)
    movl    %ecx,OUT_ARG1(%esp)
    call    dvmInstanceofNonTrivial     # eax<- boolean result
    # fall through to OP_INSTANCE_OF_store

    /*
     * eax holds boolean result
     * rINST holds BA
     */
.LOP_INSTANCE_OF_store:
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    andb    $0xf,rINSTbl               # <- A
    ADVANCE_PC 2
    SET_VREG %eax rINST                 # vA<- eax
    GOTO_NEXT_R %ecx

    /*
     * Trivial test succeeded, save and bail.
     *  r9 holds A
     */
.LOP_INSTANCE_OF_trivial:
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    andb    $0xf,rINSTbl               # <- A
    ADVANCE_PC 2
    movl    $1,%eax
    SET_VREG %eax rINST                 # vA<- true
    GOTO_NEXT_R %ecx

    /*
     * Resolution required.  This is the least-likely path.
     *
     *  rIBASE holds BBBB
     *  rINST holds BA
     */
.LOP_INSTANCE_OF_resolve:
    movl    rIBASE,OUT_ARG1(%esp)         # arg1<- BBBB
    movl    rSELF,%ecx
    movl    offThread_method(%ecx),%ecx
    movl    $1,OUT_ARG2(%esp)          # arg2<- true
    movl    offMethod_clazz(%ecx),%ecx  # ecx<- method->clazz
    EXPORT_PC
    movl    %ecx,OUT_ARG0(%esp)         # arg0<- method->clazz
    call    dvmResolveClass             # eax<- resolved ClassObject ptr
    testl   %eax,%eax                   # success?
    je      common_exceptionThrown      # no, handle exception
/* Now, we need to sync up with fast path.  We need eax to
 * hold the obj->clazz, and ecx to hold the resolved class
 */
    movl    %eax,%ecx                   # ecx<- resolved class
    movl    rINST,%eax                  # eax<- BA
    sarl    $4,%eax                    # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB (obj)
    movl    offObject_clazz(%eax),%eax  # eax<- obj->clazz
    jmp     .LOP_INSTANCE_OF_resolved

/* ------------------------------ */
.L_OP_ARRAY_LENGTH: /* 0x21 */
/* File: x86/OP_ARRAY_LENGTH.S */
    /*
     * Return the length of an array.
     */
   mov      rINST,%eax                # eax<- BA
   sarl     $4,rINST                 # rINST<- B
   GET_VREG_R %ecx rINST              # ecx<- vB (object ref)
   andb     $0xf,%al                 # eax<- A
   testl    %ecx,%ecx                 # is null?
   je       common_errNullObject
   movl     offArrayObject_length(%ecx),rINST
   FETCH_INST_OPCODE 1 %ecx
   ADVANCE_PC 1
   SET_VREG rINST %eax
   GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_NEW_INSTANCE: /* 0x22 */
/* File: x86/OP_NEW_INSTANCE.S */
    /*
     * Create a new instance of a class.
     */
    /* new-instance vAA, class@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- pDvmDex
    SPILL(rIBASE)
    SPILL_TMP2(%ebx)
    movl      offDvmDex_pResClasses(%ecx),%ecx # ecx<- pDvmDex->pResClasses
    EXPORT_PC
#if defined(WITH_JIT)
    lea       (%ecx,%eax,4),%ebx        # ebx <- &resolved class
#endif
    movl      (%ecx,%eax,4),%ecx        # ecx<- resolved class
    testl     %ecx,%ecx                 # resolved?
    je        .LOP_NEW_INSTANCE_resolve       # no, go do it
.LOP_NEW_INSTANCE_resolved:  # on entry, ecx<- class
    cmpb      $CLASS_INITIALIZED,offClassObject_status(%ecx)
    jne       .LOP_NEW_INSTANCE_needinit
.LOP_NEW_INSTANCE_initialized:  # on entry, ecx<- class
    movl      $ALLOC_DONT_TRACK,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    call     dvmAllocObject             # eax<- new object
    testl    %eax,%eax                  # success?
    je       common_exceptionThrown     # no, bail out
#if defined(WITH_JIT)
        /*
     * The JIT needs the class to be fully resolved before it can
     * include this instruction in a trace.
     */
    movl    rSELF, %ecx
    movl    offThread_subMode(%ecx), %ecx
    andl    $kSubModeJitTraceBuild, %ecx # under construction?
    jne     .LOP_NEW_INSTANCE_jitCheck
#endif
.LOP_NEW_INSTANCE_end:
    UNSPILL_TMP2(%ebx)
    SET_VREG %eax rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

#if defined(WITH_JIT)
    /*
     * Check to see if we need to stop the trace building early.
     * eax: new object
     */
.LOP_NEW_INSTANCE_jitCheck:
    cmp     $0, (%ebx)                   # okay?
    jne     .LOP_NEW_INSTANCE_end        # yes, finish
    SPILL_TMP1(%eax)                     # preserve new object
    movl    rSELF, %ecx
    movl    %ecx, OUT_ARG0(%esp)
    movl    rPC, OUT_ARG1(%esp)
    call    dvmJitEndTraceSelect         # (self, pc)
    UNSPILL_TMP1(%eax)
    UNSPILL_TMP2(%ebx)
    SET_VREG %eax rINST                  # vAA <- new object
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
#endif

    /*
     * Class initialization required.
     *
     *  ecx holds class object
     */
.LOP_NEW_INSTANCE_needinit:
    SPILL_TMP1(%ecx)                    # save object
    movl    %ecx,OUT_ARG0(%esp)
    call    dvmInitClass                # initialize class
    UNSPILL_TMP1(%ecx)                  # restore object
    testl   %eax,%eax                   # success?
    jne     .LOP_NEW_INSTANCE_initialized     # success, continue
    jmp     common_exceptionThrown      # go deal with init exception

    /*
     * Resolution required.  This is the least-likely path.
     *
     */
.LOP_NEW_INSTANCE_resolve:
    movl    rSELF,%ecx
    movzwl  2(rPC),%eax
    movl    offThread_method(%ecx),%ecx   # ecx<- self->method
    movl    %eax,OUT_ARG1(%esp)
    movl    offMethod_clazz(%ecx),%ecx  # ecx<- method->clazz
    movl    $0,OUT_ARG2(%esp)
    movl    %ecx,OUT_ARG0(%esp)
    call    dvmResolveClass             # call(clazz,off,flags)
    movl    %eax,%ecx                   # ecx<- resolved ClassObject ptr
    testl   %ecx,%ecx                   # success?
    jne     .LOP_NEW_INSTANCE_resolved        # good to go
    jmp     common_exceptionThrown      # no, handle exception

/* ------------------------------ */
.L_OP_NEW_ARRAY: /* 0x23 */
/* File: x86/OP_NEW_ARRAY.S */
    /*
     * Allocate an array of objects, specified with the array class
     * and a count.
     *
     * The verifier guarantees that this is an array class, so we don't
     * check for it here.
     */
    /* new-array vA, vB, class@CCCC */
    movl    rSELF,%ecx
    EXPORT_PC
    movl    offThread_methodClassDex(%ecx),%ecx # ecx<- pDvmDex
    movzwl  2(rPC),%eax                       # eax<- CCCC
    movl    offDvmDex_pResClasses(%ecx),%ecx  # ecx<- pDvmDex->pResClasses
    SPILL(rIBASE)
    movl    (%ecx,%eax,4),%ecx                # ecx<- resolved class
    movzbl  rINSTbl,%eax
    sarl    $4,%eax                          # eax<- B
    GET_VREG_R %eax %eax                      # eax<- vB (array length)
    andb    $0xf,rINSTbl                     # rINST<- A
    testl   %eax,%eax
    js      common_errNegativeArraySize       # bail, passing len in eax
    testl   %ecx,%ecx                         # already resolved?
    jne     .LOP_NEW_ARRAY_finish                # yes, fast path
    /*
     * Resolve class.  (This is an uncommon case.)
     *  ecx holds class (null here)
     *  eax holds array length (vB)
     */
    movl    rSELF,%ecx
    SPILL_TMP1(%eax)                   # save array length
    movl    offThread_method(%ecx),%ecx  # ecx<- self->method
    movzwl  2(rPC),%eax                # eax<- CCCC
    movl    offMethod_clazz(%ecx),%ecx # ecx<- method->clazz
    movl    %eax,OUT_ARG1(%esp)
    movl    $0,OUT_ARG2(%esp)
    movl    %ecx,OUT_ARG0(%esp)
    call    dvmResolveClass            # eax<- call(clazz,ref,flag)
    movl    %eax,%ecx
    UNSPILL_TMP1(%eax)
    testl   %ecx,%ecx                  # successful resolution?
    je      common_exceptionThrown     # no, bail.
# fall through to OP_NEW_ARRAY_finish

    /*
     * Finish allocation
     *
     * ecx holds class
     * eax holds array length (vB)
     */
.LOP_NEW_ARRAY_finish:
    movl    %ecx,OUT_ARG0(%esp)
    movl    %eax,OUT_ARG1(%esp)
    movl    $ALLOC_DONT_TRACK,OUT_ARG2(%esp)
    call    dvmAllocArrayByClass    # eax<- call(clazz,length,flags)
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    testl   %eax,%eax               # failed?
    je      common_exceptionThrown  # yup - go handle
    SET_VREG %eax rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_FILLED_NEW_ARRAY: /* 0x24 */
/* File: x86/OP_FILLED_NEW_ARRAY.S */
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type@BBBB */
    movl    rSELF,%eax
    movl    offThread_methodClassDex(%eax),%eax # eax<- pDvmDex
    movzwl  2(rPC),%ecx                       # ecx<- BBBB
    movl    offDvmDex_pResClasses(%eax),%eax  # eax<- pDvmDex->pResClasses
    SPILL(rIBASE)                             # preserve rIBASE
    movl    (%eax,%ecx,4),%eax                # eax<- resolved class
    EXPORT_PC
    testl   %eax,%eax                         # already resolved?
    jne     .LOP_FILLED_NEW_ARRAY_continue              # yes, continue
    # less frequent path, so we'll redo some work
    movl    rSELF,%eax
    movl    $0,OUT_ARG2(%esp)                # arg2<- false
    movl    %ecx,OUT_ARG1(%esp)               # arg1<- BBBB
    movl    offThread_method(%eax),%eax         # eax<- self->method
    movl    offMethod_clazz(%eax),%eax        # eax<- method->clazz
    movl    %eax,OUT_ARG0(%esp)               # arg0<- clazz
    call    dvmResolveClass                   # eax<- call(clazz,ref,flag)
    testl   %eax,%eax                         # null?
    je      common_exceptionThrown            # yes, handle it

       # note: fall through to .LOP_FILLED_NEW_ARRAY_continue

    /*
     * On entry:
     *    eax holds array class [r0]
     *    rINST holds AA or BB [r10]
     *    ecx is scratch
     */
.LOP_FILLED_NEW_ARRAY_continue:
    movl    offClassObject_descriptor(%eax),%ecx  # ecx<- arrayClass->descriptor
    movl    $ALLOC_DONT_TRACK,OUT_ARG2(%esp)     # arg2<- flags
    movzbl  1(%ecx),%ecx                          # ecx<- descriptor[1]
    movl    %eax,OUT_ARG0(%esp)                   # arg0<- arrayClass
    movl    rSELF,%eax
    cmpb    $'I',%cl                             # supported?
    je      1f
    cmpb    $'L',%cl
    je      1f
    cmpb    $'[',%cl
    jne      .LOP_FILLED_NEW_ARRAY_notimpl                  # no, not handled yet
1:
    movl    %ecx,offThread_retval+4(%eax)           # save type
    .if      (!0)
    SPILL_TMP1(rINST)                              # save copy, need "B" later
    sarl    $4,rINST
    .endif
    movl    rINST,OUT_ARG1(%esp)                  # arg1<- A or AA (length)
    call    dvmAllocArrayByClass     # eax<- call(arrayClass, length, flags)
    movl    rSELF,%ecx
    testl   %eax,%eax                             # alloc successful?
    je      common_exceptionThrown                # no, handle exception
    movl    %eax,offThread_retval(%ecx)             # retval.l<- new array
    movzwl  4(rPC),%ecx                           # ecx<- FEDC or CCCC
    leal    offArrayObject_contents(%eax),%eax    # eax<- newArray->contents

/* at this point:
 *     eax is pointer to tgt
 *     rINST is length
 *     ecx is FEDC or CCCC
 *     TMP_SPILL1 is BA
 *  We now need to copy values from registers into the array
 */

    .if 0
    # set up src pointer
    SPILL_TMP2(%esi)
    SPILL_TMP3(%edi)
    leal    (rFP,%ecx,4),%esi # set up src ptr
    movl    %eax,%edi         # set up dst ptr
    movl    rINST,%ecx        # load count register
    rep
    movsd
    UNSPILL_TMP2(%esi)
    UNSPILL_TMP3(%edi)
    movl    rSELF,%ecx
    movl    offThread_retval+4(%ecx),%eax      # eax<- type
    .else
    testl  rINST,rINST
    je     4f
    UNSPILL_TMP1(rIBASE)      # restore "BA"
    andl   $0x0f,rIBASE      # rIBASE<- 0000000A
    sall   $16,rIBASE        # rIBASE<- 000A0000
    orl    %ecx,rIBASE        # rIBASE<- 000AFEDC
3:
    movl   $0xf,%ecx
    andl   rIBASE,%ecx        # ecx<- next reg to load
    GET_VREG_R %ecx %ecx
    shrl   $4,rIBASE
    leal   4(%eax),%eax
    movl   %ecx,-4(%eax)
    sub    $1,rINST
    jne    3b
4:
    movl   rSELF,%ecx
    movl    offThread_retval+4(%ecx),%eax      # eax<- type
    .endif

    cmpb    $'I',%al                        # Int array?
    je      5f                               # skip card mark if so
    movl    offThread_retval(%ecx),%eax        # eax<- object head
    movl    offThread_cardTable(%ecx),%ecx     # card table base
    shrl    $GC_CARD_SHIFT,%eax             # convert to card num
    movb    %cl,(%ecx,%eax)                  # mark card based on object head
5:
    UNSPILL(rIBASE)                          # restore rIBASE
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx


    /*
     * Throw an exception indicating that we have not implemented this
     * mode of filled-new-array.
     */
.LOP_FILLED_NEW_ARRAY_notimpl:
    movl    $.LstrFilledNewArrayNotImplA,%eax
    movl    %eax,OUT_ARG0(%esp)
    call    dvmThrowInternalError
    jmp     common_exceptionThrown

/* ------------------------------ */
.L_OP_FILLED_NEW_ARRAY_RANGE: /* 0x25 */
/* File: x86/OP_FILLED_NEW_ARRAY_RANGE.S */
/* File: x86/OP_FILLED_NEW_ARRAY.S */
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type@BBBB */
    movl    rSELF,%eax
    movl    offThread_methodClassDex(%eax),%eax # eax<- pDvmDex
    movzwl  2(rPC),%ecx                       # ecx<- BBBB
    movl    offDvmDex_pResClasses(%eax),%eax  # eax<- pDvmDex->pResClasses
    SPILL(rIBASE)                             # preserve rIBASE
    movl    (%eax,%ecx,4),%eax                # eax<- resolved class
    EXPORT_PC
    testl   %eax,%eax                         # already resolved?
    jne     .LOP_FILLED_NEW_ARRAY_RANGE_continue              # yes, continue
    # less frequent path, so we'll redo some work
    movl    rSELF,%eax
    movl    $0,OUT_ARG2(%esp)                # arg2<- false
    movl    %ecx,OUT_ARG1(%esp)               # arg1<- BBBB
    movl    offThread_method(%eax),%eax         # eax<- self->method
    movl    offMethod_clazz(%eax),%eax        # eax<- method->clazz
    movl    %eax,OUT_ARG0(%esp)               # arg0<- clazz
    call    dvmResolveClass                   # eax<- call(clazz,ref,flag)
    testl   %eax,%eax                         # null?
    je      common_exceptionThrown            # yes, handle it

       # note: fall through to .LOP_FILLED_NEW_ARRAY_RANGE_continue

    /*
     * On entry:
     *    eax holds array class [r0]
     *    rINST holds AA or BB [r10]
     *    ecx is scratch
     */
.LOP_FILLED_NEW_ARRAY_RANGE_continue:
    movl    offClassObject_descriptor(%eax),%ecx  # ecx<- arrayClass->descriptor
    movl    $ALLOC_DONT_TRACK,OUT_ARG2(%esp)     # arg2<- flags
    movzbl  1(%ecx),%ecx                          # ecx<- descriptor[1]
    movl    %eax,OUT_ARG0(%esp)                   # arg0<- arrayClass
    movl    rSELF,%eax
    cmpb    $'I',%cl                             # supported?
    je      1f
    cmpb    $'L',%cl
    je      1f
    cmpb    $'[',%cl
    jne      .LOP_FILLED_NEW_ARRAY_RANGE_notimpl                  # no, not handled yet
1:
    movl    %ecx,offThread_retval+4(%eax)           # save type
    .if      (!1)
    SPILL_TMP1(rINST)                              # save copy, need "B" later
    sarl    $4,rINST
    .endif
    movl    rINST,OUT_ARG1(%esp)                  # arg1<- A or AA (length)
    call    dvmAllocArrayByClass     # eax<- call(arrayClass, length, flags)
    movl    rSELF,%ecx
    testl   %eax,%eax                             # alloc successful?
    je      common_exceptionThrown                # no, handle exception
    movl    %eax,offThread_retval(%ecx)             # retval.l<- new array
    movzwl  4(rPC),%ecx                           # ecx<- FEDC or CCCC
    leal    offArrayObject_contents(%eax),%eax    # eax<- newArray->contents

/* at this point:
 *     eax is pointer to tgt
 *     rINST is length
 *     ecx is FEDC or CCCC
 *     TMP_SPILL1 is BA
 *  We now need to copy values from registers into the array
 */

    .if 1
    # set up src pointer
    SPILL_TMP2(%esi)
    SPILL_TMP3(%edi)
    leal    (rFP,%ecx,4),%esi # set up src ptr
    movl    %eax,%edi         # set up dst ptr
    movl    rINST,%ecx        # load count register
    rep
    movsd
    UNSPILL_TMP2(%esi)
    UNSPILL_TMP3(%edi)
    movl    rSELF,%ecx
    movl    offThread_retval+4(%ecx),%eax      # eax<- type
    .else
    testl  rINST,rINST
    je     4f
    UNSPILL_TMP1(rIBASE)      # restore "BA"
    andl   $0x0f,rIBASE      # rIBASE<- 0000000A
    sall   $16,rIBASE        # rIBASE<- 000A0000
    orl    %ecx,rIBASE        # rIBASE<- 000AFEDC
3:
    movl   $0xf,%ecx
    andl   rIBASE,%ecx        # ecx<- next reg to load
    GET_VREG_R %ecx %ecx
    shrl   $4,rIBASE
    leal   4(%eax),%eax
    movl   %ecx,-4(%eax)
    sub    $1,rINST
    jne    3b
4:
    movl   rSELF,%ecx
    movl    offThread_retval+4(%ecx),%eax      # eax<- type
    .endif

    cmpb    $'I',%al                        # Int array?
    je      5f                               # skip card mark if so
    movl    offThread_retval(%ecx),%eax        # eax<- object head
    movl    offThread_cardTable(%ecx),%ecx     # card table base
    shrl    $GC_CARD_SHIFT,%eax             # convert to card num
    movb    %cl,(%ecx,%eax)                  # mark card based on object head
5:
    UNSPILL(rIBASE)                          # restore rIBASE
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx


    /*
     * Throw an exception indicating that we have not implemented this
     * mode of filled-new-array.
     */
.LOP_FILLED_NEW_ARRAY_RANGE_notimpl:
    movl    $.LstrFilledNewArrayNotImplA,%eax
    movl    %eax,OUT_ARG0(%esp)
    call    dvmThrowInternalError
    jmp     common_exceptionThrown


/* ------------------------------ */
.L_OP_FILL_ARRAY_DATA: /* 0x26 */
/* File: x86/OP_FILL_ARRAY_DATA.S */
    /* fill-array-data vAA, +BBBBBBBB */
    movl    2(rPC),%ecx                # ecx<- BBBBbbbb
    leal    (rPC,%ecx,2),%ecx          # ecx<- PC + BBBBbbbb*2
    GET_VREG_R %eax rINST
    EXPORT_PC
    movl    %eax,OUT_ARG0(%esp)
    movl    %ecx,OUT_ARG1(%esp)
    SPILL(rIBASE)
    call    dvmInterpHandleFillArrayData
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 3 %ecx
    testl   %eax,%eax                   # exception thrown?
    je      common_exceptionThrown
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_THROW: /* 0x27 */
/* File: x86/OP_THROW.S */
    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    EXPORT_PC
    GET_VREG_R %eax rINST              # eax<- exception object
    movl     rSELF,%ecx                # ecx<- self
    testl    %eax,%eax                 # null object?
    je       common_errNullObject
    movl     %eax,offThread_exception(%ecx) # thread->exception<- obj
    jmp      common_exceptionThrown

/* ------------------------------ */
.L_OP_GOTO: /* 0x28 */
/* File: x86/OP_GOTO.S */
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    movl    rSELF,%ecx
    movsbl  rINSTbl,%eax          # eax<- ssssssAA
    movl    offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT

/* ------------------------------ */
.L_OP_GOTO_16: /* 0x29 */
/* File: x86/OP_GOTO_16.S */
    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset
     */
    /* goto/16 +AAAA */
    movl    rSELF,%ecx
    movswl  2(rPC),%eax            # eax<- ssssAAAA
    movl    offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT

/* ------------------------------ */
.L_OP_GOTO_32: /* 0x2a */
/* File: x86/OP_GOTO_32.S */
    /*
     * Unconditional branch, 32-bit offset.
     *
     * The branch distance is a signed code-unit offset.
     */
    /* goto/32 AAAAAAAA */
    movl    rSELF,%ecx
    movl    2(rPC),%eax            # eax<- AAAAAAAA
    movl    offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT

/* ------------------------------ */
.L_OP_PACKED_SWITCH: /* 0x2b */
/* File: x86/OP_PACKED_SWITCH.S */
    /*
     * Handle a packed-switch or sparse-switch instruction.  In both cases
     * we decode it and hand it off to a helper function.
     *
     * We don't really expect backward branches in a switch statement, but
     * they're perfectly legal, so we check for them here.
     *
     * for: packed-switch, sparse-switch
     */
    /* op vAA, +BBBB */
    movl    2(rPC),%ecx           # ecx<- BBBBbbbb
    GET_VREG_R %eax rINST         # eax<- vAA
    leal    (rPC,%ecx,2),%ecx     # ecx<- PC + BBBBbbbb*2
    movl    %eax,OUT_ARG1(%esp)   # ARG1<- vAA
    movl    %ecx,OUT_ARG0(%esp)   # ARG0<- switchData
    call    dvmInterpHandlePackedSwitch
    movl    rSELF,%ecx
    ADVANCE_PC_INDEXED %eax
    movl    offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT

/* ------------------------------ */
.L_OP_SPARSE_SWITCH: /* 0x2c */
/* File: x86/OP_SPARSE_SWITCH.S */
/* File: x86/OP_PACKED_SWITCH.S */
    /*
     * Handle a packed-switch or sparse-switch instruction.  In both cases
     * we decode it and hand it off to a helper function.
     *
     * We don't really expect backward branches in a switch statement, but
     * they're perfectly legal, so we check for them here.
     *
     * for: packed-switch, sparse-switch
     */
    /* op vAA, +BBBB */
    movl    2(rPC),%ecx           # ecx<- BBBBbbbb
    GET_VREG_R %eax rINST         # eax<- vAA
    leal    (rPC,%ecx,2),%ecx     # ecx<- PC + BBBBbbbb*2
    movl    %eax,OUT_ARG1(%esp)   # ARG1<- vAA
    movl    %ecx,OUT_ARG0(%esp)   # ARG0<- switchData
    call    dvmInterpHandleSparseSwitch
    movl    rSELF,%ecx
    ADVANCE_PC_INDEXED %eax
    movl    offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_CMPL_FLOAT: /* 0x2d */
/* File: x86/OP_CMPL_FLOAT.S */
/* File: x86/OP_CMPG_DOUBLE.S */
    /* float/double_cmp[gl] vAA, vBB, vCC */
    movzbl    3(rPC),%eax             # eax<- CC
    movzbl    2(rPC),%ecx             # ecx<- BB
    .if 0
    fldl     (rFP,%eax,4)
    fldl     (rFP,%ecx,4)
    .else
    flds     (rFP,%eax,4)
    flds     (rFP,%ecx,4)
    .endif
    xorl     %ecx,%ecx
    fucompp     # z if equal, p set if NaN, c set if st0 < st1
    fnstsw   %ax
    sahf
    FETCH_INST_OPCODE 2 %eax
    jp       .LOP_CMPL_FLOAT_isNaN
    je       .LOP_CMPL_FLOAT_finish
    sbbl     %ecx,%ecx
    jb       .LOP_CMPL_FLOAT_finish
    incl     %ecx
.LOP_CMPL_FLOAT_finish:
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax

.LOP_CMPL_FLOAT_isNaN:
    movl      $-1,%ecx
    jmp       .LOP_CMPL_FLOAT_finish


/* ------------------------------ */
.L_OP_CMPG_FLOAT: /* 0x2e */
/* File: x86/OP_CMPG_FLOAT.S */
/* File: x86/OP_CMPG_DOUBLE.S */
    /* float/double_cmp[gl] vAA, vBB, vCC */
    movzbl    3(rPC),%eax             # eax<- CC
    movzbl    2(rPC),%ecx             # ecx<- BB
    .if 0
    fldl     (rFP,%eax,4)
    fldl     (rFP,%ecx,4)
    .else
    flds     (rFP,%eax,4)
    flds     (rFP,%ecx,4)
    .endif
    xorl     %ecx,%ecx
    fucompp     # z if equal, p set if NaN, c set if st0 < st1
    fnstsw   %ax
    sahf
    FETCH_INST_OPCODE 2 %eax
    jp       .LOP_CMPG_FLOAT_isNaN
    je       .LOP_CMPG_FLOAT_finish
    sbbl     %ecx,%ecx
    jb       .LOP_CMPG_FLOAT_finish
    incl     %ecx
.LOP_CMPG_FLOAT_finish:
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax

.LOP_CMPG_FLOAT_isNaN:
    movl      $1,%ecx
    jmp       .LOP_CMPG_FLOAT_finish


/* ------------------------------ */
.L_OP_CMPL_DOUBLE: /* 0x2f */
/* File: x86/OP_CMPL_DOUBLE.S */
/* File: x86/OP_CMPG_DOUBLE.S */
    /* float/double_cmp[gl] vAA, vBB, vCC */
    movzbl    3(rPC),%eax             # eax<- CC
    movzbl    2(rPC),%ecx             # ecx<- BB
    .if 1
    fldl     (rFP,%eax,4)
    fldl     (rFP,%ecx,4)
    .else
    flds     (rFP,%eax,4)
    flds     (rFP,%ecx,4)
    .endif
    xorl     %ecx,%ecx
    fucompp     # z if equal, p set if NaN, c set if st0 < st1
    fnstsw   %ax
    sahf
    FETCH_INST_OPCODE 2 %eax
    jp       .LOP_CMPL_DOUBLE_isNaN
    je       .LOP_CMPL_DOUBLE_finish
    sbbl     %ecx,%ecx
    jb       .LOP_CMPL_DOUBLE_finish
    incl     %ecx
.LOP_CMPL_DOUBLE_finish:
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax

.LOP_CMPL_DOUBLE_isNaN:
    movl      $-1,%ecx
    jmp       .LOP_CMPL_DOUBLE_finish


/* ------------------------------ */
.L_OP_CMPG_DOUBLE: /* 0x30 */
/* File: x86/OP_CMPG_DOUBLE.S */
    /* float/double_cmp[gl] vAA, vBB, vCC */
    movzbl    3(rPC),%eax             # eax<- CC
    movzbl    2(rPC),%ecx             # ecx<- BB
    .if 1
    fldl     (rFP,%eax,4)
    fldl     (rFP,%ecx,4)
    .else
    flds     (rFP,%eax,4)
    flds     (rFP,%ecx,4)
    .endif
    xorl     %ecx,%ecx
    fucompp     # z if equal, p set if NaN, c set if st0 < st1
    fnstsw   %ax
    sahf
    FETCH_INST_OPCODE 2 %eax
    jp       .LOP_CMPG_DOUBLE_isNaN
    je       .LOP_CMPG_DOUBLE_finish
    sbbl     %ecx,%ecx
    jb       .LOP_CMPG_DOUBLE_finish
    incl     %ecx
.LOP_CMPG_DOUBLE_finish:
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax

.LOP_CMPG_DOUBLE_isNaN:
    movl      $1,%ecx
    jmp       .LOP_CMPG_DOUBLE_finish

/* ------------------------------ */
.L_OP_CMP_LONG: /* 0x31 */
/* File: x86/OP_CMP_LONG.S */
    /*
     * Compare two 64-bit values.  Puts 0, 1, or -1 into the destination
     * register based on the results of the comparison.
     */
    // TUNING: rework to avoid rIBASE spill
    /* cmp-long vAA, vBB, vCC */
    movzbl    2(rPC),%ecx              # ecx<- BB
    SPILL(rIBASE)
    movzbl    3(rPC),rIBASE            # rIBASE- CC
    GET_VREG_WORD %eax %ecx,1          # eax<- v[BB+1]
    GET_VREG_WORD %ecx %ecx 0          # ecx<- v[BB+0]
    cmpl      4(rFP,rIBASE,4),%eax
    jl        .LOP_CMP_LONG_smaller
    jg        .LOP_CMP_LONG_bigger
    sub       (rFP,rIBASE,4),%ecx
    ja        .LOP_CMP_LONG_bigger
    jb        .LOP_CMP_LONG_smaller
    SET_VREG %ecx rINST
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_CMP_LONG_bigger:
    movl      $1,%ecx
    SET_VREG %ecx rINST
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_CMP_LONG_smaller:
    movl      $-1,%ecx
    SET_VREG %ecx rINST
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_IF_EQ: /* 0x32 */
/* File: x86/OP_IF_EQ.S */
/* File: x86/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    movzx    rINSTbl,%ecx          # ecx <- A+
    andb     $0xf,%cl             # ecx <- A
    GET_VREG_R %eax %ecx           # eax <- vA
    sarl     $4,rINST             # rINST<- B
    movl     rSELF,%ecx
    cmpl     (rFP,rINST,4),%eax    # compare (vA, vB)
    movl     $2,%eax              # assume not taken
    jne   1f
    movswl   2(rPC),%eax           # Get signed branch offset
1:
    movl     offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_IF_NE: /* 0x33 */
/* File: x86/OP_IF_NE.S */
/* File: x86/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    movzx    rINSTbl,%ecx          # ecx <- A+
    andb     $0xf,%cl             # ecx <- A
    GET_VREG_R %eax %ecx           # eax <- vA
    sarl     $4,rINST             # rINST<- B
    movl     rSELF,%ecx
    cmpl     (rFP,rINST,4),%eax    # compare (vA, vB)
    movl     $2,%eax              # assume not taken
    je   1f
    movswl   2(rPC),%eax           # Get signed branch offset
1:
    movl     offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_IF_LT: /* 0x34 */
/* File: x86/OP_IF_LT.S */
/* File: x86/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    movzx    rINSTbl,%ecx          # ecx <- A+
    andb     $0xf,%cl             # ecx <- A
    GET_VREG_R %eax %ecx           # eax <- vA
    sarl     $4,rINST             # rINST<- B
    movl     rSELF,%ecx
    cmpl     (rFP,rINST,4),%eax    # compare (vA, vB)
    movl     $2,%eax              # assume not taken
    jge   1f
    movswl   2(rPC),%eax           # Get signed branch offset
1:
    movl     offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_IF_GE: /* 0x35 */
/* File: x86/OP_IF_GE.S */
/* File: x86/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    movzx    rINSTbl,%ecx          # ecx <- A+
    andb     $0xf,%cl             # ecx <- A
    GET_VREG_R %eax %ecx           # eax <- vA
    sarl     $4,rINST             # rINST<- B
    movl     rSELF,%ecx
    cmpl     (rFP,rINST,4),%eax    # compare (vA, vB)
    movl     $2,%eax              # assume not taken
    jl   1f
    movswl   2(rPC),%eax           # Get signed branch offset
1:
    movl     offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_IF_GT: /* 0x36 */
/* File: x86/OP_IF_GT.S */
/* File: x86/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    movzx    rINSTbl,%ecx          # ecx <- A+
    andb     $0xf,%cl             # ecx <- A
    GET_VREG_R %eax %ecx           # eax <- vA
    sarl     $4,rINST             # rINST<- B
    movl     rSELF,%ecx
    cmpl     (rFP,rINST,4),%eax    # compare (vA, vB)
    movl     $2,%eax              # assume not taken
    jle   1f
    movswl   2(rPC),%eax           # Get signed branch offset
1:
    movl     offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_IF_LE: /* 0x37 */
/* File: x86/OP_IF_LE.S */
/* File: x86/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    movzx    rINSTbl,%ecx          # ecx <- A+
    andb     $0xf,%cl             # ecx <- A
    GET_VREG_R %eax %ecx           # eax <- vA
    sarl     $4,rINST             # rINST<- B
    movl     rSELF,%ecx
    cmpl     (rFP,rINST,4),%eax    # compare (vA, vB)
    movl     $2,%eax              # assume not taken
    jg   1f
    movswl   2(rPC),%eax           # Get signed branch offset
1:
    movl     offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_IF_EQZ: /* 0x38 */
/* File: x86/OP_IF_EQZ.S */
/* File: x86/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    cmpl     $0,(rFP,rINST,4)     # compare (vA, 0)
    movl     rSELF,%ecx
    movl     $2,%eax              # assume branch not taken
    jne   1f
    movswl   2(rPC),%eax           # fetch signed displacement
    movl     offThread_curHandlerTable(%ecx),rIBASE
1:
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_IF_NEZ: /* 0x39 */
/* File: x86/OP_IF_NEZ.S */
/* File: x86/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    cmpl     $0,(rFP,rINST,4)     # compare (vA, 0)
    movl     rSELF,%ecx
    movl     $2,%eax              # assume branch not taken
    je   1f
    movswl   2(rPC),%eax           # fetch signed displacement
    movl     offThread_curHandlerTable(%ecx),rIBASE
1:
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_IF_LTZ: /* 0x3a */
/* File: x86/OP_IF_LTZ.S */
/* File: x86/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    cmpl     $0,(rFP,rINST,4)     # compare (vA, 0)
    movl     rSELF,%ecx
    movl     $2,%eax              # assume branch not taken
    jge   1f
    movswl   2(rPC),%eax           # fetch signed displacement
    movl     offThread_curHandlerTable(%ecx),rIBASE
1:
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_IF_GEZ: /* 0x3b */
/* File: x86/OP_IF_GEZ.S */
/* File: x86/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    cmpl     $0,(rFP,rINST,4)     # compare (vA, 0)
    movl     rSELF,%ecx
    movl     $2,%eax              # assume branch not taken
    jl   1f
    movswl   2(rPC),%eax           # fetch signed displacement
    movl     offThread_curHandlerTable(%ecx),rIBASE
1:
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_IF_GTZ: /* 0x3c */
/* File: x86/OP_IF_GTZ.S */
/* File: x86/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    cmpl     $0,(rFP,rINST,4)     # compare (vA, 0)
    movl     rSELF,%ecx
    movl     $2,%eax              # assume branch not taken
    jle   1f
    movswl   2(rPC),%eax           # fetch signed displacement
    movl     offThread_curHandlerTable(%ecx),rIBASE
1:
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_IF_LEZ: /* 0x3d */
/* File: x86/OP_IF_LEZ.S */
/* File: x86/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    cmpl     $0,(rFP,rINST,4)     # compare (vA, 0)
    movl     rSELF,%ecx
    movl     $2,%eax              # assume branch not taken
    jg   1f
    movswl   2(rPC),%eax           # fetch signed displacement
    movl     offThread_curHandlerTable(%ecx),rIBASE
1:
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT


/* ------------------------------ */
.L_OP_UNUSED_3E: /* 0x3e */
/* File: x86/OP_UNUSED_3E.S */
/* File: x86/unused.S */
    jmp     common_abort


/* ------------------------------ */
.L_OP_UNUSED_3F: /* 0x3f */
/* File: x86/OP_UNUSED_3F.S */
/* File: x86/unused.S */
    jmp     common_abort


/* ------------------------------ */
.L_OP_UNUSED_40: /* 0x40 */
/* File: x86/OP_UNUSED_40.S */
/* File: x86/unused.S */
    jmp     common_abort


/* ------------------------------ */
.L_OP_UNUSED_41: /* 0x41 */
/* File: x86/OP_UNUSED_41.S */
/* File: x86/unused.S */
    jmp     common_abort


/* ------------------------------ */
.L_OP_UNUSED_42: /* 0x42 */
/* File: x86/OP_UNUSED_42.S */
/* File: x86/unused.S */
    jmp     common_abort


/* ------------------------------ */
.L_OP_UNUSED_43: /* 0x43 */
/* File: x86/OP_UNUSED_43.S */
/* File: x86/unused.S */
    jmp     common_abort


/* ------------------------------ */
.L_OP_AGET: /* 0x44 */
/* File: x86/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects
                                        #    arrayObj in eax
                                        #    index in ecx
    movl     offArrayObject_contents(%eax,%ecx,4),%eax
.LOP_AGET_finish:
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG  %eax rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_AGET_WIDE: /* 0x45 */
/* File: x86/OP_AGET_WIDE.S */
    /*
     * Array get, 64 bits.  vAA <- vBB[vCC].
     *
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects
                                        #    arrayObj in eax
                                        #    index in ecx
    leal      offArrayObject_contents(%eax,%ecx,8),%eax
    movl      (%eax),%ecx
    movl      4(%eax),%eax
    SET_VREG_WORD %ecx rINST 0
    SET_VREG_WORD %eax rINST 1
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_AGET_OBJECT: /* 0x46 */
/* File: x86/OP_AGET_OBJECT.S */
/* File: x86/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects
                                        #    arrayObj in eax
                                        #    index in ecx
    movl     offArrayObject_contents(%eax,%ecx,4),%eax
.LOP_AGET_OBJECT_finish:
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG  %eax rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_AGET_BOOLEAN: /* 0x47 */
/* File: x86/OP_AGET_BOOLEAN.S */
/* File: x86/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects
                                        #    arrayObj in eax
                                        #    index in ecx
    movzbl     offArrayObject_contents(%eax,%ecx,1),%eax
.LOP_AGET_BOOLEAN_finish:
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG  %eax rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_AGET_BYTE: /* 0x48 */
/* File: x86/OP_AGET_BYTE.S */
/* File: x86/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects
                                        #    arrayObj in eax
                                        #    index in ecx
    movsbl     offArrayObject_contents(%eax,%ecx,1),%eax
.LOP_AGET_BYTE_finish:
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG  %eax rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_AGET_CHAR: /* 0x49 */
/* File: x86/OP_AGET_CHAR.S */
/* File: x86/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects
                                        #    arrayObj in eax
                                        #    index in ecx
    movzwl     offArrayObject_contents(%eax,%ecx,2),%eax
.LOP_AGET_CHAR_finish:
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG  %eax rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_AGET_SHORT: /* 0x4a */
/* File: x86/OP_AGET_SHORT.S */
/* File: x86/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects
                                        #    arrayObj in eax
                                        #    index in ecx
    movswl     offArrayObject_contents(%eax,%ecx,2),%eax
.LOP_AGET_SHORT_finish:
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG  %eax rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_APUT: /* 0x4b */
/* File: x86/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA
     *
     * for: aput, aput-object, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects:
                                        #   arrayObj in eax
                                        #   index in ecx
    leal      offArrayObject_contents(%eax,%ecx,4),%eax
.LOP_APUT_finish:
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    movl     rINST,(%eax)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_APUT_WIDE: /* 0x4c */
/* File: x86/OP_APUT_WIDE.S */
    /*
     * Array put, 64 bits.  vBB[vCC]<-vAA.
     *
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects:
                                        #   arrayObj in eax
                                        #   index in ecx
    leal      offArrayObject_contents(%eax,%ecx,8),%eax
    GET_VREG_WORD %ecx rINST 0
    GET_VREG_WORD rINST rINST 1
    movl      %ecx,(%eax)
    FETCH_INST_OPCODE 2 %ecx
    movl      rINST,4(%eax)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_APUT_OBJECT: /* 0x4d */
/* File: x86/OP_APUT_OBJECT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA
     *
     * for: aput, aput-object, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    GET_VREG_R  rINST rINST             # rINST<- vAA
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects
                                        #    arrayObj in eax
                                        #    index in ecx
    /* On entry:
     *   eax<- array object
     *   ecx<- index
     *   rINST<- vAA
     */
    leal      offArrayObject_contents(%eax,%ecx,4),%ecx
    testl     rINST,rINST                    # storing null reference?
    je        .LOP_APUT_OBJECT_skip_check
    SPILL_TMP1(%ecx)                         # save target address
    SPILL_TMP2(%eax)                         # save object head
    movl      offObject_clazz(%eax),%eax     # eax<- arrayObj->clazz
    movl      offObject_clazz(rINST),%ecx    # ecx<- obj->clazz
    movl      %eax,OUT_ARG1(%esp)
    movl      %ecx,OUT_ARG0(%esp)
    movl      %ecx,sReg0                     # store the two classes for later
    movl      %eax,sReg1
    SPILL(rIBASE)
    call      dvmCanPutArrayElement          # test object type vs. array type
    UNSPILL(rIBASE)
    UNSPILL_TMP1(%ecx)                       # recover target address
    testl     %eax,%eax
    movl      rSELF,%eax
    jne       .LOP_APUT_OBJECT_types_okay

    # The types don't match.  We need to throw an ArrayStoreException.
    EXPORT_PC
    movl      sReg0,%eax                     # restore the two classes...
    movl      %eax,OUT_ARG0(%esp)
    movl      sReg1,%ecx
    movl      %ecx,OUT_ARG1(%esp)
    call      dvmThrowArrayStoreExceptionIncompatibleElement # ...and throw
    jmp       common_exceptionThrown

.LOP_APUT_OBJECT_types_okay:
    movl      offThread_cardTable(%eax),%eax   # get card table base
    movl      rINST,(%ecx)                   # store into array
    UNSPILL_TMP2(rINST)                      # recover object head
    FETCH_INST_OPCODE 2 %ecx
    shrl      $GC_CARD_SHIFT,rINST          # object head to card number
    movb      %al,(%eax,rINST)               # mark card using object head
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_APUT_OBJECT_skip_check:
    movl      rINST,(%ecx)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_APUT_BOOLEAN: /* 0x4e */
/* File: x86/OP_APUT_BOOLEAN.S */
/* File: x86/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA
     *
     * for: aput, aput-object, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects:
                                        #   arrayObj in eax
                                        #   index in ecx
    leal      offArrayObject_contents(%eax,%ecx,1),%eax
.LOP_APUT_BOOLEAN_finish:
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    movb     rINSTbl,(%eax)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_APUT_BYTE: /* 0x4f */
/* File: x86/OP_APUT_BYTE.S */
/* File: x86/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA
     *
     * for: aput, aput-object, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects:
                                        #   arrayObj in eax
                                        #   index in ecx
    leal      offArrayObject_contents(%eax,%ecx,1),%eax
.LOP_APUT_BYTE_finish:
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    movb     rINSTbl,(%eax)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_APUT_CHAR: /* 0x50 */
/* File: x86/OP_APUT_CHAR.S */
/* File: x86/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA
     *
     * for: aput, aput-object, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects:
                                        #   arrayObj in eax
                                        #   index in ecx
    leal      offArrayObject_contents(%eax,%ecx,2),%eax
.LOP_APUT_CHAR_finish:
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    movw     rINSTw,(%eax)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_APUT_SHORT: /* 0x51 */
/* File: x86/OP_APUT_SHORT.S */
/* File: x86/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA
     *
     * for: aput, aput-object, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects:
                                        #   arrayObj in eax
                                        #   index in ecx
    leal      offArrayObject_contents(%eax,%ecx,2),%eax
.LOP_APUT_SHORT_finish:
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    movw     rINSTw,(%eax)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_IGET: /* 0x52 */
/* File: x86/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)                               # preserve rIBASE
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IGET_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)               # needed by dvmResolveInstField
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           #  returns InstrField ptr
    jne     .LOP_IGET_finish
    jmp     common_exceptionThrown

.LOP_IGET_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    movl   (%ecx,%eax,1),%ecx                  # ecx<- obj.field (8/16/32 bits)
    FETCH_INST_OPCODE 2 %eax
    UNSPILL(rIBASE)
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax

/* ------------------------------ */
.L_OP_IGET_WIDE: /* 0x53 */
/* File: x86/OP_IGET_WIDE.S */
    /*
     * 64-bit instance field get.
     *
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)                               # preserve rIBASE
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IGET_WIDE_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)               # for dvmResolveInstField
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save objpointer across call
    movl    rPC,OUT_ARG0(%esp)                  # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           # returns InstrField ptr
    jne     .LOP_IGET_WIDE_finish
    jmp     common_exceptionThrown

.LOP_IGET_WIDE_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    leal    (%ecx,%eax,1),%eax                  # eax<- address of field
    movl    (%eax),%ecx                         # ecx<- lsw
    movl    4(%eax),%eax                        # eax<- msw
    SET_VREG_WORD %ecx rINST 0
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)                             # restore rIBASE
    SET_VREG_WORD %eax rINST 1
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_IGET_OBJECT: /* 0x54 */
/* File: x86/OP_IGET_OBJECT.S */
/* File: x86/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)                               # preserve rIBASE
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IGET_OBJECT_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)               # needed by dvmResolveInstField
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           #  returns InstrField ptr
    jne     .LOP_IGET_OBJECT_finish
    jmp     common_exceptionThrown

.LOP_IGET_OBJECT_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    movl   (%ecx,%eax,1),%ecx                  # ecx<- obj.field (8/16/32 bits)
    FETCH_INST_OPCODE 2 %eax
    UNSPILL(rIBASE)
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_IGET_BOOLEAN: /* 0x55 */
/* File: x86/OP_IGET_BOOLEAN.S */
/* File: x86/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)                               # preserve rIBASE
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IGET_BOOLEAN_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)               # needed by dvmResolveInstField
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           #  returns InstrField ptr
    jne     .LOP_IGET_BOOLEAN_finish
    jmp     common_exceptionThrown

.LOP_IGET_BOOLEAN_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    movzbl   (%ecx,%eax,1),%ecx                  # ecx<- obj.field (8/16/32 bits)
    FETCH_INST_OPCODE 2 %eax
    UNSPILL(rIBASE)
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_IGET_BYTE: /* 0x56 */
/* File: x86/OP_IGET_BYTE.S */
/* File: x86/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)                               # preserve rIBASE
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IGET_BYTE_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)               # needed by dvmResolveInstField
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           #  returns InstrField ptr
    jne     .LOP_IGET_BYTE_finish
    jmp     common_exceptionThrown

.LOP_IGET_BYTE_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    movsbl   (%ecx,%eax,1),%ecx                  # ecx<- obj.field (8/16/32 bits)
    FETCH_INST_OPCODE 2 %eax
    UNSPILL(rIBASE)
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_IGET_CHAR: /* 0x57 */
/* File: x86/OP_IGET_CHAR.S */
/* File: x86/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)                               # preserve rIBASE
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IGET_CHAR_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)               # needed by dvmResolveInstField
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           #  returns InstrField ptr
    jne     .LOP_IGET_CHAR_finish
    jmp     common_exceptionThrown

.LOP_IGET_CHAR_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    movzwl   (%ecx,%eax,1),%ecx                  # ecx<- obj.field (8/16/32 bits)
    FETCH_INST_OPCODE 2 %eax
    UNSPILL(rIBASE)
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_IGET_SHORT: /* 0x58 */
/* File: x86/OP_IGET_SHORT.S */
/* File: x86/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)                               # preserve rIBASE
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IGET_SHORT_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)               # needed by dvmResolveInstField
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           #  returns InstrField ptr
    jne     .LOP_IGET_SHORT_finish
    jmp     common_exceptionThrown

.LOP_IGET_SHORT_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    movswl   (%ecx,%eax,1),%ecx                  # ecx<- obj.field (8/16/32 bits)
    FETCH_INST_OPCODE 2 %eax
    UNSPILL(rIBASE)
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_IPUT: /* 0x59 */
/* File: x86/OP_IPUT.S */

    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL   (rIBASE)
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IPUT_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           # returns InstrField ptr
    jne     .LOP_IPUT_finish
    jmp     common_exceptionThrown

.LOP_IPUT_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    GET_VREG_R rINST rINST                       # rINST<- v[A]
    movl    offInstField_byteOffset(%eax),%eax   # eax<- byte offset of field
    testl   %ecx,%ecx                            # object null?
    je      common_errNullObject                 # object was null
    movl   rINST,(%ecx,%eax,1)            # obj.field <- v[A](8/16/32 bits)
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_IPUT_WIDE: /* 0x5a */
/* File: x86/OP_IPUT_WIDE.S */
    /*
     * 64-bit instance field put.
     *
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IPUT_WIDE_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           #  ... which returns InstrField ptr
    jne     .LOP_IPUT_WIDE_finish
    jmp     common_exceptionThrown

.LOP_IPUT_WIDE_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rIBASE is scratch, but needs to be unspilled
     *   rINST holds A
     */
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    leal    (%ecx,%eax,1),%eax                  # eax<- address of field
    GET_VREG_WORD %ecx rINST 0                  # ecx<- lsw
    GET_VREG_WORD rINST rINST 1                 # rINST<- msw
    movl    rINST,4(%eax)
    movl    %ecx,(%eax)
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_IPUT_OBJECT: /* 0x5b */
/* File: x86/OP_IPUT_OBJECT.S */
    /*
     * Object field put.
     *
     * for: iput-object
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                  # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IPUT_OBJECT_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           # returns InstrField ptr
    jne     .LOP_IPUT_OBJECT_finish
    jmp     common_exceptionThrown

.LOP_IPUT_OBJECT_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rIBASE is scratch, but needs to be unspilled
     *   rINST holds A
     */
    GET_VREG_R rINST rINST                      # rINST<- v[A]
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    movl    rINST,(%ecx,%eax)      # obj.field <- v[A](8/16/32 bits)
    movl    rSELF,%eax
    testl   rINST,rINST                         # stored a NULL?
    movl    offThread_cardTable(%eax),%eax      # get card table base
    je      1f                                  # skip card mark if null store
    shrl    $GC_CARD_SHIFT,%ecx                # object head to card number
    movb    %al,(%eax,%ecx)                     # mark card using object head
1:
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_IPUT_BOOLEAN: /* 0x5c */
/* File: x86/OP_IPUT_BOOLEAN.S */
/* File: x86/OP_IPUT.S */

    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL   (rIBASE)
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IPUT_BOOLEAN_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           # returns InstrField ptr
    jne     .LOP_IPUT_BOOLEAN_finish
    jmp     common_exceptionThrown

.LOP_IPUT_BOOLEAN_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    GET_VREG_R rINST rINST                       # rINST<- v[A]
    movl    offInstField_byteOffset(%eax),%eax   # eax<- byte offset of field
    testl   %ecx,%ecx                            # object null?
    je      common_errNullObject                 # object was null
    movb   rINSTbl,(%ecx,%eax,1)            # obj.field <- v[A](8/16/32 bits)
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_IPUT_BYTE: /* 0x5d */
/* File: x86/OP_IPUT_BYTE.S */
/* File: x86/OP_IPUT.S */

    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL   (rIBASE)
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IPUT_BYTE_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           # returns InstrField ptr
    jne     .LOP_IPUT_BYTE_finish
    jmp     common_exceptionThrown

.LOP_IPUT_BYTE_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    GET_VREG_R rINST rINST                       # rINST<- v[A]
    movl    offInstField_byteOffset(%eax),%eax   # eax<- byte offset of field
    testl   %ecx,%ecx                            # object null?
    je      common_errNullObject                 # object was null
    movb   rINSTbl,(%ecx,%eax,1)            # obj.field <- v[A](8/16/32 bits)
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_IPUT_CHAR: /* 0x5e */
/* File: x86/OP_IPUT_CHAR.S */
/* File: x86/OP_IPUT.S */

    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL   (rIBASE)
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IPUT_CHAR_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           # returns InstrField ptr
    jne     .LOP_IPUT_CHAR_finish
    jmp     common_exceptionThrown

.LOP_IPUT_CHAR_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    GET_VREG_R rINST rINST                       # rINST<- v[A]
    movl    offInstField_byteOffset(%eax),%eax   # eax<- byte offset of field
    testl   %ecx,%ecx                            # object null?
    je      common_errNullObject                 # object was null
    movw   rINSTw,(%ecx,%eax,1)            # obj.field <- v[A](8/16/32 bits)
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_IPUT_SHORT: /* 0x5f */
/* File: x86/OP_IPUT_SHORT.S */
/* File: x86/OP_IPUT.S */

    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL   (rIBASE)
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IPUT_SHORT_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           # returns InstrField ptr
    jne     .LOP_IPUT_SHORT_finish
    jmp     common_exceptionThrown

.LOP_IPUT_SHORT_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    GET_VREG_R rINST rINST                       # rINST<- v[A]
    movl    offInstField_byteOffset(%eax),%eax   # eax<- byte offset of field
    testl   %ecx,%ecx                            # object null?
    je      common_errNullObject                 # object was null
    movw   rINSTw,(%ecx,%eax,1)            # obj.field <- v[A](8/16/32 bits)
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SGET: /* 0x60 */
/* File: x86/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SGET_resolve                # if not, make it so
.LOP_SGET_finish:     # field ptr in eax
    movl      offStaticField_value(%eax),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SGET_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SGET_finish                 # success, continue

/* ------------------------------ */
.L_OP_SGET_WIDE: /* 0x61 */
/* File: x86/OP_SGET_WIDE.S */
    /*
     * 64-bit SGET handler.
     *
     */
    /* sget-wide vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SGET_WIDE_resolve                # if not, make it so
.LOP_SGET_WIDE_finish:     # field ptr in eax
    movl      offStaticField_value(%eax),%ecx    # ecx<- lsw
    movl      4+offStaticField_value(%eax),%eax  # eax<- msw
    SET_VREG_WORD %ecx rINST 0
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG_WORD %eax rINST 1
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SGET_WIDE_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SGET_WIDE_finish                 # success, continue

/* ------------------------------ */
.L_OP_SGET_OBJECT: /* 0x62 */
/* File: x86/OP_SGET_OBJECT.S */
/* File: x86/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SGET_OBJECT_resolve                # if not, make it so
.LOP_SGET_OBJECT_finish:     # field ptr in eax
    movl      offStaticField_value(%eax),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SGET_OBJECT_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SGET_OBJECT_finish                 # success, continue


/* ------------------------------ */
.L_OP_SGET_BOOLEAN: /* 0x63 */
/* File: x86/OP_SGET_BOOLEAN.S */
/* File: x86/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SGET_BOOLEAN_resolve                # if not, make it so
.LOP_SGET_BOOLEAN_finish:     # field ptr in eax
    movl      offStaticField_value(%eax),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SGET_BOOLEAN_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SGET_BOOLEAN_finish                 # success, continue


/* ------------------------------ */
.L_OP_SGET_BYTE: /* 0x64 */
/* File: x86/OP_SGET_BYTE.S */
/* File: x86/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SGET_BYTE_resolve                # if not, make it so
.LOP_SGET_BYTE_finish:     # field ptr in eax
    movl      offStaticField_value(%eax),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SGET_BYTE_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SGET_BYTE_finish                 # success, continue


/* ------------------------------ */
.L_OP_SGET_CHAR: /* 0x65 */
/* File: x86/OP_SGET_CHAR.S */
/* File: x86/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SGET_CHAR_resolve                # if not, make it so
.LOP_SGET_CHAR_finish:     # field ptr in eax
    movl      offStaticField_value(%eax),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SGET_CHAR_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SGET_CHAR_finish                 # success, continue


/* ------------------------------ */
.L_OP_SGET_SHORT: /* 0x66 */
/* File: x86/OP_SGET_SHORT.S */
/* File: x86/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SGET_SHORT_resolve                # if not, make it so
.LOP_SGET_SHORT_finish:     # field ptr in eax
    movl      offStaticField_value(%eax),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SGET_SHORT_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SGET_SHORT_finish                 # success, continue


/* ------------------------------ */
.L_OP_SPUT: /* 0x67 */
/* File: x86/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SPUT_resolve                # if not, make it so
.LOP_SPUT_finish:     # field ptr in eax
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    movl      rINST,offStaticField_value(%eax)
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SPUT_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx        # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SPUT_finish                 # success, continue
/* ------------------------------ */
.L_OP_SPUT_WIDE: /* 0x68 */
/* File: x86/OP_SPUT_WIDE.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-object, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SPUT_WIDE_resolve                # if not, make it so
.LOP_SPUT_WIDE_finish:     # field ptr in eax
    GET_VREG_WORD %ecx rINST 0                  # rINST<- lsw
    GET_VREG_WORD rINST rINST 1                 # ecx<- msw
    movl      %ecx,offStaticField_value(%eax)
    FETCH_INST_OPCODE 2 %ecx
    movl      rINST,4+offStaticField_value(%eax)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SPUT_WIDE_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SPUT_WIDE_finish                 # success, continue

/* ------------------------------ */
.L_OP_SPUT_OBJECT: /* 0x69 */
/* File: x86/OP_SPUT_OBJECT.S */
    /*
     * SPUT object handler.
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SPUT_OBJECT_resolve                # if not, make it so
.LOP_SPUT_OBJECT_finish:                              # field ptr in eax
    movzbl    rINSTbl,%ecx                       # ecx<- AA
    GET_VREG_R  %ecx %ecx
    movl      %ecx,offStaticField_value(%eax)    # do the store
    testl     %ecx,%ecx                          # stored null object ptr?
    je        1f                                 # skip card mark if null
    movl      rSELF,%ecx
    movl      offField_clazz(%eax),%eax          # eax<- method->clazz
    movl      offThread_cardTable(%ecx),%ecx       # get card table base
    shrl      $GC_CARD_SHIFT,%eax               # head to card number
    movb      %cl,(%ecx,%eax)                    # mark card
1:
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_SPUT_OBJECT_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SPUT_OBJECT_finish                 # success, continue

/* ------------------------------ */
.L_OP_SPUT_BOOLEAN: /* 0x6a */
/* File: x86/OP_SPUT_BOOLEAN.S */
/* File: x86/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SPUT_BOOLEAN_resolve                # if not, make it so
.LOP_SPUT_BOOLEAN_finish:     # field ptr in eax
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    movl      rINST,offStaticField_value(%eax)
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SPUT_BOOLEAN_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx        # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SPUT_BOOLEAN_finish                 # success, continue

/* ------------------------------ */
.L_OP_SPUT_BYTE: /* 0x6b */
/* File: x86/OP_SPUT_BYTE.S */
/* File: x86/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SPUT_BYTE_resolve                # if not, make it so
.LOP_SPUT_BYTE_finish:     # field ptr in eax
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    movl      rINST,offStaticField_value(%eax)
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SPUT_BYTE_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx        # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SPUT_BYTE_finish                 # success, continue

/* ------------------------------ */
.L_OP_SPUT_CHAR: /* 0x6c */
/* File: x86/OP_SPUT_CHAR.S */
/* File: x86/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SPUT_CHAR_resolve                # if not, make it so
.LOP_SPUT_CHAR_finish:     # field ptr in eax
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    movl      rINST,offStaticField_value(%eax)
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SPUT_CHAR_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx        # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SPUT_CHAR_finish                 # success, continue

/* ------------------------------ */
.L_OP_SPUT_SHORT: /* 0x6d */
/* File: x86/OP_SPUT_SHORT.S */
/* File: x86/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SPUT_SHORT_resolve                # if not, make it so
.LOP_SPUT_SHORT_finish:     # field ptr in eax
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    movl      rINST,offStaticField_value(%eax)
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SPUT_SHORT_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx        # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SPUT_SHORT_finish                 # success, continue

/* ------------------------------ */
.L_OP_INVOKE_VIRTUAL: /* 0x6e */
/* File: x86/OP_INVOKE_VIRTUAL.S */

    /*
     * Handle a virtual method call.
     *
     * for: invoke-virtual, invoke-virtual/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,%eax
    movzwl    2(rPC),%ecx                 # ecx<- BBBB
    movl      offThread_methodClassDex(%eax),%eax  # eax<- pDvmDex
    EXPORT_PC
    movl      offDvmDex_pResMethods(%eax),%eax   # eax<- pDvmDex->pResMethods
    movl      (%eax,%ecx,4),%eax          # eax<- resolved baseMethod
    testl     %eax,%eax                   # already resolved?
    jne       .LOP_INVOKE_VIRTUAL_continue        # yes, continue
    movl      rSELF,%eax
    movl      %ecx,OUT_ARG1(%esp)         # arg1<- ref
    movl      offThread_method(%eax),%eax   # eax<- self->method
    movl      offMethod_clazz(%eax),%eax  # ecx<- method->clazz
    movl      %eax,OUT_ARG0(%esp)         # arg0<- clazz
    movl      $METHOD_VIRTUAL,OUT_ARG2(%esp) # arg2<- flags
    call      dvmResolveMethod            # eax<- call(clazz, ref, flags)
    testl     %eax,%eax                   # got null?
    jne       .LOP_INVOKE_VIRTUAL_continue        # no, continue
    jmp       common_exceptionThrown      # yes, handle exception

    /* At this point:
     *   eax = resolved base method
     *   ecx = scratch
     */
.LOP_INVOKE_VIRTUAL_continue:
    movzwl    4(rPC),%ecx               # ecx<- GFED or CCCC
    .if       (!0)
    andl      $0xf,%ecx                # ecx<- D (or stays CCCC)
    .endif
    GET_VREG_R  %ecx %ecx               # ecx<- "this"
    movzwl    offMethod_methodIndex(%eax),%eax  # eax<- baseMethod->methodIndex
    testl     %ecx,%ecx                 # null this?
    je        common_errNullObject      # go if so
    movl      offObject_clazz(%ecx),%edx  # edx<- thisPtr->clazz
    movl      offClassObject_vtable(%edx),%edx # edx<- thisPtr->clazz->vtable
    movl      (%edx,%eax,4),%eax        # eax<- vtable[methodIndex]
    jmp       common_invokeMethodNoRange

/* ------------------------------ */
.L_OP_INVOKE_SUPER: /* 0x6f */
/* File: x86/OP_INVOKE_SUPER.S */
    /*
     * Handle a "super" method call.
     *
     * for: invoke-super, invoke-super/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,rINST
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offThread_methodClassDex(rINST),%ecx # ecx<- pDvmDex
    EXPORT_PC
    movl      offDvmDex_pResMethods(%ecx),%ecx # ecx<- pDvmDex->pResMethods
    movl      (%ecx,%eax,4),%ecx        # ecx<- resolved baseMethod
    movl      offThread_method(rINST),%eax # eax<- method
    movzwl    4(rPC),rINST              # rINST<- GFED or CCCC
    .if       (!0)
    andl      $0xf,rINST               # rINST<- D (or stays CCCC)
    .endif
    GET_VREG_R  %edx rINST             # %edx<- "this" ptr
    testl     %edx,%edx                # null "this"?
    SPILL_TMP1(%edx)
    je        common_errNullObject      # yes, throw
    movl      offMethod_clazz(%eax),%eax # eax<- method->clazz
    testl     %ecx,%ecx                 # already resolved?
    je       .LOP_INVOKE_SUPER_resolve
    /*
     * At this point:
     *  ecx = resolved base method [r0]
     *  eax = method->clazz [r9]
     */
.LOP_INVOKE_SUPER_continue:
    movl    offClassObject_super(%eax),%eax   # eax<- method->clazz->super
    movzwl  offMethod_methodIndex(%ecx),%edx  # edx<- baseMthod->methodIndex
    cmpl    offClassObject_vtableCount(%eax),%edx # compare(methodIndex,vtableCount)
    jae     .LOP_INVOKE_SUPER_nsm           # method not present in superclass
    movl    offClassObject_vtable(%eax),%eax   # eax<- ...clazz->super->vtable
    movl    (%eax,%edx,4),%eax        # eax<- vtable[methodIndex]
    UNSPILL_TMP1(%edx)
    movl    %edx, %ecx
    jmp     common_invokeMethodNoRange


    /* At this point:
     * ecx = null (needs to be resolved base method)
     * eax = method->clazz
    */
.LOP_INVOKE_SUPER_resolve:
    SPILL_TMP2(%eax)                    # method->clazz
    movl    %eax,OUT_ARG0(%esp)         # arg0<- method->clazz
    movzwl  2(rPC),%ecx                 # ecx<- BBBB
    movl    $METHOD_VIRTUAL,OUT_ARG2(%esp)  # arg2<- resolver method type
    movl    %ecx,OUT_ARG1(%esp)         # arg1<- ref
    call    dvmResolveMethod            # eax<- call(clazz, ref, flags)
    testl   %eax,%eax                   # got null?
    movl    %eax,%ecx                   # ecx<- resolved base method
    UNSPILL_TMP2(%eax)                  # restore method->clazz
    jne     .LOP_INVOKE_SUPER_continue        # good to go - continue
    jmp     common_exceptionThrown      # handle exception

    /*
     * Throw a NoSuchMethodError with the method name as the message.
     *  ecx = resolved base method
     */
.LOP_INVOKE_SUPER_nsm:
    movl    offMethod_name(%ecx),%eax
    jmp     common_errNoSuchMethod

/* ------------------------------ */
.L_OP_INVOKE_DIRECT: /* 0x70 */
/* File: x86/OP_INVOKE_DIRECT.S */
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
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax              # eax<- BBBB
    movl      offThread_methodClassDex(%ecx),%ecx # ecx<- pDvmDex
    EXPORT_PC
    movl      offDvmDex_pResMethods(%ecx),%ecx  # ecx<- pDvmDex->pResMethods
    movzwl    4(rPC),rIBASE            # rIBASE<- GFED or CCCC
    movl      (%ecx,%eax,4),%eax       # eax<- resolved methodToCall
    .if       (!0)
    andl      $0xf,rIBASE             # rIBASE<- D (or stays CCCC)
    .endif
    testl     %eax,%eax                # already resolved?
    GET_VREG_R  %ecx rIBASE            # ecx<- "this" ptr
    je        .LOP_INVOKE_DIRECT_resolve      # not resolved, do it now
.LOP_INVOKE_DIRECT_finish:
    testl     %ecx,%ecx                # null "this"?
    jne       common_invokeMethodNoRange  # no, continue on
    jmp       common_errNullObject

    /*
     * On entry:
     *   TMP_SPILL  <- "this" register
     * Things a bit ugly on this path, but it's the less
     * frequent one.  We'll have to do some reloading.
     */
.LOP_INVOKE_DIRECT_resolve:
     SPILL_TMP1(%ecx)
     movl     rSELF,%ecx
     movl     offThread_method(%ecx),%ecx  # ecx<- self->method
     movzwl   2(rPC),%eax      # reference (BBBB or CCCC)
     movl     offMethod_clazz(%ecx),%ecx # ecx<- method->clazz
     movl     $METHOD_DIRECT,OUT_ARG2(%esp)
     movl     %eax,OUT_ARG1(%esp)
     movl     %ecx,OUT_ARG0(%esp)
     call     dvmResolveMethod # eax<- call(clazz, ref, flags)
     UNSPILL_TMP1(%ecx)
     testl    %eax,%eax
     jne      .LOP_INVOKE_DIRECT_finish
     jmp      common_exceptionThrown

/* ------------------------------ */
.L_OP_INVOKE_STATIC: /* 0x71 */
/* File: x86/OP_INVOKE_STATIC.S */
    /*
     * Handle a static method call.
     *
     * for: invoke-static, invoke-static/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offThread_methodClassDex(%ecx),%ecx # ecx<- pDvmDex
    EXPORT_PC
    movl      offDvmDex_pResMethods(%ecx),%ecx  # ecx<- pDvmDex->pResMethods
#if defined(WITH_JIT)
    movl     %edx, TMP_SPILL1(%ebp)
    lea      (%ecx,%eax,4), %edx
    movl     %edx, TMP_SPILL2(%ebp)
    movl     TMP_SPILL1(%ebp), %edx
#endif
    movl      (%ecx,%eax,4),%eax        # eax<- resolved methodToCall
    movl      $0, %ecx                 # make "this" null
    testl     %eax,%eax
    jne       common_invokeMethodNoRange

    movl      rSELF,%ecx
    movl      offThread_method(%ecx),%ecx # ecx<- self->method
    movzwl    2(rPC),%eax
    movl      offMethod_clazz(%ecx),%ecx# ecx<- method->clazz
    movl      %eax,OUT_ARG1(%esp)       # arg1<- BBBB
    movl      %ecx,OUT_ARG0(%esp)       # arg0<- clazz
    movl      $METHOD_STATIC,%eax
    movl      %eax,OUT_ARG2(%esp)       # arg2<- flags
    SPILL(rIBASE)
    call      dvmResolveMethod          # call(clazz,ref,flags)
    UNSPILL(rIBASE)
    testl     %eax,%eax                 # got null?
#if defined(WITH_JIT)
    movl      TMP_SPILL1(%ebp), %edx
    movl      rSELF,%ecx
    movzwl    offThread_subMode(%ecx), %ecx
    je        common_exceptionThrown    # null, handle exception
    andl      $kSubModeJitTraceBuild, %ecx # is trace under construction?
    movl      $0, %ecx                 # make "this" null
    je        common_invokeMethodNoRange # no (%eax=method, %ecx="this")
    movl      TMP_SPILL2(%ebp), %edx
    cmpl      $0, (%edx)                  # finished resolving
    movl      TMP_SPILL1(%ebp), %edx
    jne        common_invokeMethodNoRange # yes (%eax=method, %ecx="this")
    movl      rSELF, %edx
    movl      %edx, OUT_ARG0(%esp)
    movl      rPC, OUT_ARG1(%esp)
    movl      %eax, TMP_SPILL2(%ebp)
    movl      %ecx, TMP_SPILL3(%ebp)
    SPILL(rIBASE)
    call      dvmJitEndTraceSelect
    UNSPILL(rIBASE)
    movl      TMP_SPILL1(%ebp), %edx
    movl      TMP_SPILL2(%ebp), %eax
    movl      TMP_SPILL3(%ebp), %ecx
    jmp       common_invokeMethodNoRange
#else
    movl      $0, %ecx                 # make "this" null
    jne       common_invokeMethodNoRange
    jmp       common_exceptionThrown
#endif


/* ------------------------------ */
.L_OP_INVOKE_INTERFACE: /* 0x72 */
/* File: x86/OP_INVOKE_INTERFACE.S */
    /*
     * Handle an interface method call.
     *
     * for: invoke-interface, invoke-interface/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movzwl     4(rPC),%eax              # eax<- FEDC or CCCC
    movl       rSELF,%ecx
    .if        (!0)
    andl       $0xf,%eax               # eax<- C (or stays CCCC)
    .endif
    GET_VREG_R   %eax %eax              # eax<- "this"
    EXPORT_PC
    testl      %eax,%eax                # null this?
    je         common_errNullObject     # yes, fail
    movl       %eax, TMP_SPILL1(%ebp)
    movl       offObject_clazz(%eax),%eax# eax<- thisPtr->clazz
    movl       %eax,OUT_ARG0(%esp)                 # arg0<- class
    movl       offThread_methodClassDex(%ecx),%eax   # eax<- methodClassDex
    movl       offThread_method(%ecx),%ecx           # ecx<- method
    movl       %eax,OUT_ARG3(%esp)                 # arg3<- dex
    movzwl     2(rPC),%eax                         # eax<- BBBB
    movl       %ecx,OUT_ARG2(%esp)                 # arg2<- method
    movl       %eax,OUT_ARG1(%esp)                 # arg1<- BBBB
    call       dvmFindInterfaceMethodInCache # eax<- call(class, ref, method, dex)
    testl      %eax,%eax
    je         common_exceptionThrown
    movl       TMP_SPILL1(%ebp), %ecx
    jmp        common_invokeMethodNoRange

/* ------------------------------ */
.L_OP_UNUSED_73: /* 0x73 */
/* File: x86/OP_UNUSED_73.S */
/* File: x86/unused.S */
    jmp     common_abort


/* ------------------------------ */
.L_OP_INVOKE_VIRTUAL_RANGE: /* 0x74 */
/* File: x86/OP_INVOKE_VIRTUAL_RANGE.S */
/* File: x86/OP_INVOKE_VIRTUAL.S */

    /*
     * Handle a virtual method call.
     *
     * for: invoke-virtual, invoke-virtual/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,%eax
    movzwl    2(rPC),%ecx                 # ecx<- BBBB
    movl      offThread_methodClassDex(%eax),%eax  # eax<- pDvmDex
    EXPORT_PC
    movl      offDvmDex_pResMethods(%eax),%eax   # eax<- pDvmDex->pResMethods
    movl      (%eax,%ecx,4),%eax          # eax<- resolved baseMethod
    testl     %eax,%eax                   # already resolved?
    jne       .LOP_INVOKE_VIRTUAL_RANGE_continue        # yes, continue
    movl      rSELF,%eax
    movl      %ecx,OUT_ARG1(%esp)         # arg1<- ref
    movl      offThread_method(%eax),%eax   # eax<- self->method
    movl      offMethod_clazz(%eax),%eax  # ecx<- method->clazz
    movl      %eax,OUT_ARG0(%esp)         # arg0<- clazz
    movl      $METHOD_VIRTUAL,OUT_ARG2(%esp) # arg2<- flags
    call      dvmResolveMethod            # eax<- call(clazz, ref, flags)
    testl     %eax,%eax                   # got null?
    jne       .LOP_INVOKE_VIRTUAL_RANGE_continue        # no, continue
    jmp       common_exceptionThrown      # yes, handle exception

    /* At this point:
     *   eax = resolved base method
     *   ecx = scratch
     */
.LOP_INVOKE_VIRTUAL_RANGE_continue:
    movzwl    4(rPC),%ecx               # ecx<- GFED or CCCC
    .if       (!1)
    andl      $0xf,%ecx                # ecx<- D (or stays CCCC)
    .endif
    GET_VREG_R  %ecx %ecx               # ecx<- "this"
    movzwl    offMethod_methodIndex(%eax),%eax  # eax<- baseMethod->methodIndex
    testl     %ecx,%ecx                 # null this?
    je        common_errNullObject      # go if so
    movl      offObject_clazz(%ecx),%edx  # edx<- thisPtr->clazz
    movl      offClassObject_vtable(%edx),%edx # edx<- thisPtr->clazz->vtable
    movl      (%edx,%eax,4),%eax        # eax<- vtable[methodIndex]
    jmp       common_invokeMethodRange


/* ------------------------------ */
.L_OP_INVOKE_SUPER_RANGE: /* 0x75 */
/* File: x86/OP_INVOKE_SUPER_RANGE.S */
/* File: x86/OP_INVOKE_SUPER.S */
    /*
     * Handle a "super" method call.
     *
     * for: invoke-super, invoke-super/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,rINST
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offThread_methodClassDex(rINST),%ecx # ecx<- pDvmDex
    EXPORT_PC
    movl      offDvmDex_pResMethods(%ecx),%ecx # ecx<- pDvmDex->pResMethods
    movl      (%ecx,%eax,4),%ecx        # ecx<- resolved baseMethod
    movl      offThread_method(rINST),%eax # eax<- method
    movzwl    4(rPC),rINST              # rINST<- GFED or CCCC
    .if       (!1)
    andl      $0xf,rINST               # rINST<- D (or stays CCCC)
    .endif
    GET_VREG_R  %edx rINST             # %edx<- "this" ptr
    testl     %edx,%edx                # null "this"?
    SPILL_TMP1(%edx)
    je        common_errNullObject      # yes, throw
    movl      offMethod_clazz(%eax),%eax # eax<- method->clazz
    testl     %ecx,%ecx                 # already resolved?
    je       .LOP_INVOKE_SUPER_RANGE_resolve
    /*
     * At this point:
     *  ecx = resolved base method [r0]
     *  eax = method->clazz [r9]
     */
.LOP_INVOKE_SUPER_RANGE_continue:
    movl    offClassObject_super(%eax),%eax   # eax<- method->clazz->super
    movzwl  offMethod_methodIndex(%ecx),%edx  # edx<- baseMthod->methodIndex
    cmpl    offClassObject_vtableCount(%eax),%edx # compare(methodIndex,vtableCount)
    jae     .LOP_INVOKE_SUPER_RANGE_nsm           # method not present in superclass
    movl    offClassObject_vtable(%eax),%eax   # eax<- ...clazz->super->vtable
    movl    (%eax,%edx,4),%eax        # eax<- vtable[methodIndex]
    UNSPILL_TMP1(%edx)
    movl    %edx, %ecx
    jmp     common_invokeMethodRange


    /* At this point:
     * ecx = null (needs to be resolved base method)
     * eax = method->clazz
    */
.LOP_INVOKE_SUPER_RANGE_resolve:
    SPILL_TMP2(%eax)                    # method->clazz
    movl    %eax,OUT_ARG0(%esp)         # arg0<- method->clazz
    movzwl  2(rPC),%ecx                 # ecx<- BBBB
    movl    $METHOD_VIRTUAL,OUT_ARG2(%esp)  # arg2<- resolver method type
    movl    %ecx,OUT_ARG1(%esp)         # arg1<- ref
    call    dvmResolveMethod            # eax<- call(clazz, ref, flags)
    testl   %eax,%eax                   # got null?
    movl    %eax,%ecx                   # ecx<- resolved base method
    UNSPILL_TMP2(%eax)                  # restore method->clazz
    jne     .LOP_INVOKE_SUPER_RANGE_continue        # good to go - continue
    jmp     common_exceptionThrown      # handle exception

    /*
     * Throw a NoSuchMethodError with the method name as the message.
     *  ecx = resolved base method
     */
.LOP_INVOKE_SUPER_RANGE_nsm:
    movl    offMethod_name(%ecx),%eax
    jmp     common_errNoSuchMethod


/* ------------------------------ */
.L_OP_INVOKE_DIRECT_RANGE: /* 0x76 */
/* File: x86/OP_INVOKE_DIRECT_RANGE.S */
/* File: x86/OP_INVOKE_DIRECT.S */
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
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax              # eax<- BBBB
    movl      offThread_methodClassDex(%ecx),%ecx # ecx<- pDvmDex
    EXPORT_PC
    movl      offDvmDex_pResMethods(%ecx),%ecx  # ecx<- pDvmDex->pResMethods
    movzwl    4(rPC),rIBASE            # rIBASE<- GFED or CCCC
    movl      (%ecx,%eax,4),%eax       # eax<- resolved methodToCall
    .if       (!1)
    andl      $0xf,rIBASE             # rIBASE<- D (or stays CCCC)
    .endif
    testl     %eax,%eax                # already resolved?
    GET_VREG_R  %ecx rIBASE            # ecx<- "this" ptr
    je        .LOP_INVOKE_DIRECT_RANGE_resolve      # not resolved, do it now
.LOP_INVOKE_DIRECT_RANGE_finish:
    testl     %ecx,%ecx                # null "this"?
    jne       common_invokeMethodRange  # no, continue on
    jmp       common_errNullObject

    /*
     * On entry:
     *   TMP_SPILL  <- "this" register
     * Things a bit ugly on this path, but it's the less
     * frequent one.  We'll have to do some reloading.
     */
.LOP_INVOKE_DIRECT_RANGE_resolve:
     SPILL_TMP1(%ecx)
     movl     rSELF,%ecx
     movl     offThread_method(%ecx),%ecx  # ecx<- self->method
     movzwl   2(rPC),%eax      # reference (BBBB or CCCC)
     movl     offMethod_clazz(%ecx),%ecx # ecx<- method->clazz
     movl     $METHOD_DIRECT,OUT_ARG2(%esp)
     movl     %eax,OUT_ARG1(%esp)
     movl     %ecx,OUT_ARG0(%esp)
     call     dvmResolveMethod # eax<- call(clazz, ref, flags)
     UNSPILL_TMP1(%ecx)
     testl    %eax,%eax
     jne      .LOP_INVOKE_DIRECT_RANGE_finish
     jmp      common_exceptionThrown


/* ------------------------------ */
.L_OP_INVOKE_STATIC_RANGE: /* 0x77 */
/* File: x86/OP_INVOKE_STATIC_RANGE.S */
/* File: x86/OP_INVOKE_STATIC.S */
    /*
     * Handle a static method call.
     *
     * for: invoke-static, invoke-static/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offThread_methodClassDex(%ecx),%ecx # ecx<- pDvmDex
    EXPORT_PC
    movl      offDvmDex_pResMethods(%ecx),%ecx  # ecx<- pDvmDex->pResMethods
#if defined(WITH_JIT)
    movl     %edx, TMP_SPILL1(%ebp)
    lea      (%ecx,%eax,4), %edx
    movl     %edx, TMP_SPILL2(%ebp)
    movl     TMP_SPILL1(%ebp), %edx
#endif
    movl      (%ecx,%eax,4),%eax        # eax<- resolved methodToCall
    movl      $0, %ecx                 # make "this" null
    testl     %eax,%eax
    jne       common_invokeMethodRange

    movl      rSELF,%ecx
    movl      offThread_method(%ecx),%ecx # ecx<- self->method
    movzwl    2(rPC),%eax
    movl      offMethod_clazz(%ecx),%ecx# ecx<- method->clazz
    movl      %eax,OUT_ARG1(%esp)       # arg1<- BBBB
    movl      %ecx,OUT_ARG0(%esp)       # arg0<- clazz
    movl      $METHOD_STATIC,%eax
    movl      %eax,OUT_ARG2(%esp)       # arg2<- flags
    SPILL(rIBASE)
    call      dvmResolveMethod          # call(clazz,ref,flags)
    UNSPILL(rIBASE)
    testl     %eax,%eax                 # got null?
#if defined(WITH_JIT)
    movl      TMP_SPILL1(%ebp), %edx
    movl      rSELF,%ecx
    movzwl    offThread_subMode(%ecx), %ecx
    je        common_exceptionThrown    # null, handle exception
    andl      $kSubModeJitTraceBuild, %ecx # is trace under construction?
    movl      $0, %ecx                 # make "this" null
    je        common_invokeMethodRange # no (%eax=method, %ecx="this")
    movl      TMP_SPILL2(%ebp), %edx
    cmpl      $0, (%edx)                  # finished resolving
    movl      TMP_SPILL1(%ebp), %edx
    jne        common_invokeMethodRange # yes (%eax=method, %ecx="this")
    movl      rSELF, %edx
    movl      %edx, OUT_ARG0(%esp)
    movl      rPC, OUT_ARG1(%esp)
    movl      %eax, TMP_SPILL2(%ebp)
    movl      %ecx, TMP_SPILL3(%ebp)
    SPILL(rIBASE)
    call      dvmJitEndTraceSelect
    UNSPILL(rIBASE)
    movl      TMP_SPILL1(%ebp), %edx
    movl      TMP_SPILL2(%ebp), %eax
    movl      TMP_SPILL3(%ebp), %ecx
    jmp       common_invokeMethodRange
#else
    movl      $0, %ecx                 # make "this" null
    jne       common_invokeMethodRange
    jmp       common_exceptionThrown
#endif



/* ------------------------------ */
.L_OP_INVOKE_INTERFACE_RANGE: /* 0x78 */
/* File: x86/OP_INVOKE_INTERFACE_RANGE.S */
/* File: x86/OP_INVOKE_INTERFACE.S */
    /*
     * Handle an interface method call.
     *
     * for: invoke-interface, invoke-interface/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movzwl     4(rPC),%eax              # eax<- FEDC or CCCC
    movl       rSELF,%ecx
    .if        (!1)
    andl       $0xf,%eax               # eax<- C (or stays CCCC)
    .endif
    GET_VREG_R   %eax %eax              # eax<- "this"
    EXPORT_PC
    testl      %eax,%eax                # null this?
    je         common_errNullObject     # yes, fail
    movl       %eax, TMP_SPILL1(%ebp)
    movl       offObject_clazz(%eax),%eax# eax<- thisPtr->clazz
    movl       %eax,OUT_ARG0(%esp)                 # arg0<- class
    movl       offThread_methodClassDex(%ecx),%eax   # eax<- methodClassDex
    movl       offThread_method(%ecx),%ecx           # ecx<- method
    movl       %eax,OUT_ARG3(%esp)                 # arg3<- dex
    movzwl     2(rPC),%eax                         # eax<- BBBB
    movl       %ecx,OUT_ARG2(%esp)                 # arg2<- method
    movl       %eax,OUT_ARG1(%esp)                 # arg1<- BBBB
    call       dvmFindInterfaceMethodInCache # eax<- call(class, ref, method, dex)
    testl      %eax,%eax
    je         common_exceptionThrown
    movl       TMP_SPILL1(%ebp), %ecx
    jmp        common_invokeMethodRange


/* ------------------------------ */
.L_OP_UNUSED_79: /* 0x79 */
/* File: x86/OP_UNUSED_79.S */
/* File: x86/unused.S */
    jmp     common_abort


/* ------------------------------ */
.L_OP_UNUSED_7A: /* 0x7a */
/* File: x86/OP_UNUSED_7A.S */
/* File: x86/unused.S */
    jmp     common_abort


/* ------------------------------ */
.L_OP_NEG_INT: /* 0x7b */
/* File: x86/OP_NEG_INT.S */
/* File: x86/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op eax".
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx           # ecx<- A+
    sarl     $4,rINST             # rINST<- B
    GET_VREG_R %eax rINST           # eax<- vB
    andb     $0xf,%cl              # ecx<- A
    
    
    negl %eax
    SET_VREG %eax %ecx
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_NOT_INT: /* 0x7c */
/* File: x86/OP_NOT_INT.S */
/* File: x86/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op eax".
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx           # ecx<- A+
    sarl     $4,rINST             # rINST<- B
    GET_VREG_R %eax rINST           # eax<- vB
    andb     $0xf,%cl              # ecx<- A
    
    
    notl %eax
    SET_VREG %eax %ecx
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_NEG_LONG: /* 0x7d */
/* File: x86/OP_NEG_LONG.S */
    /* unop vA, vB */
    movzbl    rINSTbl,%ecx        # ecx<- BA
    sarl      $4,%ecx            # ecx<- B
    andb      $0xf,rINSTbl       # rINST<- A
    GET_VREG_WORD %eax %ecx 0     # eax<- v[B+0]
    GET_VREG_WORD %ecx %ecx 1     # ecx<- v[B+1]
    negl      %eax
    adcl      $0,%ecx
    negl      %ecx
    SET_VREG_WORD %eax rINST 0    # v[A+0]<- eax
    FETCH_INST_OPCODE 1 %eax
    SET_VREG_WORD %ecx rINST 1    # v[A+1]<- ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %eax

/* ------------------------------ */
.L_OP_NOT_LONG: /* 0x7e */
/* File: x86/OP_NOT_LONG.S */
    /* unop vA, vB */
    movzbl    rINSTbl,%ecx       # ecx<- BA
    sarl      $4,%ecx           # ecx<- B
    andb      $0xf,rINSTbl      # rINST<- A
    GET_VREG_WORD %eax %ecx 0    # eax<- v[B+0]
    GET_VREG_WORD %ecx %ecx 1    # ecx<- v[B+1]
    notl      %eax
    notl      %ecx
    SET_VREG_WORD %eax rINST 0   # v[A+0]<- eax
    FETCH_INST_OPCODE 1 %eax
    SET_VREG_WORD %ecx rINST 1   # v[A+1]<- ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %eax

/* ------------------------------ */
.L_OP_NEG_FLOAT: /* 0x7f */
/* File: x86/OP_NEG_FLOAT.S */
/* File: x86/fpcvt.S */
    /*
     * Generic 32-bit FP conversion operation.
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx       # ecx<- A+
    sarl     $4,rINST         # rINST<- B
    flds    (rFP,rINST,4)      # %st0<- vB
    andb     $0xf,%cl          # ecx<- A
    fchs
    fstps  (rFP,%ecx,4)        # vA<- %st0
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_NEG_DOUBLE: /* 0x80 */
/* File: x86/OP_NEG_DOUBLE.S */
/* File: x86/fpcvt.S */
    /*
     * Generic 32-bit FP conversion operation.
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx       # ecx<- A+
    sarl     $4,rINST         # rINST<- B
    fldl    (rFP,rINST,4)      # %st0<- vB
    andb     $0xf,%cl          # ecx<- A
    fchs
    fstpl  (rFP,%ecx,4)        # vA<- %st0
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_INT_TO_LONG: /* 0x81 */
/* File: x86/OP_INT_TO_LONG.S */
    /* int to long vA, vB */
    movzbl  rINSTbl,%eax                # eax<- +A
    sarl    $4,%eax                    # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB
    andb    $0xf,rINSTbl               # rINST<- A
    SPILL(rIBASE)                       # cltd trashes rIBASE/edx
    cltd                                # rINST:eax<- sssssssBBBBBBBB
    SET_VREG_WORD rIBASE rINST 1        # v[A+1]<- rIBASE/rPC
    FETCH_INST_OPCODE 1 %ecx
    UNSPILL(rIBASE)
    SET_VREG_WORD %eax rINST 0          # v[A+0]<- %eax
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_INT_TO_FLOAT: /* 0x82 */
/* File: x86/OP_INT_TO_FLOAT.S */
/* File: x86/fpcvt.S */
    /*
     * Generic 32-bit FP conversion operation.
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx       # ecx<- A+
    sarl     $4,rINST         # rINST<- B
    fildl    (rFP,rINST,4)      # %st0<- vB
    andb     $0xf,%cl          # ecx<- A
    
    fstps  (rFP,%ecx,4)        # vA<- %st0
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_INT_TO_DOUBLE: /* 0x83 */
/* File: x86/OP_INT_TO_DOUBLE.S */
/* File: x86/fpcvt.S */
    /*
     * Generic 32-bit FP conversion operation.
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx       # ecx<- A+
    sarl     $4,rINST         # rINST<- B
    fildl    (rFP,rINST,4)      # %st0<- vB
    andb     $0xf,%cl          # ecx<- A
    
    fstpl  (rFP,%ecx,4)        # vA<- %st0
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_LONG_TO_INT: /* 0x84 */
/* File: x86/OP_LONG_TO_INT.S */
/* we ignore the high word, making this equivalent to a 32-bit reg move */
/* File: x86/OP_MOVE.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    movzbl rINSTbl,%eax          # eax<- BA
    andb   $0xf,%al             # eax<- A
    shrl   $4,rINST            # rINST<- B
    GET_VREG_R rINST rINST
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    SET_VREG rINST %eax           # fp[A]<-fp[B]
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_LONG_TO_FLOAT: /* 0x85 */
/* File: x86/OP_LONG_TO_FLOAT.S */
/* File: x86/fpcvt.S */
    /*
     * Generic 32-bit FP conversion operation.
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx       # ecx<- A+
    sarl     $4,rINST         # rINST<- B
    fildll    (rFP,rINST,4)      # %st0<- vB
    andb     $0xf,%cl          # ecx<- A
    
    fstps  (rFP,%ecx,4)        # vA<- %st0
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_LONG_TO_DOUBLE: /* 0x86 */
/* File: x86/OP_LONG_TO_DOUBLE.S */
/* File: x86/fpcvt.S */
    /*
     * Generic 32-bit FP conversion operation.
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx       # ecx<- A+
    sarl     $4,rINST         # rINST<- B
    fildll    (rFP,rINST,4)      # %st0<- vB
    andb     $0xf,%cl          # ecx<- A
    
    fstpl  (rFP,%ecx,4)        # vA<- %st0
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_FLOAT_TO_INT: /* 0x87 */
/* File: x86/OP_FLOAT_TO_INT.S */
/* File: x86/cvtfp_int.S */
/* On fp to int conversions, Java requires that
 * if the result > maxint, it should be clamped to maxint.  If it is less
 * than minint, it should be clamped to minint.  If it is a nan, the result
 * should be zero.  Further, the rounding mode is to truncate.  This model
 * differs from what is delivered normally via the x86 fpu, so we have
 * to play some games.
 */
    /* float/double to int/long vA, vB */
    movzbl    rINSTbl,%ecx       # ecx<- A+
    sarl      $4,rINST         # rINST<- B
    .if 0
    fldl     (rFP,rINST,4)       # %st0<- vB
    .else
    flds     (rFP,rINST,4)       # %st0<- vB
    .endif
    ftst
    fnstcw   LOCAL0_OFFSET(%ebp)      # remember original rounding mode
    movzwl   LOCAL0_OFFSET(%ebp),%eax
    movb     $0xc,%ah
    movw     %ax,LOCAL0_OFFSET+2(%ebp)
    fldcw    LOCAL0_OFFSET+2(%ebp)    # set "to zero" rounding mode
    andb     $0xf,%cl                # ecx<- A
    .if 0
    fistpll  (rFP,%ecx,4)             # convert and store
    .else
    fistpl   (rFP,%ecx,4)             # convert and store
    .endif
    fldcw    LOCAL0_OFFSET(%ebp)      # restore previous rounding mode
    .if 0
    movl     $0x80000000,%eax
    xorl     4(rFP,%ecx,4),%eax
    orl      (rFP,%ecx,4),%eax
    .else
    cmpl     $0x80000000,(rFP,%ecx,4)
    .endif
    je       .LOP_FLOAT_TO_INT_special_case # fix up result

.LOP_FLOAT_TO_INT_finish:
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

.LOP_FLOAT_TO_INT_special_case:
    fnstsw   %ax
    sahf
    jp       .LOP_FLOAT_TO_INT_isNaN
    adcl     $-1,(rFP,%ecx,4)
    .if 0
    adcl     $-1,4(rFP,%ecx,4)
    .endif
   jmp       .LOP_FLOAT_TO_INT_finish
.LOP_FLOAT_TO_INT_isNaN:
    movl      $0,(rFP,%ecx,4)
    .if 0
    movl      $0,4(rFP,%ecx,4)
    .endif
    jmp       .LOP_FLOAT_TO_INT_finish


/* ------------------------------ */
.L_OP_FLOAT_TO_LONG: /* 0x88 */
/* File: x86/OP_FLOAT_TO_LONG.S */
/* File: x86/cvtfp_int.S */
/* On fp to int conversions, Java requires that
 * if the result > maxint, it should be clamped to maxint.  If it is less
 * than minint, it should be clamped to minint.  If it is a nan, the result
 * should be zero.  Further, the rounding mode is to truncate.  This model
 * differs from what is delivered normally via the x86 fpu, so we have
 * to play some games.
 */
    /* float/double to int/long vA, vB */
    movzbl    rINSTbl,%ecx       # ecx<- A+
    sarl      $4,rINST         # rINST<- B
    .if 0
    fldl     (rFP,rINST,4)       # %st0<- vB
    .else
    flds     (rFP,rINST,4)       # %st0<- vB
    .endif
    ftst
    fnstcw   LOCAL0_OFFSET(%ebp)      # remember original rounding mode
    movzwl   LOCAL0_OFFSET(%ebp),%eax
    movb     $0xc,%ah
    movw     %ax,LOCAL0_OFFSET+2(%ebp)
    fldcw    LOCAL0_OFFSET+2(%ebp)    # set "to zero" rounding mode
    andb     $0xf,%cl                # ecx<- A
    .if 1
    fistpll  (rFP,%ecx,4)             # convert and store
    .else
    fistpl   (rFP,%ecx,4)             # convert and store
    .endif
    fldcw    LOCAL0_OFFSET(%ebp)      # restore previous rounding mode
    .if 1
    movl     $0x80000000,%eax
    xorl     4(rFP,%ecx,4),%eax
    orl      (rFP,%ecx,4),%eax
    .else
    cmpl     $0x80000000,(rFP,%ecx,4)
    .endif
    je       .LOP_FLOAT_TO_LONG_special_case # fix up result

.LOP_FLOAT_TO_LONG_finish:
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

.LOP_FLOAT_TO_LONG_special_case:
    fnstsw   %ax
    sahf
    jp       .LOP_FLOAT_TO_LONG_isNaN
    adcl     $-1,(rFP,%ecx,4)
    .if 1
    adcl     $-1,4(rFP,%ecx,4)
    .endif
   jmp       .LOP_FLOAT_TO_LONG_finish
.LOP_FLOAT_TO_LONG_isNaN:
    movl      $0,(rFP,%ecx,4)
    .if 1
    movl      $0,4(rFP,%ecx,4)
    .endif
    jmp       .LOP_FLOAT_TO_LONG_finish


/* ------------------------------ */
.L_OP_FLOAT_TO_DOUBLE: /* 0x89 */
/* File: x86/OP_FLOAT_TO_DOUBLE.S */
/* File: x86/fpcvt.S */
    /*
     * Generic 32-bit FP conversion operation.
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx       # ecx<- A+
    sarl     $4,rINST         # rINST<- B
    flds    (rFP,rINST,4)      # %st0<- vB
    andb     $0xf,%cl          # ecx<- A
    
    fstpl  (rFP,%ecx,4)        # vA<- %st0
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_DOUBLE_TO_INT: /* 0x8a */
/* File: x86/OP_DOUBLE_TO_INT.S */
/* File: x86/cvtfp_int.S */
/* On fp to int conversions, Java requires that
 * if the result > maxint, it should be clamped to maxint.  If it is less
 * than minint, it should be clamped to minint.  If it is a nan, the result
 * should be zero.  Further, the rounding mode is to truncate.  This model
 * differs from what is delivered normally via the x86 fpu, so we have
 * to play some games.
 */
    /* float/double to int/long vA, vB */
    movzbl    rINSTbl,%ecx       # ecx<- A+
    sarl      $4,rINST         # rINST<- B
    .if 1
    fldl     (rFP,rINST,4)       # %st0<- vB
    .else
    flds     (rFP,rINST,4)       # %st0<- vB
    .endif
    ftst
    fnstcw   LOCAL0_OFFSET(%ebp)      # remember original rounding mode
    movzwl   LOCAL0_OFFSET(%ebp),%eax
    movb     $0xc,%ah
    movw     %ax,LOCAL0_OFFSET+2(%ebp)
    fldcw    LOCAL0_OFFSET+2(%ebp)    # set "to zero" rounding mode
    andb     $0xf,%cl                # ecx<- A
    .if 0
    fistpll  (rFP,%ecx,4)             # convert and store
    .else
    fistpl   (rFP,%ecx,4)             # convert and store
    .endif
    fldcw    LOCAL0_OFFSET(%ebp)      # restore previous rounding mode
    .if 0
    movl     $0x80000000,%eax
    xorl     4(rFP,%ecx,4),%eax
    orl      (rFP,%ecx,4),%eax
    .else
    cmpl     $0x80000000,(rFP,%ecx,4)
    .endif
    je       .LOP_DOUBLE_TO_INT_special_case # fix up result

.LOP_DOUBLE_TO_INT_finish:
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

.LOP_DOUBLE_TO_INT_special_case:
    fnstsw   %ax
    sahf
    jp       .LOP_DOUBLE_TO_INT_isNaN
    adcl     $-1,(rFP,%ecx,4)
    .if 0
    adcl     $-1,4(rFP,%ecx,4)
    .endif
   jmp       .LOP_DOUBLE_TO_INT_finish
.LOP_DOUBLE_TO_INT_isNaN:
    movl      $0,(rFP,%ecx,4)
    .if 0
    movl      $0,4(rFP,%ecx,4)
    .endif
    jmp       .LOP_DOUBLE_TO_INT_finish


/* ------------------------------ */
.L_OP_DOUBLE_TO_LONG: /* 0x8b */
/* File: x86/OP_DOUBLE_TO_LONG.S */
/* File: x86/cvtfp_int.S */
/* On fp to int conversions, Java requires that
 * if the result > maxint, it should be clamped to maxint.  If it is less
 * than minint, it should be clamped to minint.  If it is a nan, the result
 * should be zero.  Further, the rounding mode is to truncate.  This model
 * differs from what is delivered normally via the x86 fpu, so we have
 * to play some games.
 */
    /* float/double to int/long vA, vB */
    movzbl    rINSTbl,%ecx       # ecx<- A+
    sarl      $4,rINST         # rINST<- B
    .if 1
    fldl     (rFP,rINST,4)       # %st0<- vB
    .else
    flds     (rFP,rINST,4)       # %st0<- vB
    .endif
    ftst
    fnstcw   LOCAL0_OFFSET(%ebp)      # remember original rounding mode
    movzwl   LOCAL0_OFFSET(%ebp),%eax
    movb     $0xc,%ah
    movw     %ax,LOCAL0_OFFSET+2(%ebp)
    fldcw    LOCAL0_OFFSET+2(%ebp)    # set "to zero" rounding mode
    andb     $0xf,%cl                # ecx<- A
    .if 1
    fistpll  (rFP,%ecx,4)             # convert and store
    .else
    fistpl   (rFP,%ecx,4)             # convert and store
    .endif
    fldcw    LOCAL0_OFFSET(%ebp)      # restore previous rounding mode
    .if 1
    movl     $0x80000000,%eax
    xorl     4(rFP,%ecx,4),%eax
    orl      (rFP,%ecx,4),%eax
    .else
    cmpl     $0x80000000,(rFP,%ecx,4)
    .endif
    je       .LOP_DOUBLE_TO_LONG_special_case # fix up result

.LOP_DOUBLE_TO_LONG_finish:
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

.LOP_DOUBLE_TO_LONG_special_case:
    fnstsw   %ax
    sahf
    jp       .LOP_DOUBLE_TO_LONG_isNaN
    adcl     $-1,(rFP,%ecx,4)
    .if 1
    adcl     $-1,4(rFP,%ecx,4)
    .endif
   jmp       .LOP_DOUBLE_TO_LONG_finish
.LOP_DOUBLE_TO_LONG_isNaN:
    movl      $0,(rFP,%ecx,4)
    .if 1
    movl      $0,4(rFP,%ecx,4)
    .endif
    jmp       .LOP_DOUBLE_TO_LONG_finish


/* ------------------------------ */
.L_OP_DOUBLE_TO_FLOAT: /* 0x8c */
/* File: x86/OP_DOUBLE_TO_FLOAT.S */
/* File: x86/fpcvt.S */
    /*
     * Generic 32-bit FP conversion operation.
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx       # ecx<- A+
    sarl     $4,rINST         # rINST<- B
    fldl    (rFP,rINST,4)      # %st0<- vB
    andb     $0xf,%cl          # ecx<- A
    
    fstps  (rFP,%ecx,4)        # vA<- %st0
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_INT_TO_BYTE: /* 0x8d */
/* File: x86/OP_INT_TO_BYTE.S */
/* File: x86/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op eax".
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx           # ecx<- A+
    sarl     $4,rINST             # rINST<- B
    GET_VREG_R %eax rINST           # eax<- vB
    andb     $0xf,%cl              # ecx<- A
    
    
    movsbl %al,%eax
    SET_VREG %eax %ecx
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_INT_TO_CHAR: /* 0x8e */
/* File: x86/OP_INT_TO_CHAR.S */
/* File: x86/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op eax".
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx           # ecx<- A+
    sarl     $4,rINST             # rINST<- B
    GET_VREG_R %eax rINST           # eax<- vB
    andb     $0xf,%cl              # ecx<- A
    
    
    movzwl %ax,%eax
    SET_VREG %eax %ecx
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_INT_TO_SHORT: /* 0x8f */
/* File: x86/OP_INT_TO_SHORT.S */
/* File: x86/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op eax".
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx           # ecx<- A+
    sarl     $4,rINST             # rINST<- B
    GET_VREG_R %eax rINST           # eax<- vB
    andb     $0xf,%cl              # ecx<- A
    
    
    movswl %ax,%eax
    SET_VREG %eax %ecx
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_ADD_INT: /* 0x90 */
/* File: x86/OP_ADD_INT.S */
/* File: x86/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = eax op (rFP,%ecx,4)".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than eax, you can override "result".)
     *
     * For: add-int, sub-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax   # eax<- BB
    movzbl   3(rPC),%ecx   # ecx<- CC
    GET_VREG_R %eax %eax   # eax<- vBB
    addl (rFP,%ecx,4),%eax                 # ex: addl    (rFP,%ecx,4),%eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SUB_INT: /* 0x91 */
/* File: x86/OP_SUB_INT.S */
/* File: x86/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = eax op (rFP,%ecx,4)".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than eax, you can override "result".)
     *
     * For: add-int, sub-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax   # eax<- BB
    movzbl   3(rPC),%ecx   # ecx<- CC
    GET_VREG_R %eax %eax   # eax<- vBB
    subl   (rFP,%ecx,4),%eax                 # ex: addl    (rFP,%ecx,4),%eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_MUL_INT: /* 0x92 */
/* File: x86/OP_MUL_INT.S */
    /*
     * 32-bit binary multiplication.
     */
    /* mul vAA, vBB, vCC */
    movzbl   2(rPC),%eax            # eax<- BB
    movzbl   3(rPC),%ecx            # ecx<- CC
    GET_VREG_R %eax %eax            # eax<- vBB
    SPILL(rIBASE)
    imull    (rFP,%ecx,4),%eax      # trashes rIBASE/edx
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_DIV_INT: /* 0x93 */
/* File: x86/OP_DIV_INT.S */
/* File: x86/bindiv.S */

    /*
     * 32-bit binary div/rem operation.  Handles special case of op0=minint and
     * op1=-1.
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax            # eax<- BB
    movzbl   3(rPC),%ecx            # ecx<- CC
    GET_VREG_R %eax %eax            # eax<- vBB
    GET_VREG_R %ecx %ecx            # eax<- vBB
    SPILL(rIBASE)
    cmpl     $0,%ecx
    je       common_errDivideByZero
    cmpl     $-1,%ecx
    jne      .LOP_DIV_INT_continue_div
    cmpl     $0x80000000,%eax
    jne      .LOP_DIV_INT_continue_div
    movl     $0x80000000,%eax
    SET_VREG %eax rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_DIV_INT_continue_div:
    cltd
    idivl   %ecx
    SET_VREG %eax rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_REM_INT: /* 0x94 */
/* File: x86/OP_REM_INT.S */
/* File: x86/bindiv.S */

    /*
     * 32-bit binary div/rem operation.  Handles special case of op0=minint and
     * op1=-1.
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax            # eax<- BB
    movzbl   3(rPC),%ecx            # ecx<- CC
    GET_VREG_R %eax %eax            # eax<- vBB
    GET_VREG_R %ecx %ecx            # eax<- vBB
    SPILL(rIBASE)
    cmpl     $0,%ecx
    je       common_errDivideByZero
    cmpl     $-1,%ecx
    jne      .LOP_REM_INT_continue_div
    cmpl     $0x80000000,%eax
    jne      .LOP_REM_INT_continue_div
    movl     $0,rIBASE
    SET_VREG rIBASE rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_REM_INT_continue_div:
    cltd
    idivl   %ecx
    SET_VREG rIBASE rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_AND_INT: /* 0x95 */
/* File: x86/OP_AND_INT.S */
/* File: x86/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = eax op (rFP,%ecx,4)".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than eax, you can override "result".)
     *
     * For: add-int, sub-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax   # eax<- BB
    movzbl   3(rPC),%ecx   # ecx<- CC
    GET_VREG_R %eax %eax   # eax<- vBB
    andl   (rFP,%ecx,4),%eax                 # ex: addl    (rFP,%ecx,4),%eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_OR_INT: /* 0x96 */
/* File: x86/OP_OR_INT.S */
/* File: x86/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = eax op (rFP,%ecx,4)".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than eax, you can override "result".)
     *
     * For: add-int, sub-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax   # eax<- BB
    movzbl   3(rPC),%ecx   # ecx<- CC
    GET_VREG_R %eax %eax   # eax<- vBB
    orl   (rFP,%ecx,4),%eax                 # ex: addl    (rFP,%ecx,4),%eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_XOR_INT: /* 0x97 */
/* File: x86/OP_XOR_INT.S */
/* File: x86/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = eax op (rFP,%ecx,4)".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than eax, you can override "result".)
     *
     * For: add-int, sub-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax   # eax<- BB
    movzbl   3(rPC),%ecx   # ecx<- CC
    GET_VREG_R %eax %eax   # eax<- vBB
    xorl   (rFP,%ecx,4),%eax                 # ex: addl    (rFP,%ecx,4),%eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SHL_INT: /* 0x98 */
/* File: x86/OP_SHL_INT.S */
/* File: x86/binop1.S */
    /*
     * Generic 32-bit binary operation in which both operands loaded to
     * registers (op0 in eax, op1 in ecx).
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax            # eax<- BB
    movzbl   3(rPC),%ecx            # ecx<- CC
    GET_VREG_R %eax %eax            # eax<- vBB
    GET_VREG_R %ecx %ecx            # eax<- vBB
    sall    %cl,%eax                          # ex: addl    %ecx,%eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SHR_INT: /* 0x99 */
/* File: x86/OP_SHR_INT.S */
/* File: x86/binop1.S */
    /*
     * Generic 32-bit binary operation in which both operands loaded to
     * registers (op0 in eax, op1 in ecx).
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax            # eax<- BB
    movzbl   3(rPC),%ecx            # ecx<- CC
    GET_VREG_R %eax %eax            # eax<- vBB
    GET_VREG_R %ecx %ecx            # eax<- vBB
    sarl    %cl,%eax                          # ex: addl    %ecx,%eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_USHR_INT: /* 0x9a */
/* File: x86/OP_USHR_INT.S */
/* File: x86/binop1.S */
    /*
     * Generic 32-bit binary operation in which both operands loaded to
     * registers (op0 in eax, op1 in ecx).
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax            # eax<- BB
    movzbl   3(rPC),%ecx            # ecx<- CC
    GET_VREG_R %eax %eax            # eax<- vBB
    GET_VREG_R %ecx %ecx            # eax<- vBB
    shrl    %cl,%eax                          # ex: addl    %ecx,%eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_ADD_LONG: /* 0x9b */
/* File: x86/OP_ADD_LONG.S */
/* File: x86/binopWide.S */
    /*
     * Generic 64-bit binary operation.
     */
    /* binop vAA, vBB, vCC */

    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    SPILL(rIBASE)                       # save rIBASE
    GET_VREG_WORD rIBASE %eax 0         # rIBASE<- v[BB+0]
    GET_VREG_WORD %eax %eax 1           # eax<- v[BB+1]
    addl (rFP,%ecx,4),rIBASE         # ex: addl   (rFP,%ecx,4),rIBASE
    adcl 4(rFP,%ecx,4),%eax         # ex: adcl   4(rFP,%ecx,4),%eax
    SET_VREG_WORD rIBASE rINST 0        # v[AA+0] <- rIBASE
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)                     # restore rIBASE
    SET_VREG_WORD %eax rINST 1          # v[AA+1] <- eax
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SUB_LONG: /* 0x9c */
/* File: x86/OP_SUB_LONG.S */
/* File: x86/binopWide.S */
    /*
     * Generic 64-bit binary operation.
     */
    /* binop vAA, vBB, vCC */

    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    SPILL(rIBASE)                       # save rIBASE
    GET_VREG_WORD rIBASE %eax 0         # rIBASE<- v[BB+0]
    GET_VREG_WORD %eax %eax 1           # eax<- v[BB+1]
    subl (rFP,%ecx,4),rIBASE         # ex: addl   (rFP,%ecx,4),rIBASE
    sbbl 4(rFP,%ecx,4),%eax         # ex: adcl   4(rFP,%ecx,4),%eax
    SET_VREG_WORD rIBASE rINST 0        # v[AA+0] <- rIBASE
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)                     # restore rIBASE
    SET_VREG_WORD %eax rINST 1          # v[AA+1] <- eax
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_MUL_LONG: /* 0x9d */
/* File: x86/OP_MUL_LONG.S */
    /*
     * Signed 64-bit integer multiply.
     *
     * We could definately use more free registers for
     * this code.   We spill rINSTw (ebx),
     * giving us eax, ebc, ecx and edx as computational
     * temps.  On top of that, we'll spill edi (rFP)
     * for use as the vB pointer and esi (rPC) for use
     * as the vC pointer.  Yuck.
     */
    /* mul-long vAA, vBB, vCC */
    movzbl    2(rPC),%eax              # eax<- B
    movzbl    3(rPC),%ecx              # ecx<- C
    SPILL_TMP2(%esi)                   # save Dalvik PC
    SPILL(rFP)
    SPILL(rINST)
    SPILL(rIBASE)
    leal      (rFP,%eax,4),%esi        # esi<- &v[B]
    leal      (rFP,%ecx,4),rFP         # rFP<- &v[C]
    movl      4(%esi),%ecx             # ecx<- Bmsw
    imull     (rFP),%ecx               # ecx<- (Bmsw*Clsw)
    movl      4(rFP),%eax              # eax<- Cmsw
    imull     (%esi),%eax              # eax<- (Cmsw*Blsw)
    addl      %eax,%ecx                # ecx<- (Bmsw*Clsw)+(Cmsw*Blsw)
    movl      (rFP),%eax               # eax<- Clsw
    mull      (%esi)                   # eax<- (Clsw*Alsw)
    UNSPILL(rINST)
    UNSPILL(rFP)
    leal      (%ecx,rIBASE),rIBASE # full result now in rIBASE:%eax
    UNSPILL_TMP2(%esi)             # Restore Dalvik PC
    FETCH_INST_OPCODE 2 %ecx       # Fetch next instruction
    movl      rIBASE,4(rFP,rINST,4)# v[B+1]<- rIBASE
    UNSPILL(rIBASE)
    movl      %eax,(rFP,rINST,4)   # v[B]<- %eax
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_DIV_LONG: /* 0x9e */
/* File: x86/OP_DIV_LONG.S */
    /* div vAA, vBB, vCC */
    movzbl    3(rPC),%eax              # eax<- CC
    movzbl    2(rPC),%ecx              # ecx<- BB
    SPILL(rIBASE)                      # save rIBASE/%edx
    GET_VREG_WORD rIBASE %eax 0
    GET_VREG_WORD %eax %eax 1
    movl     rIBASE,OUT_ARG2(%esp)
    testl    %eax,%eax
    je       .LOP_DIV_LONG_check_zero
    cmpl     $-1,%eax
    je       .LOP_DIV_LONG_check_neg1
.LOP_DIV_LONG_notSpecial:
    GET_VREG_WORD rIBASE %ecx 0
    GET_VREG_WORD %ecx %ecx 1
.LOP_DIV_LONG_notSpecial1:
    movl     %eax,OUT_ARG3(%esp)
    movl     rIBASE,OUT_ARG0(%esp)
    movl     %ecx,OUT_ARG1(%esp)
    call     __divdi3
.LOP_DIV_LONG_finish:
    SET_VREG_WORD rIBASE rINST 1
    UNSPILL(rIBASE)                 # restore rIBASE/%edx
    SET_VREG_WORD %eax rINST 0
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_DIV_LONG_check_zero:
    testl   rIBASE,rIBASE
    jne     .LOP_DIV_LONG_notSpecial
    jmp     common_errDivideByZero
.LOP_DIV_LONG_check_neg1:
    testl   rIBASE,%eax
    jne     .LOP_DIV_LONG_notSpecial
    GET_VREG_WORD rIBASE %ecx 0
    GET_VREG_WORD %ecx %ecx 1
    testl    rIBASE,rIBASE
    jne      .LOP_DIV_LONG_notSpecial1
    cmpl     $0x80000000,%ecx
    jne      .LOP_DIV_LONG_notSpecial1
    /* minint / -1, return minint on div, 0 on rem */
    xorl     %eax,%eax
    movl     $0x80000000,rIBASE
    jmp      .LOP_DIV_LONG_finish

/* ------------------------------ */
.L_OP_REM_LONG: /* 0x9f */
/* File: x86/OP_REM_LONG.S */
/* File: x86/OP_DIV_LONG.S */
    /* div vAA, vBB, vCC */
    movzbl    3(rPC),%eax              # eax<- CC
    movzbl    2(rPC),%ecx              # ecx<- BB
    SPILL(rIBASE)                      # save rIBASE/%edx
    GET_VREG_WORD rIBASE %eax 0
    GET_VREG_WORD %eax %eax 1
    movl     rIBASE,OUT_ARG2(%esp)
    testl    %eax,%eax
    je       .LOP_REM_LONG_check_zero
    cmpl     $-1,%eax
    je       .LOP_REM_LONG_check_neg1
.LOP_REM_LONG_notSpecial:
    GET_VREG_WORD rIBASE %ecx 0
    GET_VREG_WORD %ecx %ecx 1
.LOP_REM_LONG_notSpecial1:
    movl     %eax,OUT_ARG3(%esp)
    movl     rIBASE,OUT_ARG0(%esp)
    movl     %ecx,OUT_ARG1(%esp)
    call     __moddi3
.LOP_REM_LONG_finish:
    SET_VREG_WORD rIBASE rINST 1
    UNSPILL(rIBASE)                 # restore rIBASE/%edx
    SET_VREG_WORD %eax rINST 0
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_REM_LONG_check_zero:
    testl   rIBASE,rIBASE
    jne     .LOP_REM_LONG_notSpecial
    jmp     common_errDivideByZero
.LOP_REM_LONG_check_neg1:
    testl   rIBASE,%eax
    jne     .LOP_REM_LONG_notSpecial
    GET_VREG_WORD rIBASE %ecx 0
    GET_VREG_WORD %ecx %ecx 1
    testl    rIBASE,rIBASE
    jne      .LOP_REM_LONG_notSpecial1
    cmpl     $0x80000000,%ecx
    jne      .LOP_REM_LONG_notSpecial1
    /* minint / -1, return minint on div, 0 on rem */
    xorl     %eax,%eax
    movl     $0,rIBASE
    jmp      .LOP_REM_LONG_finish


/* ------------------------------ */
.L_OP_AND_LONG: /* 0xa0 */
/* File: x86/OP_AND_LONG.S */
/* File: x86/binopWide.S */
    /*
     * Generic 64-bit binary operation.
     */
    /* binop vAA, vBB, vCC */

    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    SPILL(rIBASE)                       # save rIBASE
    GET_VREG_WORD rIBASE %eax 0         # rIBASE<- v[BB+0]
    GET_VREG_WORD %eax %eax 1           # eax<- v[BB+1]
    andl (rFP,%ecx,4),rIBASE         # ex: addl   (rFP,%ecx,4),rIBASE
    andl 4(rFP,%ecx,4),%eax         # ex: adcl   4(rFP,%ecx,4),%eax
    SET_VREG_WORD rIBASE rINST 0        # v[AA+0] <- rIBASE
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)                     # restore rIBASE
    SET_VREG_WORD %eax rINST 1          # v[AA+1] <- eax
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_OR_LONG: /* 0xa1 */
/* File: x86/OP_OR_LONG.S */
/* File: x86/binopWide.S */
    /*
     * Generic 64-bit binary operation.
     */
    /* binop vAA, vBB, vCC */

    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    SPILL(rIBASE)                       # save rIBASE
    GET_VREG_WORD rIBASE %eax 0         # rIBASE<- v[BB+0]
    GET_VREG_WORD %eax %eax 1           # eax<- v[BB+1]
    orl (rFP,%ecx,4),rIBASE         # ex: addl   (rFP,%ecx,4),rIBASE
    orl 4(rFP,%ecx,4),%eax         # ex: adcl   4(rFP,%ecx,4),%eax
    SET_VREG_WORD rIBASE rINST 0        # v[AA+0] <- rIBASE
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)                     # restore rIBASE
    SET_VREG_WORD %eax rINST 1          # v[AA+1] <- eax
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_XOR_LONG: /* 0xa2 */
/* File: x86/OP_XOR_LONG.S */
/* File: x86/binopWide.S */
    /*
     * Generic 64-bit binary operation.
     */
    /* binop vAA, vBB, vCC */

    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    SPILL(rIBASE)                       # save rIBASE
    GET_VREG_WORD rIBASE %eax 0         # rIBASE<- v[BB+0]
    GET_VREG_WORD %eax %eax 1           # eax<- v[BB+1]
    xorl (rFP,%ecx,4),rIBASE         # ex: addl   (rFP,%ecx,4),rIBASE
    xorl 4(rFP,%ecx,4),%eax         # ex: adcl   4(rFP,%ecx,4),%eax
    SET_VREG_WORD rIBASE rINST 0        # v[AA+0] <- rIBASE
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)                     # restore rIBASE
    SET_VREG_WORD %eax rINST 1          # v[AA+1] <- eax
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SHL_LONG: /* 0xa3 */
/* File: x86/OP_SHL_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.  x86 shifts automatically mask off
     * the low 5 bits of %cl, so have to handle the 64 > shiftcount > 31
     * case specially.
     */
    /* shl-long vAA, vBB, vCC */
    /* ecx gets shift count */
    /* Need to spill rINST */
    /* rINSTw gets AA */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    SPILL(rIBASE)
    GET_VREG_WORD rIBASE %eax 1         # ecx<- v[BB+1]
    GET_VREG_R   %ecx %ecx              # ecx<- vCC
    GET_VREG_WORD %eax %eax 0           # eax<- v[BB+0]
    shldl     %eax,rIBASE
    sall      %cl,%eax
    testb     $32,%cl
    je        2f
    movl      %eax,rIBASE
    xorl      %eax,%eax
2:
    SET_VREG_WORD rIBASE rINST 1        # v[AA+1]<- rIBASE
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    SET_VREG_WORD %eax rINST 0          # v[AA+0]<- %eax
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_SHR_LONG: /* 0xa4 */
/* File: x86/OP_SHR_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.  x86 shifts automatically mask off
     * the low 5 bits of %cl, so have to handle the 64 > shiftcount > 31
     * case specially.
     */
    /* shr-long vAA, vBB, vCC */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    SPILL(rIBASE)
    GET_VREG_WORD rIBASE %eax 1         # rIBASE<- v[BB+1]
    GET_VREG_R   %ecx %ecx              # ecx<- vCC
    GET_VREG_WORD %eax %eax 0           # eax<- v[BB+0]
    shrdl     rIBASE,%eax
    sarl      %cl,rIBASE
    testb     $32,%cl
    je        2f
    movl      rIBASE,%eax
    sarl      $31,rIBASE
2:
    SET_VREG_WORD rIBASE rINST 1        # v[AA+1]<- rIBASE
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    SET_VREG_WORD %eax rINST 0          # v[AA+0]<- eax
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_USHR_LONG: /* 0xa5 */
/* File: x86/OP_USHR_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.  x86 shifts automatically mask off
     * the low 5 bits of %cl, so have to handle the 64 > shiftcount > 31
     * case specially.
     */
    /* shr-long vAA, vBB, vCC */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    SPILL(rIBASE)
    GET_VREG_WORD rIBASE %eax 1         # rIBASE<- v[BB+1]
    GET_VREG_R  %ecx %ecx               # ecx<- vCC
    GET_VREG_WORD %eax %eax 0           # eax<- v[BB+0]
    shrdl     rIBASE,%eax
    shrl      %cl,rIBASE
    testb     $32,%cl
    je        2f
    movl      rIBASE,%eax
    xorl      rIBASE,rIBASE
2:
    SET_VREG_WORD rIBASE rINST 1          # v[AA+1]<- rIBASE
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    SET_VREG_WORD %eax rINST 0         # v[BB+0]<- eax
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_ADD_FLOAT: /* 0xa6 */
/* File: x86/OP_ADD_FLOAT.S */
/* File: x86/binflop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax          # eax<- CC
    movzbl   3(rPC),%ecx          # ecx<- BB
    flds    (rFP,%eax,4)         # vCC to fp stack
    fadds   (rFP,%ecx,4)         # ex: faddp
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    fstps   (rFP,rINST,4)         # %st to vAA
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SUB_FLOAT: /* 0xa7 */
/* File: x86/OP_SUB_FLOAT.S */
/* File: x86/binflop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax          # eax<- CC
    movzbl   3(rPC),%ecx          # ecx<- BB
    flds    (rFP,%eax,4)         # vCC to fp stack
    fsubs   (rFP,%ecx,4)         # ex: faddp
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    fstps   (rFP,rINST,4)         # %st to vAA
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_MUL_FLOAT: /* 0xa8 */
/* File: x86/OP_MUL_FLOAT.S */
/* File: x86/binflop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax          # eax<- CC
    movzbl   3(rPC),%ecx          # ecx<- BB
    flds    (rFP,%eax,4)         # vCC to fp stack
    fmuls   (rFP,%ecx,4)         # ex: faddp
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    fstps   (rFP,rINST,4)         # %st to vAA
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_DIV_FLOAT: /* 0xa9 */
/* File: x86/OP_DIV_FLOAT.S */
/* File: x86/binflop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax          # eax<- CC
    movzbl   3(rPC),%ecx          # ecx<- BB
    flds    (rFP,%eax,4)         # vCC to fp stack
    fdivs   (rFP,%ecx,4)         # ex: faddp
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    fstps   (rFP,rINST,4)         # %st to vAA
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_REM_FLOAT: /* 0xaa */
/* File: x86/OP_REM_FLOAT.S */
    /* rem_float vAA, vBB, vCC */
    movzbl   3(rPC),%ecx            # ecx<- BB
    movzbl   2(rPC),%eax            # eax<- CC
    flds     (rFP,%ecx,4)           # vCC to fp stack
    flds     (rFP,%eax,4)           # vCC to fp stack
    movzbl   rINSTbl,%ecx           # ecx<- AA
1:
    fprem
    fstsw     %ax
    sahf
    jp        1b
    fstp      %st(1)
    FETCH_INST_OPCODE 2 %eax
    ADVANCE_PC 2
    fstps    (rFP,%ecx,4)           # %st to vAA
    GOTO_NEXT_R %eax

/* ------------------------------ */
.L_OP_ADD_DOUBLE: /* 0xab */
/* File: x86/OP_ADD_DOUBLE.S */
   /*
    * File: OP_ADD_DOUBLE.S
    */

    movzbl   2(rPC),%eax                # eax<- BB
    movzbl   3(rPC),%ecx                # ecx<- CC
    movq     (rFP, %eax, 4), %xmm0      # %xmm0<- vBB
    movq     (rFP, %ecx, 4), %xmm1      # %xmm1<- vCC
    FETCH_INST_OPCODE 2 %ecx
    addsd    %xmm1, %xmm0
    ADVANCE_PC 2
    movq     %xmm0, (rFP, rINST, 4)     # vAA<- vBB * vCC
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SUB_DOUBLE: /* 0xac */
/* File: x86/OP_SUB_DOUBLE.S */
   /*
    * File: OP_SUB_DOUBLE.S
    */

    movzbl   2(rPC),%eax                # eax<- BB
    movzbl   3(rPC),%ecx                # ecx<- CC
    # TODO: movsd?
    movq        (rFP, %eax, 4), %xmm0   # %xmm0<- vBB
    movq        (rFP, %ecx, 4), %xmm1   # %xmm1<- vCC
    FETCH_INST_OPCODE 2 %ecx
    subsd       %xmm1, %xmm0
    ADVANCE_PC 2
    movq        %xmm0, (rFP, rINST, 4)  # vAA<- vBB - vCC
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_MUL_DOUBLE: /* 0xad */
/* File: x86/OP_MUL_DOUBLE.S */
   /*
    * File: OP_MUL_DOUBLE.S
    */

    movzbl   2(rPC),%eax                # eax<- BB
    movzbl   3(rPC),%ecx                # ecx<- CC
    # TODO: movsd?
    movq        (rFP, %eax, 4), %xmm0   # %xmm0<- vBB
    movq        (rFP, %ecx, 4), %xmm1   # %xmm1<- vCC
    FETCH_INST_OPCODE 2 %ecx
    mulsd       %xmm1, %xmm0
    ADVANCE_PC 2
    movq        %xmm0, (rFP, rINST, 4)  # vAA<- vBB * vCC
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_DIV_DOUBLE: /* 0xae */
/* File: x86/OP_DIV_DOUBLE.S */
/* File: x86/binflop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax          # eax<- CC
    movzbl   3(rPC),%ecx          # ecx<- BB
    fldl    (rFP,%eax,4)         # vCC to fp stack
    fdivl   (rFP,%ecx,4)         # ex: faddp
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    fstpl   (rFP,rINST,4)         # %st to vAA
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_REM_DOUBLE: /* 0xaf */
/* File: x86/OP_REM_DOUBLE.S */
    /* rem_float vAA, vBB, vCC */
    movzbl   3(rPC),%ecx            # ecx<- BB
    movzbl   2(rPC),%eax            # eax<- CC
    fldl     (rFP,%ecx,4)           # vCC to fp stack
    fldl     (rFP,%eax,4)           # vCC to fp stack
    FETCH_INST_OPCODE 2 %ecx
1:
    fprem
    fstsw     %ax
    sahf
    jp        1b
    fstp      %st(1)
    ADVANCE_PC 2
    fstpl    (rFP,rINST,4)           # %st to vAA
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_ADD_INT_2ADDR: /* 0xb0 */
/* File: x86/OP_ADD_INT_2ADDR.S */
/* File: x86/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an instruction or a function call.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx               # ecx<- A+
    sarl    $4,rINST                  # rINST<- B
    GET_VREG_R %eax rINST              # eax<- vB
    andb    $0xf,%cl                  # ecx<- A
    addl     %eax,(rFP,%ecx,4)                             # for ex: addl   %eax,(rFP,%ecx,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SUB_INT_2ADDR: /* 0xb1 */
/* File: x86/OP_SUB_INT_2ADDR.S */
/* File: x86/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an instruction or a function call.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx               # ecx<- A+
    sarl    $4,rINST                  # rINST<- B
    GET_VREG_R %eax rINST              # eax<- vB
    andb    $0xf,%cl                  # ecx<- A
    subl     %eax,(rFP,%ecx,4)                             # for ex: addl   %eax,(rFP,%ecx,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_MUL_INT_2ADDR: /* 0xb2 */
/* File: x86/OP_MUL_INT_2ADDR.S */
    /* mul vA, vB */
    movzx   rINSTbl,%ecx              # ecx<- A+
    sarl    $4,rINST                 # rINST<- B
    GET_VREG_R %eax rINST             # eax<- vB
    andb    $0xf,%cl                 # ecx<- A
    SPILL(rIBASE)
    imull   (rFP,%ecx,4),%eax         # trashes rIBASE/edx
    UNSPILL(rIBASE)
    SET_VREG %eax %ecx
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_DIV_INT_2ADDR: /* 0xb3 */
/* File: x86/OP_DIV_INT_2ADDR.S */
/* File: x86/bindiv2addr.S */
    /*
     * 32-bit binary div/rem operation.  Handles special case of op0=minint and
     * op1=-1.
     */
    /* div/rem/2addr vA, vB */
    movzx    rINSTbl,%ecx          # eax<- BA
    SPILL(rIBASE)
    sarl     $4,%ecx              # ecx<- B
    GET_VREG_R %ecx %ecx           # eax<- vBB
    andb     $0xf,rINSTbl         # rINST<- A
    GET_VREG_R %eax rINST          # eax<- vBB
    cmpl     $0,%ecx
    je       common_errDivideByZero
    cmpl     $-1,%ecx
    jne      .LOP_DIV_INT_2ADDR_continue_div2addr
    cmpl     $0x80000000,%eax
    jne      .LOP_DIV_INT_2ADDR_continue_div2addr
    movl     $0x80000000,%eax
    SET_VREG %eax rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

.LOP_DIV_INT_2ADDR_continue_div2addr:
    cltd
    idivl   %ecx
    SET_VREG %eax rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_REM_INT_2ADDR: /* 0xb4 */
/* File: x86/OP_REM_INT_2ADDR.S */
/* File: x86/bindiv2addr.S */
    /*
     * 32-bit binary div/rem operation.  Handles special case of op0=minint and
     * op1=-1.
     */
    /* div/rem/2addr vA, vB */
    movzx    rINSTbl,%ecx          # eax<- BA
    SPILL(rIBASE)
    sarl     $4,%ecx              # ecx<- B
    GET_VREG_R %ecx %ecx           # eax<- vBB
    andb     $0xf,rINSTbl         # rINST<- A
    GET_VREG_R %eax rINST          # eax<- vBB
    cmpl     $0,%ecx
    je       common_errDivideByZero
    cmpl     $-1,%ecx
    jne      .LOP_REM_INT_2ADDR_continue_div2addr
    cmpl     $0x80000000,%eax
    jne      .LOP_REM_INT_2ADDR_continue_div2addr
    movl     $0,rIBASE
    SET_VREG rIBASE rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

.LOP_REM_INT_2ADDR_continue_div2addr:
    cltd
    idivl   %ecx
    SET_VREG rIBASE rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_AND_INT_2ADDR: /* 0xb5 */
/* File: x86/OP_AND_INT_2ADDR.S */
/* File: x86/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an instruction or a function call.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx               # ecx<- A+
    sarl    $4,rINST                  # rINST<- B
    GET_VREG_R %eax rINST              # eax<- vB
    andb    $0xf,%cl                  # ecx<- A
    andl     %eax,(rFP,%ecx,4)                             # for ex: addl   %eax,(rFP,%ecx,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_OR_INT_2ADDR: /* 0xb6 */
/* File: x86/OP_OR_INT_2ADDR.S */
/* File: x86/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an instruction or a function call.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx               # ecx<- A+
    sarl    $4,rINST                  # rINST<- B
    GET_VREG_R %eax rINST              # eax<- vB
    andb    $0xf,%cl                  # ecx<- A
    orl     %eax,(rFP,%ecx,4)                             # for ex: addl   %eax,(rFP,%ecx,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_XOR_INT_2ADDR: /* 0xb7 */
/* File: x86/OP_XOR_INT_2ADDR.S */
/* File: x86/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an instruction or a function call.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx               # ecx<- A+
    sarl    $4,rINST                  # rINST<- B
    GET_VREG_R %eax rINST              # eax<- vB
    andb    $0xf,%cl                  # ecx<- A
    xorl     %eax,(rFP,%ecx,4)                             # for ex: addl   %eax,(rFP,%ecx,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SHL_INT_2ADDR: /* 0xb8 */
/* File: x86/OP_SHL_INT_2ADDR.S */
/* File: x86/shop2addr.S */
    /*
     * Generic 32-bit "shift/2addr" operation.
     */
    /* shift/2addr vA, vB */
    movzx    rINSTbl,%ecx           # eax<- BA
    sarl     $4,%ecx               # ecx<- B
    GET_VREG_R %ecx %ecx            # eax<- vBB
    andb     $0xf,rINSTbl          # rINST<- A
    GET_VREG_R %eax rINST           # eax<- vAA
    sall    %cl,%eax                          # ex: sarl %cl,%eax
    FETCH_INST_OPCODE 1 %ecx
    SET_VREG %eax rINST
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SHR_INT_2ADDR: /* 0xb9 */
/* File: x86/OP_SHR_INT_2ADDR.S */
/* File: x86/shop2addr.S */
    /*
     * Generic 32-bit "shift/2addr" operation.
     */
    /* shift/2addr vA, vB */
    movzx    rINSTbl,%ecx           # eax<- BA
    sarl     $4,%ecx               # ecx<- B
    GET_VREG_R %ecx %ecx            # eax<- vBB
    andb     $0xf,rINSTbl          # rINST<- A
    GET_VREG_R %eax rINST           # eax<- vAA
    sarl    %cl,%eax                          # ex: sarl %cl,%eax
    FETCH_INST_OPCODE 1 %ecx
    SET_VREG %eax rINST
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_USHR_INT_2ADDR: /* 0xba */
/* File: x86/OP_USHR_INT_2ADDR.S */
/* File: x86/shop2addr.S */
    /*
     * Generic 32-bit "shift/2addr" operation.
     */
    /* shift/2addr vA, vB */
    movzx    rINSTbl,%ecx           # eax<- BA
    sarl     $4,%ecx               # ecx<- B
    GET_VREG_R %ecx %ecx            # eax<- vBB
    andb     $0xf,rINSTbl          # rINST<- A
    GET_VREG_R %eax rINST           # eax<- vAA
    shrl    %cl,%eax                          # ex: sarl %cl,%eax
    FETCH_INST_OPCODE 1 %ecx
    SET_VREG %eax rINST
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_ADD_LONG_2ADDR: /* 0xbb */
/* File: x86/OP_ADD_LONG_2ADDR.S */
/* File: x86/binopWide2addr.S */
    /*
     * Generic 64-bit binary operation.
     */
    /* binop/2addr vA, vB */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $4,%ecx                  # ecx<- B
    GET_VREG_WORD %eax %ecx 0           # eax<- v[B+0]
    GET_VREG_WORD %ecx %ecx 1           # eax<- v[B+1]
    andb      $0xF,rINSTbl             # rINST<- A
    addl %eax,(rFP,rINST,4)         # example: addl   %eax,(rFP,rINST,4)
    adcl %ecx,4(rFP,rINST,4)         # example: adcl   %ecx,4(rFP,rINST,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SUB_LONG_2ADDR: /* 0xbc */
/* File: x86/OP_SUB_LONG_2ADDR.S */
/* File: x86/binopWide2addr.S */
    /*
     * Generic 64-bit binary operation.
     */
    /* binop/2addr vA, vB */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $4,%ecx                  # ecx<- B
    GET_VREG_WORD %eax %ecx 0           # eax<- v[B+0]
    GET_VREG_WORD %ecx %ecx 1           # eax<- v[B+1]
    andb      $0xF,rINSTbl             # rINST<- A
    subl %eax,(rFP,rINST,4)         # example: addl   %eax,(rFP,rINST,4)
    sbbl %ecx,4(rFP,rINST,4)         # example: adcl   %ecx,4(rFP,rINST,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_MUL_LONG_2ADDR: /* 0xbd */
/* File: x86/OP_MUL_LONG_2ADDR.S */
    /*
     * Signed 64-bit integer multiply, 2-addr version
     *
     * We could definately use more free registers for
     * this code.  We must spill %edx (rIBASE) because it
     * is used by imul.  We'll also spill rINST (ebx),
     * giving us eax, ebc, ecx and rIBASE as computational
     * temps.  On top of that, we'll spill %esi (edi)
     * for use as the vA pointer and rFP (esi) for use
     * as the vB pointer.  Yuck.
     */
    /* mul-long/2addr vA, vB */
    movzbl    rINSTbl,%eax             # eax<- BA
    andb      $0xf,%al                # eax<- A
    sarl      $4,rINST                # rINST<- B
    SPILL_TMP2(%esi)
    SPILL(rFP)
    SPILL(rIBASE)
    leal      (rFP,%eax,4),%esi        # %esi<- &v[A]
    leal      (rFP,rINST,4),rFP        # rFP<- &v[B]
    movl      4(%esi),%ecx             # ecx<- Amsw
    imull     (rFP),%ecx               # ecx<- (Amsw*Blsw)
    movl      4(rFP),%eax              # eax<- Bmsw
    imull     (%esi),%eax              # eax<- (Bmsw*Alsw)
    addl      %eax,%ecx                # ecx<- (Amsw*Blsw)+(Bmsw*Alsw)
    movl      (rFP),%eax               # eax<- Blsw
    mull      (%esi)                   # eax<- (Blsw*Alsw)
    leal      (%ecx,rIBASE),rIBASE     # full result now in %edx:%eax
    movl      rIBASE,4(%esi)           # v[A+1]<- rIBASE
    movl      %eax,(%esi)              # v[A]<- %eax
    UNSPILL_TMP2(%esi)
    FETCH_INST_OPCODE 1 %ecx
    UNSPILL(rIBASE)
    UNSPILL(rFP)
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_DIV_LONG_2ADDR: /* 0xbe */
/* File: x86/OP_DIV_LONG_2ADDR.S */
    /* div/2addr vA, vB */
    movzbl    rINSTbl,%eax
    shrl      $4,%eax                  # eax<- B
    andb      $0xf,rINSTbl             # rINST<- A
    SPILL(rIBASE)                       # save rIBASE/%edx
    GET_VREG_WORD rIBASE %eax 0
    GET_VREG_WORD %eax %eax 1
    movl     rIBASE,OUT_ARG2(%esp)
    testl    %eax,%eax
    je       .LOP_DIV_LONG_2ADDR_check_zero
    cmpl     $-1,%eax
    je       .LOP_DIV_LONG_2ADDR_check_neg1
.LOP_DIV_LONG_2ADDR_notSpecial:
    GET_VREG_WORD rIBASE rINST 0
    GET_VREG_WORD %ecx rINST 1
.LOP_DIV_LONG_2ADDR_notSpecial1:
    movl     %eax,OUT_ARG3(%esp)
    movl     rIBASE,OUT_ARG0(%esp)
    movl     %ecx,OUT_ARG1(%esp)
    call     __divdi3
.LOP_DIV_LONG_2ADDR_finish:
    SET_VREG_WORD rIBASE rINST 1
    UNSPILL(rIBASE)                    # restore rIBASE/%edx
    SET_VREG_WORD %eax rINST 0
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

.LOP_DIV_LONG_2ADDR_check_zero:
    testl   rIBASE,rIBASE
    jne     .LOP_DIV_LONG_2ADDR_notSpecial
    jmp     common_errDivideByZero
.LOP_DIV_LONG_2ADDR_check_neg1:
    testl   rIBASE,%eax
    jne     .LOP_DIV_LONG_2ADDR_notSpecial
    GET_VREG_WORD rIBASE rINST 0
    GET_VREG_WORD %ecx rINST 1
    testl    rIBASE,rIBASE
    jne      .LOP_DIV_LONG_2ADDR_notSpecial1
    cmpl     $0x80000000,%ecx
    jne      .LOP_DIV_LONG_2ADDR_notSpecial1
    /* minint / -1, return minint on div, 0 on rem */
    xorl     %eax,%eax
    movl     $0x80000000,rIBASE
    jmp      .LOP_DIV_LONG_2ADDR_finish

/* ------------------------------ */
.L_OP_REM_LONG_2ADDR: /* 0xbf */
/* File: x86/OP_REM_LONG_2ADDR.S */
/* File: x86/OP_DIV_LONG_2ADDR.S */
    /* div/2addr vA, vB */
    movzbl    rINSTbl,%eax
    shrl      $4,%eax                  # eax<- B
    andb      $0xf,rINSTbl             # rINST<- A
    SPILL(rIBASE)                       # save rIBASE/%edx
    GET_VREG_WORD rIBASE %eax 0
    GET_VREG_WORD %eax %eax 1
    movl     rIBASE,OUT_ARG2(%esp)
    testl    %eax,%eax
    je       .LOP_REM_LONG_2ADDR_check_zero
    cmpl     $-1,%eax
    je       .LOP_REM_LONG_2ADDR_check_neg1
.LOP_REM_LONG_2ADDR_notSpecial:
    GET_VREG_WORD rIBASE rINST 0
    GET_VREG_WORD %ecx rINST 1
.LOP_REM_LONG_2ADDR_notSpecial1:
    movl     %eax,OUT_ARG3(%esp)
    movl     rIBASE,OUT_ARG0(%esp)
    movl     %ecx,OUT_ARG1(%esp)
    call     __moddi3
.LOP_REM_LONG_2ADDR_finish:
    SET_VREG_WORD rIBASE rINST 1
    UNSPILL(rIBASE)                    # restore rIBASE/%edx
    SET_VREG_WORD %eax rINST 0
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

.LOP_REM_LONG_2ADDR_check_zero:
    testl   rIBASE,rIBASE
    jne     .LOP_REM_LONG_2ADDR_notSpecial
    jmp     common_errDivideByZero
.LOP_REM_LONG_2ADDR_check_neg1:
    testl   rIBASE,%eax
    jne     .LOP_REM_LONG_2ADDR_notSpecial
    GET_VREG_WORD rIBASE rINST 0
    GET_VREG_WORD %ecx rINST 1
    testl    rIBASE,rIBASE
    jne      .LOP_REM_LONG_2ADDR_notSpecial1
    cmpl     $0x80000000,%ecx
    jne      .LOP_REM_LONG_2ADDR_notSpecial1
    /* minint / -1, return minint on div, 0 on rem */
    xorl     %eax,%eax
    movl     $0,rIBASE
    jmp      .LOP_REM_LONG_2ADDR_finish


/* ------------------------------ */
.L_OP_AND_LONG_2ADDR: /* 0xc0 */
/* File: x86/OP_AND_LONG_2ADDR.S */
/* File: x86/binopWide2addr.S */
    /*
     * Generic 64-bit binary operation.
     */
    /* binop/2addr vA, vB */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $4,%ecx                  # ecx<- B
    GET_VREG_WORD %eax %ecx 0           # eax<- v[B+0]
    GET_VREG_WORD %ecx %ecx 1           # eax<- v[B+1]
    andb      $0xF,rINSTbl             # rINST<- A
    andl %eax,(rFP,rINST,4)         # example: addl   %eax,(rFP,rINST,4)
    andl %ecx,4(rFP,rINST,4)         # example: adcl   %ecx,4(rFP,rINST,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_OR_LONG_2ADDR: /* 0xc1 */
/* File: x86/OP_OR_LONG_2ADDR.S */
/* File: x86/binopWide2addr.S */
    /*
     * Generic 64-bit binary operation.
     */
    /* binop/2addr vA, vB */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $4,%ecx                  # ecx<- B
    GET_VREG_WORD %eax %ecx 0           # eax<- v[B+0]
    GET_VREG_WORD %ecx %ecx 1           # eax<- v[B+1]
    andb      $0xF,rINSTbl             # rINST<- A
    orl %eax,(rFP,rINST,4)         # example: addl   %eax,(rFP,rINST,4)
    orl %ecx,4(rFP,rINST,4)         # example: adcl   %ecx,4(rFP,rINST,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_XOR_LONG_2ADDR: /* 0xc2 */
/* File: x86/OP_XOR_LONG_2ADDR.S */
/* File: x86/binopWide2addr.S */
    /*
     * Generic 64-bit binary operation.
     */
    /* binop/2addr vA, vB */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $4,%ecx                  # ecx<- B
    GET_VREG_WORD %eax %ecx 0           # eax<- v[B+0]
    GET_VREG_WORD %ecx %ecx 1           # eax<- v[B+1]
    andb      $0xF,rINSTbl             # rINST<- A
    xorl %eax,(rFP,rINST,4)         # example: addl   %eax,(rFP,rINST,4)
    xorl %ecx,4(rFP,rINST,4)         # example: adcl   %ecx,4(rFP,rINST,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SHL_LONG_2ADDR: /* 0xc3 */
/* File: x86/OP_SHL_LONG_2ADDR.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shl-long/2addr vA, vB */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl    rINSTbl,%ecx             # ecx<- BA
    andb      $0xf,rINSTbl            # rINST<- A
    GET_VREG_WORD %eax rINST 0         # eax<- v[AA+0]
    sarl      $4,%ecx                 # ecx<- B
    SPILL(rIBASE)
    GET_VREG_WORD rIBASE rINST 1       # rIBASE<- v[AA+1]
    GET_VREG_R  %ecx %ecx              # ecx<- vBB
    shldl     %eax,rIBASE
    sall      %cl,%eax
    testb     $32,%cl
    je        2f
    movl      %eax,rIBASE
    xorl      %eax,%eax
2:
    SET_VREG_WORD rIBASE rINST 1       # v[AA+1]<- rIBASE
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 1 %ecx
    SET_VREG_WORD %eax rINST 0         # v[AA+0]<- eax
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_SHR_LONG_2ADDR: /* 0xc4 */
/* File: x86/OP_SHR_LONG_2ADDR.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shl-long/2addr vA, vB */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl    rINSTbl,%ecx         # ecx<- BA
    andb      $0xf,rINSTbl        # rINST<- A
    GET_VREG_WORD %eax rINST 0     # eax<- v[AA+0]
    sarl      $4,%ecx             # ecx<- B
    SPILL(rIBASE)
    GET_VREG_WORD rIBASE rINST 1   # rIBASE<- v[AA+1]
    GET_VREG_R %ecx %ecx           # ecx<- vBB
    shrdl     rIBASE,%eax
    sarl      %cl,rIBASE
    testb     $32,%cl
    je        2f
    movl      rIBASE,%eax
    sarl      $31,rIBASE
2:
    SET_VREG_WORD rIBASE rINST 1   # v[AA+1]<- rIBASE
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 1 %ecx
    SET_VREG_WORD %eax rINST 0    # v[AA+0]<- eax
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_USHR_LONG_2ADDR: /* 0xc5 */
/* File: x86/OP_USHR_LONG_2ADDR.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shl-long/2addr vA, vB */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl    rINSTbl,%ecx             # ecx<- BA
    andb      $0xf,rINSTbl            # rINST<- A
    GET_VREG_WORD %eax rINST 0         # eax<- v[AA+0]
    sarl      $4,%ecx                 # ecx<- B
    SPILL(rIBASE)
    GET_VREG_WORD rIBASE rINST 1       # rIBASE<- v[AA+1]
    GET_VREG_R %ecx %ecx               # ecx<- vBB
    shrdl     rIBASE,%eax
    shrl      %cl,rIBASE
    testb     $32,%cl
    je        2f
    movl      rIBASE,%eax
    xorl      rIBASE,rIBASE
2:
    SET_VREG_WORD rIBASE rINST 1       # v[AA+1]<- rIBASE
    FETCH_INST_OPCODE 1 %ecx
    UNSPILL(rIBASE)
    SET_VREG_WORD %eax rINST 0         # v[AA+0]<- eax
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_ADD_FLOAT_2ADDR: /* 0xc6 */
/* File: x86/OP_ADD_FLOAT_2ADDR.S */
/* File: x86/binflop2addr.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx           # ecx<- A+
    andb    $0xf,%cl              # ecx<- A
    flds    (rFP,%ecx,4)          # vAA to fp stack
    sarl    $4,rINST             # rINST<- B
    fadds   (rFP,rINST,4)         # ex: faddp
    FETCH_INST_OPCODE 1 %eax
    ADVANCE_PC 1
    fstps    (rFP,%ecx,4)         # %st to vA
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_SUB_FLOAT_2ADDR: /* 0xc7 */
/* File: x86/OP_SUB_FLOAT_2ADDR.S */
/* File: x86/binflop2addr.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx           # ecx<- A+
    andb    $0xf,%cl              # ecx<- A
    flds    (rFP,%ecx,4)          # vAA to fp stack
    sarl    $4,rINST             # rINST<- B
    fsubs   (rFP,rINST,4)         # ex: faddp
    FETCH_INST_OPCODE 1 %eax
    ADVANCE_PC 1
    fstps    (rFP,%ecx,4)         # %st to vA
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_MUL_FLOAT_2ADDR: /* 0xc8 */
/* File: x86/OP_MUL_FLOAT_2ADDR.S */
/* File: x86/binflop2addr.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx           # ecx<- A+
    andb    $0xf,%cl              # ecx<- A
    flds    (rFP,%ecx,4)          # vAA to fp stack
    sarl    $4,rINST             # rINST<- B
    fmuls   (rFP,rINST,4)         # ex: faddp
    FETCH_INST_OPCODE 1 %eax
    ADVANCE_PC 1
    fstps    (rFP,%ecx,4)         # %st to vA
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_DIV_FLOAT_2ADDR: /* 0xc9 */
/* File: x86/OP_DIV_FLOAT_2ADDR.S */
/* File: x86/binflop2addr.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx           # ecx<- A+
    andb    $0xf,%cl              # ecx<- A
    flds    (rFP,%ecx,4)          # vAA to fp stack
    sarl    $4,rINST             # rINST<- B
    fdivs   (rFP,rINST,4)         # ex: faddp
    FETCH_INST_OPCODE 1 %eax
    ADVANCE_PC 1
    fstps    (rFP,%ecx,4)         # %st to vA
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_REM_FLOAT_2ADDR: /* 0xca */
/* File: x86/OP_REM_FLOAT_2ADDR.S */
    /* rem_float/2addr vA, vB */
    movzx   rINSTbl,%ecx                # ecx<- A+
    sarl    $4,rINST                  # rINST<- B
    flds     (rFP,rINST,4)              # vBB to fp stack
    andb    $0xf,%cl                   # ecx<- A
    flds     (rFP,%ecx,4)               # vAA to fp stack
1:
    fprem
    fstsw     %ax
    sahf
    jp        1b
    fstp      %st(1)
    FETCH_INST_OPCODE 1 %eax
    ADVANCE_PC 1
    fstps    (rFP,%ecx,4)               # %st to vA
    GOTO_NEXT_R %eax

/* ------------------------------ */
.L_OP_ADD_DOUBLE_2ADDR: /* 0xcb */
/* File: x86/OP_ADD_DOUBLE_2ADDR.S */
   /*
    * File: OP_ADD_DOUBLE_2ADDR.S
    */

    movzx       rINSTbl,%ecx            # ecx<- A+
    andb        $0xf,%cl               # ecx<- A
    sarl        $4,rINST               # rINST<- B
    movq        (rFP, rINST, 4), %xmm1  # %xmm1<- vB
    movq        (rFP, %ecx, 4), %xmm0   # %xmm0<- vA
    FETCH_INST_OPCODE 1 %eax
    addsd       %xmm1, %xmm0            # %xmm0<- vA op vB
    ADVANCE_PC 1
    movq        %xmm0, (rFP, %ecx, 4)   # vA<- %xmm0; result
    GOTO_NEXT_R %eax

/* ------------------------------ */
.L_OP_SUB_DOUBLE_2ADDR: /* 0xcc */
/* File: x86/OP_SUB_DOUBLE_2ADDR.S */
   /*
    * File: OP_SUB_DOUBLE_2ADDR.S
    */

    movzx       rINSTbl,%ecx            # ecx<- A+
    andb        $0xf,%cl               # ecx<- A
    sarl        $4,rINST               # rINST<- B
    # TODO: movsd?
    movq        (rFP, rINST, 4), %xmm1  # %xmm1<- vB
    movq        (rFP, %ecx, 4), %xmm0   # %xmm0<- vA
    FETCH_INST_OPCODE 1 %eax
    subsd       %xmm1, %xmm0            # %xmm0<- vA op vB
    ADVANCE_PC 1
    movq        %xmm0, (rFP, %ecx, 4)   # vA<- %xmm0; result
    GOTO_NEXT_R %eax

/* ------------------------------ */
.L_OP_MUL_DOUBLE_2ADDR: /* 0xcd */
/* File: x86/OP_MUL_DOUBLE_2ADDR.S */
   /*
    * File: OP_MUL_DOUBLE_2ADDR.S
    */

    movzx       rINSTbl,%ecx            # ecx<- A+
    andb        $0xf,%cl               # ecx<- A
    sarl        $4,rINST               # rINST<- B
    # TODO: movsd?
    movq        (rFP, rINST, 4), %xmm1  # %xmm1<- vB
    movq        (rFP, %ecx, 4), %xmm0   # %xmm0<- vA
    FETCH_INST_OPCODE 1 %eax
    mulsd       %xmm1, %xmm0            # %xmm0<- vA op vB
    ADVANCE_PC 1
    movq        %xmm0, (rFP, %ecx, 4)   # vA<- %xmm0; result
    GOTO_NEXT_R %eax

/* ------------------------------ */
.L_OP_DIV_DOUBLE_2ADDR: /* 0xce */
/* File: x86/OP_DIV_DOUBLE_2ADDR.S */
/* File: x86/binflop2addr.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx           # ecx<- A+
    andb    $0xf,%cl              # ecx<- A
    fldl    (rFP,%ecx,4)          # vAA to fp stack
    sarl    $4,rINST             # rINST<- B
    fdivl   (rFP,rINST,4)         # ex: faddp
    FETCH_INST_OPCODE 1 %eax
    ADVANCE_PC 1
    fstpl    (rFP,%ecx,4)         # %st to vA
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_REM_DOUBLE_2ADDR: /* 0xcf */
/* File: x86/OP_REM_DOUBLE_2ADDR.S */
    /* rem_float/2addr vA, vB */
    movzx   rINSTbl,%ecx                # ecx<- A+
    sarl    $4,rINST                  # rINST<- B
    fldl     (rFP,rINST,4)              # vBB to fp stack
    andb    $0xf,%cl                   # ecx<- A
    fldl     (rFP,%ecx,4)               # vAA to fp stack
1:
    fprem
    fstsw     %ax
    sahf
    jp        1b
    fstp      %st(1)
    FETCH_INST_OPCODE 1 %eax
    ADVANCE_PC 1
    fstpl    (rFP,%ecx,4)               # %st to vA
    GOTO_NEXT_R %eax

/* ------------------------------ */
.L_OP_ADD_INT_LIT16: /* 0xd0 */
/* File: x86/OP_ADD_INT_LIT16.S */
/* File: x86/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than eax, you can override "result".)
     *
     * For: add-int/lit16, rsub-int,
     *      and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    movzbl   rINSTbl,%eax               # eax<- 000000BA
    sarl     $4,%eax                   # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB
    movswl   2(rPC),%ecx                # ecx<- ssssCCCC
    andb     $0xf,rINSTbl              # rINST<- A
    addl %ecx,%eax                              # for example: addl %ecx, %eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_RSUB_INT: /* 0xd1 */
/* File: x86/OP_RSUB_INT.S */
/* File: x86/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than eax, you can override "result".)
     *
     * For: add-int/lit16, rsub-int,
     *      and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    movzbl   rINSTbl,%eax               # eax<- 000000BA
    sarl     $4,%eax                   # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB
    movswl   2(rPC),%ecx                # ecx<- ssssCCCC
    andb     $0xf,rINSTbl              # rINST<- A
    subl %eax,%ecx                              # for example: addl %ecx, %eax
    SET_VREG %ecx rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_MUL_INT_LIT16: /* 0xd2 */
/* File: x86/OP_MUL_INT_LIT16.S */
    /* mul/lit16 vA, vB, #+CCCC */
    /* Need A in rINST, ssssCCCC in ecx, vB in eax */
    movzbl   rINSTbl,%eax               # eax<- 000000BA
    sarl     $4,%eax                   # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB
    movswl   2(rPC),%ecx                # ecx<- ssssCCCC
    andb     $0xf,rINSTbl              # rINST<- A
    SPILL(rIBASE)
    imull     %ecx,%eax                 # trashes rIBASE/edx
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_DIV_INT_LIT16: /* 0xd3 */
/* File: x86/OP_DIV_INT_LIT16.S */
/* File: x86/bindivLit16.S */
    /*
     * 32-bit binary div/rem operation.  Handles special case of op0=minint and
     * op1=-1.
     */
    /* div/rem/lit16 vA, vB, #+CCCC */
    /* Need A in rINST, ssssCCCC in ecx, vB in eax */
    movzbl   rINSTbl,%eax         # eax<- 000000BA
    SPILL(rIBASE)
    sarl     $4,%eax             # eax<- B
    GET_VREG_R %eax %eax          # eax<- vB
    movswl   2(rPC),%ecx          # ecx<- ssssCCCC
    andb     $0xf,rINSTbl        # rINST<- A
    cmpl     $0,%ecx
    je       common_errDivideByZero
    cmpl     $-1,%ecx
    jne      .LOP_DIV_INT_LIT16_continue_div
    cmpl     $0x80000000,%eax
    jne      .LOP_DIV_INT_LIT16_continue_div
    movl     $0x80000000,%eax
    SET_VREG %eax rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_DIV_INT_LIT16_continue_div:
    cltd
    idivl   %ecx
    SET_VREG %eax rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_REM_INT_LIT16: /* 0xd4 */
/* File: x86/OP_REM_INT_LIT16.S */
/* File: x86/bindivLit16.S */
    /*
     * 32-bit binary div/rem operation.  Handles special case of op0=minint and
     * op1=-1.
     */
    /* div/rem/lit16 vA, vB, #+CCCC */
    /* Need A in rINST, ssssCCCC in ecx, vB in eax */
    movzbl   rINSTbl,%eax         # eax<- 000000BA
    SPILL(rIBASE)
    sarl     $4,%eax             # eax<- B
    GET_VREG_R %eax %eax          # eax<- vB
    movswl   2(rPC),%ecx          # ecx<- ssssCCCC
    andb     $0xf,rINSTbl        # rINST<- A
    cmpl     $0,%ecx
    je       common_errDivideByZero
    cmpl     $-1,%ecx
    jne      .LOP_REM_INT_LIT16_continue_div
    cmpl     $0x80000000,%eax
    jne      .LOP_REM_INT_LIT16_continue_div
    movl     $0,rIBASE
    SET_VREG rIBASE rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_REM_INT_LIT16_continue_div:
    cltd
    idivl   %ecx
    SET_VREG rIBASE rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_AND_INT_LIT16: /* 0xd5 */
/* File: x86/OP_AND_INT_LIT16.S */
/* File: x86/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than eax, you can override "result".)
     *
     * For: add-int/lit16, rsub-int,
     *      and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    movzbl   rINSTbl,%eax               # eax<- 000000BA
    sarl     $4,%eax                   # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB
    movswl   2(rPC),%ecx                # ecx<- ssssCCCC
    andb     $0xf,rINSTbl              # rINST<- A
    andl %ecx,%eax                              # for example: addl %ecx, %eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_OR_INT_LIT16: /* 0xd6 */
/* File: x86/OP_OR_INT_LIT16.S */
/* File: x86/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than eax, you can override "result".)
     *
     * For: add-int/lit16, rsub-int,
     *      and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    movzbl   rINSTbl,%eax               # eax<- 000000BA
    sarl     $4,%eax                   # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB
    movswl   2(rPC),%ecx                # ecx<- ssssCCCC
    andb     $0xf,rINSTbl              # rINST<- A
    orl     %ecx,%eax                              # for example: addl %ecx, %eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_XOR_INT_LIT16: /* 0xd7 */
/* File: x86/OP_XOR_INT_LIT16.S */
/* File: x86/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than eax, you can override "result".)
     *
     * For: add-int/lit16, rsub-int,
     *      and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    movzbl   rINSTbl,%eax               # eax<- 000000BA
    sarl     $4,%eax                   # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB
    movswl   2(rPC),%ecx                # ecx<- ssssCCCC
    andb     $0xf,rINSTbl              # rINST<- A
    xor    %ecx,%eax                              # for example: addl %ecx, %eax
    SET_VREG %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_ADD_INT_LIT8: /* 0xd8 */
/* File: x86/OP_ADD_INT_LIT8.S */
/* File: x86/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * For: add-int/lit8, rsub-int/lit8
     *      and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax              # eax<- BB
    movsbl    3(rPC),%ecx              # ecx<- ssssssCC
    GET_VREG_R   %eax %eax             # eax<- rBB
    addl %ecx,%eax                             # ex: addl %ecx,%eax
    SET_VREG   %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_RSUB_INT_LIT8: /* 0xd9 */
/* File: x86/OP_RSUB_INT_LIT8.S */
/* File: x86/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * For: add-int/lit8, rsub-int/lit8
     *      and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax              # eax<- BB
    movsbl    3(rPC),%ecx              # ecx<- ssssssCC
    GET_VREG_R   %eax %eax             # eax<- rBB
    subl  %eax,%ecx                             # ex: addl %ecx,%eax
    SET_VREG   %ecx rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_MUL_INT_LIT8: /* 0xda */
/* File: x86/OP_MUL_INT_LIT8.S */
    /* mul/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax              # eax<- BB
    movsbl    3(rPC),%ecx              # ecx<- ssssssCC
    GET_VREG_R   %eax %eax             # eax<- rBB
    SPILL(rIBASE)
    imull     %ecx,%eax                # trashes rIBASE/edx
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG  %eax rINST
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_DIV_INT_LIT8: /* 0xdb */
/* File: x86/OP_DIV_INT_LIT8.S */
/* File: x86/bindivLit8.S */
    /*
     * 32-bit div/rem "lit8" binary operation.  Handles special case of
     * op0=minint & op1=-1
     */
    /* div/rem/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax        # eax<- BB
    movsbl    3(rPC),%ecx        # ecx<- ssssssCC
    SPILL(rIBASE)
    GET_VREG_R  %eax %eax        # eax<- rBB
    cmpl     $0,%ecx
    je       common_errDivideByZero
    cmpl     $0x80000000,%eax
    jne      .LOP_DIV_INT_LIT8_continue_div
    cmpl     $-1,%ecx
    jne      .LOP_DIV_INT_LIT8_continue_div
    movl     $0x80000000,%eax
    SET_VREG %eax rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_DIV_INT_LIT8_continue_div:
    cltd
    idivl   %ecx
    SET_VREG %eax rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_REM_INT_LIT8: /* 0xdc */
/* File: x86/OP_REM_INT_LIT8.S */
/* File: x86/bindivLit8.S */
    /*
     * 32-bit div/rem "lit8" binary operation.  Handles special case of
     * op0=minint & op1=-1
     */
    /* div/rem/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax        # eax<- BB
    movsbl    3(rPC),%ecx        # ecx<- ssssssCC
    SPILL(rIBASE)
    GET_VREG_R  %eax %eax        # eax<- rBB
    cmpl     $0,%ecx
    je       common_errDivideByZero
    cmpl     $0x80000000,%eax
    jne      .LOP_REM_INT_LIT8_continue_div
    cmpl     $-1,%ecx
    jne      .LOP_REM_INT_LIT8_continue_div
    movl     $0,rIBASE
    SET_VREG rIBASE rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_REM_INT_LIT8_continue_div:
    cltd
    idivl   %ecx
    SET_VREG rIBASE rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_AND_INT_LIT8: /* 0xdd */
/* File: x86/OP_AND_INT_LIT8.S */
/* File: x86/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * For: add-int/lit8, rsub-int/lit8
     *      and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax              # eax<- BB
    movsbl    3(rPC),%ecx              # ecx<- ssssssCC
    GET_VREG_R   %eax %eax             # eax<- rBB
    andl %ecx,%eax                             # ex: addl %ecx,%eax
    SET_VREG   %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_OR_INT_LIT8: /* 0xde */
/* File: x86/OP_OR_INT_LIT8.S */
/* File: x86/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * For: add-int/lit8, rsub-int/lit8
     *      and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax              # eax<- BB
    movsbl    3(rPC),%ecx              # ecx<- ssssssCC
    GET_VREG_R   %eax %eax             # eax<- rBB
    orl     %ecx,%eax                             # ex: addl %ecx,%eax
    SET_VREG   %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_XOR_INT_LIT8: /* 0xdf */
/* File: x86/OP_XOR_INT_LIT8.S */
/* File: x86/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * For: add-int/lit8, rsub-int/lit8
     *      and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax              # eax<- BB
    movsbl    3(rPC),%ecx              # ecx<- ssssssCC
    GET_VREG_R   %eax %eax             # eax<- rBB
    xor    %ecx,%eax                             # ex: addl %ecx,%eax
    SET_VREG   %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SHL_INT_LIT8: /* 0xe0 */
/* File: x86/OP_SHL_INT_LIT8.S */
/* File: x86/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * For: add-int/lit8, rsub-int/lit8
     *      and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax              # eax<- BB
    movsbl    3(rPC),%ecx              # ecx<- ssssssCC
    GET_VREG_R   %eax %eax             # eax<- rBB
    sall  %cl,%eax                             # ex: addl %ecx,%eax
    SET_VREG   %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SHR_INT_LIT8: /* 0xe1 */
/* File: x86/OP_SHR_INT_LIT8.S */
/* File: x86/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * For: add-int/lit8, rsub-int/lit8
     *      and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax              # eax<- BB
    movsbl    3(rPC),%ecx              # ecx<- ssssssCC
    GET_VREG_R   %eax %eax             # eax<- rBB
    sarl    %cl,%eax                             # ex: addl %ecx,%eax
    SET_VREG   %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_USHR_INT_LIT8: /* 0xe2 */
/* File: x86/OP_USHR_INT_LIT8.S */
/* File: x86/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = eax op ecx".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * For: add-int/lit8, rsub-int/lit8
     *      and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax              # eax<- BB
    movsbl    3(rPC),%ecx              # ecx<- ssssssCC
    GET_VREG_R   %eax %eax             # eax<- rBB
    shrl     %cl,%eax                             # ex: addl %ecx,%eax
    SET_VREG   %eax rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_IGET_VOLATILE: /* 0xe3 */
/* File: x86/OP_IGET_VOLATILE.S */
/* File: x86/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)                               # preserve rIBASE
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IGET_VOLATILE_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)               # needed by dvmResolveInstField
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           #  returns InstrField ptr
    jne     .LOP_IGET_VOLATILE_finish
    jmp     common_exceptionThrown

.LOP_IGET_VOLATILE_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    movl   (%ecx,%eax,1),%ecx                  # ecx<- obj.field (8/16/32 bits)
    FETCH_INST_OPCODE 2 %eax
    UNSPILL(rIBASE)
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_IPUT_VOLATILE: /* 0xe4 */
/* File: x86/OP_IPUT_VOLATILE.S */
/* File: x86/OP_IPUT.S */

    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-object, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL   (rIBASE)
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IPUT_VOLATILE_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           # returns InstrField ptr
    jne     .LOP_IPUT_VOLATILE_finish
    jmp     common_exceptionThrown

.LOP_IPUT_VOLATILE_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    GET_VREG_R rINST rINST                       # rINST<- v[A]
    movl    offInstField_byteOffset(%eax),%eax   # eax<- byte offset of field
    testl   %ecx,%ecx                            # object null?
    je      common_errNullObject                 # object was null
    movl   rINST,(%ecx,%eax,1)            # obj.field <- v[A](8/16/32 bits)
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SGET_VOLATILE: /* 0xe5 */
/* File: x86/OP_SGET_VOLATILE.S */
/* File: x86/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SGET_VOLATILE_resolve                # if not, make it so
.LOP_SGET_VOLATILE_finish:     # field ptr in eax
    movl      offStaticField_value(%eax),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SGET_VOLATILE_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SGET_VOLATILE_finish                 # success, continue


/* ------------------------------ */
.L_OP_SPUT_VOLATILE: /* 0xe6 */
/* File: x86/OP_SPUT_VOLATILE.S */
/* File: x86/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SPUT_VOLATILE_resolve                # if not, make it so
.LOP_SPUT_VOLATILE_finish:     # field ptr in eax
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    movl      rINST,offStaticField_value(%eax)
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SPUT_VOLATILE_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx        # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SPUT_VOLATILE_finish                 # success, continue

/* ------------------------------ */
.L_OP_IGET_OBJECT_VOLATILE: /* 0xe7 */
/* File: x86/OP_IGET_OBJECT_VOLATILE.S */
/* File: x86/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)                               # preserve rIBASE
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IGET_OBJECT_VOLATILE_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)               # needed by dvmResolveInstField
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           #  returns InstrField ptr
    jne     .LOP_IGET_OBJECT_VOLATILE_finish
    jmp     common_exceptionThrown

.LOP_IGET_OBJECT_VOLATILE_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    movl   (%ecx,%eax,1),%ecx                  # ecx<- obj.field (8/16/32 bits)
    FETCH_INST_OPCODE 2 %eax
    UNSPILL(rIBASE)
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax


/* ------------------------------ */
.L_OP_IGET_WIDE_VOLATILE: /* 0xe8 */
    /* (stub) */
    SAVE_PC_FP_TO_SELF %ecx          # leaves rSELF in %ecx
    movl %ecx,OUT_ARG0(%esp)         # self is first arg to function
    call      dvmMterp_OP_IGET_WIDE_VOLATILE     # do the real work
    movl      rSELF,%ecx
    LOAD_PC_FP_FROM_SELF             # retrieve updated values
    movl      offThread_curHandlerTable(%ecx),rIBASE  # set up rIBASE
    FETCH_INST
    GOTO_NEXT
/* ------------------------------ */
.L_OP_IPUT_WIDE_VOLATILE: /* 0xe9 */
    /* (stub) */
    SAVE_PC_FP_TO_SELF %ecx          # leaves rSELF in %ecx
    movl %ecx,OUT_ARG0(%esp)         # self is first arg to function
    call      dvmMterp_OP_IPUT_WIDE_VOLATILE     # do the real work
    movl      rSELF,%ecx
    LOAD_PC_FP_FROM_SELF             # retrieve updated values
    movl      offThread_curHandlerTable(%ecx),rIBASE  # set up rIBASE
    FETCH_INST
    GOTO_NEXT
/* ------------------------------ */
.L_OP_SGET_WIDE_VOLATILE: /* 0xea */
    /* (stub) */
    SAVE_PC_FP_TO_SELF %ecx          # leaves rSELF in %ecx
    movl %ecx,OUT_ARG0(%esp)         # self is first arg to function
    call      dvmMterp_OP_SGET_WIDE_VOLATILE     # do the real work
    movl      rSELF,%ecx
    LOAD_PC_FP_FROM_SELF             # retrieve updated values
    movl      offThread_curHandlerTable(%ecx),rIBASE  # set up rIBASE
    FETCH_INST
    GOTO_NEXT
/* ------------------------------ */
.L_OP_SPUT_WIDE_VOLATILE: /* 0xeb */
    /* (stub) */
    SAVE_PC_FP_TO_SELF %ecx          # leaves rSELF in %ecx
    movl %ecx,OUT_ARG0(%esp)         # self is first arg to function
    call      dvmMterp_OP_SPUT_WIDE_VOLATILE     # do the real work
    movl      rSELF,%ecx
    LOAD_PC_FP_FROM_SELF             # retrieve updated values
    movl      offThread_curHandlerTable(%ecx),rIBASE  # set up rIBASE
    FETCH_INST
    GOTO_NEXT
/* ------------------------------ */
.L_OP_BREAKPOINT: /* 0xec */
/* File: x86/OP_BREAKPOINT.S */
    /*
     * Breakpoint handler.
     *
     * Restart this instruction with the original opcode.  By
     * the time we get here, the breakpoint will have already been
     * handled.  We also assume that all other special "checkBefore"
     * actions have been handled, so we'll transition directly
     * to the real handler
     */
    SPILL(rIBASE)
    movl    rPC,OUT_ARG0(%esp)
    call    dvmGetOriginalOpcode
    UNSPILL(rIBASE)
    movl    rSELF,%ecx
    movzbl  1(rPC),rINST
    movl    offThread_mainHandlerTable(%ecx),%ecx
    jmp     *(%ecx,%eax,4)


/* ------------------------------ */
.L_OP_THROW_VERIFICATION_ERROR: /* 0xed */
/* File: x86/OP_THROW_VERIFICATION_ERROR.S */
    /*
     * Handle a throw-verification-error instruction.  This throws an
     * exception for an error discovered during verification.  The
     * exception is indicated by AA, with some detail provided by BBBB.
     */
    /* op AA, ref@BBBB */
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                     # eax<- BBBB
    movl     offThread_method(%ecx),%ecx       # ecx<- self->method
    EXPORT_PC
    movl     %eax,OUT_ARG2(%esp)             # arg2<- BBBB
    movl     rINST,OUT_ARG1(%esp)            # arg1<- AA
    movl     %ecx,OUT_ARG0(%esp)             # arg0<- method
    call     dvmThrowVerificationError       # call(method, kind, ref)
    jmp      common_exceptionThrown          # handle exception

/* ------------------------------ */
.L_OP_EXECUTE_INLINE: /* 0xee */
/* File: x86/OP_EXECUTE_INLINE.S */
    /*
     * Execute a "native inline" instruction.
     *
     * We will be calling through a function table:
     *
     * (*gDvmInlineOpsTable[opIndex].func)(arg0, arg1, arg2, arg3, pResult)
     *
     * Ignores argument count - always loads 4.
     *
     */
    /* [opt] execute-inline vAA, {vC, vD, vE, vF}, inline@BBBB */
    movl      rSELF,%ecx
    EXPORT_PC
    movzwl    2(rPC),%eax               # eax<- BBBB
    SPILL(rIBASE)                       # preserve rIBASE
    movl      offThread_subMode(%ecx), %edx # edx<- submode flags
    andl      $kSubModeDebugProfile, %edx # debug or profile mode active?
    jnz       .LOP_EXECUTE_INLINE_debugprofile   # yes, take slow path
.LOP_EXECUTE_INLINE_resume:
    leal      offThread_retval(%ecx),%ecx # ecx<- &self->retval
    movl      %ecx,OUT_ARG4(%esp)
    call      .LOP_EXECUTE_INLINE_continue      # make call; will return after
    UNSPILL(rIBASE)                     # restore rIBASE
    testl     %eax,%eax                 # successful?
    jz        common_exceptionThrown    # no, handle exception
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx

.LOP_EXECUTE_INLINE_continue:
    /*
     * Extract args, call function.
     *  ecx = #of args (0-4)
     *  eax = call index
     *  @esp = return addr
     *  esp is -4 from normal
     *
     *  Go ahead and load all 4 args, even if not used.
     */
    movzwl    4(rPC),rIBASE

    movl      $0xf,%ecx
    andl      rIBASE,%ecx
    GET_VREG_R  %ecx %ecx
    sarl      $4,rIBASE
    movl      %ecx,4+OUT_ARG0(%esp)

    movl      $0xf,%ecx
    andl      rIBASE,%ecx
    GET_VREG_R  %ecx %ecx
    sarl      $4,rIBASE
    movl      %ecx,4+OUT_ARG1(%esp)

    movl      $0xf,%ecx
    andl      rIBASE,%ecx
    GET_VREG_R  %ecx %ecx
    sarl      $4,rIBASE
    movl      %ecx,4+OUT_ARG2(%esp)

    movl      $0xf,%ecx
    andl      rIBASE,%ecx
    GET_VREG_R  %ecx %ecx
    sarl      $4,rIBASE
    movl      %ecx,4+OUT_ARG3(%esp)

    sall      $4,%eax      # index *= sizeof(table entry)
    jmp       *gDvmInlineOpsTable(%eax)
    # will return to caller of .LOP_EXECUTE_INLINE_continue

    /*
     * We're debugging or profiling.
     * eax: opIndex
     */
.LOP_EXECUTE_INLINE_debugprofile:
    movl      %eax,OUT_ARG0(%esp)       # arg0<- BBBB
    SPILL_TMP1(%eax)                    # save opIndex
    call      dvmResolveInlineNative    # dvmResolveInlineNative(opIndex)
    movl      rSELF,%ecx                # restore self
    testl     %eax,%eax                 # method resolved?
    movl      %eax,%edx                 # save possibly resolved method in edx
    UNSPILL_TMP1(%eax)                  # in case not resolved, restore opIndex
    jz        .LOP_EXECUTE_INLINE_resume        # not resolved, just move on
    SPILL_TMP2(%edx)                    # save method
    movl      %edx,OUT_ARG0(%esp)       # arg0<- method
    movl      %ecx,OUT_ARG1(%esp)       # arg1<- self
    call      dvmFastMethodTraceEnter   # dvmFastMethodTraceEnter(method,self)
    movl      rSELF,%ecx                # restore self
    UNSPILL_TMP1(%eax)                  # restore opIndex
    leal      offThread_retval(%ecx),%ecx # ecx<- &self->retval
    movl      %ecx,OUT_ARG4(%esp)       # needed for pResult of inline operation handler
    call      .LOP_EXECUTE_INLINE_continue      # make call; will return after
    SPILL_TMP1(%eax)                    # save result of inline
    UNSPILL_TMP2(%eax)                  # restore method
    movl      rSELF,%ecx                # restore self
    movl      %eax,OUT_ARG0(%esp)       # arg0<- method
    movl      %ecx,OUT_ARG1(%esp)       # arg1<- self
    call      dvmFastNativeMethodTraceExit # dvmFastNativeMethodTraceExit(method,self)
    UNSPILL(rIBASE)                     # restore rIBASE
    UNSPILL_TMP1(%eax)                  # restore result of inline
    testl     %eax,%eax                 # successful?
    jz        common_exceptionThrown    # no, handle exception
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_EXECUTE_INLINE_RANGE: /* 0xef */
    /* (stub) */
    SAVE_PC_FP_TO_SELF %ecx          # leaves rSELF in %ecx
    movl %ecx,OUT_ARG0(%esp)         # self is first arg to function
    call      dvmMterp_OP_EXECUTE_INLINE_RANGE     # do the real work
    movl      rSELF,%ecx
    LOAD_PC_FP_FROM_SELF             # retrieve updated values
    movl      offThread_curHandlerTable(%ecx),rIBASE  # set up rIBASE
    FETCH_INST
    GOTO_NEXT
/* ------------------------------ */
.L_OP_INVOKE_OBJECT_INIT_RANGE: /* 0xf0 */
    /* (stub) */
    SAVE_PC_FP_TO_SELF %ecx          # leaves rSELF in %ecx
    movl %ecx,OUT_ARG0(%esp)         # self is first arg to function
    call      dvmMterp_OP_INVOKE_OBJECT_INIT_RANGE     # do the real work
    movl      rSELF,%ecx
    LOAD_PC_FP_FROM_SELF             # retrieve updated values
    movl      offThread_curHandlerTable(%ecx),rIBASE  # set up rIBASE
    FETCH_INST
    GOTO_NEXT
/* ------------------------------ */
.L_OP_RETURN_VOID_BARRIER: /* 0xf1 */
    /* (stub) */
    SAVE_PC_FP_TO_SELF %ecx          # leaves rSELF in %ecx
    movl %ecx,OUT_ARG0(%esp)         # self is first arg to function
    call      dvmMterp_OP_RETURN_VOID_BARRIER     # do the real work
    movl      rSELF,%ecx
    LOAD_PC_FP_FROM_SELF             # retrieve updated values
    movl      offThread_curHandlerTable(%ecx),rIBASE  # set up rIBASE
    FETCH_INST
    GOTO_NEXT
/* ------------------------------ */
.L_OP_IGET_QUICK: /* 0xf2 */
/* File: x86/OP_IGET_QUICK.S */
    /* For: iget-quick, iget-object-quick */
    /* op vA, vB, offset@CCCC */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $4,%ecx                  # ecx<- B
    GET_VREG_R  %ecx %ecx               # vB (object we're operating on)
    movzwl    2(rPC),%eax               # eax<- field byte offset
    cmpl      $0,%ecx                  # is object null?
    je        common_errNullObject
    movl      (%ecx,%eax,1),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    andb      $0xf,rINSTbl             # rINST<- A
    SET_VREG  %eax rINST                # fp[A]<- result
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_IGET_WIDE_QUICK: /* 0xf3 */
/* File: x86/OP_IGET_WIDE_QUICK.S */
    /* For: iget-wide-quick */
    /* op vA, vB, offset@CCCC */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $4,%ecx                  # ecx<- B
    GET_VREG_R  %ecx %ecx               # vB (object we're operating on)
    movzwl    2(rPC),%eax               # eax<- field byte offset
    cmpl      $0,%ecx                  # is object null?
    je        common_errNullObject
    leal      (%ecx,%eax,1),%eax        # eax<- address of 64-bit source
    movl      (%eax),%ecx               # ecx<- lsw
    movl      4(%eax),%eax              # eax<- msw
    andb      $0xf,rINSTbl             # rINST<- A
    SET_VREG_WORD %ecx rINST 0          # v[A+0]<- lsw
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG_WORD %eax rINST 1          # v[A+1]<- msw
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_IGET_OBJECT_QUICK: /* 0xf4 */
/* File: x86/OP_IGET_OBJECT_QUICK.S */
/* File: x86/OP_IGET_QUICK.S */
    /* For: iget-quick, iget-object-quick */
    /* op vA, vB, offset@CCCC */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $4,%ecx                  # ecx<- B
    GET_VREG_R  %ecx %ecx               # vB (object we're operating on)
    movzwl    2(rPC),%eax               # eax<- field byte offset
    cmpl      $0,%ecx                  # is object null?
    je        common_errNullObject
    movl      (%ecx,%eax,1),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    andb      $0xf,rINSTbl             # rINST<- A
    SET_VREG  %eax rINST                # fp[A]<- result
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_IPUT_QUICK: /* 0xf5 */
/* File: x86/OP_IPUT_QUICK.S */
    /* For: iput-quick */
    /* op vA, vB, offset@CCCC */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $4,%ecx                  # ecx<- B
    GET_VREG_R  %ecx %ecx               # vB (object we're operating on)
    andb      $0xf,rINSTbl             # rINST<- A
    GET_VREG_R  rINST,rINST             # rINST<- v[A]
    movzwl    2(rPC),%eax               # eax<- field byte offset
    testl     %ecx,%ecx                 # is object null?
    je        common_errNullObject
    movl      rINST,(%ecx,%eax,1)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_IPUT_WIDE_QUICK: /* 0xf6 */
/* File: x86/OP_IPUT_WIDE_QUICK.S */
    /* For: iput-wide-quick */
    /* op vA, vB, offset@CCCC */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $4,%ecx                  # ecx<- B
    GET_VREG_R  %ecx %ecx               # vB (object we're operating on)
    movzwl    2(rPC),%eax               # eax<- field byte offset
    testl      %ecx,%ecx                # is object null?
    je        common_errNullObject
    leal      (%ecx,%eax,1),%ecx        # ecx<- Address of 64-bit target
    andb      $0xf,rINSTbl             # rINST<- A
    GET_VREG_WORD %eax rINST 0          # eax<- lsw
    GET_VREG_WORD rINST rINST 1         # rINST<- msw
    movl      %eax,(%ecx)
    movl      rINST,4(%ecx)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_IPUT_OBJECT_QUICK: /* 0xf7 */
/* File: x86/OP_IPUT_OBJECT_QUICK.S */
    /* For: iput-object-quick */
    /* op vA, vB, offset@CCCC */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $4,%ecx                  # ecx<- B
    GET_VREG_R  %ecx %ecx               # vB (object we're operating on)
    andb      $0xf,rINSTbl             # rINST<- A
    GET_VREG_R  rINST rINST             # rINST<- v[A]
    movzwl    2(rPC),%eax               # eax<- field byte offset
    testl     %ecx,%ecx                 # is object null?
    je        common_errNullObject
    movl      rINST,(%ecx,%eax,1)
    movl      rSELF,%eax
    testl     rINST,rINST               # did we store null?
    movl      offThread_cardTable(%eax),%eax  # get card table base
    je        1f                            # skip card mark if null store
    shrl      $GC_CARD_SHIFT,%ecx          # object head to card number
    movb      %al,(%eax,%ecx)               # mark card based on object head
1:
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

/* ------------------------------ */
.L_OP_INVOKE_VIRTUAL_QUICK: /* 0xf8 */
/* File: x86/OP_INVOKE_VIRTUAL_QUICK.S */
    /*
     * Handle an optimized virtual method call.
     *
     * for: [opt] invoke-virtual-quick, invoke-virtual-quick/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movzwl    4(rPC),%ecx               # eax<- FEDC or CCCC
    movzwl    2(rPC),%edx               # ecx<- BBBB
    .if     (!0)
    andl      $0xf,%ecx                # eax<- C (or stays CCCC)
    .endif
    GET_VREG_R  %ecx %ecx               # ecx<- vC ("this" ptr)
    testl     %ecx,%ecx                 # null?
    je        common_errNullObject      # yep, throw exception
    movl      offObject_clazz(%ecx),%eax # eax<- thisPtr->clazz
    movl      offClassObject_vtable(%eax),%eax # eax<- thisPtr->clazz->vtable
    EXPORT_PC                           # might throw later - get ready
    movl      (%eax,%edx,4),%eax        # eax<- vtable[BBBB]
    jmp       common_invokeMethodNoRange

/* ------------------------------ */
.L_OP_INVOKE_VIRTUAL_QUICK_RANGE: /* 0xf9 */
/* File: x86/OP_INVOKE_VIRTUAL_QUICK_RANGE.S */
/* File: x86/OP_INVOKE_VIRTUAL_QUICK.S */
    /*
     * Handle an optimized virtual method call.
     *
     * for: [opt] invoke-virtual-quick, invoke-virtual-quick/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movzwl    4(rPC),%ecx               # eax<- FEDC or CCCC
    movzwl    2(rPC),%edx               # ecx<- BBBB
    .if     (!1)
    andl      $0xf,%ecx                # eax<- C (or stays CCCC)
    .endif
    GET_VREG_R  %ecx %ecx               # ecx<- vC ("this" ptr)
    testl     %ecx,%ecx                 # null?
    je        common_errNullObject      # yep, throw exception
    movl      offObject_clazz(%ecx),%eax # eax<- thisPtr->clazz
    movl      offClassObject_vtable(%eax),%eax # eax<- thisPtr->clazz->vtable
    EXPORT_PC                           # might throw later - get ready
    movl      (%eax,%edx,4),%eax        # eax<- vtable[BBBB]
    jmp       common_invokeMethodRange


/* ------------------------------ */
.L_OP_INVOKE_SUPER_QUICK: /* 0xfa */
/* File: x86/OP_INVOKE_SUPER_QUICK.S */
    /*
     * Handle an optimized "super" method call.
     *
     * for: [opt] invoke-super-quick, invoke-super-quick/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,%ecx
    movzwl    4(rPC),%eax               # eax<- GFED or CCCC
    movl      offThread_method(%ecx),%ecx # ecx<- current method
    .if       (!0)
    andl      $0xf,%eax                # eax<- D (or stays CCCC)
    .endif
    movl      offMethod_clazz(%ecx),%ecx # ecx<- method->clazz
    GET_VREG_R  %eax %eax               # eax<- "this"
    movl      offClassObject_super(%ecx),%ecx # ecx<- method->clazz->super
    testl     %eax,%eax                 # null "this"?
    je        common_errNullObject      # "this" is null, throw exception
    movl       %eax, TMP_SPILL1(%ebp)
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offClassObject_vtable(%ecx),%ecx # ecx<- vtable
    EXPORT_PC
    movl      (%ecx,%eax,4),%eax        # eax<- super->vtable[BBBB]
    movl      TMP_SPILL1(%ebp), %ecx
    jmp       common_invokeMethodNoRange

/* ------------------------------ */
.L_OP_INVOKE_SUPER_QUICK_RANGE: /* 0xfb */
/* File: x86/OP_INVOKE_SUPER_QUICK_RANGE.S */
/* File: x86/OP_INVOKE_SUPER_QUICK.S */
    /*
     * Handle an optimized "super" method call.
     *
     * for: [opt] invoke-super-quick, invoke-super-quick/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,%ecx
    movzwl    4(rPC),%eax               # eax<- GFED or CCCC
    movl      offThread_method(%ecx),%ecx # ecx<- current method
    .if       (!1)
    andl      $0xf,%eax                # eax<- D (or stays CCCC)
    .endif
    movl      offMethod_clazz(%ecx),%ecx # ecx<- method->clazz
    GET_VREG_R  %eax %eax               # eax<- "this"
    movl      offClassObject_super(%ecx),%ecx # ecx<- method->clazz->super
    testl     %eax,%eax                 # null "this"?
    je        common_errNullObject      # "this" is null, throw exception
    movl       %eax, TMP_SPILL1(%ebp)
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offClassObject_vtable(%ecx),%ecx # ecx<- vtable
    EXPORT_PC
    movl      (%ecx,%eax,4),%eax        # eax<- super->vtable[BBBB]
    movl      TMP_SPILL1(%ebp), %ecx
    jmp       common_invokeMethodRange


/* ------------------------------ */
.L_OP_IPUT_OBJECT_VOLATILE: /* 0xfc */
/* File: x86/OP_IPUT_OBJECT_VOLATILE.S */
/* File: x86/OP_IPUT_OBJECT.S */
    /*
     * Object field put.
     *
     * for: iput-object
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                  # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .LOP_IPUT_OBJECT_VOLATILE_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save obj pointer across call
    movl    rIBASE,OUT_ARG0(%esp)               # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           # returns InstrField ptr
    jne     .LOP_IPUT_OBJECT_VOLATILE_finish
    jmp     common_exceptionThrown

.LOP_IPUT_OBJECT_VOLATILE_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rIBASE is scratch, but needs to be unspilled
     *   rINST holds A
     */
    GET_VREG_R rINST rINST                      # rINST<- v[A]
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    movl    rINST,(%ecx,%eax)      # obj.field <- v[A](8/16/32 bits)
    movl    rSELF,%eax
    testl   rINST,rINST                         # stored a NULL?
    movl    offThread_cardTable(%eax),%eax      # get card table base
    je      1f                                  # skip card mark if null store
    shrl    $GC_CARD_SHIFT,%ecx                # object head to card number
    movb    %al,(%eax,%ecx)                     # mark card using object head
1:
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx


/* ------------------------------ */
.L_OP_SGET_OBJECT_VOLATILE: /* 0xfd */
/* File: x86/OP_SGET_OBJECT_VOLATILE.S */
/* File: x86/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SGET_OBJECT_VOLATILE_resolve                # if not, make it so
.LOP_SGET_OBJECT_VOLATILE_finish:     # field ptr in eax
    movl      offStaticField_value(%eax),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.LOP_SGET_OBJECT_VOLATILE_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SGET_OBJECT_VOLATILE_finish                 # success, continue


/* ------------------------------ */
.L_OP_SPUT_OBJECT_VOLATILE: /* 0xfe */
/* File: x86/OP_SPUT_OBJECT_VOLATILE.S */
/* File: x86/OP_SPUT_OBJECT.S */
    /*
     * SPUT object handler.
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField
    testl     %eax,%eax                          # resolved entry null?
    je        .LOP_SPUT_OBJECT_VOLATILE_resolve                # if not, make it so
.LOP_SPUT_OBJECT_VOLATILE_finish:                              # field ptr in eax
    movzbl    rINSTbl,%ecx                       # ecx<- AA
    GET_VREG_R  %ecx %ecx
    movl      %ecx,offStaticField_value(%eax)    # do the store
    testl     %ecx,%ecx                          # stored null object ptr?
    je        1f                                 # skip card mark if null
    movl      rSELF,%ecx
    movl      offField_clazz(%eax),%eax          # eax<- method->clazz
    movl      offThread_cardTable(%ecx),%ecx       # get card table base
    shrl      $GC_CARD_SHIFT,%eax               # head to card number
    movb      %cl,(%ecx,%eax)                    # mark card
1:
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.LOP_SPUT_OBJECT_VOLATILE_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .LOP_SPUT_OBJECT_VOLATILE_finish                 # success, continue


/* ------------------------------ */
.L_OP_UNUSED_FF: /* 0xff */
/* File: x86/OP_UNUSED_FF.S */
/* File: x86/unused.S */
    jmp     common_abort


    .size   dvmAsmInstructionStartCode, .-dvmAsmInstructionStartCode
    .global dvmAsmInstructionEndCode
dvmAsmInstructionEndCode:

    .global dvmAsmAltInstructionStartCode
    .type   dvmAsmAltInstructionStartCode, %function
    .text

dvmAsmAltInstructionStartCode = .L_ALT_OP_NOP
/* ------------------------------ */
.L_ALT_OP_NOP: /* 0x00 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(0*4)

/* ------------------------------ */
.L_ALT_OP_MOVE: /* 0x01 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(1*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_FROM16: /* 0x02 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(2*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_16: /* 0x03 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(3*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_WIDE: /* 0x04 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(4*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_WIDE_FROM16: /* 0x05 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(5*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_WIDE_16: /* 0x06 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(6*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_OBJECT: /* 0x07 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(7*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_OBJECT_FROM16: /* 0x08 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(8*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_OBJECT_16: /* 0x09 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(9*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_RESULT: /* 0x0a */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(10*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_RESULT_WIDE: /* 0x0b */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(11*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_RESULT_OBJECT: /* 0x0c */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(12*4)

/* ------------------------------ */
.L_ALT_OP_MOVE_EXCEPTION: /* 0x0d */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(13*4)

/* ------------------------------ */
.L_ALT_OP_RETURN_VOID: /* 0x0e */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(14*4)

/* ------------------------------ */
.L_ALT_OP_RETURN: /* 0x0f */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(15*4)

/* ------------------------------ */
.L_ALT_OP_RETURN_WIDE: /* 0x10 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(16*4)

/* ------------------------------ */
.L_ALT_OP_RETURN_OBJECT: /* 0x11 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(17*4)

/* ------------------------------ */
.L_ALT_OP_CONST_4: /* 0x12 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(18*4)

/* ------------------------------ */
.L_ALT_OP_CONST_16: /* 0x13 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(19*4)

/* ------------------------------ */
.L_ALT_OP_CONST: /* 0x14 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(20*4)

/* ------------------------------ */
.L_ALT_OP_CONST_HIGH16: /* 0x15 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(21*4)

/* ------------------------------ */
.L_ALT_OP_CONST_WIDE_16: /* 0x16 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(22*4)

/* ------------------------------ */
.L_ALT_OP_CONST_WIDE_32: /* 0x17 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(23*4)

/* ------------------------------ */
.L_ALT_OP_CONST_WIDE: /* 0x18 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(24*4)

/* ------------------------------ */
.L_ALT_OP_CONST_WIDE_HIGH16: /* 0x19 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(25*4)

/* ------------------------------ */
.L_ALT_OP_CONST_STRING: /* 0x1a */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(26*4)

/* ------------------------------ */
.L_ALT_OP_CONST_STRING_JUMBO: /* 0x1b */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(27*4)

/* ------------------------------ */
.L_ALT_OP_CONST_CLASS: /* 0x1c */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(28*4)

/* ------------------------------ */
.L_ALT_OP_MONITOR_ENTER: /* 0x1d */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(29*4)

/* ------------------------------ */
.L_ALT_OP_MONITOR_EXIT: /* 0x1e */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(30*4)

/* ------------------------------ */
.L_ALT_OP_CHECK_CAST: /* 0x1f */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(31*4)

/* ------------------------------ */
.L_ALT_OP_INSTANCE_OF: /* 0x20 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(32*4)

/* ------------------------------ */
.L_ALT_OP_ARRAY_LENGTH: /* 0x21 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(33*4)

/* ------------------------------ */
.L_ALT_OP_NEW_INSTANCE: /* 0x22 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(34*4)

/* ------------------------------ */
.L_ALT_OP_NEW_ARRAY: /* 0x23 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(35*4)

/* ------------------------------ */
.L_ALT_OP_FILLED_NEW_ARRAY: /* 0x24 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(36*4)

/* ------------------------------ */
.L_ALT_OP_FILLED_NEW_ARRAY_RANGE: /* 0x25 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(37*4)

/* ------------------------------ */
.L_ALT_OP_FILL_ARRAY_DATA: /* 0x26 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(38*4)

/* ------------------------------ */
.L_ALT_OP_THROW: /* 0x27 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(39*4)

/* ------------------------------ */
.L_ALT_OP_GOTO: /* 0x28 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(40*4)

/* ------------------------------ */
.L_ALT_OP_GOTO_16: /* 0x29 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(41*4)

/* ------------------------------ */
.L_ALT_OP_GOTO_32: /* 0x2a */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(42*4)

/* ------------------------------ */
.L_ALT_OP_PACKED_SWITCH: /* 0x2b */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(43*4)

/* ------------------------------ */
.L_ALT_OP_SPARSE_SWITCH: /* 0x2c */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(44*4)

/* ------------------------------ */
.L_ALT_OP_CMPL_FLOAT: /* 0x2d */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(45*4)

/* ------------------------------ */
.L_ALT_OP_CMPG_FLOAT: /* 0x2e */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(46*4)

/* ------------------------------ */
.L_ALT_OP_CMPL_DOUBLE: /* 0x2f */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(47*4)

/* ------------------------------ */
.L_ALT_OP_CMPG_DOUBLE: /* 0x30 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(48*4)

/* ------------------------------ */
.L_ALT_OP_CMP_LONG: /* 0x31 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(49*4)

/* ------------------------------ */
.L_ALT_OP_IF_EQ: /* 0x32 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(50*4)

/* ------------------------------ */
.L_ALT_OP_IF_NE: /* 0x33 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(51*4)

/* ------------------------------ */
.L_ALT_OP_IF_LT: /* 0x34 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(52*4)

/* ------------------------------ */
.L_ALT_OP_IF_GE: /* 0x35 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(53*4)

/* ------------------------------ */
.L_ALT_OP_IF_GT: /* 0x36 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(54*4)

/* ------------------------------ */
.L_ALT_OP_IF_LE: /* 0x37 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(55*4)

/* ------------------------------ */
.L_ALT_OP_IF_EQZ: /* 0x38 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(56*4)

/* ------------------------------ */
.L_ALT_OP_IF_NEZ: /* 0x39 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(57*4)

/* ------------------------------ */
.L_ALT_OP_IF_LTZ: /* 0x3a */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(58*4)

/* ------------------------------ */
.L_ALT_OP_IF_GEZ: /* 0x3b */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(59*4)

/* ------------------------------ */
.L_ALT_OP_IF_GTZ: /* 0x3c */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(60*4)

/* ------------------------------ */
.L_ALT_OP_IF_LEZ: /* 0x3d */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(61*4)

/* ------------------------------ */
.L_ALT_OP_UNUSED_3E: /* 0x3e */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(62*4)

/* ------------------------------ */
.L_ALT_OP_UNUSED_3F: /* 0x3f */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(63*4)

/* ------------------------------ */
.L_ALT_OP_UNUSED_40: /* 0x40 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(64*4)

/* ------------------------------ */
.L_ALT_OP_UNUSED_41: /* 0x41 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(65*4)

/* ------------------------------ */
.L_ALT_OP_UNUSED_42: /* 0x42 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(66*4)

/* ------------------------------ */
.L_ALT_OP_UNUSED_43: /* 0x43 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(67*4)

/* ------------------------------ */
.L_ALT_OP_AGET: /* 0x44 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(68*4)

/* ------------------------------ */
.L_ALT_OP_AGET_WIDE: /* 0x45 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(69*4)

/* ------------------------------ */
.L_ALT_OP_AGET_OBJECT: /* 0x46 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(70*4)

/* ------------------------------ */
.L_ALT_OP_AGET_BOOLEAN: /* 0x47 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(71*4)

/* ------------------------------ */
.L_ALT_OP_AGET_BYTE: /* 0x48 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(72*4)

/* ------------------------------ */
.L_ALT_OP_AGET_CHAR: /* 0x49 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(73*4)

/* ------------------------------ */
.L_ALT_OP_AGET_SHORT: /* 0x4a */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(74*4)

/* ------------------------------ */
.L_ALT_OP_APUT: /* 0x4b */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(75*4)

/* ------------------------------ */
.L_ALT_OP_APUT_WIDE: /* 0x4c */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(76*4)

/* ------------------------------ */
.L_ALT_OP_APUT_OBJECT: /* 0x4d */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(77*4)

/* ------------------------------ */
.L_ALT_OP_APUT_BOOLEAN: /* 0x4e */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(78*4)

/* ------------------------------ */
.L_ALT_OP_APUT_BYTE: /* 0x4f */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(79*4)

/* ------------------------------ */
.L_ALT_OP_APUT_CHAR: /* 0x50 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(80*4)

/* ------------------------------ */
.L_ALT_OP_APUT_SHORT: /* 0x51 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(81*4)

/* ------------------------------ */
.L_ALT_OP_IGET: /* 0x52 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(82*4)

/* ------------------------------ */
.L_ALT_OP_IGET_WIDE: /* 0x53 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(83*4)

/* ------------------------------ */
.L_ALT_OP_IGET_OBJECT: /* 0x54 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(84*4)

/* ------------------------------ */
.L_ALT_OP_IGET_BOOLEAN: /* 0x55 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(85*4)

/* ------------------------------ */
.L_ALT_OP_IGET_BYTE: /* 0x56 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(86*4)

/* ------------------------------ */
.L_ALT_OP_IGET_CHAR: /* 0x57 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(87*4)

/* ------------------------------ */
.L_ALT_OP_IGET_SHORT: /* 0x58 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(88*4)

/* ------------------------------ */
.L_ALT_OP_IPUT: /* 0x59 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(89*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_WIDE: /* 0x5a */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(90*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_OBJECT: /* 0x5b */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(91*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_BOOLEAN: /* 0x5c */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(92*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_BYTE: /* 0x5d */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(93*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_CHAR: /* 0x5e */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(94*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_SHORT: /* 0x5f */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(95*4)

/* ------------------------------ */
.L_ALT_OP_SGET: /* 0x60 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(96*4)

/* ------------------------------ */
.L_ALT_OP_SGET_WIDE: /* 0x61 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(97*4)

/* ------------------------------ */
.L_ALT_OP_SGET_OBJECT: /* 0x62 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(98*4)

/* ------------------------------ */
.L_ALT_OP_SGET_BOOLEAN: /* 0x63 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(99*4)

/* ------------------------------ */
.L_ALT_OP_SGET_BYTE: /* 0x64 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(100*4)

/* ------------------------------ */
.L_ALT_OP_SGET_CHAR: /* 0x65 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(101*4)

/* ------------------------------ */
.L_ALT_OP_SGET_SHORT: /* 0x66 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(102*4)

/* ------------------------------ */
.L_ALT_OP_SPUT: /* 0x67 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(103*4)

/* ------------------------------ */
.L_ALT_OP_SPUT_WIDE: /* 0x68 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(104*4)

/* ------------------------------ */
.L_ALT_OP_SPUT_OBJECT: /* 0x69 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(105*4)

/* ------------------------------ */
.L_ALT_OP_SPUT_BOOLEAN: /* 0x6a */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(106*4)

/* ------------------------------ */
.L_ALT_OP_SPUT_BYTE: /* 0x6b */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(107*4)

/* ------------------------------ */
.L_ALT_OP_SPUT_CHAR: /* 0x6c */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(108*4)

/* ------------------------------ */
.L_ALT_OP_SPUT_SHORT: /* 0x6d */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(109*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_VIRTUAL: /* 0x6e */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(110*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_SUPER: /* 0x6f */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(111*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_DIRECT: /* 0x70 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(112*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_STATIC: /* 0x71 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(113*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_INTERFACE: /* 0x72 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(114*4)

/* ------------------------------ */
.L_ALT_OP_UNUSED_73: /* 0x73 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(115*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_VIRTUAL_RANGE: /* 0x74 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(116*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_SUPER_RANGE: /* 0x75 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(117*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_DIRECT_RANGE: /* 0x76 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(118*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_STATIC_RANGE: /* 0x77 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(119*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_INTERFACE_RANGE: /* 0x78 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(120*4)

/* ------------------------------ */
.L_ALT_OP_UNUSED_79: /* 0x79 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(121*4)

/* ------------------------------ */
.L_ALT_OP_UNUSED_7A: /* 0x7a */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(122*4)

/* ------------------------------ */
.L_ALT_OP_NEG_INT: /* 0x7b */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(123*4)

/* ------------------------------ */
.L_ALT_OP_NOT_INT: /* 0x7c */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(124*4)

/* ------------------------------ */
.L_ALT_OP_NEG_LONG: /* 0x7d */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(125*4)

/* ------------------------------ */
.L_ALT_OP_NOT_LONG: /* 0x7e */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(126*4)

/* ------------------------------ */
.L_ALT_OP_NEG_FLOAT: /* 0x7f */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(127*4)

/* ------------------------------ */
.L_ALT_OP_NEG_DOUBLE: /* 0x80 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(128*4)

/* ------------------------------ */
.L_ALT_OP_INT_TO_LONG: /* 0x81 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(129*4)

/* ------------------------------ */
.L_ALT_OP_INT_TO_FLOAT: /* 0x82 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(130*4)

/* ------------------------------ */
.L_ALT_OP_INT_TO_DOUBLE: /* 0x83 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(131*4)

/* ------------------------------ */
.L_ALT_OP_LONG_TO_INT: /* 0x84 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(132*4)

/* ------------------------------ */
.L_ALT_OP_LONG_TO_FLOAT: /* 0x85 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(133*4)

/* ------------------------------ */
.L_ALT_OP_LONG_TO_DOUBLE: /* 0x86 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(134*4)

/* ------------------------------ */
.L_ALT_OP_FLOAT_TO_INT: /* 0x87 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(135*4)

/* ------------------------------ */
.L_ALT_OP_FLOAT_TO_LONG: /* 0x88 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(136*4)

/* ------------------------------ */
.L_ALT_OP_FLOAT_TO_DOUBLE: /* 0x89 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(137*4)

/* ------------------------------ */
.L_ALT_OP_DOUBLE_TO_INT: /* 0x8a */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(138*4)

/* ------------------------------ */
.L_ALT_OP_DOUBLE_TO_LONG: /* 0x8b */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(139*4)

/* ------------------------------ */
.L_ALT_OP_DOUBLE_TO_FLOAT: /* 0x8c */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(140*4)

/* ------------------------------ */
.L_ALT_OP_INT_TO_BYTE: /* 0x8d */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(141*4)

/* ------------------------------ */
.L_ALT_OP_INT_TO_CHAR: /* 0x8e */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(142*4)

/* ------------------------------ */
.L_ALT_OP_INT_TO_SHORT: /* 0x8f */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(143*4)

/* ------------------------------ */
.L_ALT_OP_ADD_INT: /* 0x90 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(144*4)

/* ------------------------------ */
.L_ALT_OP_SUB_INT: /* 0x91 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(145*4)

/* ------------------------------ */
.L_ALT_OP_MUL_INT: /* 0x92 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(146*4)

/* ------------------------------ */
.L_ALT_OP_DIV_INT: /* 0x93 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(147*4)

/* ------------------------------ */
.L_ALT_OP_REM_INT: /* 0x94 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(148*4)

/* ------------------------------ */
.L_ALT_OP_AND_INT: /* 0x95 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(149*4)

/* ------------------------------ */
.L_ALT_OP_OR_INT: /* 0x96 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(150*4)

/* ------------------------------ */
.L_ALT_OP_XOR_INT: /* 0x97 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(151*4)

/* ------------------------------ */
.L_ALT_OP_SHL_INT: /* 0x98 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(152*4)

/* ------------------------------ */
.L_ALT_OP_SHR_INT: /* 0x99 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(153*4)

/* ------------------------------ */
.L_ALT_OP_USHR_INT: /* 0x9a */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(154*4)

/* ------------------------------ */
.L_ALT_OP_ADD_LONG: /* 0x9b */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(155*4)

/* ------------------------------ */
.L_ALT_OP_SUB_LONG: /* 0x9c */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(156*4)

/* ------------------------------ */
.L_ALT_OP_MUL_LONG: /* 0x9d */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(157*4)

/* ------------------------------ */
.L_ALT_OP_DIV_LONG: /* 0x9e */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(158*4)

/* ------------------------------ */
.L_ALT_OP_REM_LONG: /* 0x9f */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(159*4)

/* ------------------------------ */
.L_ALT_OP_AND_LONG: /* 0xa0 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(160*4)

/* ------------------------------ */
.L_ALT_OP_OR_LONG: /* 0xa1 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(161*4)

/* ------------------------------ */
.L_ALT_OP_XOR_LONG: /* 0xa2 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(162*4)

/* ------------------------------ */
.L_ALT_OP_SHL_LONG: /* 0xa3 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(163*4)

/* ------------------------------ */
.L_ALT_OP_SHR_LONG: /* 0xa4 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(164*4)

/* ------------------------------ */
.L_ALT_OP_USHR_LONG: /* 0xa5 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(165*4)

/* ------------------------------ */
.L_ALT_OP_ADD_FLOAT: /* 0xa6 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(166*4)

/* ------------------------------ */
.L_ALT_OP_SUB_FLOAT: /* 0xa7 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(167*4)

/* ------------------------------ */
.L_ALT_OP_MUL_FLOAT: /* 0xa8 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(168*4)

/* ------------------------------ */
.L_ALT_OP_DIV_FLOAT: /* 0xa9 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(169*4)

/* ------------------------------ */
.L_ALT_OP_REM_FLOAT: /* 0xaa */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(170*4)

/* ------------------------------ */
.L_ALT_OP_ADD_DOUBLE: /* 0xab */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(171*4)

/* ------------------------------ */
.L_ALT_OP_SUB_DOUBLE: /* 0xac */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(172*4)

/* ------------------------------ */
.L_ALT_OP_MUL_DOUBLE: /* 0xad */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(173*4)

/* ------------------------------ */
.L_ALT_OP_DIV_DOUBLE: /* 0xae */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(174*4)

/* ------------------------------ */
.L_ALT_OP_REM_DOUBLE: /* 0xaf */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(175*4)

/* ------------------------------ */
.L_ALT_OP_ADD_INT_2ADDR: /* 0xb0 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(176*4)

/* ------------------------------ */
.L_ALT_OP_SUB_INT_2ADDR: /* 0xb1 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(177*4)

/* ------------------------------ */
.L_ALT_OP_MUL_INT_2ADDR: /* 0xb2 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(178*4)

/* ------------------------------ */
.L_ALT_OP_DIV_INT_2ADDR: /* 0xb3 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(179*4)

/* ------------------------------ */
.L_ALT_OP_REM_INT_2ADDR: /* 0xb4 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(180*4)

/* ------------------------------ */
.L_ALT_OP_AND_INT_2ADDR: /* 0xb5 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(181*4)

/* ------------------------------ */
.L_ALT_OP_OR_INT_2ADDR: /* 0xb6 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(182*4)

/* ------------------------------ */
.L_ALT_OP_XOR_INT_2ADDR: /* 0xb7 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(183*4)

/* ------------------------------ */
.L_ALT_OP_SHL_INT_2ADDR: /* 0xb8 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(184*4)

/* ------------------------------ */
.L_ALT_OP_SHR_INT_2ADDR: /* 0xb9 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(185*4)

/* ------------------------------ */
.L_ALT_OP_USHR_INT_2ADDR: /* 0xba */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(186*4)

/* ------------------------------ */
.L_ALT_OP_ADD_LONG_2ADDR: /* 0xbb */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(187*4)

/* ------------------------------ */
.L_ALT_OP_SUB_LONG_2ADDR: /* 0xbc */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(188*4)

/* ------------------------------ */
.L_ALT_OP_MUL_LONG_2ADDR: /* 0xbd */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(189*4)

/* ------------------------------ */
.L_ALT_OP_DIV_LONG_2ADDR: /* 0xbe */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(190*4)

/* ------------------------------ */
.L_ALT_OP_REM_LONG_2ADDR: /* 0xbf */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(191*4)

/* ------------------------------ */
.L_ALT_OP_AND_LONG_2ADDR: /* 0xc0 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(192*4)

/* ------------------------------ */
.L_ALT_OP_OR_LONG_2ADDR: /* 0xc1 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(193*4)

/* ------------------------------ */
.L_ALT_OP_XOR_LONG_2ADDR: /* 0xc2 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(194*4)

/* ------------------------------ */
.L_ALT_OP_SHL_LONG_2ADDR: /* 0xc3 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(195*4)

/* ------------------------------ */
.L_ALT_OP_SHR_LONG_2ADDR: /* 0xc4 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(196*4)

/* ------------------------------ */
.L_ALT_OP_USHR_LONG_2ADDR: /* 0xc5 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(197*4)

/* ------------------------------ */
.L_ALT_OP_ADD_FLOAT_2ADDR: /* 0xc6 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(198*4)

/* ------------------------------ */
.L_ALT_OP_SUB_FLOAT_2ADDR: /* 0xc7 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(199*4)

/* ------------------------------ */
.L_ALT_OP_MUL_FLOAT_2ADDR: /* 0xc8 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(200*4)

/* ------------------------------ */
.L_ALT_OP_DIV_FLOAT_2ADDR: /* 0xc9 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(201*4)

/* ------------------------------ */
.L_ALT_OP_REM_FLOAT_2ADDR: /* 0xca */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(202*4)

/* ------------------------------ */
.L_ALT_OP_ADD_DOUBLE_2ADDR: /* 0xcb */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(203*4)

/* ------------------------------ */
.L_ALT_OP_SUB_DOUBLE_2ADDR: /* 0xcc */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(204*4)

/* ------------------------------ */
.L_ALT_OP_MUL_DOUBLE_2ADDR: /* 0xcd */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(205*4)

/* ------------------------------ */
.L_ALT_OP_DIV_DOUBLE_2ADDR: /* 0xce */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(206*4)

/* ------------------------------ */
.L_ALT_OP_REM_DOUBLE_2ADDR: /* 0xcf */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(207*4)

/* ------------------------------ */
.L_ALT_OP_ADD_INT_LIT16: /* 0xd0 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(208*4)

/* ------------------------------ */
.L_ALT_OP_RSUB_INT: /* 0xd1 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(209*4)

/* ------------------------------ */
.L_ALT_OP_MUL_INT_LIT16: /* 0xd2 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(210*4)

/* ------------------------------ */
.L_ALT_OP_DIV_INT_LIT16: /* 0xd3 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(211*4)

/* ------------------------------ */
.L_ALT_OP_REM_INT_LIT16: /* 0xd4 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(212*4)

/* ------------------------------ */
.L_ALT_OP_AND_INT_LIT16: /* 0xd5 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(213*4)

/* ------------------------------ */
.L_ALT_OP_OR_INT_LIT16: /* 0xd6 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(214*4)

/* ------------------------------ */
.L_ALT_OP_XOR_INT_LIT16: /* 0xd7 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(215*4)

/* ------------------------------ */
.L_ALT_OP_ADD_INT_LIT8: /* 0xd8 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(216*4)

/* ------------------------------ */
.L_ALT_OP_RSUB_INT_LIT8: /* 0xd9 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(217*4)

/* ------------------------------ */
.L_ALT_OP_MUL_INT_LIT8: /* 0xda */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(218*4)

/* ------------------------------ */
.L_ALT_OP_DIV_INT_LIT8: /* 0xdb */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(219*4)

/* ------------------------------ */
.L_ALT_OP_REM_INT_LIT8: /* 0xdc */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(220*4)

/* ------------------------------ */
.L_ALT_OP_AND_INT_LIT8: /* 0xdd */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(221*4)

/* ------------------------------ */
.L_ALT_OP_OR_INT_LIT8: /* 0xde */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(222*4)

/* ------------------------------ */
.L_ALT_OP_XOR_INT_LIT8: /* 0xdf */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(223*4)

/* ------------------------------ */
.L_ALT_OP_SHL_INT_LIT8: /* 0xe0 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(224*4)

/* ------------------------------ */
.L_ALT_OP_SHR_INT_LIT8: /* 0xe1 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(225*4)

/* ------------------------------ */
.L_ALT_OP_USHR_INT_LIT8: /* 0xe2 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(226*4)

/* ------------------------------ */
.L_ALT_OP_IGET_VOLATILE: /* 0xe3 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(227*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_VOLATILE: /* 0xe4 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(228*4)

/* ------------------------------ */
.L_ALT_OP_SGET_VOLATILE: /* 0xe5 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(229*4)

/* ------------------------------ */
.L_ALT_OP_SPUT_VOLATILE: /* 0xe6 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(230*4)

/* ------------------------------ */
.L_ALT_OP_IGET_OBJECT_VOLATILE: /* 0xe7 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(231*4)

/* ------------------------------ */
.L_ALT_OP_IGET_WIDE_VOLATILE: /* 0xe8 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(232*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_WIDE_VOLATILE: /* 0xe9 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(233*4)

/* ------------------------------ */
.L_ALT_OP_SGET_WIDE_VOLATILE: /* 0xea */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(234*4)

/* ------------------------------ */
.L_ALT_OP_SPUT_WIDE_VOLATILE: /* 0xeb */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(235*4)

/* ------------------------------ */
.L_ALT_OP_BREAKPOINT: /* 0xec */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(236*4)

/* ------------------------------ */
.L_ALT_OP_THROW_VERIFICATION_ERROR: /* 0xed */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(237*4)

/* ------------------------------ */
.L_ALT_OP_EXECUTE_INLINE: /* 0xee */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(238*4)

/* ------------------------------ */
.L_ALT_OP_EXECUTE_INLINE_RANGE: /* 0xef */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(239*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_OBJECT_INIT_RANGE: /* 0xf0 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(240*4)

/* ------------------------------ */
.L_ALT_OP_RETURN_VOID_BARRIER: /* 0xf1 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(241*4)

/* ------------------------------ */
.L_ALT_OP_IGET_QUICK: /* 0xf2 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(242*4)

/* ------------------------------ */
.L_ALT_OP_IGET_WIDE_QUICK: /* 0xf3 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(243*4)

/* ------------------------------ */
.L_ALT_OP_IGET_OBJECT_QUICK: /* 0xf4 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(244*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_QUICK: /* 0xf5 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(245*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_WIDE_QUICK: /* 0xf6 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(246*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_OBJECT_QUICK: /* 0xf7 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(247*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_VIRTUAL_QUICK: /* 0xf8 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(248*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_VIRTUAL_QUICK_RANGE: /* 0xf9 */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(249*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_SUPER_QUICK: /* 0xfa */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(250*4)

/* ------------------------------ */
.L_ALT_OP_INVOKE_SUPER_QUICK_RANGE: /* 0xfb */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(251*4)

/* ------------------------------ */
.L_ALT_OP_IPUT_OBJECT_VOLATILE: /* 0xfc */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(252*4)

/* ------------------------------ */
.L_ALT_OP_SGET_OBJECT_VOLATILE: /* 0xfd */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(253*4)

/* ------------------------------ */
.L_ALT_OP_SPUT_OBJECT_VOLATILE: /* 0xfe */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(254*4)

/* ------------------------------ */
.L_ALT_OP_UNUSED_FF: /* 0xff */
/* File: x86/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(255*4)

    .size   dvmAsmAltInstructionStartCode, .-dvmAsmAltInstructionStartCode
    .global dvmAsmAltInstructionEndCode
dvmAsmAltInstructionEndCode:

    .global dvmAsmInstructionStart
    .text
dvmAsmInstructionStart:
    .long .L_OP_NOP /* 0x00 */
    .long .L_OP_MOVE /* 0x01 */
    .long .L_OP_MOVE_FROM16 /* 0x02 */
    .long .L_OP_MOVE_16 /* 0x03 */
    .long .L_OP_MOVE_WIDE /* 0x04 */
    .long .L_OP_MOVE_WIDE_FROM16 /* 0x05 */
    .long .L_OP_MOVE_WIDE_16 /* 0x06 */
    .long .L_OP_MOVE_OBJECT /* 0x07 */
    .long .L_OP_MOVE_OBJECT_FROM16 /* 0x08 */
    .long .L_OP_MOVE_OBJECT_16 /* 0x09 */
    .long .L_OP_MOVE_RESULT /* 0x0a */
    .long .L_OP_MOVE_RESULT_WIDE /* 0x0b */
    .long .L_OP_MOVE_RESULT_OBJECT /* 0x0c */
    .long .L_OP_MOVE_EXCEPTION /* 0x0d */
    .long .L_OP_RETURN_VOID /* 0x0e */
    .long .L_OP_RETURN /* 0x0f */
    .long .L_OP_RETURN_WIDE /* 0x10 */
    .long .L_OP_RETURN_OBJECT /* 0x11 */
    .long .L_OP_CONST_4 /* 0x12 */
    .long .L_OP_CONST_16 /* 0x13 */
    .long .L_OP_CONST /* 0x14 */
    .long .L_OP_CONST_HIGH16 /* 0x15 */
    .long .L_OP_CONST_WIDE_16 /* 0x16 */
    .long .L_OP_CONST_WIDE_32 /* 0x17 */
    .long .L_OP_CONST_WIDE /* 0x18 */
    .long .L_OP_CONST_WIDE_HIGH16 /* 0x19 */
    .long .L_OP_CONST_STRING /* 0x1a */
    .long .L_OP_CONST_STRING_JUMBO /* 0x1b */
    .long .L_OP_CONST_CLASS /* 0x1c */
    .long .L_OP_MONITOR_ENTER /* 0x1d */
    .long .L_OP_MONITOR_EXIT /* 0x1e */
    .long .L_OP_CHECK_CAST /* 0x1f */
    .long .L_OP_INSTANCE_OF /* 0x20 */
    .long .L_OP_ARRAY_LENGTH /* 0x21 */
    .long .L_OP_NEW_INSTANCE /* 0x22 */
    .long .L_OP_NEW_ARRAY /* 0x23 */
    .long .L_OP_FILLED_NEW_ARRAY /* 0x24 */
    .long .L_OP_FILLED_NEW_ARRAY_RANGE /* 0x25 */
    .long .L_OP_FILL_ARRAY_DATA /* 0x26 */
    .long .L_OP_THROW /* 0x27 */
    .long .L_OP_GOTO /* 0x28 */
    .long .L_OP_GOTO_16 /* 0x29 */
    .long .L_OP_GOTO_32 /* 0x2a */
    .long .L_OP_PACKED_SWITCH /* 0x2b */
    .long .L_OP_SPARSE_SWITCH /* 0x2c */
    .long .L_OP_CMPL_FLOAT /* 0x2d */
    .long .L_OP_CMPG_FLOAT /* 0x2e */
    .long .L_OP_CMPL_DOUBLE /* 0x2f */
    .long .L_OP_CMPG_DOUBLE /* 0x30 */
    .long .L_OP_CMP_LONG /* 0x31 */
    .long .L_OP_IF_EQ /* 0x32 */
    .long .L_OP_IF_NE /* 0x33 */
    .long .L_OP_IF_LT /* 0x34 */
    .long .L_OP_IF_GE /* 0x35 */
    .long .L_OP_IF_GT /* 0x36 */
    .long .L_OP_IF_LE /* 0x37 */
    .long .L_OP_IF_EQZ /* 0x38 */
    .long .L_OP_IF_NEZ /* 0x39 */
    .long .L_OP_IF_LTZ /* 0x3a */
    .long .L_OP_IF_GEZ /* 0x3b */
    .long .L_OP_IF_GTZ /* 0x3c */
    .long .L_OP_IF_LEZ /* 0x3d */
    .long .L_OP_UNUSED_3E /* 0x3e */
    .long .L_OP_UNUSED_3F /* 0x3f */
    .long .L_OP_UNUSED_40 /* 0x40 */
    .long .L_OP_UNUSED_41 /* 0x41 */
    .long .L_OP_UNUSED_42 /* 0x42 */
    .long .L_OP_UNUSED_43 /* 0x43 */
    .long .L_OP_AGET /* 0x44 */
    .long .L_OP_AGET_WIDE /* 0x45 */
    .long .L_OP_AGET_OBJECT /* 0x46 */
    .long .L_OP_AGET_BOOLEAN /* 0x47 */
    .long .L_OP_AGET_BYTE /* 0x48 */
    .long .L_OP_AGET_CHAR /* 0x49 */
    .long .L_OP_AGET_SHORT /* 0x4a */
    .long .L_OP_APUT /* 0x4b */
    .long .L_OP_APUT_WIDE /* 0x4c */
    .long .L_OP_APUT_OBJECT /* 0x4d */
    .long .L_OP_APUT_BOOLEAN /* 0x4e */
    .long .L_OP_APUT_BYTE /* 0x4f */
    .long .L_OP_APUT_CHAR /* 0x50 */
    .long .L_OP_APUT_SHORT /* 0x51 */
    .long .L_OP_IGET /* 0x52 */
    .long .L_OP_IGET_WIDE /* 0x53 */
    .long .L_OP_IGET_OBJECT /* 0x54 */
    .long .L_OP_IGET_BOOLEAN /* 0x55 */
    .long .L_OP_IGET_BYTE /* 0x56 */
    .long .L_OP_IGET_CHAR /* 0x57 */
    .long .L_OP_IGET_SHORT /* 0x58 */
    .long .L_OP_IPUT /* 0x59 */
    .long .L_OP_IPUT_WIDE /* 0x5a */
    .long .L_OP_IPUT_OBJECT /* 0x5b */
    .long .L_OP_IPUT_BOOLEAN /* 0x5c */
    .long .L_OP_IPUT_BYTE /* 0x5d */
    .long .L_OP_IPUT_CHAR /* 0x5e */
    .long .L_OP_IPUT_SHORT /* 0x5f */
    .long .L_OP_SGET /* 0x60 */
    .long .L_OP_SGET_WIDE /* 0x61 */
    .long .L_OP_SGET_OBJECT /* 0x62 */
    .long .L_OP_SGET_BOOLEAN /* 0x63 */
    .long .L_OP_SGET_BYTE /* 0x64 */
    .long .L_OP_SGET_CHAR /* 0x65 */
    .long .L_OP_SGET_SHORT /* 0x66 */
    .long .L_OP_SPUT /* 0x67 */
    .long .L_OP_SPUT_WIDE /* 0x68 */
    .long .L_OP_SPUT_OBJECT /* 0x69 */
    .long .L_OP_SPUT_BOOLEAN /* 0x6a */
    .long .L_OP_SPUT_BYTE /* 0x6b */
    .long .L_OP_SPUT_CHAR /* 0x6c */
    .long .L_OP_SPUT_SHORT /* 0x6d */
    .long .L_OP_INVOKE_VIRTUAL /* 0x6e */
    .long .L_OP_INVOKE_SUPER /* 0x6f */
    .long .L_OP_INVOKE_DIRECT /* 0x70 */
    .long .L_OP_INVOKE_STATIC /* 0x71 */
    .long .L_OP_INVOKE_INTERFACE /* 0x72 */
    .long .L_OP_UNUSED_73 /* 0x73 */
    .long .L_OP_INVOKE_VIRTUAL_RANGE /* 0x74 */
    .long .L_OP_INVOKE_SUPER_RANGE /* 0x75 */
    .long .L_OP_INVOKE_DIRECT_RANGE /* 0x76 */
    .long .L_OP_INVOKE_STATIC_RANGE /* 0x77 */
    .long .L_OP_INVOKE_INTERFACE_RANGE /* 0x78 */
    .long .L_OP_UNUSED_79 /* 0x79 */
    .long .L_OP_UNUSED_7A /* 0x7a */
    .long .L_OP_NEG_INT /* 0x7b */
    .long .L_OP_NOT_INT /* 0x7c */
    .long .L_OP_NEG_LONG /* 0x7d */
    .long .L_OP_NOT_LONG /* 0x7e */
    .long .L_OP_NEG_FLOAT /* 0x7f */
    .long .L_OP_NEG_DOUBLE /* 0x80 */
    .long .L_OP_INT_TO_LONG /* 0x81 */
    .long .L_OP_INT_TO_FLOAT /* 0x82 */
    .long .L_OP_INT_TO_DOUBLE /* 0x83 */
    .long .L_OP_LONG_TO_INT /* 0x84 */
    .long .L_OP_LONG_TO_FLOAT /* 0x85 */
    .long .L_OP_LONG_TO_DOUBLE /* 0x86 */
    .long .L_OP_FLOAT_TO_INT /* 0x87 */
    .long .L_OP_FLOAT_TO_LONG /* 0x88 */
    .long .L_OP_FLOAT_TO_DOUBLE /* 0x89 */
    .long .L_OP_DOUBLE_TO_INT /* 0x8a */
    .long .L_OP_DOUBLE_TO_LONG /* 0x8b */
    .long .L_OP_DOUBLE_TO_FLOAT /* 0x8c */
    .long .L_OP_INT_TO_BYTE /* 0x8d */
    .long .L_OP_INT_TO_CHAR /* 0x8e */
    .long .L_OP_INT_TO_SHORT /* 0x8f */
    .long .L_OP_ADD_INT /* 0x90 */
    .long .L_OP_SUB_INT /* 0x91 */
    .long .L_OP_MUL_INT /* 0x92 */
    .long .L_OP_DIV_INT /* 0x93 */
    .long .L_OP_REM_INT /* 0x94 */
    .long .L_OP_AND_INT /* 0x95 */
    .long .L_OP_OR_INT /* 0x96 */
    .long .L_OP_XOR_INT /* 0x97 */
    .long .L_OP_SHL_INT /* 0x98 */
    .long .L_OP_SHR_INT /* 0x99 */
    .long .L_OP_USHR_INT /* 0x9a */
    .long .L_OP_ADD_LONG /* 0x9b */
    .long .L_OP_SUB_LONG /* 0x9c */
    .long .L_OP_MUL_LONG /* 0x9d */
    .long .L_OP_DIV_LONG /* 0x9e */
    .long .L_OP_REM_LONG /* 0x9f */
    .long .L_OP_AND_LONG /* 0xa0 */
    .long .L_OP_OR_LONG /* 0xa1 */
    .long .L_OP_XOR_LONG /* 0xa2 */
    .long .L_OP_SHL_LONG /* 0xa3 */
    .long .L_OP_SHR_LONG /* 0xa4 */
    .long .L_OP_USHR_LONG /* 0xa5 */
    .long .L_OP_ADD_FLOAT /* 0xa6 */
    .long .L_OP_SUB_FLOAT /* 0xa7 */
    .long .L_OP_MUL_FLOAT /* 0xa8 */
    .long .L_OP_DIV_FLOAT /* 0xa9 */
    .long .L_OP_REM_FLOAT /* 0xaa */
    .long .L_OP_ADD_DOUBLE /* 0xab */
    .long .L_OP_SUB_DOUBLE /* 0xac */
    .long .L_OP_MUL_DOUBLE /* 0xad */
    .long .L_OP_DIV_DOUBLE /* 0xae */
    .long .L_OP_REM_DOUBLE /* 0xaf */
    .long .L_OP_ADD_INT_2ADDR /* 0xb0 */
    .long .L_OP_SUB_INT_2ADDR /* 0xb1 */
    .long .L_OP_MUL_INT_2ADDR /* 0xb2 */
    .long .L_OP_DIV_INT_2ADDR /* 0xb3 */
    .long .L_OP_REM_INT_2ADDR /* 0xb4 */
    .long .L_OP_AND_INT_2ADDR /* 0xb5 */
    .long .L_OP_OR_INT_2ADDR /* 0xb6 */
    .long .L_OP_XOR_INT_2ADDR /* 0xb7 */
    .long .L_OP_SHL_INT_2ADDR /* 0xb8 */
    .long .L_OP_SHR_INT_2ADDR /* 0xb9 */
    .long .L_OP_USHR_INT_2ADDR /* 0xba */
    .long .L_OP_ADD_LONG_2ADDR /* 0xbb */
    .long .L_OP_SUB_LONG_2ADDR /* 0xbc */
    .long .L_OP_MUL_LONG_2ADDR /* 0xbd */
    .long .L_OP_DIV_LONG_2ADDR /* 0xbe */
    .long .L_OP_REM_LONG_2ADDR /* 0xbf */
    .long .L_OP_AND_LONG_2ADDR /* 0xc0 */
    .long .L_OP_OR_LONG_2ADDR /* 0xc1 */
    .long .L_OP_XOR_LONG_2ADDR /* 0xc2 */
    .long .L_OP_SHL_LONG_2ADDR /* 0xc3 */
    .long .L_OP_SHR_LONG_2ADDR /* 0xc4 */
    .long .L_OP_USHR_LONG_2ADDR /* 0xc5 */
    .long .L_OP_ADD_FLOAT_2ADDR /* 0xc6 */
    .long .L_OP_SUB_FLOAT_2ADDR /* 0xc7 */
    .long .L_OP_MUL_FLOAT_2ADDR /* 0xc8 */
    .long .L_OP_DIV_FLOAT_2ADDR /* 0xc9 */
    .long .L_OP_REM_FLOAT_2ADDR /* 0xca */
    .long .L_OP_ADD_DOUBLE_2ADDR /* 0xcb */
    .long .L_OP_SUB_DOUBLE_2ADDR /* 0xcc */
    .long .L_OP_MUL_DOUBLE_2ADDR /* 0xcd */
    .long .L_OP_DIV_DOUBLE_2ADDR /* 0xce */
    .long .L_OP_REM_DOUBLE_2ADDR /* 0xcf */
    .long .L_OP_ADD_INT_LIT16 /* 0xd0 */
    .long .L_OP_RSUB_INT /* 0xd1 */
    .long .L_OP_MUL_INT_LIT16 /* 0xd2 */
    .long .L_OP_DIV_INT_LIT16 /* 0xd3 */
    .long .L_OP_REM_INT_LIT16 /* 0xd4 */
    .long .L_OP_AND_INT_LIT16 /* 0xd5 */
    .long .L_OP_OR_INT_LIT16 /* 0xd6 */
    .long .L_OP_XOR_INT_LIT16 /* 0xd7 */
    .long .L_OP_ADD_INT_LIT8 /* 0xd8 */
    .long .L_OP_RSUB_INT_LIT8 /* 0xd9 */
    .long .L_OP_MUL_INT_LIT8 /* 0xda */
    .long .L_OP_DIV_INT_LIT8 /* 0xdb */
    .long .L_OP_REM_INT_LIT8 /* 0xdc */
    .long .L_OP_AND_INT_LIT8 /* 0xdd */
    .long .L_OP_OR_INT_LIT8 /* 0xde */
    .long .L_OP_XOR_INT_LIT8 /* 0xdf */
    .long .L_OP_SHL_INT_LIT8 /* 0xe0 */
    .long .L_OP_SHR_INT_LIT8 /* 0xe1 */
    .long .L_OP_USHR_INT_LIT8 /* 0xe2 */
    .long .L_OP_IGET_VOLATILE /* 0xe3 */
    .long .L_OP_IPUT_VOLATILE /* 0xe4 */
    .long .L_OP_SGET_VOLATILE /* 0xe5 */
    .long .L_OP_SPUT_VOLATILE /* 0xe6 */
    .long .L_OP_IGET_OBJECT_VOLATILE /* 0xe7 */
    .long .L_OP_IGET_WIDE_VOLATILE /* 0xe8 */
    .long .L_OP_IPUT_WIDE_VOLATILE /* 0xe9 */
    .long .L_OP_SGET_WIDE_VOLATILE /* 0xea */
    .long .L_OP_SPUT_WIDE_VOLATILE /* 0xeb */
    .long .L_OP_BREAKPOINT /* 0xec */
    .long .L_OP_THROW_VERIFICATION_ERROR /* 0xed */
    .long .L_OP_EXECUTE_INLINE /* 0xee */
    .long .L_OP_EXECUTE_INLINE_RANGE /* 0xef */
    .long .L_OP_INVOKE_OBJECT_INIT_RANGE /* 0xf0 */
    .long .L_OP_RETURN_VOID_BARRIER /* 0xf1 */
    .long .L_OP_IGET_QUICK /* 0xf2 */
    .long .L_OP_IGET_WIDE_QUICK /* 0xf3 */
    .long .L_OP_IGET_OBJECT_QUICK /* 0xf4 */
    .long .L_OP_IPUT_QUICK /* 0xf5 */
    .long .L_OP_IPUT_WIDE_QUICK /* 0xf6 */
    .long .L_OP_IPUT_OBJECT_QUICK /* 0xf7 */
    .long .L_OP_INVOKE_VIRTUAL_QUICK /* 0xf8 */
    .long .L_OP_INVOKE_VIRTUAL_QUICK_RANGE /* 0xf9 */
    .long .L_OP_INVOKE_SUPER_QUICK /* 0xfa */
    .long .L_OP_INVOKE_SUPER_QUICK_RANGE /* 0xfb */
    .long .L_OP_IPUT_OBJECT_VOLATILE /* 0xfc */
    .long .L_OP_SGET_OBJECT_VOLATILE /* 0xfd */
    .long .L_OP_SPUT_OBJECT_VOLATILE /* 0xfe */
    .long .L_OP_UNUSED_FF /* 0xff */

    .global dvmAsmAltInstructionStart
    .text
dvmAsmAltInstructionStart:
    .long .L_ALT_OP_NOP /* 0x00 */
    .long .L_ALT_OP_MOVE /* 0x01 */
    .long .L_ALT_OP_MOVE_FROM16 /* 0x02 */
    .long .L_ALT_OP_MOVE_16 /* 0x03 */
    .long .L_ALT_OP_MOVE_WIDE /* 0x04 */
    .long .L_ALT_OP_MOVE_WIDE_FROM16 /* 0x05 */
    .long .L_ALT_OP_MOVE_WIDE_16 /* 0x06 */
    .long .L_ALT_OP_MOVE_OBJECT /* 0x07 */
    .long .L_ALT_OP_MOVE_OBJECT_FROM16 /* 0x08 */
    .long .L_ALT_OP_MOVE_OBJECT_16 /* 0x09 */
    .long .L_ALT_OP_MOVE_RESULT /* 0x0a */
    .long .L_ALT_OP_MOVE_RESULT_WIDE /* 0x0b */
    .long .L_ALT_OP_MOVE_RESULT_OBJECT /* 0x0c */
    .long .L_ALT_OP_MOVE_EXCEPTION /* 0x0d */
    .long .L_ALT_OP_RETURN_VOID /* 0x0e */
    .long .L_ALT_OP_RETURN /* 0x0f */
    .long .L_ALT_OP_RETURN_WIDE /* 0x10 */
    .long .L_ALT_OP_RETURN_OBJECT /* 0x11 */
    .long .L_ALT_OP_CONST_4 /* 0x12 */
    .long .L_ALT_OP_CONST_16 /* 0x13 */
    .long .L_ALT_OP_CONST /* 0x14 */
    .long .L_ALT_OP_CONST_HIGH16 /* 0x15 */
    .long .L_ALT_OP_CONST_WIDE_16 /* 0x16 */
    .long .L_ALT_OP_CONST_WIDE_32 /* 0x17 */
    .long .L_ALT_OP_CONST_WIDE /* 0x18 */
    .long .L_ALT_OP_CONST_WIDE_HIGH16 /* 0x19 */
    .long .L_ALT_OP_CONST_STRING /* 0x1a */
    .long .L_ALT_OP_CONST_STRING_JUMBO /* 0x1b */
    .long .L_ALT_OP_CONST_CLASS /* 0x1c */
    .long .L_ALT_OP_MONITOR_ENTER /* 0x1d */
    .long .L_ALT_OP_MONITOR_EXIT /* 0x1e */
    .long .L_ALT_OP_CHECK_CAST /* 0x1f */
    .long .L_ALT_OP_INSTANCE_OF /* 0x20 */
    .long .L_ALT_OP_ARRAY_LENGTH /* 0x21 */
    .long .L_ALT_OP_NEW_INSTANCE /* 0x22 */
    .long .L_ALT_OP_NEW_ARRAY /* 0x23 */
    .long .L_ALT_OP_FILLED_NEW_ARRAY /* 0x24 */
    .long .L_ALT_OP_FILLED_NEW_ARRAY_RANGE /* 0x25 */
    .long .L_ALT_OP_FILL_ARRAY_DATA /* 0x26 */
    .long .L_ALT_OP_THROW /* 0x27 */
    .long .L_ALT_OP_GOTO /* 0x28 */
    .long .L_ALT_OP_GOTO_16 /* 0x29 */
    .long .L_ALT_OP_GOTO_32 /* 0x2a */
    .long .L_ALT_OP_PACKED_SWITCH /* 0x2b */
    .long .L_ALT_OP_SPARSE_SWITCH /* 0x2c */
    .long .L_ALT_OP_CMPL_FLOAT /* 0x2d */
    .long .L_ALT_OP_CMPG_FLOAT /* 0x2e */
    .long .L_ALT_OP_CMPL_DOUBLE /* 0x2f */
    .long .L_ALT_OP_CMPG_DOUBLE /* 0x30 */
    .long .L_ALT_OP_CMP_LONG /* 0x31 */
    .long .L_ALT_OP_IF_EQ /* 0x32 */
    .long .L_ALT_OP_IF_NE /* 0x33 */
    .long .L_ALT_OP_IF_LT /* 0x34 */
    .long .L_ALT_OP_IF_GE /* 0x35 */
    .long .L_ALT_OP_IF_GT /* 0x36 */
    .long .L_ALT_OP_IF_LE /* 0x37 */
    .long .L_ALT_OP_IF_EQZ /* 0x38 */
    .long .L_ALT_OP_IF_NEZ /* 0x39 */
    .long .L_ALT_OP_IF_LTZ /* 0x3a */
    .long .L_ALT_OP_IF_GEZ /* 0x3b */
    .long .L_ALT_OP_IF_GTZ /* 0x3c */
    .long .L_ALT_OP_IF_LEZ /* 0x3d */
    .long .L_ALT_OP_UNUSED_3E /* 0x3e */
    .long .L_ALT_OP_UNUSED_3F /* 0x3f */
    .long .L_ALT_OP_UNUSED_40 /* 0x40 */
    .long .L_ALT_OP_UNUSED_41 /* 0x41 */
    .long .L_ALT_OP_UNUSED_42 /* 0x42 */
    .long .L_ALT_OP_UNUSED_43 /* 0x43 */
    .long .L_ALT_OP_AGET /* 0x44 */
    .long .L_ALT_OP_AGET_WIDE /* 0x45 */
    .long .L_ALT_OP_AGET_OBJECT /* 0x46 */
    .long .L_ALT_OP_AGET_BOOLEAN /* 0x47 */
    .long .L_ALT_OP_AGET_BYTE /* 0x48 */
    .long .L_ALT_OP_AGET_CHAR /* 0x49 */
    .long .L_ALT_OP_AGET_SHORT /* 0x4a */
    .long .L_ALT_OP_APUT /* 0x4b */
    .long .L_ALT_OP_APUT_WIDE /* 0x4c */
    .long .L_ALT_OP_APUT_OBJECT /* 0x4d */
    .long .L_ALT_OP_APUT_BOOLEAN /* 0x4e */
    .long .L_ALT_OP_APUT_BYTE /* 0x4f */
    .long .L_ALT_OP_APUT_CHAR /* 0x50 */
    .long .L_ALT_OP_APUT_SHORT /* 0x51 */
    .long .L_ALT_OP_IGET /* 0x52 */
    .long .L_ALT_OP_IGET_WIDE /* 0x53 */
    .long .L_ALT_OP_IGET_OBJECT /* 0x54 */
    .long .L_ALT_OP_IGET_BOOLEAN /* 0x55 */
    .long .L_ALT_OP_IGET_BYTE /* 0x56 */
    .long .L_ALT_OP_IGET_CHAR /* 0x57 */
    .long .L_ALT_OP_IGET_SHORT /* 0x58 */
    .long .L_ALT_OP_IPUT /* 0x59 */
    .long .L_ALT_OP_IPUT_WIDE /* 0x5a */
    .long .L_ALT_OP_IPUT_OBJECT /* 0x5b */
    .long .L_ALT_OP_IPUT_BOOLEAN /* 0x5c */
    .long .L_ALT_OP_IPUT_BYTE /* 0x5d */
    .long .L_ALT_OP_IPUT_CHAR /* 0x5e */
    .long .L_ALT_OP_IPUT_SHORT /* 0x5f */
    .long .L_ALT_OP_SGET /* 0x60 */
    .long .L_ALT_OP_SGET_WIDE /* 0x61 */
    .long .L_ALT_OP_SGET_OBJECT /* 0x62 */
    .long .L_ALT_OP_SGET_BOOLEAN /* 0x63 */
    .long .L_ALT_OP_SGET_BYTE /* 0x64 */
    .long .L_ALT_OP_SGET_CHAR /* 0x65 */
    .long .L_ALT_OP_SGET_SHORT /* 0x66 */
    .long .L_ALT_OP_SPUT /* 0x67 */
    .long .L_ALT_OP_SPUT_WIDE /* 0x68 */
    .long .L_ALT_OP_SPUT_OBJECT /* 0x69 */
    .long .L_ALT_OP_SPUT_BOOLEAN /* 0x6a */
    .long .L_ALT_OP_SPUT_BYTE /* 0x6b */
    .long .L_ALT_OP_SPUT_CHAR /* 0x6c */
    .long .L_ALT_OP_SPUT_SHORT /* 0x6d */
    .long .L_ALT_OP_INVOKE_VIRTUAL /* 0x6e */
    .long .L_ALT_OP_INVOKE_SUPER /* 0x6f */
    .long .L_ALT_OP_INVOKE_DIRECT /* 0x70 */
    .long .L_ALT_OP_INVOKE_STATIC /* 0x71 */
    .long .L_ALT_OP_INVOKE_INTERFACE /* 0x72 */
    .long .L_ALT_OP_UNUSED_73 /* 0x73 */
    .long .L_ALT_OP_INVOKE_VIRTUAL_RANGE /* 0x74 */
    .long .L_ALT_OP_INVOKE_SUPER_RANGE /* 0x75 */
    .long .L_ALT_OP_INVOKE_DIRECT_RANGE /* 0x76 */
    .long .L_ALT_OP_INVOKE_STATIC_RANGE /* 0x77 */
    .long .L_ALT_OP_INVOKE_INTERFACE_RANGE /* 0x78 */
    .long .L_ALT_OP_UNUSED_79 /* 0x79 */
    .long .L_ALT_OP_UNUSED_7A /* 0x7a */
    .long .L_ALT_OP_NEG_INT /* 0x7b */
    .long .L_ALT_OP_NOT_INT /* 0x7c */
    .long .L_ALT_OP_NEG_LONG /* 0x7d */
    .long .L_ALT_OP_NOT_LONG /* 0x7e */
    .long .L_ALT_OP_NEG_FLOAT /* 0x7f */
    .long .L_ALT_OP_NEG_DOUBLE /* 0x80 */
    .long .L_ALT_OP_INT_TO_LONG /* 0x81 */
    .long .L_ALT_OP_INT_TO_FLOAT /* 0x82 */
    .long .L_ALT_OP_INT_TO_DOUBLE /* 0x83 */
    .long .L_ALT_OP_LONG_TO_INT /* 0x84 */
    .long .L_ALT_OP_LONG_TO_FLOAT /* 0x85 */
    .long .L_ALT_OP_LONG_TO_DOUBLE /* 0x86 */
    .long .L_ALT_OP_FLOAT_TO_INT /* 0x87 */
    .long .L_ALT_OP_FLOAT_TO_LONG /* 0x88 */
    .long .L_ALT_OP_FLOAT_TO_DOUBLE /* 0x89 */
    .long .L_ALT_OP_DOUBLE_TO_INT /* 0x8a */
    .long .L_ALT_OP_DOUBLE_TO_LONG /* 0x8b */
    .long .L_ALT_OP_DOUBLE_TO_FLOAT /* 0x8c */
    .long .L_ALT_OP_INT_TO_BYTE /* 0x8d */
    .long .L_ALT_OP_INT_TO_CHAR /* 0x8e */
    .long .L_ALT_OP_INT_TO_SHORT /* 0x8f */
    .long .L_ALT_OP_ADD_INT /* 0x90 */
    .long .L_ALT_OP_SUB_INT /* 0x91 */
    .long .L_ALT_OP_MUL_INT /* 0x92 */
    .long .L_ALT_OP_DIV_INT /* 0x93 */
    .long .L_ALT_OP_REM_INT /* 0x94 */
    .long .L_ALT_OP_AND_INT /* 0x95 */
    .long .L_ALT_OP_OR_INT /* 0x96 */
    .long .L_ALT_OP_XOR_INT /* 0x97 */
    .long .L_ALT_OP_SHL_INT /* 0x98 */
    .long .L_ALT_OP_SHR_INT /* 0x99 */
    .long .L_ALT_OP_USHR_INT /* 0x9a */
    .long .L_ALT_OP_ADD_LONG /* 0x9b */
    .long .L_ALT_OP_SUB_LONG /* 0x9c */
    .long .L_ALT_OP_MUL_LONG /* 0x9d */
    .long .L_ALT_OP_DIV_LONG /* 0x9e */
    .long .L_ALT_OP_REM_LONG /* 0x9f */
    .long .L_ALT_OP_AND_LONG /* 0xa0 */
    .long .L_ALT_OP_OR_LONG /* 0xa1 */
    .long .L_ALT_OP_XOR_LONG /* 0xa2 */
    .long .L_ALT_OP_SHL_LONG /* 0xa3 */
    .long .L_ALT_OP_SHR_LONG /* 0xa4 */
    .long .L_ALT_OP_USHR_LONG /* 0xa5 */
    .long .L_ALT_OP_ADD_FLOAT /* 0xa6 */
    .long .L_ALT_OP_SUB_FLOAT /* 0xa7 */
    .long .L_ALT_OP_MUL_FLOAT /* 0xa8 */
    .long .L_ALT_OP_DIV_FLOAT /* 0xa9 */
    .long .L_ALT_OP_REM_FLOAT /* 0xaa */
    .long .L_ALT_OP_ADD_DOUBLE /* 0xab */
    .long .L_ALT_OP_SUB_DOUBLE /* 0xac */
    .long .L_ALT_OP_MUL_DOUBLE /* 0xad */
    .long .L_ALT_OP_DIV_DOUBLE /* 0xae */
    .long .L_ALT_OP_REM_DOUBLE /* 0xaf */
    .long .L_ALT_OP_ADD_INT_2ADDR /* 0xb0 */
    .long .L_ALT_OP_SUB_INT_2ADDR /* 0xb1 */
    .long .L_ALT_OP_MUL_INT_2ADDR /* 0xb2 */
    .long .L_ALT_OP_DIV_INT_2ADDR /* 0xb3 */
    .long .L_ALT_OP_REM_INT_2ADDR /* 0xb4 */
    .long .L_ALT_OP_AND_INT_2ADDR /* 0xb5 */
    .long .L_ALT_OP_OR_INT_2ADDR /* 0xb6 */
    .long .L_ALT_OP_XOR_INT_2ADDR /* 0xb7 */
    .long .L_ALT_OP_SHL_INT_2ADDR /* 0xb8 */
    .long .L_ALT_OP_SHR_INT_2ADDR /* 0xb9 */
    .long .L_ALT_OP_USHR_INT_2ADDR /* 0xba */
    .long .L_ALT_OP_ADD_LONG_2ADDR /* 0xbb */
    .long .L_ALT_OP_SUB_LONG_2ADDR /* 0xbc */
    .long .L_ALT_OP_MUL_LONG_2ADDR /* 0xbd */
    .long .L_ALT_OP_DIV_LONG_2ADDR /* 0xbe */
    .long .L_ALT_OP_REM_LONG_2ADDR /* 0xbf */
    .long .L_ALT_OP_AND_LONG_2ADDR /* 0xc0 */
    .long .L_ALT_OP_OR_LONG_2ADDR /* 0xc1 */
    .long .L_ALT_OP_XOR_LONG_2ADDR /* 0xc2 */
    .long .L_ALT_OP_SHL_LONG_2ADDR /* 0xc3 */
    .long .L_ALT_OP_SHR_LONG_2ADDR /* 0xc4 */
    .long .L_ALT_OP_USHR_LONG_2ADDR /* 0xc5 */
    .long .L_ALT_OP_ADD_FLOAT_2ADDR /* 0xc6 */
    .long .L_ALT_OP_SUB_FLOAT_2ADDR /* 0xc7 */
    .long .L_ALT_OP_MUL_FLOAT_2ADDR /* 0xc8 */
    .long .L_ALT_OP_DIV_FLOAT_2ADDR /* 0xc9 */
    .long .L_ALT_OP_REM_FLOAT_2ADDR /* 0xca */
    .long .L_ALT_OP_ADD_DOUBLE_2ADDR /* 0xcb */
    .long .L_ALT_OP_SUB_DOUBLE_2ADDR /* 0xcc */
    .long .L_ALT_OP_MUL_DOUBLE_2ADDR /* 0xcd */
    .long .L_ALT_OP_DIV_DOUBLE_2ADDR /* 0xce */
    .long .L_ALT_OP_REM_DOUBLE_2ADDR /* 0xcf */
    .long .L_ALT_OP_ADD_INT_LIT16 /* 0xd0 */
    .long .L_ALT_OP_RSUB_INT /* 0xd1 */
    .long .L_ALT_OP_MUL_INT_LIT16 /* 0xd2 */
    .long .L_ALT_OP_DIV_INT_LIT16 /* 0xd3 */
    .long .L_ALT_OP_REM_INT_LIT16 /* 0xd4 */
    .long .L_ALT_OP_AND_INT_LIT16 /* 0xd5 */
    .long .L_ALT_OP_OR_INT_LIT16 /* 0xd6 */
    .long .L_ALT_OP_XOR_INT_LIT16 /* 0xd7 */
    .long .L_ALT_OP_ADD_INT_LIT8 /* 0xd8 */
    .long .L_ALT_OP_RSUB_INT_LIT8 /* 0xd9 */
    .long .L_ALT_OP_MUL_INT_LIT8 /* 0xda */
    .long .L_ALT_OP_DIV_INT_LIT8 /* 0xdb */
    .long .L_ALT_OP_REM_INT_LIT8 /* 0xdc */
    .long .L_ALT_OP_AND_INT_LIT8 /* 0xdd */
    .long .L_ALT_OP_OR_INT_LIT8 /* 0xde */
    .long .L_ALT_OP_XOR_INT_LIT8 /* 0xdf */
    .long .L_ALT_OP_SHL_INT_LIT8 /* 0xe0 */
    .long .L_ALT_OP_SHR_INT_LIT8 /* 0xe1 */
    .long .L_ALT_OP_USHR_INT_LIT8 /* 0xe2 */
    .long .L_ALT_OP_IGET_VOLATILE /* 0xe3 */
    .long .L_ALT_OP_IPUT_VOLATILE /* 0xe4 */
    .long .L_ALT_OP_SGET_VOLATILE /* 0xe5 */
    .long .L_ALT_OP_SPUT_VOLATILE /* 0xe6 */
    .long .L_ALT_OP_IGET_OBJECT_VOLATILE /* 0xe7 */
    .long .L_ALT_OP_IGET_WIDE_VOLATILE /* 0xe8 */
    .long .L_ALT_OP_IPUT_WIDE_VOLATILE /* 0xe9 */
    .long .L_ALT_OP_SGET_WIDE_VOLATILE /* 0xea */
    .long .L_ALT_OP_SPUT_WIDE_VOLATILE /* 0xeb */
    .long .L_ALT_OP_BREAKPOINT /* 0xec */
    .long .L_ALT_OP_THROW_VERIFICATION_ERROR /* 0xed */
    .long .L_ALT_OP_EXECUTE_INLINE /* 0xee */
    .long .L_ALT_OP_EXECUTE_INLINE_RANGE /* 0xef */
    .long .L_ALT_OP_INVOKE_OBJECT_INIT_RANGE /* 0xf0 */
    .long .L_ALT_OP_RETURN_VOID_BARRIER /* 0xf1 */
    .long .L_ALT_OP_IGET_QUICK /* 0xf2 */
    .long .L_ALT_OP_IGET_WIDE_QUICK /* 0xf3 */
    .long .L_ALT_OP_IGET_OBJECT_QUICK /* 0xf4 */
    .long .L_ALT_OP_IPUT_QUICK /* 0xf5 */
    .long .L_ALT_OP_IPUT_WIDE_QUICK /* 0xf6 */
    .long .L_ALT_OP_IPUT_OBJECT_QUICK /* 0xf7 */
    .long .L_ALT_OP_INVOKE_VIRTUAL_QUICK /* 0xf8 */
    .long .L_ALT_OP_INVOKE_VIRTUAL_QUICK_RANGE /* 0xf9 */
    .long .L_ALT_OP_INVOKE_SUPER_QUICK /* 0xfa */
    .long .L_ALT_OP_INVOKE_SUPER_QUICK_RANGE /* 0xfb */
    .long .L_ALT_OP_IPUT_OBJECT_VOLATILE /* 0xfc */
    .long .L_ALT_OP_SGET_OBJECT_VOLATILE /* 0xfd */
    .long .L_ALT_OP_SPUT_OBJECT_VOLATILE /* 0xfe */
    .long .L_ALT_OP_UNUSED_FF /* 0xff */
/* File: x86/entry.S */
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


    .text
    .global dvmMterpStdRun
    .type   dvmMterpStdRun, %function
/*
 * bool dvmMterpStdRun(Thread* self)
 *
 * Interpreter entry point.  Returns changeInterp.
 *
 */
dvmMterpStdRun:
    push    %ebp                 # save caller base pointer
    movl    %esp, %ebp           # set our %ebp
    movl    rSELF, %ecx          # get incoming rSELF
/*
 * At this point we've allocated one slot on the stack
 * via push and stack is 8-byte aligned.  Allocate space
 * for 9 spill slots, 4 local slots, 5 arg slots to bring
 * us to 16-byte alignment
 */
    subl    $(FRAME_SIZE-4), %esp

/* Spill callee save regs */
    movl    %edi,EDI_SPILL(%ebp)
    movl    %esi,ESI_SPILL(%ebp)
    movl    %ebx,EBX_SPILL(%ebp)

/* Set up "named" registers */
    movl    offThread_pc(%ecx),rPC
    movl    offThread_curFrame(%ecx),rFP
    movl    offThread_curHandlerTable(%ecx),rIBASE

    /* Remember %esp for future "longjmp" */
    movl    %esp,offThread_bailPtr(%ecx)

    /* Fetch next instruction before potential jump */
    FETCH_INST
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    movl        $0, offThread_inJitCodeCache(%ecx)
    cmpl        $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif

   /* Normal case: start executing the instruction at rPC */
    GOTO_NEXT

    .global dvmMterpStdBail
    .type   dvmMterpStdBail, %function
/*
 * void dvmMterpStdBail(Thread* self, bool changeInterp)
 *
 * Restore the stack pointer and PC from the save point established on entry.
 * This is essentially the same as a longjmp, but should be cheaper.  The
 * last instruction causes us to return to whoever called dvmMterpStdRun.
 *
 * We're not going to build a standard frame here, so the arg accesses will
 * look a little strange.
 *
 * On entry:
 *  esp+4 (arg0)  Thread* self
 *  esp+8 (arg1)  bool changeInterp
 */
dvmMterpStdBail:
    movl    4(%esp),%ecx                 # grab self
    movl    8(%esp),%eax                 # changeInterp to return reg
    movl    offThread_bailPtr(%ecx),%esp # Restore "setjmp" esp
    movl    %esp,%ebp
    addl    $(FRAME_SIZE-4), %ebp       # Restore %ebp at point of setjmp
    movl    EDI_SPILL(%ebp),%edi
    movl    ESI_SPILL(%ebp),%esi
    movl    EBX_SPILL(%ebp),%ebx
    movl    %ebp, %esp                   # strip frame
    pop     %ebp                         # restore caller's ebp
    ret                                  # return to dvmMterpStdRun's caller


#ifdef WITH_JIT
    .global     dvmNcgInvokeInterpreter
    .type       dvmNcgInvokeInterpreter, %function
/* input: start of method in %eax */
dvmNcgInvokeInterpreter:
    movl        %eax, rPC
    movl   rSELF, %ecx
    movl   offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_R %ecx                    # %edx<- opcode
    GOTO_NEXT_R %ecx                    # start executing the instruction at rPC
#endif

/*
 * Strings
 */
    .section    .rodata
.LstrBadEntryPoint:
    .asciz  "Bad entry point %d\n"


/* File: x86/footer.S */
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
 * Common subroutines and data.
 */

#if defined(WITH_JIT)
/*
 * JIT-related re-entries into the interpreter.  In general, if the
 * exit from a translation can at some point be chained, the entry
 * here requires that control arrived via a call, and that the "rp"
 * on TOS is actually a pointer to a 32-bit cell containing the Dalvik PC
 * of the next insn to handle.  If no chaining will happen, the entry
 * should be reached via a direct jump and rPC set beforehand.
 */

    .global dvmJitToInterpPunt
/*
 * The compiler will generate a jump to this entry point when it is
 * having difficulty translating a Dalvik instruction.  We must skip
 * the code cache lookup & prevent chaining to avoid bouncing between
 * the interpreter and code cache. rPC must be set on entry.
 */
dvmJitToInterpPunt:
    GET_PC
#if defined(WITH_JIT_TUNING)
    movl   rPC, OUT_ARG0(%esp)
    call   dvmBumpPunt
#endif
    movl   rSELF, %ecx
    movl   offThread_curHandlerTable(%ecx),rIBASE
    movl        $0, offThread_inJitCodeCache(%ecx)
    FETCH_INST_R %ecx
    GOTO_NEXT_R %ecx

    .global dvmJitToInterpSingleStep
/*
 * Return to the interpreter to handle a single instruction.
 * Should be reached via a call.
 * On entry:
 *   0(%esp)          <= native return address within trace
 *   rPC              <= Dalvik PC of this instruction
 *   OUT_ARG0+4(%esp) <= Dalvik PC of next instruction
 */
dvmJitToInterpSingleStep:
/* TODO */
    call     dvmAbort
#if 0
    pop    %eax
    movl   rSELF, %ecx
    movl   OUT_ARG0(%esp), %edx
    movl   %eax,offThread_jitResumeNPC(%ecx)
    movl   %edx,offThread_jitResumeDPC(%ecx)
    movl   $kInterpEntryInstr,offThread_entryPoint(%ecx)
    movl   $1,rINST     # changeInterp <= true
    jmp    common_gotoBail
#endif

    .global dvmJitToInterpNoChainNoProfile
/*
 * Return from the translation cache to the interpreter to do method
 * invocation.  Check if the translation exists for the callee, but don't
 * chain to it. rPC must be set on entry.
 */
dvmJitToInterpNoChainNoProfile:
#if defined(WITH_JIT_TUNING)
    SPILL_TMP1(%eax)
    call   dvmBumpNoChain
    UNSPILL_TMP1(%eax)
#endif
    movl   %eax, rPC
    movl   rSELF, %eax
    movl   rPC,OUT_ARG0(%esp)
    movl   %eax,OUT_ARG1(%esp)
    call   dvmJitGetTraceAddrThread  # (pc, self)
    movl   rSELF,%ecx                # ecx <- self
    movl   %eax,offThread_inJitCodeCache(%ecx)  # set inJitCodeCache flag
    cmpl   $0, %eax
    jz     1f
    jmp    *%eax                     # exec translation if we've got one
    # won't return
1:
    EXPORT_PC
    movl   rSELF, %ecx
    movl   offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_R %ecx
    GOTO_NEXT_R %ecx

/*
 * Return from the translation cache and immediately request a
 * translation from the exit target, but don't attempt to chain.
 * rPC set on entry.
 */
    .global dvmJitToInterpTraceSelectNoChain
dvmJitToInterpTraceSelectNoChain:
#if defined(WITH_JIT_TUNING)
    movl   %edx, OUT_ARG0(%esp)
    call   dvmBumpNoChain
#endif
    movl   %ebx, rPC
    lea    4(%esp), %esp #to recover the esp update due to function call
    movl   rSELF, %eax
    movl   rPC,OUT_ARG0(%esp)
    movl   %eax,OUT_ARG1(%esp)
    call   dvmJitGetTraceAddrThread  # (pc, self)
    movl   rSELF,%ecx
    cmpl   $0,%eax
    movl   %eax,offThread_inJitCodeCache(%ecx)  # set inJitCodeCache flag
    jz     1f
    jmp    *%eax              # jump to tranlation
    # won't return

/* No Translation - request one */
1:
    GET_JIT_PROF_TABLE %ecx %eax
    cmpl   $0, %eax          # JIT enabled?
    jnz    2f                 # Request one if so
    movl   rSELF, %ecx
    movl   offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_R %ecx         # Continue interpreting if not
    GOTO_NEXT_R %ecx
2:
    ## Looks like an EXPORT_PC is needed here. Now jmp to common_selectTrace2
    movl   $kJitTSelectRequestHot,%eax # ask for trace select
    jmp    common_selectTrace

/*
 * Return from the translation cache and immediately request a
 * translation for the exit target.  Reached via a call, and
 * (TOS)->rPC.
 */
    .global dvmJitToInterpTraceSelect
dvmJitToInterpTraceSelect:
    movl   0(%esp), %eax          # get return address
    movl   %ebx, rPC              # get first argument (target rPC)

    ## TODO, need to clean up stack manipulation ... this isn't signal safe and
    ## doesn't use the calling conventions of header.S
    lea    4(%esp), %esp #to recover the esp update due to function call

    ## An additional 5B instruction "jump 0" was added for a thread-safe
    ## chaining cell update in JIT code cache. So the offset is now -17=-12-5.
    lea    -17(%eax), %ebx #$JIT_OFFSET_CHAIN_START(%eax), %ebx
    lea    -4(%esp), %esp
    movl   rSELF, %eax
    movl   rPC,OUT_ARG0(%esp)
    movl   %eax,OUT_ARG1(%esp)
    call   dvmJitGetTraceAddrThread # (pc, self)
    lea    4(%esp), %esp
    cmpl   $0,%eax
    movl   rSELF, %ecx
    movl   %eax,offThread_inJitCodeCache(%ecx)  # set inJitCodeCache flag
    jz     1b                 # no - ask for one
    movl   %eax,OUT_ARG0(%esp)
    movl   rINST,OUT_ARG1(%esp)
    call   dvmJitChain        # Attempt dvmJitChain(codeAddr,chainAddr)
    cmpl   $0,%eax           # Success?
    jz     toInterpreter      # didn't chain - interpret
    jmp    *%eax
    # won't return

/*
 * Placeholder entries for x86 JIT
 */
    .global dvmJitToInterpBackwardBranch
dvmJitToInterpBackwardBranch:

    .global     dvmJitToExceptionThrown
dvmJitToExceptionThrown: //rPC in
    movl   rSELF, %edx
    GET_PC
    movl   $0, offThread_inJitCodeCache(%edx)
    jmp common_exceptionThrown

    .global dvmJitToInterpNormal
dvmJitToInterpNormal:
/* one input: the target rPC value */
    movl        0(%esp), %eax          # get return address
    movl        %ebx, rPC              # get first argument (target rPC)

    ## TODO, need to clean up stack manipulation ... this isn't signal safe and
    ## doesn't use the calling conventions of header.S

    ## An additional 5B instruction "jump 0" was added for a thread-safe
    ## chaining cell update in JIT code cache. So the offset is now -17=-12-5.
    lea         -17(%eax), %ebx #$JIT_OFFSET_CHAIN_START(%eax), %ebx
    lea         4(%esp), %esp
    movl        rPC, OUT_ARG0(%esp)
    movl        rSELF, %ecx
    movl        %ecx, OUT_ARG1(%esp)
    call        dvmJitGetTraceAddrThread
    ## Here is the change from using rGLUE to rSELF for accessing the
    ## JIT code cache flag
    movl        rSELF, %ecx
    movl        %eax, offThread_inJitCodeCache(%ecx)  # set inJitCodeCache flag
    #lea         4(%esp), %esp
    cmp         $0, %eax
    je          toInterpreter
    #lea         -8(%esp), %esp
    movl        %ebx, OUT_ARG1(%esp)    # %ebx live thorugh dvmJitGetTraceAddrThread
    movl        %eax, OUT_ARG0(%esp)    # first argument
    call        dvmJitChain
    #lea         8(%esp), %esp
    cmp         $0, %eax
    je          toInterpreter
    jmp         *%eax                   #to native address

    .global dvmJitToInterpNoChain
dvmJitToInterpNoChain:
dvmJitToInterpNoChain: #rPC in eax
#if defined(WITH_JIT_TUNING)
    SPILL_TMP1(%eax)
    call   dvmBumpNoChain
    UNSPILL_TMP1(%eax)
#endif
    ## TODO, need to clean up stack manipulation ... this isn't signal safe and
    ## doesn't use the calling conventions of header.S
    movl        %eax, rPC
    movl        rPC, OUT_ARG0(%esp)
    movl        rSELF, %ecx
    movl        %ecx, OUT_ARG1(%esp)
    call        dvmJitGetTraceAddrThread
    ## Here is the change from using rGLUE to rSELF for accessing the
    ## JIT code cache flag
    movl        rSELF, %ecx
    movl        %eax, offThread_inJitCodeCache(%ecx)  # set inJitCodeCache flag
    cmp         $0, %eax
    je          toInterpreter
    jmp         *%eax                   #to native address

toInterpreter:
    EXPORT_PC
    movl        rSELF, %ecx
    movl        offThread_curHandlerTable(%ecx), rIBASE
    FETCH_INST
    movl        offThread_pJitProfTable(%ecx), %eax
    #Fallthrough

/* ebx holds the pointer to the jit profile table
   edx has the opCode */
common_testUpdateProfile:
    cmp         $0, %eax
    je          4f
/* eax holds the pointer to the jit profile table
   edx has the opCode
   rPC points to the next bytecode */

common_updateProfile:
    # quick & dirty hash
    movl   rPC, %ecx
    shrl   $12, %ecx
    xorl   rPC, %ecx
    andl   $((1<<JIT_PROF_SIZE_LOG_2)-1), %ecx
    decb   (%ecx,%eax)
    #jmp    1f # remove
    jz     2f
1:
    GOTO_NEXT
2:
common_Profile:
/*
 * Here, we switch to the debug interpreter to request
 * trace selection.  First, though, check to see if there
 * is already a native translation in place (and, if so,
 * jump to it now.
 */
    SPILL(rIBASE)
    SPILL_TMP1(rINST)
    movl        rSELF, rIBASE
    GET_JIT_THRESHOLD rIBASE rINST  # leaves rSELF in %ecx
    EXPORT_PC
    movb   rINSTbl,(%ecx,%eax)   # reset counter
    movl   rIBASE,rINST            # preserve rSELF
    movl   rSELF, %eax
    movl   rPC,OUT_ARG0(%esp)
    movl   rIBASE,OUT_ARG1(%esp)
    call   dvmJitGetTraceAddrThread  # (pc, self)
    UNSPILL(rIBASE)
    movl   %eax,offThread_inJitCodeCache(rINST)   # set the inJitCodeCache flag
    UNSPILL_TMP1(rINST)
    cmpl   $0,%eax
    #jmp    1f # remove
    jz     1f
    jmp   *%eax        # TODO: decide call vs/ jmp!.  No return either way
1:
    movl   $kJitTSelectRequest,%eax
    # On entry, eax<- jitState, rPC valid
common_selectTrace:
    mov         %ebx, EBX_SPILL(%ebp)
    movl        rSELF, %ebx
    movzwl      offThread_subMode(%ebx), %ecx
    and         $(kSubModeJitTraceBuild | kSubModeJitSV), %ecx
    jne         3f                     # already doing JIT work, continue
    movl        %eax, offThread_jitState(%ebx)
    movl        rSELF, %eax
    movl       %eax, OUT_ARG0(%esp)

/*
 * Call out to validate trace-building request. If successful, rIBASE will be swapped
 * to send us into single-steppign trace building mode, so we need to refresh before
 * we continue.
 */

   EXPORT_PC
   SAVE_PC_FP_TO_SELF %ecx
   call dvmJitCheckTraceRequest
3:
   mov          EBX_SPILL(%ebp), %ebx
   FETCH_INST
   movl rSELF, %ecx
   movl offThread_curHandlerTable(%ecx), rIBASE
4:
   GOTO_NEXT

common_selectTrace2:
    mov         %ebx, EBX_SPILL(%ebp)
    movl        rSELF, %ebx
    movl        %ebx, OUT_ARG0(%esp)
    movl        %eax, offThread_jitState(%ebx)
    movzwl      offThread_subMode(%ebx), %ecx
    mov         EBX_SPILL(%ebp), %ebx
    and         (kSubModeJitTraceBuild | kSubModeJitSV), %ecx
    jne         3f                     # already doing JIT work, continue



/*
 * Call out to validate trace-building request. If successful, rIBASE will be swapped
 * to send us into single-steppign trace building mode, so we need to refresh before
 * we continue.
 */

   EXPORT_PC
   SAVE_PC_FP_TO_SELF %ecx
   call dvmJitCheckTraceRequest
3:
   FETCH_INST
   movl rSELF, %ecx
   movl offThread_curHandlerTable(%ecx), rIBASE
4:
   GOTO_NEXT

#endif

/*
 * For the invoke codes we need to know what register holds the "this" pointer. However
 * it seems the this pointer is assigned consistently most times it is in %ecx but other
 * times it is in OP_INVOKE_INTERFACE, OP_INVOKE_SUPER_QUICK, or OP_INVOKE_VIRTUAL_QUICK.
*/

/*
 * Common code for method invocation with range.
 *
 * On entry:
 *   eax = Method* methodToCall
 *   ecx = "this"
 *   rINSTw trashed, must reload
 *   rIBASE trashed, must reload before resuming interpreter
 */

common_invokeMethodRange:
.LinvokeNewRange:
#if defined(WITH_JIT)
    SPILL_TMP1(%edx)
    SPILL_TMP2(%ebx)
    movl        rSELF, %edx
    movzwl      offThread_subMode(%edx), %ebx
    and         $kSubModeJitTraceBuild, %ebx
    jz          6f
    call        save_callsiteinfo
6:
    UNSPILL_TMP2(%ebx)
    UNSPILL_TMP1(%edx)
#endif
   /*
    * prepare to copy args to "outs" area of current frame
    */

    movzbl      1(rPC),rINST       # rINST<- AA
    movzwl      4(rPC), %ecx            # %ecx<- CCCC
    SAVEAREA_FROM_FP %edx               # %edx<- &StackSaveArea
    test        rINST, rINST
    movl        rINST, LOCAL0_OFFSET(%ebp) # LOCAL0_OFFSET(%ebp)<- AA
    jz          .LinvokeArgsDone        # no args; jump to args done


   /*
    * %eax=methodToCall, %ecx=CCCC, LOCAL0_OFFSET(%ebp)=count,
    * %edx=&outs (&stackSaveArea).  (very few methods have > 10 args;
    * could unroll for common cases)
    */

.LinvokeRangeArgs:
    movl        %ebx, LOCAL1_OFFSET(%ebp)       # LOCAL1_OFFSET(%ebp)<- save %ebx
    lea         (rFP, %ecx, 4), %ecx    # %ecx<- &vCCCC
    shll        $2, LOCAL0_OFFSET(%ebp)        # LOCAL0_OFFSET(%ebp)<- offset
    subl        LOCAL0_OFFSET(%ebp), %edx       # %edx<- update &outs
    shrl        $2, LOCAL0_OFFSET(%ebp)        # LOCAL0_OFFSET(%ebp)<- offset
1:
    movl        (%ecx), %ebx            # %ebx<- vCCCC
    lea         4(%ecx), %ecx           # %ecx<- &vCCCC++
    subl        $1, LOCAL0_OFFSET(%ebp)        # LOCAL0_OFFSET<- LOCAL0_OFFSET--
    movl        %ebx, (%edx)            # *outs<- vCCCC
    lea         4(%edx), %edx           # outs++
    jne         1b                      # loop if count (LOCAL0_OFFSET(%ebp)) not zero
    movl        LOCAL1_OFFSET(%ebp), %ebx       # %ebx<- restore %ebx
    jmp         .LinvokeArgsDone        # continue

   /*
    * %eax is "Method* methodToCall", the method we're trying to call
    * prepare to copy args to "outs" area of current frame
    * rIBASE trashed, must reload before resuming interpreter
    */

common_invokeMethodNoRange:
#if defined(WITH_JIT)
    SPILL_TMP1(%edx)
    SPILL_TMP2(%ebx)
    movl        rSELF, %edx
    movzwl      offThread_subMode(%edx), %ebx
    and         $kSubModeJitTraceBuild, %ebx
    jz          6f
    call        save_callsiteinfo
6:
    UNSPILL_TMP2(%ebx)
    UNSPILL_TMP1(%edx)
#endif
.LinvokeNewNoRange:
    movzbl      1(rPC),rINST       # rINST<- BA
    movl        rINST, LOCAL0_OFFSET(%ebp) # LOCAL0_OFFSET(%ebp)<- BA
    shrl        $4, LOCAL0_OFFSET(%ebp)        # LOCAL0_OFFSET(%ebp)<- B
    je          .LinvokeArgsDone        # no args; jump to args done
    movzwl      4(rPC), %ecx            # %ecx<- GFED
    SAVEAREA_FROM_FP %edx               # %edx<- &StackSaveArea

   /*
    * %eax=methodToCall, %ecx=GFED, LOCAL0_OFFSET(%ebp)=count, %edx=outs
    */

.LinvokeNonRange:
    cmp         $2, LOCAL0_OFFSET(%ebp)        # compare LOCAL0_OFFSET(%ebp) to 2
    movl        %ecx, LOCAL1_OFFSET(%ebp)       # LOCAL1_OFFSET(%ebp)<- GFED
    jl          1f                      # handle 1 arg
    je          2f                      # handle 2 args
    cmp         $4, LOCAL0_OFFSET(%ebp)        # compare LOCAL0_OFFSET(%ebp) to 4
    jl          3f                      # handle 3 args
    je          4f                      # handle 4 args
5:
    andl        $15, rINST             # rINSTw<- A
    lea         -4(%edx), %edx          # %edx<- update &outs; &outs--
    movl        (rFP, rINST, 4), %ecx   # %ecx<- vA
    movl        %ecx, (%edx)            # *outs<- vA
    movl        LOCAL1_OFFSET(%ebp), %ecx       # %ecx<- GFED
4:
    shr         $12, %ecx              # %ecx<- G
    lea         -4(%edx), %edx          # %edx<- update &outs; &outs--
    movl        (rFP, %ecx, 4), %ecx    # %ecx<- vG
    movl        %ecx, (%edx)            # *outs<- vG
    movl        LOCAL1_OFFSET(%ebp), %ecx       # %ecx<- GFED
3:
    and         $0x0f00, %ecx          # %ecx<- 0F00
    shr         $8, %ecx               # %ecx<- F
    lea         -4(%edx), %edx          # %edx<- update &outs; &outs--
    movl        (rFP, %ecx, 4), %ecx    # %ecx<- vF
    movl        %ecx, (%edx)            # *outs<- vF
    movl        LOCAL1_OFFSET(%ebp), %ecx       # %ecx<- GFED
2:
    and         $0x00f0, %ecx          # %ecx<- 00E0
    shr         $4, %ecx               # %ecx<- E
    lea         -4(%edx), %edx          # %edx<- update &outs; &outs--
    movl        (rFP, %ecx, 4), %ecx    # %ecx<- vE
    movl        %ecx, (%edx)            # *outs<- vE
    movl        LOCAL1_OFFSET(%ebp), %ecx       # %ecx<- GFED
1:
    and         $0x000f, %ecx          # %ecx<- 000D
    movl        (rFP, %ecx, 4), %ecx    # %ecx<- vD
    movl        %ecx, -4(%edx)          # *--outs<- vD
0:

   /*
    * %eax is "Method* methodToCall", the method we're trying to call
    * find space for the new stack frame, check for overflow
    */

.LinvokeArgsDone:
    movzwl      offMethod_registersSize(%eax), %edx # %edx<- methodToCall->regsSize
    movzwl      offMethod_outsSize(%eax), %ecx # %ecx<- methodToCall->outsSize
    movl        %eax, LOCAL0_OFFSET(%ebp)       # LOCAL0_OFFSET<- methodToCall
    shl         $2, %edx               # %edx<- update offset
    SAVEAREA_FROM_FP %eax               # %eax<- &StackSaveArea
    subl        %edx, %eax              # %eax<- newFP; (old savearea - regsSize)
    movl        rSELF,%edx              # %edx<- pthread
    movl        %eax, LOCAL1_OFFSET(%ebp)       # LOCAL1_OFFSET(%ebp)<- &outs
    subl        $sizeofStackSaveArea, %eax # %eax<- newSaveArea (stack save area using newFP)
    movl        offThread_interpStackEnd(%edx), %edx # %edx<- self->interpStackEnd
    movl        %edx, TMP_SPILL1(%ebp)  # spill self->interpStackEnd
    shl         $2, %ecx               # %ecx<- update offset for outsSize
    movl        %eax, %edx              # %edx<- newSaveArea
    sub         %ecx, %eax              # %eax<- bottom; (newSaveArea - outsSize)
    cmp         TMP_SPILL1(%ebp), %eax  # compare interpStackEnd and bottom
    movl        LOCAL0_OFFSET(%ebp), %eax       # %eax<- restore methodToCall
    jl          .LstackOverflow         # handle frame overflow

   /*
    * set up newSaveArea
    */

#ifdef EASY_GDB
    SAVEAREA_FROM_FP %ecx               # %ecx<- &StackSaveArea
    movl        %ecx, offStackSaveArea_prevSave(%edx) # newSaveArea->prevSave<- &outs
#endif
    movl        rSELF,%ecx              # %ecx<- pthread
    movl        rFP, offStackSaveArea_prevFrame(%edx) # newSaveArea->prevFrame<- rFP
    movl        rPC, offStackSaveArea_savedPc(%edx) # newSaveArea->savedPc<- rPC
#if defined(WITH_JIT)
    movl        $0, offStackSaveArea_returnAddr(%edx)
#endif

    /* Any special actions to take? */
    cmpw        $0, offThread_subMode(%ecx)
    jne         2f                     # Yes - handle them
1:
    testl       $ACC_NATIVE, offMethod_accessFlags(%eax) # check for native call
    movl        %eax, offStackSaveArea_method(%edx) # newSaveArea->method<- method to call
    jne         .LinvokeNative          # handle native call

   /*
    * Update "self" values for the new method
    * %eax=methodToCall, LOCAL1_OFFSET(%ebp)=newFp
    */
    movl        offMethod_clazz(%eax), %edx # %edx<- method->clazz
    movl        offClassObject_pDvmDex(%edx), %edx # %edx<- method->clazz->pDvmDex
    movl        %eax, offThread_method(%ecx) # self->method<- methodToCall
    movl        %edx, offThread_methodClassDex(%ecx) # self->methodClassDex<- method->clazz->pDvmDex
    movl        offMethod_insns(%eax), rPC # rPC<- methodToCall->insns
    movl        $1, offThread_debugIsMethodEntry(%ecx)
    movl        LOCAL1_OFFSET(%ebp), rFP # rFP<- newFP
    movl        rFP, offThread_curFrame(%ecx) # curFrame<-newFP
    movl        offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST
#if defined(WITH_JIT)
    /* rPC is already updated */
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT                           # jump to methodToCall->insns

2:
    /*
     * On entry, preserve all:
     *  %eax: method
     *  %ecx: self
     *  %edx: new save area
     */
    SPILL_TMP1(%eax)                   # preserve methodToCall
    SPILL_TMP2(%edx)                   # preserve newSaveArea
    movl        rPC, offThread_pc(%ecx) # update interpSave.pc
    movl        %ecx, OUT_ARG0(%esp)
    movl        %eax, OUT_ARG1(%esp)
    call        dvmReportInvoke        # (self, method)
    UNSPILL_TMP1(%eax)
    UNSPILL_TMP2(%edx)
    movl        rSELF,%ecx             # restore rSELF
    jmp         1b

   /*
    * Prep for the native call
    * %eax=methodToCall, LOCAL1_OFFSET(%ebp)=newFP, %edx=newSaveArea, %ecx=self
    */

.LinvokeNative:
    movl        offThread_jniLocal_topCookie(%ecx), rINST # rINST<- self->localRef->...
    movl        rINST, offStackSaveArea_localRefCookie(%edx) # newSaveArea->localRefCookie<- top
    movl        %edx, LOCAL2_OFFSET(%ebp)  # save newSaveArea
    movl        LOCAL1_OFFSET(%ebp), rINST # rINST<- newFP
    movl        rINST, offThread_curFrame(%ecx)  # curFrame<- newFP
    cmpw        $0, offThread_subMode(%ecx)  # Anything special going on?
    jne         11f                     # yes - handle it
    movl        %ecx, OUT_ARG3(%esp)    # push parameter self
    movl        %eax, OUT_ARG2(%esp)    # push parameter methodToCall
    lea         offThread_retval(%ecx), %ecx # %ecx<- &retval
    movl        %ecx, OUT_ARG1(%esp)    # push parameter &retval
    movl        rINST, OUT_ARG0(%esp)    # push parameter newFP
    call        *offMethod_nativeFunc(%eax) # call methodToCall->nativeFunc
7:
    movl        LOCAL2_OFFSET(%ebp), %ecx    # %ecx<- newSaveArea
    movl        rSELF, %eax             # %eax<- self
    movl        offStackSaveArea_localRefCookie(%ecx), %edx # %edx<- old top
    cmp         $0, offThread_exception(%eax) # check for exception
    movl        rFP, offThread_curFrame(%eax) # curFrame<- rFP
    movl        %edx, offThread_jniLocal_topCookie(%eax) # new top <- old top
    jne         common_exceptionThrown  # handle exception
    movl        offThread_curHandlerTable(%eax),rIBASE
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx                    # jump to next instruction

11:
    /*
     * Handle any special subMode actions
     * %eax=methodToCall, rINST=newFP, %ecx=self
     */
    SPILL_TMP1(%eax)                    # save methodTocall
    movl        rPC, offThread_pc(%ecx)
    movl        %ecx, OUT_ARG1(%esp)
    movl        %eax, OUT_ARG0(%esp)
    movl        rFP, OUT_ARG2(%esp)
    call        dvmReportPreNativeInvoke # (methodToCall, self, fp)
    UNSPILL_TMP1(%eax)                  # restore methodToCall
    movl        rSELF,%ecx              # restore self

    /* Do the native call */
    movl        %ecx, OUT_ARG3(%esp)    # push parameter self
    lea         offThread_retval(%ecx), %ecx # %ecx<- &retval
    movl        %eax, OUT_ARG2(%esp)    # push parameter methodToCall
    movl        %ecx, OUT_ARG1(%esp)    # push parameter &retval
    movl        rINST, OUT_ARG0(%esp)   # push parameter newFP
    call        *offMethod_nativeFunc(%eax) # call methodToCall->nativeFunc

    UNSPILL_TMP1(%eax)                  # restore methodToCall
    movl        rSELF, %ecx
    movl        %ecx, OUT_ARG1(%esp)
    movl        %eax, OUT_ARG0(%esp)
    movl        rFP, OUT_ARG2(%esp)
    call        dvmReportPostNativeInvoke # (methodToCall, self, fp)
    jmp         7b                      # rejoin

.LstackOverflow:    # eax=methodToCall
    movl        %eax, OUT_ARG1(%esp)    # push parameter methodToCall
    movl        rSELF,%eax              # %eax<- self
    movl        %eax, OUT_ARG0(%esp)    # push parameter self
    call        dvmHandleStackOverflow  # call: (Thread* self, Method* meth)
    jmp         common_exceptionThrown  # handle exception


/*
 * Common code for handling a return instruction
 */
common_returnFromMethod:
    movl    rSELF, %ecx
    SAVEAREA_FROM_FP %eax                       # %eax<- saveArea(old)
    cmpw    $0, offThread_subMode(%ecx)          # special action needed?
    jne     19f                                   # go if so
14:

    movl        offStackSaveArea_prevFrame(%eax), rFP # rFP<- saveArea->PrevFrame
    movl        (offStackSaveArea_method - sizeofStackSaveArea)(rFP), rINST # rINST<- method we are returning to
    cmpl        $0, rINST               # check for break frame
    je          common_gotoBail         # bail if break frame
    movl        offThread_curHandlerTable(%ecx),rIBASE
    movl        offStackSaveArea_savedPc(%eax), rPC # rPC<- saveAreaOld->savedPc
#if defined(WITH_JIT)
    movl        offStackSaveArea_returnAddr(%eax), %ecx
#endif
    movl        rSELF, %eax
    movl        rINST, offThread_method(%eax) # glue->method<- newSave->method
    movl        offMethod_clazz(rINST), rINST # rINST<- method->clazz
    movl        rFP, offThread_curFrame(%eax) # glue->self->curFrame<- rFP
#if defined(WITH_JIT)
    //update self->offThread_inJitCodeCache
    movl        %ecx, offThread_inJitCodeCache(%eax)
#endif
    movl        offClassObject_pDvmDex(rINST), rINST # rINST<- method->clazz->pDvmDex
    movl        rINST, offThread_methodClassDex(%eax) # glue->pDvmDex<- method->clazz->pDvmDex
#if defined(WITH_JIT)
    cmp         $0, %ecx
    je          .returnToBC
    movl        %ecx, %eax
    jmp         *%eax
#endif

.returnToBC:

#if defined(WITH_JIT)
    FETCH_INST_OPCODE  3, %ecx                 # %eax<- next instruction hi; fetch, advance
    // %ecx has the opcode
    addl         $6, rPC               # 3*2 = 6
    SPILL_TMP1   (%ecx)
    movl         rSELF, %ecx
    FETCH_INST
    UNSPILL_TMP1   (%ecx)
    movzbl      1(rPC), rINST
    jmp     *(rIBASE,%ecx,4)
#else
    FETCH_INST_WORD 3
    ADVANCE_PC 3
    GOTO_NEXT
#endif

19:
    /*
     * Handle special subMode actions
     * On entry, rFP: prevFP, %ecx: self, %eax: saveArea
     */
    SPILL_TMP1(%ebx)
    movl     offStackSaveArea_prevFrame(%eax), %ebx # %ebx<- saveArea->PrevFrame
    movl     rPC, offThread_pc(%ecx)          # update interpSave.pc
    movl     %ebx, offThread_curFrame(%ecx)    # update interpSave.curFrame
    movl     %ecx, OUT_ARG0(%esp)             # parameter self
    call     dvmReportReturn                  # (self)
    UNSPILL_TMP1(%ebx)
    movl     rSELF, %ecx                      # restore self
    SAVEAREA_FROM_FP %eax                     # restore saveArea
    jmp      14b


/*
 * Prepare to strip the current frame and "longjump" back to caller of
 * dvmMterpStdRun.
 *
 * on entry:
 *    rINST holds changeInterp
 *    ecx holds self pointer
 *
 * expected profile: dvmMterpStdBail(Thread *self, bool changeInterp)
 */
common_gotoBail:
    movl   rPC,offThread_pc(%ecx)     # export state to self
    movl   rFP,offThread_curFrame(%ecx)
    movl   %ecx,OUT_ARG0(%esp)      # self in arg0
    movl   rINST,OUT_ARG1(%esp)     # changeInterp in arg1
    call   dvmMterpStdBail          # bail out....

/*
 * The JIT's invoke method needs to remember the callsite class and
 * target pair.  Save them here so that they are available to
 * dvmCheckJit following the interpretation of this invoke.
 *
 * eax = Method* methodToCall
 * ecx = "this"
 * edx = rSELF
 * ebx = free to use
 */
#if defined(WITH_JIT)
save_callsiteinfo:
    cmp     $0, %ecx
    je      2f
    movl    offObject_clazz(%ecx), %ecx
2:
    movl    rSELF, %ebx
    movl    %eax, offThread_methodToCall(%ebx)
    movl    %ecx, offThread_callsiteClass(%ebx)
    ret
#endif

#if defined(WITH_JIT)

    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including the current
     * instruction.
     *
     * On entry:
     *     %ecx: &dvmDex->pResFields[field]
     *     %eax:  field pointer (must preserve)
     */
common_verifyField:
    movl    %ebx, TMP_SPILL1(%ebp)
    movl     rSELF, %ebx
    movzwl   offThread_subMode(%ebx), %ebx
    andl     $kSubModeJitTraceBuild, %ebx
    movl    TMP_SPILL1(%ebp), %ebx
    jne      1f
    ret
1:
    movl    (%ecx), %ecx
    cmp     $0, %ecx
    je      1f
    ret
1:
    SPILL_TMP1(%eax)
    SPILL_TMP2(%edx)
    movl     rSELF, %ecx
    # Because we call into this helper from a bytecode, we have
    # to be careful not to write over the return address when using
    # the OUT_ARG macros
    lea      -8(%esp), %esp
    movl     %ecx, OUT_ARG0(%esp)
    movl     rPC, OUT_ARG1(%esp)
    call     dvmJitEndTraceSelect
    lea      8(%esp), %esp
    UNSPILL_TMP2(%edx)
    UNSPILL_TMP1(%eax)
    ret
#endif

/*
 * After returning from a "selfd" function, pull out the updated values
 * and start executing at the next instruction.
 */
common_resumeAfterGlueCall:
     movl  rSELF, %eax
     movl  offThread_pc(%eax),rPC
     movl  offThread_curFrame(%eax),rFP
     movl  offThread_curHandlerTable(%eax),rIBASE
     FETCH_INST
     GOTO_NEXT

/*
 * Integer divide or mod by zero
 */
common_errDivideByZero:
    EXPORT_PC
    movl    $.LstrDivideByZero,%eax
    movl    %eax,OUT_ARG0(%esp)
    call    dvmThrowArithmeticException
    jmp     common_exceptionThrown

/*
 * Attempt to allocate an array with a negative size.
 * On entry, len in eax
 */
common_errNegativeArraySize:
    EXPORT_PC
    movl    %eax,OUT_ARG0(%esp)                  # arg0<- len
    call    dvmThrowNegativeArraySizeException   # (len)
    jmp     common_exceptionThrown

/*
 * Attempt to allocate an array with a negative size.
 * On entry, method name in eax
 */
common_errNoSuchMethod:
    EXPORT_PC
    movl    %eax,OUT_ARG0(%esp)
    call    dvmThrowNoSuchMethodError
    jmp     common_exceptionThrown

/*
 * Hit a null object when we weren't expecting one.  Export the PC, throw a
 * NullPointerException and goto the exception processing code.
 */
common_errNullObject:
    EXPORT_PC
    xorl    %eax,%eax
    movl    %eax,OUT_ARG0(%esp)
    call    dvmThrowNullPointerException
    jmp     common_exceptionThrown

/*
 * Array index exceeds max.
 * On entry:
 *    eax <- array object
 *    ecx <- index
 */
common_errArrayIndex:
    EXPORT_PC
    movl    offArrayObject_length(%eax), %eax
    movl    %eax,OUT_ARG0(%esp)
    movl    %ecx,OUT_ARG1(%esp)
    call    dvmThrowArrayIndexOutOfBoundsException   # args (length, index)
    jmp     common_exceptionThrown

/*
 * Somebody has thrown an exception.  Handle it.
 *
 * If the exception processing code returns to us (instead of falling
 * out of the interpreter), continue with whatever the next instruction
 * now happens to be.
 *
 * NOTE: special subMode handling done in dvmMterp_exceptionThrown
 *
 * This does not return.
 */
common_exceptionThrown:
.LexceptionNew:

    EXPORT_PC
    movl       rSELF, %ecx
    movl       %ecx, OUT_ARG0(%esp)
    call       dvmCheckSuspendPending

    movl       rSELF, %ecx
    movl       offThread_exception(%ecx), %edx   # %edx <- self->exception
    movl       %edx, OUT_ARG0(%esp)
    movl       %ecx, OUT_ARG1(%esp)
    SPILL_TMP1(%edx)
    call       dvmAddTrackedAlloc      # don't let the exception be GCed
    UNSPILL_TMP1(%edx)
    movl       rSELF, %ecx
    movl       offThread_subMode(%ecx), %eax    # get subMode flags
    movl       $0, offThread_exception(%ecx)

    # Special subMode?
    cmpl       $0, %eax                # any special subMode handling needed?
    je         8f                      # go if so

    # Manage debugger bookkeeping
    movl       rPC, offThread_pc(%ecx) # update interpSave.pc
    movl       rFP, offThread_curFrame(%ecx) # update interpSave.curFrame
    movl       %ecx, OUT_ARG0(%esp)
    movl       %edx, OUT_ARG1(%esp)
    SPILL_TMP1(%edx)
    call       dvmReportExceptionThrow # (self, exception)
    UNSPILL_TMP1(%edx)
    movl       rSELF, %ecx

8:
    /*
    * set up args and a local for &fp
    */
    lea        20(%esp), %esp          # raise %esp
    movl       rFP, (%esp)               # save fp
    movl       %esp, %eax              # %eax = &fp
    lea        -20(%esp), %esp         # reset %esp
    movl       %eax, OUT_ARG4(%esp)    # Arg 4 = &fp
    movl       $0, OUT_ARG3(%esp)      # Arg 3 = false
    movl       %edx, OUT_ARG2(%esp)    # Arg 2 = exception
    movl       %ecx, OUT_ARG0(%esp)    # Arg 0 = self

    movl       offThread_method(%ecx), %eax # %eax = self->method
    movl       offMethod_insns(%eax), %eax  # %eax = self->method->insn
    movl       rPC, %ecx
    subl       %eax, %ecx              # %ecx = pc - self->method->insn
    sar        $1, %ecx                # adjust %ecx for code offset
    movl       %ecx, OUT_ARG1(%esp)    # Arg 1 = %ecx

    /* call, %eax gets catchRelPc (a code-unit offset) */
    SPILL_TMP1(%edx)                   # save exception
    call       dvmFindCatchBlock       # call(self, relPc, exc, scan?, &fp)
    UNSPILL_TMP1(%edx)                 # restore exception

    /* fix earlier stack overflow if necessary; may trash rFP */
    movl       rSELF, %ecx
    cmpl       $0, offThread_stackOverflowed(%ecx) # did we overflow?
    je         1f                         # no, skip ahead
    movl       %eax, rFP                  # save relPc result in rFP
    movl       %ecx, OUT_ARG0(%esp)       # Arg 0 = self
    movl       %edx, OUT_ARG1(%esp)       # Arg 1 = exception
    SPILL_TMP1(%edx)
    call       dvmCleanupStackOverflow    # call(self, exception)
    UNSPILL_TMP1(%edx)
    movl       rFP, %eax                  # restore result
    movl       rSELF, %ecx
1:

    /* update frame pointer and check result from dvmFindCatchBlock */
    movl       20(%esp), rFP              # retrieve the updated rFP
    cmpl       $0, %eax                  # is catchRelPc < 0?
    jl         .LnotCaughtLocally

    /* adjust locals to match self->interpSave.curFrame and updated PC */
    SAVEAREA_FROM_FP rINST             # rINST<- new save area
    movl       offStackSaveArea_method(rINST), rINST # rINST<- new method
    movl       rINST, offThread_method(%ecx)         # self->method = new method
    movl       offMethod_clazz(rINST), %ecx          # %ecx = method->clazz
    movl       offMethod_insns(rINST), rINST         # rINST = method->insn
    movl       offClassObject_pDvmDex(%ecx), %ecx    # %ecx = method->clazz->pDvmDex
    lea        (rINST, %eax, 2), rPC      # rPC<- method->insns + catchRelPc
    movl       rSELF, rINST
    movl       %ecx, offThread_methodClassDex(rINST) # self->pDvmDex = method->clazz->pDvmDex

    /* release the tracked alloc on the exception */
    movl       %edx, OUT_ARG0(%esp)       # Arg 0 = exception
    movl       rINST, OUT_ARG1(%esp)      # Arg 1 = self
    SPILL_TMP1(%edx)
    call       dvmReleaseTrackedAlloc     # release the exception
    UNSPILL_TMP1(%edx)

    /* restore the exception if the handler wants it */
    movl       rSELF, %ecx
    FETCH_INST
    movzbl     rINSTbl, %eax
    cmpl       $OP_MOVE_EXCEPTION, %eax   # is it "move-exception"?
    jne        1f
    movl       %edx, offThread_exception(%ecx) # restore exception
1:
    movl       offThread_curHandlerTable(%ecx), rIBASE # refresh rIBASE
    GOTO_NEXT

.LnotCaughtLocally: # %edx = exception
    /* fix stack overflow if necessary */
    movl       rSELF, %ecx
    movl       offThread_stackOverflowed(%ecx), %eax
    cmpl       $0, %eax                   # did we overflow earlier?
    je         1f
    movl       %ecx, OUT_ARG0(%esp)
    movl       %edx, OUT_ARG1(%esp)
    SPILL_TMP1(%edx)
    call       dvmCleanupStackOverflow
    UNSPILL_TMP1(%edx)

1:
    movl       rSELF, %ecx
    movl       %edx, offThread_exception(%ecx) #restore exception
    movl       %edx, OUT_ARG0(%esp)
    movl       %ecx, OUT_ARG1(%esp)
    call       dvmReleaseTrackedAlloc     # release the exception
    movl       rSELF, %ecx
    jmp        common_gotoBail            # bail out

common_abort:
    movl    $0xdeadf00d,%eax
    call     *%eax


/*
 * Strings
 */

    .section     .rodata
.LstrDivideByZero:
    .asciz  "divide by zero"
.LstrFilledNewArrayNotImplA:
    .asciz  "filled-new-array only implemented for 'int'"

